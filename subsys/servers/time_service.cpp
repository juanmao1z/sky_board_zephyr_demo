/**
 * @file time_service.cpp
 * @brief 北京时间同步服务实现：基于 SNTP 周期获取 UTC 并写入 RTC。
 */

#include "servers/time_service.hpp"

#include <errno.h>
#include <stdio.h>
#include <time.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/rtc.h>
#include <zephyr/net/ethernet.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/sntp.h>

#include "platform/platform_logger.hpp"

namespace servers {

/**
 * @brief 线程入口静态适配函数。
 * @param p1 TimeService 对象指针。
 * @param p2 未使用。
 * @param p3 未使用。
 */
void TimeService::threadEntry(void* p1, void*, void*) { static_cast<TimeService*>(p1)->threads(); }

/**
 * @brief 服务线程主循环。
 * @note 线程常驻，按状态机执行周期同步，直到收到 stop 请求。
 */
void TimeService::threads() noexcept {
  log_.info("time service starting");

  while (atomic_get(&stop_requested_) == 0) {
    maybe_sync_beijing_time();
    k_sleep(K_MSEC(kLoopSleepMs));
  }

  atomic_set(&running_, 0);
  thread_id_ = nullptr;
  log_.info("time service stopped");
}

/**
 * @brief 查询首次同步完成状态。
 * @return true 表示已完成；false 表示未完成。
 */
bool TimeService::is_first_sync_done() const noexcept { return atomic_get(&first_sync_done_) != 0; }

/**
 * @brief 等待首次 SNTP+RTC 同步完成。
 * @param timeout_ms 超时时间（毫秒）。
 * @return 0 表示成功；-ETIMEDOUT 表示超时；负值表示参数错误。
 */
int TimeService::wait_first_sync(int64_t timeout_ms) const noexcept {
  if (timeout_ms <= 0) {
    return -EINVAL;
  }

  const int64_t deadline_ms = k_uptime_get() + timeout_ms;
  while (k_uptime_get() < deadline_ms) {
    if (is_first_sync_done()) {
      return 0;
    }
    k_sleep(K_MSEC(200));
  }

  return -ETIMEDOUT;
}

/**
 * @brief 在满足条件时执行一次 SNTP 同步。
 * @note 成功后进入 10 分钟周期；失败后 10 秒重试。
 */
void TimeService::maybe_sync_beijing_time() noexcept {
  const int64_t now_ms = k_uptime_get();

  if (!is_ipv4_ready()) {
    if (last_ipv4_ready_) {
      log_.info("[time] IPv4 lost, SNTP paused");
    }
    last_ipv4_ready_ = false;
    return;
  }

  if (!last_ipv4_ready_) {
    log_.info("[time] IPv4 ready, SNTP sync enabled");
    last_ipv4_ready_ = true;
  }

  if (next_retry_after_ms_ != 0 && now_ms < next_retry_after_ms_) {
    return;
  }

  if (next_sync_due_ms_ != 0 && now_ms < next_sync_due_ms_) {
    return;
  }

  time_t utc_epoch_sec = 0;
  const int ret = fetch_utc_epoch_from_sntp(utc_epoch_sec);
  if (ret < 0) {
    next_retry_after_ms_ = now_ms + kRetryDelayMs;
    char msg[96];
    (void)snprintf(msg, sizeof(msg), "[time] SNTP sync failed: err=%d, retry in 10s", ret);
    log_.info(msg);
    return;
  }

  next_retry_after_ms_ = 0;
  next_sync_due_ms_ = now_ms + kSyncPeriodMs;

  const int rtc_ret = write_beijing_time_to_rtc(utc_epoch_sec);
  if (rtc_ret < 0) {
    log_.error("failed to write beijing time to rtc", rtc_ret);
  } else {
    atomic_set(&first_sync_done_, 1);
    log_.info("[time] RTC updated with Beijing time");
    maybe_enable_rtc_log_timestamp();
  }

  print_beijing_time(utc_epoch_sec);
}

/**
 * @brief 检查以太网接口是否已有可用 IPv4 地址。
 * @return true 表示 IPv4 已就绪；false 表示未就绪。
 */
bool TimeService::is_ipv4_ready() const noexcept {
  struct net_if* iface = net_if_get_first_by_type(&NET_L2_GET_NAME(ETHERNET));
  if (iface == nullptr) {
    return false;
  }

  struct net_in_addr* addr = net_if_ipv4_get_global_addr(iface, NET_ADDR_PREFERRED);
  if (addr == nullptr) {
    addr = net_if_ipv4_get_global_addr(iface, NET_ADDR_TENTATIVE);
  }

  return addr != nullptr;
}

/**
 * @brief 通过 SNTP 获取 UTC epoch 秒。
 * @param[out] out_epoch_sec 输出 UTC epoch 秒。
 * @return 0 表示成功；负值表示失败。
 */
int TimeService::fetch_utc_epoch_from_sntp(time_t& out_epoch_sec) const noexcept {
  struct sntp_time ts = {};
  const int ret = sntp_simple(kSntpServer, kSntpTimeoutMs, &ts);
  if (ret < 0) {
    return ret;
  }

  out_epoch_sec = static_cast<time_t>(ts.seconds);
  return 0;
}

/**
 * @brief 在首次成功后切换日志时间戳源为 RTC。
 */
void TimeService::maybe_enable_rtc_log_timestamp() noexcept {
  if (rtc_timestamp_enabled_) {
    return;
  }

  const int ret = platform::logger_enable_rtc_timestamp();
  if (ret < 0) {
    log_.error("failed to switch log timestamp to rtc", ret);
    return;
  }

  rtc_timestamp_enabled_ = true;
  log_.info("[time] log timestamp switched to rtc");
}

/**
 * @brief 打印北京时间（UTC+8）。
 * @param utc_epoch_sec UTC epoch 秒。
 */
void TimeService::print_beijing_time(time_t utc_epoch_sec) const noexcept {
  time_t beijing_epoch_sec = utc_epoch_sec + (8 * 3600);
  struct tm beijing_tm = {};
  if (gmtime_r(&beijing_epoch_sec, &beijing_tm) == nullptr) {
    log_.info("[time] Beijing format failed");
    return;
  }

  char msg[96];
  (void)snprintf(msg, sizeof(msg), "[time] Beijing: %04d-%02d-%02d %02d:%02d:%02d (UTC+8)",
                 beijing_tm.tm_year + 1900, beijing_tm.tm_mon + 1, beijing_tm.tm_mday,
                 beijing_tm.tm_hour, beijing_tm.tm_min, beijing_tm.tm_sec);
  log_.info(msg);
}

/**
 * @brief 把北京时间写入片上 RTC。
 * @param utc_epoch_sec UTC epoch 秒。
 * @return 0 表示成功；负值表示失败。
 */
int TimeService::write_beijing_time_to_rtc(time_t utc_epoch_sec) noexcept {
  const struct device* rtc_dev = DEVICE_DT_GET_OR_NULL(DT_NODELABEL(rtc));
  if (rtc_dev == nullptr || !device_is_ready(rtc_dev)) {
    return -ENODEV;
  }

  time_t beijing_epoch_sec = utc_epoch_sec + (8 * 3600);
  struct tm beijing_tm = {};
  if (gmtime_r(&beijing_epoch_sec, &beijing_tm) == nullptr) {
    return -EINVAL;
  }

  struct rtc_time rtc_tm = {};
  rtc_tm.tm_sec = beijing_tm.tm_sec;
  rtc_tm.tm_min = beijing_tm.tm_min;
  rtc_tm.tm_hour = beijing_tm.tm_hour;
  rtc_tm.tm_mday = beijing_tm.tm_mday;
  rtc_tm.tm_mon = beijing_tm.tm_mon;
  rtc_tm.tm_year = beijing_tm.tm_year;
  rtc_tm.tm_wday = beijing_tm.tm_wday;
  rtc_tm.tm_yday = beijing_tm.tm_yday;
  rtc_tm.tm_isdst = -1;
  rtc_tm.tm_nsec = 0;

  return rtc_set_time(rtc_dev, &rtc_tm);
}

/**
 * @brief 请求停止时间服务线程。
 * @note 仅设置停止标志并尝试唤醒线程，不阻塞等待退出。
 */
void TimeService::stop() noexcept {
  if (atomic_get(&running_) == 0) {
    return;
  }

  atomic_set(&stop_requested_, 1);
  if (thread_id_ != nullptr) {
    k_wakeup(thread_id_);
  }
}

/**
 * @brief 启动时间服务线程（幂等）。
 * @return 0 表示成功或已在运行；负值表示失败。
 */
int TimeService::run() noexcept {
  if (!atomic_cas(&running_, 0, 1)) {
    log_.info("time service already running");
    return 0;
  }

  atomic_set(&stop_requested_, 0);
  next_sync_due_ms_ = 0;
  next_retry_after_ms_ = 0;
  rtc_timestamp_enabled_ = false;
  atomic_set(&first_sync_done_, 0);
  last_ipv4_ready_ = false;

  thread_id_ = k_thread_create(&thread_, stack_, K_THREAD_STACK_SIZEOF(stack_), threadEntry, this,
                               nullptr, nullptr, kPriority, 0, K_NO_WAIT);
  if (thread_id_ == nullptr) {
    atomic_set(&running_, 0);
    log_.error("failed to create time service thread", -1);
    return -1;
  }

  k_thread_name_set(thread_id_, "time_service");
  return 0;
}

}  // namespace servers
