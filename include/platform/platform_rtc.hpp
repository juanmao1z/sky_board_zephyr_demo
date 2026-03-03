/**
 * @file platform_rtc.hpp
 * @brief 平台 RTC 设备选择接口。
 */

#pragma once

struct device;
struct rtc_time;

namespace platform {

/**
 * @brief 获取外部 RTC 设备（devicetree alias: rtc-external）。
 * @return 设备就绪返回设备指针，否则返回 nullptr。
 */
const struct device* external_rtc_device() noexcept;

/**
 * @brief 获取片上 RTC 设备（node label: rtc）。
 * @return 设备就绪返回设备指针，否则返回 nullptr。
 */
const struct device* internal_rtc_device() noexcept;

/**
 * @brief 获取优先 RTC 设备。
 * @return 外部 RTC 可读且时间字段有效时返回外部设备；否则回退片上 RTC；都不可用时返回 nullptr。
 */
const struct device* preferred_rtc_device() noexcept;

/**
 * @brief 读取 RTC 时间（外部优先，异常回退片上）。
 * @param[out] out 输出 RTC 时间。
 * @return 0 表示成功；负值表示失败。
 */
int rtc_get_time_best_effort(struct rtc_time& out) noexcept;

/**
 * @brief 读取外部 RTC 时间。
 * @param[out] out 输出 RTC 时间。
 * @return 0 表示成功；负值表示失败。
 */
int rtc_get_time_external(struct rtc_time& out) noexcept;

/**
 * @brief 写入 RTC 时间（优先写片上，同时尝试外部）。
 * @param in 输入 RTC 时间。
 * @return 任一设备写入成功返回 0；否则返回错误码。
 */
int rtc_set_time_best_effort(const struct rtc_time& in) noexcept;

/**
 * @brief 仅写入片上 RTC 时间。
 * @param in 输入 RTC 时间。
 * @return 0 表示成功；负值表示失败。
 */
int rtc_set_time_internal(const struct rtc_time& in) noexcept;

}  // namespace platform
