/**
 * @file zephyr_pca9555.cpp
 * @brief PCA9555 平台驱动 Zephyr 实现。
 */

#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#include <cstdint>

#include "platform/platform_logger.hpp"
#include "platform/platform_pca9555.hpp"

namespace {

#define PCA9555_NODE DT_ALIAS(gpio_expander_0)

K_MUTEX_DEFINE(g_pca9555_mutex);

/** @brief DIP 开关起始引脚（Port0 bit0）。 */
constexpr gpio_pin_t kDipFirstPin = 0U;
/** @brief LED 起始引脚（Port1 bit0）。 */
constexpr gpio_pin_t kLedFirstPin = 8U;
/** @brief 每个端口位宽。 */
constexpr uint8_t kPortWidth = 8U;
/** @brief LED 电气特性：低电平点亮。 */
constexpr bool kLedActiveLow = true;

/**
 * @brief PCA9555 平台驱动实现。
 */
class ZephyrPca9555 final : public platform::IPca9555 {
 public:
  /** @brief 初始化设备并配置端口方向。 */
  int init() noexcept override;
  /** @brief 读取 DIP 开关位图。 */
  int read_dipsw(uint8_t& out_mask) noexcept override;
  /** @brief 设置 LED 输出位图。 */
  int set_leds(uint8_t led_mask) noexcept override;

 private:
  /** @brief 在持锁状态下配置 Port0 输入、Port1 输出。 */
  int configure_port_direction_locked_() noexcept;
  /** @brief 在持锁状态下应用 LED 位图。 */
  int apply_led_mask_locked_(uint8_t led_mask) noexcept;
  /** @brief 在持锁状态下检查初始化状态。 */
  bool is_ready_locked_() const noexcept { return initialized_; }

  /** @brief 设备树别名 `gpio_expander_0` 对应设备。 */
  const struct device* dev_ = DEVICE_DT_GET(PCA9555_NODE);
  /** @brief 初始化标记。 */
  bool initialized_ = false;
  /** @brief 日志接口。 */
  platform::ILogger& log_ = platform::logger();
};

/**
 * @brief 配置端口方向与默认电平。
 * @return 0 成功；负值失败。
 */
int ZephyrPca9555::configure_port_direction_locked_() noexcept {
  for (uint8_t i = 0U; i < kPortWidth; ++i) {
    const int ret = gpio_pin_configure(dev_, static_cast<gpio_pin_t>(kDipFirstPin + i), GPIO_INPUT);
    if (ret < 0) {
      return ret;
    }
  }

  for (uint8_t i = 0U; i < kPortWidth; ++i) {
    const gpio_flags_t init_flags = kLedActiveLow ? GPIO_OUTPUT_HIGH : GPIO_OUTPUT_INACTIVE;
    const int ret = gpio_pin_configure(dev_, static_cast<gpio_pin_t>(kLedFirstPin + i), init_flags);
    if (ret < 0) {
      return ret;
    }
  }

  return 0;
}

/**
 * @brief 根据位图设置 LED 输出。
 * @param led_mask bit0~bit7 对应 Port1 的 8 路 LED。
 * @return 0 成功；负值失败。
 */
int ZephyrPca9555::apply_led_mask_locked_(const uint8_t led_mask) noexcept {
  for (uint8_t i = 0U; i < kPortWidth; ++i) {
    uint8_t logical_on = static_cast<uint8_t>((led_mask >> i) & 0x01U);
    if (kLedActiveLow) {
      logical_on = static_cast<uint8_t>(1U - logical_on);
    }

    const int ret = gpio_pin_set_raw(dev_, static_cast<gpio_pin_t>(kLedFirstPin + i), logical_on);
    if (ret < 0) {
      return ret;
    }
  }
  return 0;
}

/**
 * @brief 初始化 PCA9555 设备并配置端口方向。
 * @return 0 成功；负值失败。
 */
int ZephyrPca9555::init() noexcept {
  int ret = k_mutex_lock(&g_pca9555_mutex, K_FOREVER);
  if (ret != 0) {
    return ret;
  }

  if (initialized_) {
    k_mutex_unlock(&g_pca9555_mutex);
    return 0;
  }

  if (!device_is_ready(dev_)) {
    log_.error("[pca9555] device not ready", -ENODEV);
    k_mutex_unlock(&g_pca9555_mutex);
    return -ENODEV;
  }

  ret = configure_port_direction_locked_();
  if (ret < 0) {
    log_.error("[pca9555] configure direction failed", ret);
    k_mutex_unlock(&g_pca9555_mutex);
    return ret;
  }

  initialized_ = true;
  k_mutex_unlock(&g_pca9555_mutex);
  return 0;
}

/**
 * @brief 读取 DIP 开关输入位图（Port0）。
 * @param[out] out_mask 输出位图。
 * @return 0 成功；负值失败。
 */
int ZephyrPca9555::read_dipsw(uint8_t& out_mask) noexcept {
  out_mask = 0U;

  int ret = k_mutex_lock(&g_pca9555_mutex, K_FOREVER);
  if (ret != 0) {
    return ret;
  }
  if (!is_ready_locked_()) {
    k_mutex_unlock(&g_pca9555_mutex);
    return -ENODEV;
  }

  for (uint8_t i = 0U; i < kPortWidth; ++i) {
    const int level = gpio_pin_get_raw(dev_, static_cast<gpio_pin_t>(kDipFirstPin + i));
    if (level < 0) {
      log_.error("[pca9555] read dip failed", level);
      k_mutex_unlock(&g_pca9555_mutex);
      return level;
    }
    if (level != 0) {
      out_mask = static_cast<uint8_t>(out_mask | static_cast<uint8_t>(1U << i));
    }
  }

  k_mutex_unlock(&g_pca9555_mutex);
  return 0;
}

/**
 * @brief 设置 LED 输出位图（Port1）。
 * @param led_mask 输入位图。
 * @return 0 成功；负值失败。
 */
int ZephyrPca9555::set_leds(const uint8_t led_mask) noexcept {
  int ret = k_mutex_lock(&g_pca9555_mutex, K_FOREVER);
  if (ret != 0) {
    return ret;
  }
  if (!is_ready_locked_()) {
    k_mutex_unlock(&g_pca9555_mutex);
    return -ENODEV;
  }

  ret = apply_led_mask_locked_(led_mask);
  if (ret < 0) {
    log_.error("[pca9555] set led failed", ret);
    k_mutex_unlock(&g_pca9555_mutex);
    return ret;
  }

  k_mutex_unlock(&g_pca9555_mutex);
  return 0;
}

/** @brief 全局 PCA9555 实例。 */
ZephyrPca9555 g_pca9555;

}  // namespace

namespace platform {

/**
 * @brief 获取全局 PCA9555 实例。
 * @return IPca9555 引用。
 */
IPca9555& pca9555() noexcept { return g_pca9555; }

}  // namespace platform
