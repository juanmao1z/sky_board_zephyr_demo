/**
 * @file imu_service.hpp
 * @brief IMU 后台服务声明：100Hz 采样 ICM42688 并输出/发布数据。
 */

#pragma once

#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>

#include "platform/ilogger.hpp"
#include "platform/platform_imu.hpp"

namespace servers {

/**
 * @brief IMU 样本发布回调。
 * @param sample 最新 IMU 样本。
 * @param user 用户私有指针。
 */
using ImuPublishCallback = void (*)(const platform::ImuSample& sample, void* user);

/**
 * @brief IMU 后台服务。
 * @note 启动后在独立线程中以 100Hz 采样 ICM42688。
 */
class ImuService {
 public:
  /**
   * @brief 构造 IMU 服务。
   * @param log 日志接口引用，必须在服务生命周期内保持有效。
   */
  explicit ImuService(platform::ILogger& log) : log_(log) {}

  /**
   * @brief 启动服务线程（幂等）。
   * @return 0 表示成功或已在运行；负值表示启动失败。
   */
  int run() noexcept;

  /**
   * @brief 请求停止服务线程。
   * @note 仅发出停止请求，不阻塞等待线程退出。
   */
  void stop() noexcept;

  /**
   * @brief 设置样本发布回调。
   * @param cb 回调函数指针，可为 nullptr。
   * @param user 回调用户参数。
   */
  void set_publish_callback(ImuPublishCallback cb, void* user) noexcept;

  /**
   * @brief 获取最新 IMU 样本。
   * @param out 输出样本。
   * @return 0 表示成功；-EAGAIN 表示暂无有效样本。
   */
  int get_latest(platform::ImuSample& out) noexcept;

 private:
  /** @brief 服务线程栈大小（字节）。 */
  static constexpr size_t kStackSize = 2048;
  /** @brief 服务线程优先级。 */
  static constexpr int kPriority = K_LOWEST_APPLICATION_THREAD_PRIO;
  /** @brief 采样周期（毫秒），10ms 对应 100Hz。 */
  static constexpr int64_t kSamplePeriodMs = 10;
  /** @brief 是否启用串口打印。 */
  static constexpr bool kEnablePrint = true;
  /** @brief 打印分频：每 N 个样本打印 1 次。10 表示 10Hz 打印。 */
  static constexpr uint32_t kPrintEveryNSamples = 10;

  /**
   * @brief 线程入口静态适配函数。
   * @param p1 ImuService 对象指针。
   * @param p2 未使用。
   * @param p3 未使用。
   */
  static void threadEntry(void* p1, void* p2, void* p3);

  /**
   * @brief 服务线程主循环。
   */
  void threads() noexcept;

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
  /** @brief 最新样本互斥锁。 */
  struct k_mutex mutex_{};
  /** @brief 最新样本缓存。 */
  platform::ImuSample latest_ = {};
  /** @brief 最新样本是否有效。 */
  bool latest_valid_ = false;
  /** @brief 发布回调函数指针。 */
  ImuPublishCallback publish_cb_ = nullptr;
  /** @brief 发布回调用户参数。 */
  void* publish_user_ = nullptr;
};

}  // namespace servers
