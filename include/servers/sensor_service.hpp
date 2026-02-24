/**
 * @file sensor_service.hpp
 * @brief 传感器后台采样服务声明：周期采集 INA226/AHT20 并缓存。
 */

#pragma once

#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>

#include <cstdint>

#include "platform/ilogger.hpp"
#include "platform/platform_sensors.hpp"

namespace servers {

/**
 * @brief 传感器后台服务。
 * @note 启动后在独立线程中周期采样并更新最新缓存，供业务线程读取。
 */
class SensorService {
 public:
  /**
   * @brief 构造传感器服务。
   * @param log 日志接口引用，必须在服务生命周期内保持有效。
   * @param sensor_hub 传感器管理中心，默认使用全局实例。
   */
  explicit SensorService(platform::ILogger& log,
                         platform::SensorHub& sensor_hub = platform::sensor_hub())
      : log_(log), sensor_hub_(sensor_hub) {}

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
   * @brief 获取最新 INA226 样本。
   * @param out 输出样本。
   * @return 0 表示成功；-EAGAIN 表示暂无有效样本。
   */
  int get_latest_ina226(platform::Ina226Sample& out) noexcept;

  /**
   * @brief 获取最新 AHT20 样本。
   * @param out 输出样本。
   * @return 0 表示成功；-EAGAIN 表示暂无有效样本。
   */
  int get_latest_aht20(platform::Aht20Sample& out) noexcept;
  /**
   * @brief 获取指定类型传感器的最新样本（通用接口）。
   * @param type 传感器类型。
   * @param out 输出缓冲区。
   * @param out_size 输出缓冲区大小。
   * @return 0 表示成功；-EAGAIN 表示暂无有效样本。
   */
  int get_latest(platform::SensorType type, void* out, size_t out_size) noexcept;

 private:
  /** @brief 服务线程栈大小（字节）。 */
  static constexpr size_t kStackSize = 1536;
  /** @brief 服务线程优先级。 */
  static constexpr int kPriority = K_LOWEST_APPLICATION_THREAD_PRIO;
  /** @brief 采样周期（毫秒）。 */
  static constexpr int64_t kSamplePeriodMs = 1000;
  /** @brief 日志输出周期（毫秒）。 */
  static constexpr int64_t kLogPeriodMs = 5000;
  /** @brief 每个样本缓存槽位的最大字节数。 */
  static constexpr size_t kMaxSampleBytes = 64;

  /**
   * @brief 线程入口静态适配函数。
   * @param p1 SensorService 对象指针。
   * @param p2 未使用。
   * @param p3 未使用。
   */
  static void threadEntry(void* p1, void* p2, void* p3);

  /**
   * @brief 服务线程主循环。
   */
  void threads() noexcept;

  /**
   * @brief 在日志周期到期时输出聚合日志。
   * @param now_ms 当前系统 uptime 毫秒。
   */
  void maybe_log_snapshot(int64_t now_ms) noexcept;
  /**
   * @brief 查找指定类型样本缓存槽位下标。
   * @param type 传感器类型。
   * @return 有效下标；未找到返回 -1。
   */
  int find_cache_index(platform::SensorType type) const noexcept;
  /**
   * @brief 根据 Hub 注册表重建缓存槽位元数据。
   * @return 0 表示成功；负值表示失败。
   */
  int rebuild_cache_layout() noexcept;
  /** @brief 日志接口。 */
  platform::ILogger& log_;
  /** @brief 传感器管理中心。 */
  platform::SensorHub& sensor_hub_;
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
  /** @brief 缓存互斥锁，保护样本读写一致性。 */
  struct k_mutex mutex_{};
  /** @brief 通用样本缓存槽位。 */
  struct SampleCacheEntry {
    platform::SensorType type = platform::SensorType::Ina226;
    size_t sample_size = 0;
    bool valid = false;
    uint8_t data[kMaxSampleBytes] = {};
    uint32_t error_streak = 0;
  };
  SampleCacheEntry cache_[platform::SensorHub::kMaxDrivers] = {};
  size_t cache_count_ = 0;
  /** @brief 下一次允许输出日志的时间点。 */
  int64_t next_log_ms_ = 0;
};

}  // namespace servers
