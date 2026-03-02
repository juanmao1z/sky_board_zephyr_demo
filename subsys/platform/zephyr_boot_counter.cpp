/**
 * @file zephyr_boot_counter.cpp
 * @brief 基于 EEPROM + SPI Flash 冗余的上电计数实现。
 */

#include <errno.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/crc.h>

#include <cstddef>
#include <cstdint>

#include "platform/platform_boot_counter.hpp"
#include "platform/platform_ext_eeprom.hpp"
#include "platform/platform_logger.hpp"
#include "platform/platform_spi_flash.hpp"

namespace {

constexpr uint32_t kBootCounterMagic = 0x424F4F54U;  // 'BOOT'
constexpr uint16_t kBootCounterVersion = 1U;

constexpr size_t kRecordSize = 32U;
constexpr size_t kEepromSlotCount = 2U;
constexpr size_t kEepromSlot0Offset = 0U;
constexpr size_t kEepromSlot1Offset = 32U;
constexpr size_t kEepromReservedBytes = 64U;

constexpr size_t kFlashSectorSize = 4096U;
constexpr size_t kFlashSlotCount = kFlashSectorSize / kRecordSize;

struct BootCounterRecord {
  uint32_t magic = kBootCounterMagic;
  uint16_t version = kBootCounterVersion;
  uint16_t reserved = 0U;
  uint32_t seq = 0U;
  uint32_t count = 0U;
  uint32_t crc32 = 0U;
  uint8_t padding[kRecordSize - 20U] = {};
};

static_assert(sizeof(BootCounterRecord) == kRecordSize, "record size must be 32 bytes");

struct ScanResult {
  bool has_valid = false;
  int latest_slot = -1;
  BootCounterRecord latest_record{};
  int first_empty_slot = -1;
};

K_MUTEX_DEFINE(g_boot_counter_mutex);

uint32_t record_crc32(const BootCounterRecord& record) noexcept {
  return crc32_ieee(reinterpret_cast<const uint8_t*>(&record), offsetof(BootCounterRecord, crc32));
}

bool is_all_ff(const uint8_t* data, const size_t len) noexcept {
  for (size_t i = 0U; i < len; ++i) {
    if (data[i] != 0xFFU) {
      return false;
    }
  }
  return true;
}

bool is_record_valid(const BootCounterRecord& record) noexcept {
  if (record.magic != kBootCounterMagic || record.version != kBootCounterVersion) {
    return false;
  }
  return record.crc32 == record_crc32(record);
}

BootCounterRecord make_record(const uint32_t seq, const uint32_t count) noexcept {
  BootCounterRecord record{};
  record.magic = kBootCounterMagic;
  record.version = kBootCounterVersion;
  record.reserved = 0U;
  record.seq = seq;
  record.count = count;
  record.crc32 = record_crc32(record);
  return record;
}

class ZephyrBootCounter final : public platform::IBootCounter {
 public:
  int init_and_get_status(platform::BootCounterStatus& out) noexcept override;

 private:
  int scan_eeprom(ScanResult& out) noexcept;
  int scan_flash(ScanResult& out, off_t& sector_base) noexcept;
  int update_eeprom(const BootCounterRecord& record, int latest_slot) noexcept;
  int update_flash(const BootCounterRecord& record, const ScanResult& scan, off_t sector_base) noexcept;

