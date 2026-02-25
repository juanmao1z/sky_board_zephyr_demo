/**
 * @file time_service.hpp
 * @brief 北京时间同步服务声明：基于 SNTP 周期同步并写入 RTC。
 */

#pragma once

#include <time.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>

#include "platform/ilogger.hpp"

namespace servers {

/**
 * @brief 北京时间同步服务。
 * @note 服务在独立线程中运行，网络就绪后执行 SNTP 校时，默认每 10 分钟同步一次。
 */
class TimeService {
 public:
  /**
   * @brief 构造时间服务。
   * @param log 日志接口引用，必须在服务生命周期内保持有效。
   */
  explicit TimeService(platform::ILogger& log) : log_(log) {}

  /**
   * @brief 启动服务线程（幂等）。
   * @return 0 表示成功或已在运行；负值表示失败。
   */
  int run() noexcept;

  /**
   * @brief 请求停止服务线程。
   * @note 仅发出停止请求，不阻塞等待线程退出。
   */
  void stop() noexcept;

  /**
   * @brief 查询首次 SNTP+RTC 同步是否已完成。
   * @return true 表示已完成；false 表示未完成。
   */
  bool is_first_sync_done() const noexcept;

  /**
   * @brief 等待首次 SNTP+RTC 同步完成。
   * @param timeout_ms 超时时间（毫秒）。
   * @return 0 表示成功；-ETIMEDOUT 表示超时；负值表示参数错误。
   */
  int wait_first_sync(int64_t timeout_ms) const noexcept;

 private:
  /** @brief 服务线程栈大小（字节）。 */
  static constexpr size_t kStackSize = 3072;
  /** @brief 服务线程优先级。 */
  static constexpr int kPriority = K_LOWEST_APPLICATION_THREAD_PRIO;
  /** @brief SNTP 服务端域名。 */
  static constexpr const char* kSntpServer = "ntp.aliyun.com";
  /** @brief SNTP 单次查询超时（毫秒）。 */
  static constexpr uint32_t kSntpTimeoutMs = 5000U;
  /** @brief 周期同步间隔（毫秒）。 */
  static constexpr int64_t kSyncPeriodMs = 10 * 60 * 1000;
  /** @brief 失败重试间隔（毫秒）。 */
  static constexpr int64_t kRetryDelayMs = 10 * 1000;
  /** @brief 主循环空闲轮询间隔（毫秒）。 */
  static constexpr int64_t kLoopSleepMs = 1000;

  /**
   * @brief 线程入口静态适配函数。
   * @param p1 TimeService 对象指针。
   * @param p2 未使用。
   * @param p3 未使用。
   */
  static void threadEntry(void* p1, void* p2, void* p3);

  /**
   * @brief 服务线程主循环。
   */
  void threads() noexcept;

  /**
   * @brief 在条件满足时执行一次 SNTP 校时。
   * @note 未到同步时刻或仍在重试冷却时直接返回，不阻塞线程。
   */
  void maybe_sync_beijing_time() noexcept;

  /**
   * @brief 检查以太网接口是否已有可用 IPv4 地址。
   * @return true 表示 IPv4 已就绪；false 表示未就绪。
   */
  bool is_ipv4_ready() const noexcept;

  /**
   * @brief 通过 SNTP 获取 UTC 时间。
   * @param[out] out_epoch_sec 输出 UTC epoch 秒。
   * @return 0 表示成功；负值表示失败。
   */
  int fetch_utc_epoch_from_sntp(time_t& out_epoch_sec) const noexcept;

  /**
   * @brief 仅在首次成功校时后切换日志时间戳源到 RTC。
   */
  void maybe_enable_rtc_log_timestamp() noexcept;

  /**
   * @brief 打印北京时间（UTC+8）。
   * @param utc_epoch_sec UTC epoch 秒。
   */
  void print_beijing_time(time_t utc_epoch_sec) const noexcept;

  /**
   * @brief 把北京时间写入片上 RTC。
   * @param utc_epoch_sec UTC epoch 秒。
   * @return 0 表示成功；负值表示失败。
   */
  int write_beijing_time_to_rtc(time_t utc_epoch_sec) noexcept;

  /** @brief 日志接口。 */
  platform::ILogger& log_;
  /** @brief Zephyr 线程控制块。 */
  struct k_thread thread_;
  /** @brief Zephyr 线程栈。 */
  K_KERNEL_STACK_MEMBER(stack_, kStackSize);
  /** @brief 线程 ID，未运行时为 nullptr。 */
  k_tid_t thread_id_ = nullptr;
  /** @brief 运行状态标志：1 运行中，0 未运行。 */
  atomic_t running_ = ATOMIC_INIT(0);
  /** @brief 停止请求标志：1 请求停止，0 继续运行。 */
  atomic_t stop_requested_ = ATOMIC_INIT(0);
  /** @brief 下一次周期同步时间点（毫秒），0 表示立即可同步。 */
  int64_t next_sync_due_ms_ = 0;
  /** @brief 下一次失败重试时间点（毫秒），0 表示立即可重试。 */
  int64_t next_retry_after_ms_ = 0;
  /** @brief 是否已切换日志时间戳为 RTC。 */
  bool rtc_timestamp_enabled_ = false;
  /** @brief 首次 SNTP+RTC 同步完成标志。 */
  atomic_t first_sync_done_ = ATOMIC_INIT(0);
  /** @brief 上一轮 IPv4 就绪状态，用于边沿日志。 */
  bool last_ipv4_ready_ = false;
};

}  // namespace servers
