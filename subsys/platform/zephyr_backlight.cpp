/**
 * @file zephyr_backlight.cpp
 * @brief IBacklight 的 Zephyr PWM 后端实现。
 */

#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/pwm.h>

#include "platform/platform_backlight.hpp"

namespace {

/** @brief 背光 PWM 节点（aliases: pwm-led0 -> backlight_pwm）。 */
#define BACKLIGHT_PWM_NODE DT_ALIAS(pwm_led0)

/**
 * @brief 基于 Zephyr PWM 的背光实现。
 */
class ZephyrBacklight final : public platform::IBacklight {
 public:
  /**
   * @brief 设置背光开关状态。
   * @param on true 打开；false 关闭。
   * @return 0 成功；负值失败。
   */
  int set_enabled(bool on) noexcept override { return set_brightness(on ? 100U : 0U); }

  /**
   * @brief 设置背光亮度百分比。
   * @param percent 亮度百分比（0~100）。
   * @return 0 成功；负值失败。
   */
  int set_brightness(uint8_t percent) noexcept override {
    if (percent > 100U) {
      percent = 100U;
    }

#if DT_NODE_HAS_STATUS(BACKLIGHT_PWM_NODE, okay)
    static const struct pwm_dt_spec backlight_pwm = PWM_DT_SPEC_GET(BACKLIGHT_PWM_NODE);
    if (!device_is_ready(backlight_pwm.dev)) {
      return -ENODEV;
    }

    /* period 与极性来自 DTS（含 PWM_POLARITY_INVERTED）。 */
    const uint32_t pulse = (backlight_pwm.period * static_cast<uint32_t>(percent)) / 100U;
    return pwm_set_dt(&backlight_pwm, backlight_pwm.period, pulse);
#else
    (void)percent;
    return -ENOTSUP;
#endif
  }
};

/** @brief 全局背光实例。 */
ZephyrBacklight g_backlight;

}  // namespace

namespace platform {

/**
 * @brief 获取全局背光实例。
 * @return IBacklight 引用。
 */
IBacklight& backlight() { return g_backlight; }

}  // namespace platform
