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

/** @brief 外置 EEPROM 全局互斥锁。 */
K_MUTEX_DEFINE(g_ext_eeprom_mutex);

/**
 * @brief 外置 AT24C02C EEPROM 驱动实现。
 */
class ZephyrExtEeprom final : public platform::IExtEeprom {
 public:
  /** @brief 初始化 EEPROM 设备。 */
  int init() noexcept override;
  /** @brief 从 EEPROM 读取数据。 */
  int read(size_t offset, void* buffer, size_t len) noexcept override;
  /** @brief 向 EEPROM 写入数据。 */
  int write(size_t offset, const void* data, size_t len) noexcept override;
  /** @brief 获取 EEPROM 容量。 */
  int get_size(size_t& out_size) noexcept override;

 private:
  /** @brief 校验 offset/len 是否在容量边界内。 */
  bool is_range_valid_(size_t offset, size_t len, size_t size) const noexcept;
  /** @brief 在持锁状态下检查初始化状态。 */
  bool is_ready_locked_() const noexcept { return initialized_; }

  /** @brief 设备树别名 `eeprom_0` 对应设备。 */
  const struct device* dev_ = DEVICE_DT_GET(DT_ALIAS(eeprom_0));
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
bool ZephyrExtEeprom::is_range_valid_(const size_t offset, const size_t len,
                                      const size_t size) const noexcept {
  if (offset > size) {
    return false;
  }
  return len <= (size - offset);
}

/**
 * @brief 初始化 EEPROM 设备并标记可用。
 * @return 0 成功；负值失败。
 */
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

/**
 * @brief 读取 EEPROM 数据。
 * @param offset 起始偏移。
 * @param buffer 输出缓冲区。
 * @param len 读取长度。
 * @return 0 成功；负值失败。
 */
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

/**
 * @brief 写入 EEPROM 数据。
 * @param offset 起始偏移。
 * @param data 输入缓冲区。
 * @param len 写入长度。
 * @return 0 成功；负值失败。
 */
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

/**
 * @brief 获取 EEPROM 总容量。
 * @param[out] out_size 返回容量（字节）。
 * @return 0 成功；负值失败。
 */
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

/** @brief 全局 EEPROM 实例。 */
ZephyrExtEeprom g_ext_eeprom;

}  // namespace

namespace platform {

/**
 * @brief 获取全局 EEPROM 实例。
 * @return IExtEeprom 引用。
 */
IExtEeprom& ext_eeprom() noexcept { return g_ext_eeprom; }

}  // namespace platform
