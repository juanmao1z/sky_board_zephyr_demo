/**
 * @file zephyr_rtc.cpp
 * @brief 平台 RTC 设备选择逻辑：外部优先，异常回退片上。
 */

#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/rtc.h>
#include <zephyr/kernel.h>

#include "platform/platform_rtc.hpp"

namespace {

/** @brief RTC 访问互斥锁，避免多线程并发读写导致总线竞争。 */
K_MUTEX_DEFINE(g_rtc_mutex);

/**
 * @brief 校验 RTC 时间字段是否在可接受范围内。
 * @param t 待校验时间结构。
 * @return true 表示字段有效；false 表示存在越界字段。
 * @note 年份范围限定为 2020~2099，用于过滤外部 RTC 异常值。
 */
bool rtc_time_fields_valid(const struct rtc_time& t) noexcept {
  if (t.tm_sec < 0 || t.tm_sec > 59) {
    return false;
  }
  if (t.tm_min < 0 || t.tm_min > 59) {
    return false;
  }
  if (t.tm_hour < 0 || t.tm_hour > 23) {
    return false;
  }
  if (t.tm_mday < 1 || t.tm_mday > 31) {
    return false;
  }
  if (t.tm_mon < 0 || t.tm_mon > 11) {
    return false;
  }

  const int year = t.tm_year + 1900;
  if (year < 2020 || year > 2099) {
    return false;
  }

  return true;
}

/**
 * @brief 从指定 RTC 设备读取时间并校验字段合法性。
 * @param dev RTC 设备指针。
 * @param[out] out 输出时间结构。
 * @return 0 表示成功；负值表示设备未就绪、读失败或字段无效。
 */
int rtc_get_time_if_valid(const struct device* dev, struct rtc_time& out) noexcept {
  if (dev == nullptr || !device_is_ready(dev)) {
    return -ENODEV;
  }

  const int ret = rtc_get_time(dev, &out);
  if (ret < 0) {
    return ret;
  }

  if (!rtc_time_fields_valid(out)) {
    return -ERANGE;
  }

  return 0;
}

/**
 * @brief 加锁 RTC 全局互斥锁。
 * @return 0 成功；负值失败。
 */
int lock_rtc_mutex() noexcept { return k_mutex_lock(&g_rtc_mutex, K_FOREVER); }

/**
 * @brief 解锁 RTC 全局互斥锁。
 */
void unlock_rtc_mutex() noexcept { (void)k_mutex_unlock(&g_rtc_mutex); }

}  // namespace

