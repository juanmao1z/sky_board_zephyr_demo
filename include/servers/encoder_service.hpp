/**
 * @file encoder_service.hpp
 * @brief EC11 编码器后台服务声明.
 */

#pragma once

#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>

#include "platform/ilogger.hpp"
#include "platform/platform_encoder.hpp"

namespace servers {

/**
 * @brief EC11 编码器后台服务.
 * @note 周期采样 QDEC, 输出位置变化并维护计数.
 */
class EncoderService {
 public:
  /**
   * @brief 构造编码器服务.
   * @param log 日志实例引用, 生命周期需覆盖服务运行期.
   */
  explicit EncoderService(platform::ILogger& log) : log_(log) {}

  /**
   * @brief 启动服务线程(幂等).
   * @return 0 表示成功或已运行. 负值表示失败.
   */
  int run() noexcept;

  /**
   * @brief 请求停止服务线程.
   */
  void stop() noexcept;

  /**
   * @brief 获取最近一次编码器样本.
   * @param out 输出样本.
   * @return 0 表示成功. -EAGAIN 表示暂无有效样本.
   */
  int get_latest(platform::EncoderSample& out) noexcept;

  /**
   * @brief 获取累计编码器步进计数.
   * @param out 输出计数值.
   * @return 0 表示成功.
   */
  int get_count(int64_t& out) noexcept;

 private:
  /** @brief 服务线程栈大小, 单位字节. */
  static constexpr size_t kStackSize = 1024;
  /** @brief 服务线程优先级. */
  static constexpr int kPriority = K_LOWEST_APPLICATION_THREAD_PRIO;
  /** @brief 采样周期, 单位毫秒. */
  static constexpr int64_t kSamplePeriodMs = 20;
  /** @brief 每个逻辑计数步对应角度(360 / 20). */
  static constexpr int32_t kDegPerStep = 18;

  /**
   * @brief 线程入口静态适配函数.
   */
  static void threadEntry(void* p1, void* p2, void* p3);

  /**
   * @brief 线程主循环实现.
   */
  void threads() noexcept;

  /** @brief 日志接口引用. */
  platform::ILogger& log_;
  /** @brief Zephyr 线程控制块. */
  struct k_thread thread_;
  /** @brief Zephyr 线程栈. */
  K_KERNEL_STACK_MEMBER(stack_, kStackSize);
  /** @brief 线程 ID, 未运行时为 nullptr. */
  k_tid_t thread_id_ = nullptr;
  /** @brief 运行标志: 1 运行中, 0 已停止. */
  atomic_t running_ = ATOMIC_INIT(0);
  /** @brief 停止请求标志: 1 请求停止, 0 继续运行. */
  atomic_t stop_requested_ = ATOMIC_INIT(0);
  /** @brief 保护 latest_ 与 count_ 的互斥锁. */
  struct k_mutex mutex_{};
  /** @brief 最近一次样本值. */
  platform::EncoderSample latest_ = {};
  /** @brief 最近样本有效标志. */
  bool latest_valid_ = false;
  /** @brief 累计编码器步进计数(可正可负). */
  int64_t count_ = 0;
};

}  // namespace servers
