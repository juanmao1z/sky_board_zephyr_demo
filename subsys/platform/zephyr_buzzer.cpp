/**
 * @file zephyr_buzzer.cpp
 * @brief IBuzzer 的 Zephyr PWM 后端实现。
 */

#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>

#include <cstdint>

#include "platform/platform_buzzer.hpp"
#include "platform/platform_logger.hpp"

namespace {

#define BUZZER_PWM_NODE DT_ALIAS(buzzer_pwm)

constexpr uint32_t kMinFreqHz = 100U;
constexpr uint32_t kMaxFreqHz = 5000U;
constexpr uint8_t kMinDutyPercent = 1U;
constexpr uint8_t kMaxDutyPercent = 100U;
constexpr uint32_t kNanosecondsPerSecond = 1000000000U;
constexpr uint32_t kOffFallbackPeriodNs = 1000000U;
constexpr uint32_t kStartupBeepFreqHz = 2000U;
constexpr uint8_t kStartupBeepDutyPercent = 45U;
constexpr int32_t kStartupBeepDurationMs = 80;

/**
 * @brief 对频率参数执行安全裁剪。
 * @param freq_hz 调用方传入频率。
 * @return 裁剪后的频率值。
 */
uint32_t clip_freq_hz(const uint32_t freq_hz) noexcept {
  if (freq_hz < kMinFreqHz) {
    return kMinFreqHz;
  }
  if (freq_hz > kMaxFreqHz) {
    return kMaxFreqHz;
  }
  return freq_hz;
}

/**
 * @brief 对占空比参数执行安全裁剪。
 * @param duty_percent 调用方传入占空比。
 * @return 裁剪后的占空比。
 */
uint8_t clip_duty_percent(const uint8_t duty_percent) noexcept {
  if (duty_percent < kMinDutyPercent) {
    return kMinDutyPercent;
  }
  if (duty_percent > kMaxDutyPercent) {
    return kMaxDutyPercent;
  }
  return duty_percent;
}

/**
 * @brief Zephyr PWM 蜂鸣器驱动实现。
 */
class ZephyrBuzzer final : public platform::IBuzzer {
 public:
  int init() noexcept override;
  int on(uint32_t freq_hz, uint8_t duty_percent) noexcept override;
  int off() noexcept override;

 private:
#if DT_NODE_HAS_STATUS(BUZZER_PWM_NODE, okay)
  int force_off_impl() noexcept;
  static const pwm_dt_spec buzzer_pwm_;
#endif
  bool initialized_ = false;
};

#if DT_NODE_HAS_STATUS(BUZZER_PWM_NODE, okay)
const pwm_dt_spec ZephyrBuzzer::buzzer_pwm_ = PWM_DT_SPEC_GET(BUZZER_PWM_NODE);
#endif

int ZephyrBuzzer::init() noexcept {
  if (initialized_) {
    return 0;
  }

#if DT_NODE_HAS_STATUS(BUZZER_PWM_NODE, okay)
  if (!device_is_ready(buzzer_pwm_.dev)) {
    platform::logger().error("buzzer pwm device not ready", -ENODEV);
    return -ENODEV;
  }

  const int off_ret = force_off_impl();
  if (off_ret < 0) {
    platform::logger().error("failed to force buzzer off at init", off_ret);
    return off_ret;
  }

  initialized_ = true;
  const int on_ret = on(kStartupBeepFreqHz, kStartupBeepDutyPercent);
  if (on_ret < 0) {
    platform::logger().error("failed to start startup buzzer beep", on_ret);
    return on_ret;
  }

  k_sleep(K_MSEC(kStartupBeepDurationMs));
  const int off_beep_ret = off();
  if (off_beep_ret < 0) {
    platform::logger().error("failed to stop startup buzzer beep", off_beep_ret);
    return off_beep_ret;
  }

  return 0;
#else
  platform::logger().error("buzzer pwm alias missing in devicetree", -ENOTSUP);
  return -ENOTSUP;
#endif
}

int ZephyrBuzzer::on(const uint32_t freq_hz, const uint8_t duty_percent) noexcept {
#if DT_NODE_HAS_STATUS(BUZZER_PWM_NODE, okay)
  const int init_ret = init();
  if (init_ret < 0) {
    return init_ret;
  }

  const uint32_t clipped_freq = clip_freq_hz(freq_hz);
  const uint8_t clipped_duty = clip_duty_percent(duty_percent);
  if (clipped_freq != freq_hz || clipped_duty != duty_percent) {
    platform::logger().infof("buzzer params clipped freq=%u->%u duty=%u->%u",
                             static_cast<unsigned>(freq_hz), static_cast<unsigned>(clipped_freq),
                             static_cast<unsigned>(duty_percent),
                             static_cast<unsigned>(clipped_duty));
  }

  uint32_t period_ns = kNanosecondsPerSecond / clipped_freq;
  if (period_ns == 0U) {
    period_ns = 1U;
  }
  const uint32_t pulse_ns =
      (period_ns * static_cast<uint32_t>(clipped_duty)) / static_cast<uint32_t>(100U);

  const int ret = pwm_set_dt(&buzzer_pwm_, period_ns, pulse_ns);
  if (ret < 0) {
    platform::logger().error("failed to enable buzzer pwm", ret);
    (void)force_off_impl();
  }
  return ret;
#else
  (void)freq_hz;
  (void)duty_percent;
  return -ENOTSUP;
#endif
}

int ZephyrBuzzer::off() noexcept {
#if DT_NODE_HAS_STATUS(BUZZER_PWM_NODE, okay)
  const int init_ret = init();
  if (init_ret < 0) {
    return init_ret;
  }

  const int ret = force_off_impl();
  if (ret < 0) {
    platform::logger().error("failed to disable buzzer pwm", ret);
  }
  return ret;
#else
  return -ENOTSUP;
#endif
}

#if DT_NODE_HAS_STATUS(BUZZER_PWM_NODE, okay)
int ZephyrBuzzer::force_off_impl() noexcept {
  const uint32_t period_ns = (buzzer_pwm_.period == 0U) ? kOffFallbackPeriodNs : buzzer_pwm_.period;
  const int ret = pwm_set_dt(&buzzer_pwm_, period_ns, 0U);
  if (ret < 0) {
    platform::logger().error("buzzer safe-off failed", ret);
  }
  return ret;
}
#endif

ZephyrBuzzer g_buzzer;

}  // namespace

#undef BUZZER_PWM_NODE

namespace platform {

IBuzzer& buzzer() noexcept { return g_buzzer; }

}  // namespace platform