namespace platform {

/**
 * @brief 获取外部 RTC 设备（由 devicetree alias `rtc-external` 指定）。
 * @return 设备就绪返回设备指针，否则返回 nullptr。
 */
const struct device* external_rtc_device() noexcept {
#if DT_HAS_ALIAS(rtc_external)
  const struct device* external_rtc = DEVICE_DT_GET_OR_NULL(DT_ALIAS(rtc_external));
  if (external_rtc != nullptr && device_is_ready(external_rtc)) {
    return external_rtc;
  }
#endif
  return nullptr;
}

/**
 * @brief 获取片上 RTC 设备（node label `rtc`）。
 * @return 设备就绪返回设备指针，否则返回 nullptr。
 */
const struct device* internal_rtc_device() noexcept {
  const struct device* internal_rtc = DEVICE_DT_GET_OR_NULL(DT_NODELABEL(rtc));
  if (internal_rtc != nullptr && device_is_ready(internal_rtc)) {
    return internal_rtc;
  }
  return nullptr;
}

/**
 * @brief 选择优先 RTC 设备。
 * @return 外部 RTC 可读且时间有效时返回外部设备，否则回退片上 RTC。
 */
const struct device* preferred_rtc_device() noexcept {
  if (lock_rtc_mutex() != 0) {
    return internal_rtc_device();
  }

  const struct device* external_rtc = external_rtc_device();
  if (external_rtc != nullptr) {
    struct rtc_time probe = {};
    if (rtc_get_time_if_valid(external_rtc, probe) == 0) {
      unlock_rtc_mutex();
      return external_rtc;
    }
  }

  const struct device* fallback = internal_rtc_device();
  unlock_rtc_mutex();
  return fallback;
}

/**
 * @brief 读取 RTC 时间（外部优先，内部回退）。
 * @param[out] out 输出时间结构。
 * @return 0 表示成功；负值表示无可用 RTC 或读取失败。
 */
int rtc_get_time_best_effort(struct rtc_time& out) noexcept {
  const int lock_ret = lock_rtc_mutex();
  if (lock_ret != 0) {
    return lock_ret;
  }

  int ret_external = -ENODEV;
  int ret_internal = -ENODEV;

  const struct device* external_rtc = external_rtc_device();
  if (external_rtc != nullptr) {
    ret_external = rtc_get_time_if_valid(external_rtc, out);
    if (ret_external == 0) {
      unlock_rtc_mutex();
      return 0;
    }
  }

  const struct device* internal_rtc = internal_rtc_device();
  if (internal_rtc != nullptr) {
    ret_internal = rtc_get_time_if_valid(internal_rtc, out);
    if (ret_internal == 0) {
      unlock_rtc_mutex();
      return 0;
    }
  }

  unlock_rtc_mutex();
  if (ret_internal != -ENODEV) {
    return ret_internal;
  }
  return ret_external;
}

/**
 * @brief 仅读取外部 RTC 时间。
 * @param[out] out 输出时间结构。
 * @return 0 表示成功；负值表示外部 RTC 不可用或时间无效。
 */
int rtc_get_time_external(struct rtc_time& out) noexcept {
  const int lock_ret = lock_rtc_mutex();
  if (lock_ret != 0) {
    return lock_ret;
  }

  const struct device* external_rtc = external_rtc_device();
  if (external_rtc == nullptr) {
    unlock_rtc_mutex();
    return -ENODEV;
  }
  const int ret = rtc_get_time_if_valid(external_rtc, out);
  unlock_rtc_mutex();
  return ret;
}

/**
 * @brief 写入 RTC 时间（同时尝试内部与外部 RTC）。
 * @param in 输入时间结构。
 * @return 任一写入成功返回 0；否则返回可诊断的错误码。
 */
int rtc_set_time_best_effort(const struct rtc_time& in) noexcept {
  const int lock_ret = lock_rtc_mutex();
  if (lock_ret != 0) {
    return lock_ret;
  }

  int ret_internal = -ENODEV;
  int ret_external = -ENODEV;

  const struct device* internal_rtc = internal_rtc_device();
  if (internal_rtc != nullptr) {
    ret_internal = rtc_set_time(internal_rtc, &in);
  }

  const struct device* external_rtc = external_rtc_device();
  if (external_rtc != nullptr) {
    ret_external = rtc_set_time(external_rtc, &in);
  }

  if (ret_internal == 0 || ret_external == 0) {
    unlock_rtc_mutex();
    return 0;
  }

  unlock_rtc_mutex();
  if (ret_internal != -ENODEV) {
    return ret_internal;
  }

  return ret_external;
}

/**
 * @brief 仅写入片上 RTC 时间。
 * @param in 输入时间结构。
 * @return 0 表示成功；负值表示片上 RTC 不可用或写入失败。
 */
int rtc_set_time_internal(const struct rtc_time& in) noexcept {
  const int lock_ret = lock_rtc_mutex();
  if (lock_ret != 0) {
    return lock_ret;
  }

  const struct device* internal_rtc = internal_rtc_device();
  if (internal_rtc == nullptr) {
    unlock_rtc_mutex();
    return -ENODEV;
  }
  const int ret = rtc_set_time(internal_rtc, &in);
  unlock_rtc_mutex();
  return ret;
}

}  // namespace platform
