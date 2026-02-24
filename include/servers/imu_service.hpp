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
  /** @brief 上电陀螺零偏校准时长（毫秒）。 */
  static constexpr int64_t kGyroBiasCalibMs = 2500;
  /** @brief 判定校准有效的最小样本数。 */
  static constexpr uint32_t kGyroBiasMinSamples = 100;
  /** @brief 在线零偏更新: 静止判定所需的连续样本数（100Hz 下 25=250ms）。 */
  static constexpr uint32_t kOnlineBiasStreakSamples = 50;
  /** @brief 在线零偏更新: 加速度模长容差（mg）。 */
  static constexpr int32_t kOnlineBiasAccelNormTolMg = 80;
  /** @brief 在线零偏更新: 陀螺静止阈值（mdps）。 */
  static constexpr int32_t kOnlineBiasGyroStillThrMdps = 80;
  /** @brief 在线零偏更新: IIR 分母（越大越慢）。 */
  static constexpr int32_t kOnlineBiasIirDiv = 64;

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

  /**
   * @brief 上电静止阶段计算陀螺零偏。
   */
  void calibrate_gyro_bias() noexcept;

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
  /** @brief 陀螺 X 轴零偏，单位 mdps。 */
  int32_t gyro_bias_x_mdps_ = 0;
  /** @brief 陀螺 Y 轴零偏，单位 mdps。 */
  int32_t gyro_bias_y_mdps_ = 0;
  /** @brief 陀螺 Z 轴零偏，单位 mdps。 */
  int32_t gyro_bias_z_mdps_ = 0;
  /** @brief 陀螺零偏是否有效。 */
  bool gyro_bias_valid_ = false;
  /** @brief 连续静止样本计数。 */
  uint32_t still_streak_ = 0;
  /** @brief 在线零偏更新计数。 */
  uint32_t online_bias_updates_ = 0;
};

}  // namespace servers
