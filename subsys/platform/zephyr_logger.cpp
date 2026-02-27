/**
 * @file zephyr_logger.cpp
 * @brief ILogger 的 Zephyr LOG 后端实现。
 */

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/rtc.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>

#include "platform/platform_logger.hpp"

LOG_MODULE_REGISTER(sky_board_demo, LOG_LEVEL_INF);

namespace {

/** @brief 格式化日志临时缓冲区长度. */
constexpr size_t kLogFormatBufferSize = 192U;

/**
 * @brief 基于 Zephyr LOG 宏的日志实现。
 */
class ZephyrLogger final : public platform::ILogger {
 public:
  /**
   * @brief 输出信息级日志。
   * @param msg 日志消息字符串。
   */
  void info(const char* msg) override { LOG_INF("%s", msg); }

  /**
   * @brief 输出错误级日志。
   * @param msg 错误消息字符串。
   * @param err 错误码。
   */
  void error(const char* msg, int err) override { LOG_ERR("%s err=%d", msg, err); }

  /**
   * @brief 输出 va_list 形式的信息级日志.
   * @param fmt printf 风格格式串.
   * @param args 可变参数列表.
   */
  void vinfof(const char* fmt, va_list args) override {
    char msg[kLogFormatBufferSize] = {};
    if (fmt == nullptr) {
      LOG_INF("%s", "(null)");
      return;
    }

    va_list args_copy;
    va_copy(args_copy, args);
    const int n = vsnprintf(msg, sizeof(msg), fmt, args_copy);
    va_end(args_copy);

    if (n < 0) {
      LOG_INF("%s", "log format error");
      return;
    }
    LOG_INF("%s", msg);
  }

  /**
   * @brief 输出 va_list 形式的错误级日志.
   * @param fmt printf 风格格式串.
   * @param args 可变参数列表.
   */
  void verrorf(const char* fmt, va_list args) override {
    char msg[kLogFormatBufferSize] = {};
    if (fmt == nullptr) {
      LOG_ERR("%s", "(null)");
      return;
    }

    va_list args_copy;
    va_copy(args_copy, args);
    const int n = vsnprintf(msg, sizeof(msg), fmt, args_copy);
    va_end(args_copy);

    if (n < 0) {
      LOG_ERR("%s", "log format error");
      return;
    }
    LOG_ERR("%s", msg);
  }
};

/** @brief 全局日志对象实例。 */
ZephyrLogger g_logger;
/** @brief RTC 设备句柄（启用 RTC 时间戳后有效）。 */
const struct device* g_rtc_dev = nullptr;

/**
 * @brief RTC 时间戳回调（返回当日毫秒数）。
 * @return 日志时间戳。
 */
log_timestamp_t rtc_timestamp_getter() {
  if (g_rtc_dev == nullptr || !device_is_ready(g_rtc_dev)) {
    return k_uptime_get_32();
  }

  struct rtc_time rtc_tm = {};
  if (rtc_get_time(g_rtc_dev, &rtc_tm) < 0) {
    return k_uptime_get_32();
  }

  const uint32_t ms = static_cast<uint32_t>(rtc_tm.tm_hour) * 3600000U +
                      static_cast<uint32_t>(rtc_tm.tm_min) * 60000U +
                      static_cast<uint32_t>(rtc_tm.tm_sec) * 1000U +
                      static_cast<uint32_t>(rtc_tm.tm_nsec / 1000000);
  return ms;
}

}  // namespace

namespace platform {

/**
 * @brief 获取全局日志实例。
 * @return ILogger 引用。
 */
ILogger& logger() { return g_logger; }

int logger_enable_rtc_timestamp() {
  const struct device* rtc_dev = DEVICE_DT_GET_OR_NULL(DT_NODELABEL(rtc));
  if (rtc_dev == nullptr || !device_is_ready(rtc_dev)) {
    return -ENODEV;
  }

  g_rtc_dev = rtc_dev;
  return log_set_timestamp_func(rtc_timestamp_getter, 1000U);
}

}  // namespace platform
