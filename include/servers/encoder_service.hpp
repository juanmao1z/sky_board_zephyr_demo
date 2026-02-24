/**
 * @file encoder_service.hpp
 * @brief EC11 encoder background service declaration.
 */

#pragma once

#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>

#include "platform/ilogger.hpp"
#include "platform/platform_encoder.hpp"

namespace servers {

/**
 * @brief EC11 encoder background service.
 * @note Periodically samples QDEC and prints position changes.
 */
class EncoderService {
 public:
  /**
   * @brief Construct encoder service.
   * @param log Logger instance; must outlive the service.
   */
  explicit EncoderService(platform::ILogger& log) : log_(log) {}

  /**
   * @brief Start service thread (idempotent).
   * @return 0 on success or already running; negative errno on failure.
   */
  int run() noexcept;

  /**
   * @brief Request service stop.
   */
  void stop() noexcept;

  /**
   * @brief Get latest sampled encoder data.
   * @param out Output sample.
   * @return 0 on success; -EAGAIN when no sample is available yet.
   */
  int get_latest(platform::EncoderSample& out) noexcept;

  /**
   * @brief Get accumulated encoder step counter.
   * @param out Output counter value.
   * @return 0 on success.
   */
  int get_count(int64_t& out) noexcept;

 private:
  /** @brief Service thread stack size in bytes. */
  static constexpr size_t kStackSize = 1024;
  /** @brief Service thread priority. */
  static constexpr int kPriority = K_LOWEST_APPLICATION_THREAD_PRIO;
  /** @brief Sampling period in milliseconds. */
  static constexpr int64_t kSamplePeriodMs = 20;
  /** @brief Degree step per one logical counter step (360 / 20). */
  static constexpr int32_t kDegPerStep = 18;

  /**
   * @brief Thread entry trampoline.
   */
  static void threadEntry(void* p1, void* p2, void* p3);

  /**
   * @brief Service loop implementation.
   */
  void threads() noexcept;

  /** @brief Logger. */
  platform::ILogger& log_;
  /** @brief Zephyr thread control block. */
  struct k_thread thread_;
  /** @brief Zephyr thread stack. */
  K_KERNEL_STACK_MEMBER(stack_, kStackSize);
  /** @brief Thread id, null when not running. */
  k_tid_t thread_id_ = nullptr;
  /** @brief Running flag: 1 running, 0 stopped. */
  atomic_t running_ = ATOMIC_INIT(0);
  /** @brief Stop-request flag: 1 stop requested, 0 keep running. */
  atomic_t stop_requested_ = ATOMIC_INIT(0);
  /** @brief Protects latest sample. */
  struct k_mutex mutex_{};
  /** @brief Latest sampled value. */
  platform::EncoderSample latest_ = {};
  /** @brief Latest sample validity flag. */
  bool latest_valid_ = false;
  /** @brief Accumulated encoder step counter (can be positive or negative). */
  int64_t count_ = 0;
};

}  // namespace servers
