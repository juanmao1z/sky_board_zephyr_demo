/**
 * @file zephyr_ext_eeprom.cpp
 * @brief 外置 AT24C02C EEPROM 的 Zephyr 驱动封装。
 */

#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/eeprom.h>
#include <zephyr/kernel.h>

#include "platform/platform_ext_eeprom.hpp"
#include "platform/platform_logger.hpp"

namespace {

K_MUTEX_DEFINE(g_ext_eeprom_mutex);

class ZephyrExtEeprom final : public platform::IExtEeprom {
 public:
  int init() noexcept override;
  int read(size_t offset, void* buffer, size_t len) noexcept override;
  int write(size_t offset, const void* data, size_t len) noexcept override;
  int get_size(size_t& out_size) noexcept override;

 private:
  bool is_range_valid_(size_t offset, size_t len, size_t size) const noexcept;
  bool is_ready_locked_() const noexcept { return initialized_; }

  const struct device* dev_ = DEVICE_DT_GET(DT_ALIAS(eeprom_0));
  bool initialized_ = false;
  platform::ILogger& log_ = platform::logger();
};

bool ZephyrExtEeprom::is_range_valid_(const size_t offset, const size_t len,
                                      const size_t size) const noexcept {
  if (offset > size) {
    return false;
  }
  return len <= (size - offset);
}

int ZephyrExtEeprom::init() noexcept {
  int ret = k_mutex_lock(&g_ext_eeprom_mutex, K_FOREVER);
  if (ret != 0) {
    return ret;
  }

  if (initialized_) {
    k_mutex_unlock(&g_ext_eeprom_mutex);
    return 0;
  }

  if (!device_is_ready(dev_)) {
    log_.error("[ext][eeprom] device not ready", -ENODEV);
    k_mutex_unlock(&g_ext_eeprom_mutex);
    return -ENODEV;
  }

  initialized_ = true;
  k_mutex_unlock(&g_ext_eeprom_mutex);
  return 0;
}

int ZephyrExtEeprom::read(const size_t offset, void* buffer, const size_t len) noexcept {
  if (len > 0U && buffer == nullptr) {
    return -EINVAL;
  }

  int ret = k_mutex_lock(&g_ext_eeprom_mutex, K_FOREVER);
  if (ret != 0) {
    return ret;
  }
  if (!is_ready_locked_()) {
    k_mutex_unlock(&g_ext_eeprom_mutex);
    return -ENODEV;
  }

  const size_t size = eeprom_get_size(dev_);
  if (!is_range_valid_(offset, len, size)) {
    k_mutex_unlock(&g_ext_eeprom_mutex);
    return -EINVAL;
  }

  ret = eeprom_read(dev_, static_cast<off_t>(offset), buffer, len);
  if (ret < 0) {
    log_.error("[ext][eeprom] read failed", ret);
    k_mutex_unlock(&g_ext_eeprom_mutex);
    return ret;
  }

  k_mutex_unlock(&g_ext_eeprom_mutex);
  return 0;
}

int ZephyrExtEeprom::write(const size_t offset, const void* data, const size_t len) noexcept {
  if (len > 0U && data == nullptr) {
    return -EINVAL;
  }

  int ret = k_mutex_lock(&g_ext_eeprom_mutex, K_FOREVER);
  if (ret != 0) {
    return ret;
  }
  if (!is_ready_locked_()) {
    k_mutex_unlock(&g_ext_eeprom_mutex);
    return -ENODEV;
  }

  const size_t size = eeprom_get_size(dev_);
  if (!is_range_valid_(offset, len, size)) {
    k_mutex_unlock(&g_ext_eeprom_mutex);
    return -EINVAL;
  }

  ret = eeprom_write(dev_, static_cast<off_t>(offset), data, len);
  if (ret < 0) {
    log_.error("[ext][eeprom] write failed", ret);
    k_mutex_unlock(&g_ext_eeprom_mutex);
    return ret;
  }

  k_mutex_unlock(&g_ext_eeprom_mutex);
  return 0;
}

int ZephyrExtEeprom::get_size(size_t& out_size) noexcept {
  int ret = k_mutex_lock(&g_ext_eeprom_mutex, K_FOREVER);
  if (ret != 0) {
    return ret;
  }
  if (!is_ready_locked_()) {
    k_mutex_unlock(&g_ext_eeprom_mutex);
    return -ENODEV;
  }

  out_size = eeprom_get_size(dev_);
  k_mutex_unlock(&g_ext_eeprom_mutex);
  return 0;
}

ZephyrExtEeprom g_ext_eeprom;

}  // namespace

namespace platform {

IExtEeprom& ext_eeprom() noexcept { return g_ext_eeprom; }

}  // namespace platform
