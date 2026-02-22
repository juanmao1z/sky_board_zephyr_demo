/**
 * @file platform_logger.hpp
 * @brief 平台日志实例访问接口。
 */

#pragma once

#include "platform/ilogger.hpp"

namespace platform {

/**
 * @brief 获取全局日志实例。
 * @return ILogger 引用，生命周期贯穿整个程序运行期。
 */
ILogger &logger();

/**
 * @brief 将日志时间戳源切换为 RTC 时分秒。
 * @return 0 表示切换成功；负值表示失败。
 */
int logger_enable_rtc_timestamp();

} // namespace platform