  platform::ILogger& log_ = platform::logger();
  bool initialized_ = false;
  bool eeprom_ready_ = false;
  bool flash_ready_ = false;
  uint32_t current_count_ = 0U;
};

int ZephyrBootCounter::scan_eeprom(ScanResult& out) noexcept {
  out = {};

  size_t eeprom_size = 0U;
  int ret = platform::ext_eeprom().get_size(eeprom_size);
  if (ret < 0) {
    return ret;
  }
  if (eeprom_size < kEepromReservedBytes) {
    return -EINVAL;
  }

  const size_t offsets[kEepromSlotCount] = {kEepromSlot0Offset, kEepromSlot1Offset};
  for (size_t i = 0U; i < kEepromSlotCount; ++i) {
    uint8_t raw[kRecordSize] = {};
    ret = platform::ext_eeprom().read(offsets[i], raw, sizeof(raw));
    if (ret < 0) {
      return ret;
    }

    if (is_all_ff(raw, sizeof(raw))) {
      if (out.first_empty_slot < 0) {
        out.first_empty_slot = static_cast<int>(i);
      }
      continue;
    }

    BootCounterRecord record{};
    (void)memcpy(&record, raw, sizeof(record));
    if (!is_record_valid(record)) {
      continue;
    }

    if (!out.has_valid || record.seq > out.latest_record.seq) {
      out.has_valid = true;
      out.latest_slot = static_cast<int>(i);
      out.latest_record = record;
    }
  }

  if (out.first_empty_slot < 0 && !out.has_valid) {
    out.first_empty_slot = 0;
  }
  return 0;
}

int ZephyrBootCounter::scan_flash(ScanResult& out, off_t& sector_base) noexcept {
  out = {};
  sector_base = 0;

  uint64_t flash_size = 0U;
  int ret = platform::spi_flash_ext().get_size(flash_size);
  if (ret < 0) {
    return ret;
  }
  if (flash_size < kFlashSectorSize) {
    return -EINVAL;
  }

  sector_base = static_cast<off_t>(flash_size - kFlashSectorSize);
  for (size_t i = 0U; i < kFlashSlotCount; ++i) {
    uint8_t raw[kRecordSize] = {};
    const off_t offset = sector_base + static_cast<off_t>(i * kRecordSize);
    ret = platform::spi_flash_ext().read(offset, raw, sizeof(raw));
    if (ret < 0) {
      return ret;
    }

    if (is_all_ff(raw, sizeof(raw))) {
      if (out.first_empty_slot < 0) {
        out.first_empty_slot = static_cast<int>(i);
      }
      continue;
    }

    BootCounterRecord record{};
    (void)memcpy(&record, raw, sizeof(record));
    if (!is_record_valid(record)) {
      continue;
    }

    if (!out.has_valid || record.seq > out.latest_record.seq) {
      out.has_valid = true;
      out.latest_slot = static_cast<int>(i);
      out.latest_record = record;
    }
  }

  if (out.first_empty_slot < 0) {
    out.first_empty_slot = -1;
  }
  return 0;
}

int ZephyrBootCounter::update_eeprom(const BootCounterRecord& record, const int latest_slot) noexcept {
  const int next_slot = (latest_slot < 0) ? 0 : ((latest_slot + 1) % static_cast<int>(kEepromSlotCount));
  const size_t offset = (next_slot == 0) ? kEepromSlot0Offset : kEepromSlot1Offset;

  int ret = platform::ext_eeprom().write(offset, &record, sizeof(record));
  if (ret < 0) {
    return ret;
  }

  BootCounterRecord verify{};
  ret = platform::ext_eeprom().read(offset, &verify, sizeof(verify));
  if (ret < 0) {
    return ret;
  }
  if (memcmp(&verify, &record, sizeof(record)) != 0) {
    return -EIO;
  }
  return 0;
}

int ZephyrBootCounter::update_flash(const BootCounterRecord& record, const ScanResult& scan,
                                    const off_t sector_base) noexcept {
  int target_slot = scan.first_empty_slot;
  int ret = 0;

  if (target_slot < 0) {
    ret = platform::spi_flash_ext().erase(sector_base, kFlashSectorSize);
    if (ret < 0) {
      return ret;
    }
    target_slot = 0;
  }

  const off_t write_offset = sector_base + static_cast<off_t>(target_slot * static_cast<int>(kRecordSize));
  ret = platform::spi_flash_ext().write(write_offset, &record, sizeof(record));
  if (ret < 0) {
    return ret;
  }

  BootCounterRecord verify{};
  ret = platform::spi_flash_ext().read(write_offset, &verify, sizeof(verify));
  if (ret < 0) {
    return ret;
  }
  if (memcmp(&verify, &record, sizeof(record)) != 0) {
    return -EIO;
  }
  return 0;
}

int ZephyrBootCounter::init_and_get_status(platform::BootCounterStatus& out) noexcept {
  int ret = k_mutex_lock(&g_boot_counter_mutex, K_FOREVER);
  if (ret != 0) {
    return ret;
  }

  if (initialized_) {
    out.count = current_count_;
    out.eeprom_ready = eeprom_ready_;
    out.flash_ready = flash_ready_;
    k_mutex_unlock(&g_boot_counter_mutex);
    return 0;
  }

  eeprom_ready_ = (platform::ext_eeprom().init() == 0);
  flash_ready_ = (platform::spi_flash_ext().init() == 0);

  ScanResult eeprom_scan{};
  ScanResult flash_scan{};
  off_t flash_sector_base = 0;

  if (eeprom_ready_) {
    ret = scan_eeprom(eeprom_scan);
    if (ret < 0) {
      eeprom_ready_ = false;
      log_.error("[bootcnt] eeprom scan failed", ret);
    }
  }

  if (flash_ready_) {
    ret = scan_flash(flash_scan, flash_sector_base);
    if (ret < 0) {
      flash_ready_ = false;
      log_.error("[bootcnt] flash scan failed", ret);
    }
  }

  const bool eeprom_valid = eeprom_ready_ && eeprom_scan.has_valid;
  const bool flash_valid = flash_ready_ && flash_scan.has_valid;

  uint32_t base_seq = 0U;
  uint32_t base_count = 0U;
  if (eeprom_valid && flash_valid) {
    base_seq = (eeprom_scan.latest_record.seq > flash_scan.latest_record.seq)
                   ? eeprom_scan.latest_record.seq
                   : flash_scan.latest_record.seq;
    base_count = (eeprom_scan.latest_record.count > flash_scan.latest_record.count)
                     ? eeprom_scan.latest_record.count
                     : flash_scan.latest_record.count;
    log_.info("[bootcnt] source=both");
  } else if (eeprom_valid) {
    base_seq = eeprom_scan.latest_record.seq;
    base_count = eeprom_scan.latest_record.count;
    log_.info("[bootcnt] source=eeprom");
  } else if (flash_valid) {
    base_seq = flash_scan.latest_record.seq;
    base_count = flash_scan.latest_record.count;
    log_.info("[bootcnt] source=flash");
  } else {
    log_.info("[bootcnt] source=none");
  }

  const BootCounterRecord new_record = make_record(base_seq + 1U, base_count + 1U);
  current_count_ = new_record.count;

  bool eeprom_update_ok = false;
  bool flash_update_ok = false;

  if (eeprom_ready_) {
    ret = update_eeprom(new_record, eeprom_scan.latest_slot);
    if (ret < 0) {
      eeprom_ready_ = false;
      log_.error("[bootcnt] eeprom update failed", ret);
    } else {
      eeprom_update_ok = true;
      log_.info("[bootcnt] eeprom update ok");
    }
  }

  if (flash_ready_) {
    ret = update_flash(new_record, flash_scan, flash_sector_base);
    if (ret < 0) {
      flash_ready_ = false;
      log_.error("[bootcnt] flash update failed", ret);
    } else {
      flash_update_ok = true;
      log_.info("[bootcnt] flash update ok");
    }
  }

  if (!eeprom_update_ok && !flash_update_ok) {
    log_.error("[bootcnt] no persistent backend available", -ENODEV);
  }

  initialized_ = true;
  out.count = current_count_;
  out.eeprom_ready = eeprom_ready_;
  out.flash_ready = flash_ready_;
  k_mutex_unlock(&g_boot_counter_mutex);
  return 0;
}

ZephyrBootCounter g_boot_counter;

}  // namespace

namespace platform {

IBootCounter& boot_counter() noexcept { return g_boot_counter; }

}  // namespace platform
