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

/** @brief 外置 SPI Flash 全局互斥锁。 */
K_MUTEX_DEFINE(g_spi_flash_mutex);

/**
 * @brief 外置 SPI NOR Flash 驱动实现。
 */
class ZephyrSpiFlash final : public platform::ISpiFlash {
 public:
  /** @brief 初始化 SPI Flash 设备。 */
  int init() noexcept override;
  /** @brief 从 SPI Flash 读取数据。 */
  int read(off_t offset, void* buffer, size_t len) noexcept override;
  /** @brief 向 SPI Flash 写入数据。 */
  int write(off_t offset, const void* data, size_t len) noexcept override;
  /** @brief 擦除 SPI Flash 指定区域。 */
  int erase(off_t offset, size_t len) noexcept override;
  /** @brief 查询 SPI Flash 总容量。 */
  int get_size(uint64_t& out_size) noexcept override;

 private:
  /** @brief 校验 offset/len 是否在容量边界内。 */
  bool is_range_valid_(off_t offset, size_t len, uint64_t size) const noexcept;
  /** @brief 在持锁状态下检查初始化状态。 */
  bool is_ready_locked_() const noexcept { return initialized_; }

  /** @brief 设备树别名 `spi_flash0` 对应设备。 */
  const struct device* dev_ = DEVICE_DT_GET(DT_ALIAS(spi_flash0));
  /** @brief 初始化标记。 */
  bool initialized_ = false;
  /** @brief 日志接口。 */
  platform::ILogger& log_ = platform::logger();
};

/**
 * @brief 校验访问区间是否越界。
 * @param offset 访问起始偏移。
 * @param len 访问长度。
 * @param size 设备总容量。
 * @return true 合法；false 越界。
 */
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

/**
 * @brief 初始化 SPI Flash 设备并标记可用。
 * @return 0 成功；负值失败。
 */
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

/**
 * @brief 读取 SPI Flash 数据。
 * @param offset 起始偏移。
 * @param buffer 输出缓冲区。
 * @param len 读取长度。
 * @return 0 成功；负值失败。
 */
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

/**
 * @brief 写入 SPI Flash 数据。
 * @param offset 起始偏移。
 * @param data 输入数据缓冲区。
 * @param len 写入长度。
 * @return 0 成功；负值失败。
 */
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

/**
 * @brief 擦除 SPI Flash 区间。
 * @param offset 起始偏移。
 * @param len 擦除长度。
 * @return 0 成功；负值失败。
 */
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

/**
 * @brief 获取 SPI Flash 总容量。
 * @param[out] out_size 返回容量（字节）。
 * @return 0 成功；负值失败。
 */
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

/** @brief 全局 SPI Flash 实例。 */
ZephyrSpiFlash g_spi_flash;

}  // namespace

namespace platform {

/**
 * @brief 获取全局 SPI Flash 实例。
 * @return ISpiFlash 引用。
 */
ISpiFlash& spi_flash_ext() noexcept { return g_spi_flash; }

}  // namespace platform
