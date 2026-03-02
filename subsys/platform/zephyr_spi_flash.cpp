/**
 * @file zephyr_spi_flash.cpp
 * @brief 外置 W25Q128 SPI NOR Flash 的 Zephyr 驱动封装。
 */

#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/kernel.h>

#include "platform/platform_logger.hpp"
#include "platform/platform_spi_flash.hpp"

namespace {

K_MUTEX_DEFINE(g_spi_flash_mutex);

class ZephyrSpiFlash final : public platform::ISpiFlash {
 public:
  int init() noexcept override;
  int read(off_t offset, void* buffer, size_t len) noexcept override;
  int write(off_t offset, const void* data, size_t len) noexcept override;
  int erase(off_t offset, size_t len) noexcept override;
  int get_size(uint64_t& out_size) noexcept override;

 private:
  bool is_range_valid_(off_t offset, size_t len, uint64_t size) const noexcept;
  bool is_ready_locked_() const noexcept { return initialized_; }

  const struct device* dev_ = DEVICE_DT_GET(DT_ALIAS(spi_flash0));
  bool initialized_ = false;
  platform::ILogger& log_ = platform::logger();
};

bool ZephyrSpiFlash::is_range_valid_(const off_t offset, const size_t len,
                                     const uint64_t size) const noexcept {
  if (offset < 0) {
    return false;
  }
  const uint64_t start = static_cast<uint64_t>(offset);
  if (start > size) {
    return false;
  }
  return static_cast<uint64_t>(len) <= (size - start);
}

int ZephyrSpiFlash::init() noexcept {
  int ret = k_mutex_lock(&g_spi_flash_mutex, K_FOREVER);
  if (ret != 0) {
    return ret;
  }

  if (initialized_) {
    k_mutex_unlock(&g_spi_flash_mutex);
    return 0;
  }

  if (!device_is_ready(dev_)) {
    log_.error("[ext][flash] device not ready", -ENODEV);
    k_mutex_unlock(&g_spi_flash_mutex);
    return -ENODEV;
  }

  initialized_ = true;
  k_mutex_unlock(&g_spi_flash_mutex);
  return 0;
}

int ZephyrSpiFlash::read(const off_t offset, void* buffer, const size_t len) noexcept {
  if (len > 0U && buffer == nullptr) {
    return -EINVAL;
  }

  int ret = k_mutex_lock(&g_spi_flash_mutex, K_FOREVER);
  if (ret != 0) {
    return ret;
  }
  if (!is_ready_locked_()) {
    k_mutex_unlock(&g_spi_flash_mutex);
    return -ENODEV;
  }

  uint64_t size = 0U;
  ret = flash_get_size(dev_, &size);
  if (ret < 0) {
    log_.error("[ext][flash] get size failed", ret);
    k_mutex_unlock(&g_spi_flash_mutex);
    return ret;
  }
  if (!is_range_valid_(offset, len, size)) {
    k_mutex_unlock(&g_spi_flash_mutex);
    return -EINVAL;
  }

  ret = flash_read(dev_, offset, buffer, len);
  if (ret < 0) {
    log_.error("[ext][flash] read failed", ret);
    k_mutex_unlock(&g_spi_flash_mutex);
    return ret;
  }

  k_mutex_unlock(&g_spi_flash_mutex);
  return 0;
}

int ZephyrSpiFlash::write(const off_t offset, const void* data, const size_t len) noexcept {
  if (len > 0U && data == nullptr) {
    return -EINVAL;
  }

  int ret = k_mutex_lock(&g_spi_flash_mutex, K_FOREVER);
  if (ret != 0) {
    return ret;
  }
  if (!is_ready_locked_()) {
    k_mutex_unlock(&g_spi_flash_mutex);
    return -ENODEV;
  }

  uint64_t size = 0U;
  ret = flash_get_size(dev_, &size);
  if (ret < 0) {
    log_.error("[ext][flash] get size failed", ret);
    k_mutex_unlock(&g_spi_flash_mutex);
    return ret;
  }
  if (!is_range_valid_(offset, len, size)) {
    k_mutex_unlock(&g_spi_flash_mutex);
    return -EINVAL;
  }

  ret = flash_write(dev_, offset, data, len);
  if (ret < 0) {
    log_.error("[ext][flash] write failed", ret);
    k_mutex_unlock(&g_spi_flash_mutex);
    return ret;
  }

  k_mutex_unlock(&g_spi_flash_mutex);
  return 0;
}

int ZephyrSpiFlash::erase(const off_t offset, const size_t len) noexcept {
  int ret = k_mutex_lock(&g_spi_flash_mutex, K_FOREVER);
  if (ret != 0) {
    return ret;
  }
  if (!is_ready_locked_()) {
    k_mutex_unlock(&g_spi_flash_mutex);
    return -ENODEV;
  }

  uint64_t size = 0U;
  ret = flash_get_size(dev_, &size);
  if (ret < 0) {
    log_.error("[ext][flash] get size failed", ret);
    k_mutex_unlock(&g_spi_flash_mutex);
    return ret;
  }
  if (!is_range_valid_(offset, len, size)) {
    k_mutex_unlock(&g_spi_flash_mutex);
    return -EINVAL;
  }

  ret = flash_erase(dev_, offset, len);
  if (ret < 0) {
    log_.error("[ext][flash] erase failed", ret);
    k_mutex_unlock(&g_spi_flash_mutex);
    return ret;
  }

  k_mutex_unlock(&g_spi_flash_mutex);
  return 0;
}

int ZephyrSpiFlash::get_size(uint64_t& out_size) noexcept {
  int ret = k_mutex_lock(&g_spi_flash_mutex, K_FOREVER);
  if (ret != 0) {
    return ret;
  }
  if (!is_ready_locked_()) {
    k_mutex_unlock(&g_spi_flash_mutex);
    return -ENODEV;
  }

  ret = flash_get_size(dev_, &out_size);
  if (ret < 0) {
    log_.error("[ext][flash] get size failed", ret);
    k_mutex_unlock(&g_spi_flash_mutex);
    return ret;
  }

  k_mutex_unlock(&g_spi_flash_mutex);
  return 0;
}

ZephyrSpiFlash g_spi_flash;

}  // namespace

namespace platform {

ISpiFlash& spi_flash_ext() noexcept { return g_spi_flash; }

}  // namespace platform
