/**
 * @file sensor_service.cpp
 * @brief 传感器后台采样服务实现：按 SensorHub 注册表泛化采样与缓存。
 */

#include "servers/sensor_service.hpp"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/rtc.h>

#include "platform/platform_storage.hpp"

namespace servers {

/**
 * @brief 读取 RTC 当前时间。
 * @param[out] out 输出 RTC 时间结构体。
 * @return 0 表示成功；负值表示失败。
 */
int read_rtc_beijing_time(struct rtc_time& out) noexcept {
  const struct device* rtc_dev = DEVICE_DT_GET_OR_NULL(DT_NODELABEL(rtc));
  if (rtc_dev == nullptr || !device_is_ready(rtc_dev)) {
    return -ENODEV;
  }
  return rtc_get_time(rtc_dev, &out);
}

/**
 * @brief 服务线程静态入口适配函数。
 * @param p1 SensorService 对象指针。
 * @param p2 未使用。
 * @param p3 未使用。
 */
void SensorService::threadEntry(void* p1, void*, void*) {
  static_cast<SensorService*>(p1)->threads();
}

/**
 * @brief 按类型在缓存槽位中查找下标。
 * @param type 传感器类型。
 * @return 有效下标；未找到返回 -1。
 */
int SensorService::find_cache_index(platform::SensorType type) const noexcept {
  for (size_t i = 0; i < cache_count_; ++i) {
    if (cache_[i].type == type) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

/**
 * @brief 按 SensorHub 注册表重建缓存布局。
 * @return 0 表示成功；负值表示失败。
 */
int SensorService::rebuild_cache_layout() noexcept {
  const size_t count = sensor_hub_.registered_count();
  if (count > platform::SensorHub::kMaxDrivers) {
    return -ENOSPC;
  }

  cache_count_ = count;
  for (size_t i = 0; i < cache_count_; ++i) {
    platform::SensorType type = platform::SensorType::Ina226;
    int ret = sensor_hub_.registered_type_at(i, type);
    if (ret < 0) {
      return ret;
    }

    size_t sample_size = 0;
    ret = sensor_hub_.sample_size(type, sample_size);
    if (ret < 0) {
      return ret;
    }
    if (sample_size > kMaxSampleBytes) {
      return -ENOSPC;
    }

    cache_[i].type = type;
    cache_[i].sample_size = sample_size;
    cache_[i].valid = false;
    cache_[i].error_streak = 0;
    (void)memset(cache_[i].data, 0, sizeof(cache_[i].data));
  }

  return 0;
}

/**
 * @brief 根据 RTC 北京时间生成本轮落盘文件路径。
 * @return 0 表示成功；负值表示失败。
 */
int SensorService::build_persist_file_path_from_rtc() noexcept {
  struct rtc_time rtc_now = {};
  const int ret = read_rtc_beijing_time(rtc_now);
  if (ret < 0) {
    return ret;
  }

  const int n = snprintf(persist_file_path_, sizeof(persist_file_path_),
                         "/SD:/%04d%02d%02d_%02d%02d%02d_sensor.csv", rtc_now.tm_year + 1900,
                         rtc_now.tm_mon + 1, rtc_now.tm_mday, rtc_now.tm_hour, rtc_now.tm_min,
                         rtc_now.tm_sec);
  if (n <= 0 || static_cast<size_t>(n) >= sizeof(persist_file_path_)) {
    return -ENOSPC;
  }

  char msg[128] = {};
  (void)snprintf(msg, sizeof(msg), "[sensor] persist file: %s", persist_file_path_);
  log_.info(msg);
  return 0;
}

void SensorService::threads() noexcept {
  /*
   * 执行步骤：
   * 1) 线程循环按注册表遍历所有已注册传感器。
   * 2) 调用 Hub 的通用 read(type, ...) 获取样本。
   * 3) 成功则更新缓存，失败则记录限频错误日志。
   * 4) 到达日志周期后输出当前缓存快照。
   * 5) 到达持久化周期后写入 SD 卡。
   * 6) 停止请求到达后退出线程。
   */

  log_.info("sensor service starting");
  const int64_t now_ms = k_uptime_get();
  next_log_ms_ = now_ms + kLogPeriodMs;
  next_persist_ms_ = now_ms + kPersistPeriodMs;

  while (atomic_get(&stop_requested_) == 0) {
    for (size_t i = 0; i < cache_count_; ++i) {
      const int ret = sensor_hub_.read(cache_[i].type, cache_[i].data, cache_[i].sample_size);
      if (ret == 0) {
        k_mutex_lock(&mutex_, K_FOREVER);
        cache_[i].valid = true;
        k_mutex_unlock(&mutex_);
        cache_[i].error_streak = 0;
      } else {
        ++cache_[i].error_streak;
        if (cache_[i].error_streak == 1U || (cache_[i].error_streak % 10U) == 0U) {
          char msg[96];
          (void)snprintf(msg, sizeof(msg), "sensor sample failed type=%u",
                         static_cast<unsigned int>(cache_[i].type));
          log_.error(msg, ret);
        }
      }
    }

    const int64_t loop_now_ms = k_uptime_get();
    maybe_log_snapshot(loop_now_ms);
    maybe_persist_snapshot(loop_now_ms);
    k_sleep(K_MSEC(kSamplePeriodMs));
  }

  atomic_set(&running_, 0);
  thread_id_ = nullptr;
  log_.info("sensor service stopped");
}

/**
 * @brief 日志周期处理：按类型打印当前快照。
 * @param now_ms 当前系统 uptime 毫秒。
 */
void SensorService::maybe_log_snapshot(int64_t now_ms) noexcept {
  if (now_ms < next_log_ms_) {
    return;
  }

  bool any_valid = false;
  for (size_t i = 0; i < cache_count_; ++i) {
    k_mutex_lock(&mutex_, K_FOREVER);
    const bool valid = cache_[i].valid;
    const platform::SensorType type = cache_[i].type;
    uint8_t sample[kMaxSampleBytes] = {};
    if (valid) {
      (void)memcpy(sample, cache_[i].data, cache_[i].sample_size);
    }
    const size_t sample_size = cache_[i].sample_size;
    k_mutex_unlock(&mutex_);

    if (!valid) {
      continue;
    }
    any_valid = true;

    char msg[180];
    switch (type) {
      case platform::SensorType::Ina226: {
        if (sample_size >= sizeof(platform::Ina226Sample)) {
          platform::Ina226Sample ina = {};
          (void)memcpy(&ina, sample, sizeof(ina));
          (void)snprintf(msg, sizeof(msg), "[sensor] INA226: V=%ldmV I=%ldmA P=%ldmW",
                         static_cast<long>(ina.bus_mv), static_cast<long>(ina.current_ma),
                         static_cast<long>(ina.power_mw));
          log_.info(msg);
        }
        break;
      }
      case platform::SensorType::Aht20: {
        if (sample_size >= sizeof(platform::Aht20Sample)) {
          platform::Aht20Sample aht = {};
          (void)memcpy(&aht, sample, sizeof(aht));
          (void)snprintf(
              msg, sizeof(msg), "[sensor] AHT20: T=%ld.%03ldC RH=%ld.%01ld%%",
              static_cast<long>(aht.temp_mc / 1000), static_cast<long>(aht.temp_mc % 1000),
              static_cast<long>(aht.rh_mpermille / 10), static_cast<long>(aht.rh_mpermille % 10));
          log_.info(msg);
        }
        break;
      }
      default:
        (void)snprintf(msg, sizeof(msg), "[sensor] type=%u sample updated",
                       static_cast<unsigned int>(type));
        log_.info(msg);
        break;
    }
  }

  if (!any_valid) {
    log_.info("[sensor] waiting first valid samples");
  }
  next_log_ms_ = now_ms + kLogPeriodMs;
}

/**
 * @brief 持久化周期处理：按周期触发一次落盘。
 * @param now_ms 当前系统 uptime 毫秒。
 */
void SensorService::maybe_persist_snapshot(const int64_t now_ms) noexcept {
  if (now_ms < next_persist_ms_) {
    return;
  }
  persist_snapshot_to_storage(now_ms);
  next_persist_ms_ = now_ms + kPersistPeriodMs;
}

/**
 * @brief 将当前传感器快照追加写入 SD 卡 CSV。
 * @param now_ms 当前系统 uptime 毫秒。
 * @note 写失败后会关闭本轮持久化，避免反复阻塞。
 */
void SensorService::persist_snapshot_to_storage(const int64_t now_ms) noexcept {
  (void)now_ms;

  /* 步骤 1：检查持久化开关。 */
  if (!storage_persist_enabled_) {
    return;
  }

  platform::Ina226Sample ina = {};
  platform::Aht20Sample aht = {};
  bool ina_valid = false;
  bool aht_valid = false;

  /* 步骤 2：从缓存中提取最新 INA226/AHT20 样本。 */
  for (size_t i = 0; i < cache_count_; ++i) {
    k_mutex_lock(&mutex_, K_FOREVER);
    const platform::SensorType type = cache_[i].type;
    const bool valid = cache_[i].valid;
    const size_t sample_size = cache_[i].sample_size;
    uint8_t sample[kMaxSampleBytes] = {};
    if (valid) {
      (void)memcpy(sample, cache_[i].data, sample_size);
    }
    k_mutex_unlock(&mutex_);

    if (!valid) {
      continue;
    }

    if (type == platform::SensorType::Ina226 && sample_size >= sizeof(platform::Ina226Sample)) {
      (void)memcpy(&ina, sample, sizeof(ina));
      ina_valid = true;
    } else if (type == platform::SensorType::Aht20 &&
               sample_size >= sizeof(platform::Aht20Sample)) {
      (void)memcpy(&aht, sample, sizeof(aht));
      aht_valid = true;
    }
  }

  if (!ina_valid && !aht_valid) {
    return;
  }

  if (persist_file_path_[0] == '\0') {
    storage_persist_enabled_ = false;
    log_.error("[sensor] persist file path not ready", -EINVAL);
    return;
  }

  /* 步骤 3：首次写入时补 CSV 表头。 */
  if (!storage_header_written_) {
    static constexpr char kHeader[] =
        "beijing_time,bus_mv,current_ma,power_mw,temp_mc,rh_mpermille\n";
    const int header_ret =
        platform::storage().write_file(persist_file_path_, kHeader, sizeof(kHeader) - 1U, false);
    if (header_ret < 0) {
      ++storage_error_streak_;
      if (storage_error_streak_ == 1U || (storage_error_streak_ % 10U) == 0U) {
        log_.error("[sensor] sd write header failed", header_ret);
      }
      storage_persist_enabled_ = false;
      log_.error("[sensor] sd persist disabled after header write failure", header_ret);
      return;
    }
    storage_header_written_ = true;
  }

  /* 步骤 4：读取 RTC 北京时间并格式化一行 CSV。 */
  struct rtc_time rtc_now = {};
  const int rtc_ret = read_rtc_beijing_time(rtc_now);
  if (rtc_ret < 0) {
    ++storage_error_streak_;
    if (storage_error_streak_ == 1U || (storage_error_streak_ % 10U) == 0U) {
      log_.error("[sensor] rtc read failed, skip persist", rtc_ret);
    }
    return;
  }

  char beijing_time[80] = {};
  (void)snprintf(beijing_time, sizeof(beijing_time), "%04d-%02d-%02d %02d:%02d:%02d",
                 rtc_now.tm_year + 1900, rtc_now.tm_mon + 1, rtc_now.tm_mday, rtc_now.tm_hour,
                 rtc_now.tm_min, rtc_now.tm_sec);

  char line[200] = {};
  const int32_t bus_mv = ina_valid ? ina.bus_mv : -1;
  const int32_t current_ma = ina_valid ? ina.current_ma : -1;
  const int32_t power_mw = ina_valid ? ina.power_mw : -1;
  const int32_t temp_mc = aht_valid ? aht.temp_mc : -1;
  const int32_t rh_mpermille = aht_valid ? aht.rh_mpermille : -1;
  const int n = snprintf(line, sizeof(line), "%s,%ld,%ld,%ld,%ld,%ld\n", beijing_time,
                         static_cast<long>(bus_mv), static_cast<long>(current_ma),
                         static_cast<long>(power_mw), static_cast<long>(temp_mc),
                         static_cast<long>(rh_mpermille));
  if (n <= 0 || static_cast<size_t>(n) >= sizeof(line)) {
    return;
  }

  const int ret =
      platform::storage().write_file(persist_file_path_, line, static_cast<size_t>(n), true);
  if (ret < 0) {
    ++storage_error_streak_;
    if (storage_error_streak_ == 1U || (storage_error_streak_ % 10U) == 0U) {
      log_.error("[sensor] sd write sample failed", ret);
    }
    storage_persist_enabled_ = false;
    log_.error("[sensor] sd persist disabled after sample write failure", ret);
    return;
  }

  storage_error_streak_ = 0U;
}

/**
 * @brief 请求停止传感器服务线程。
 */
void SensorService::stop() noexcept {
  if (atomic_get(&running_) == 0) {
    return;
  }

  atomic_set(&stop_requested_, 1);
  if (thread_id_ != nullptr) {
    k_wakeup(thread_id_);
  }
}

/**
 * @brief 启动传感器服务线程（幂等）。
 * @return 0 表示成功；负值表示失败。
 */
int SensorService::run() noexcept {
  if (!atomic_cas(&running_, 0, 1)) {
    log_.info("sensor service already running");
    return 0;
  }

  int ret = sensor_hub_.init_all();
  if (ret < 0) {
    atomic_set(&running_, 0);
    log_.error("failed to init sensors", ret);
    return ret;
  }

  k_mutex_init(&mutex_);
  ret = rebuild_cache_layout();
  if (ret < 0) {
    atomic_set(&running_, 0);
    log_.error("failed to build sensor cache layout", ret);
    return ret;
  }

  atomic_set(&stop_requested_, 0);
  next_log_ms_ = 0;
  next_persist_ms_ = 0;
  storage_error_streak_ = 0U;
  storage_header_written_ = false;
  storage_persist_enabled_ = true;
  (void)memset(persist_file_path_, 0, sizeof(persist_file_path_));

  ret = build_persist_file_path_from_rtc();
  if (ret < 0) {
    atomic_set(&running_, 0);
    log_.error("failed to build sensor persist file name from rtc", ret);
    return ret;
  }

  thread_id_ = k_thread_create(&thread_, stack_, K_THREAD_STACK_SIZEOF(stack_), threadEntry, this,
                               nullptr, nullptr, kPriority, 0, K_NO_WAIT);
  if (thread_id_ == nullptr) {
    atomic_set(&running_, 0);
    log_.error("failed to create sensor service task", -1);
    return -1;
  }

  k_thread_name_set(thread_id_, "sensor_service");
  return 0;
}

/**
 * @brief 获取指定类型传感器的最新缓存样本。
 * @param type 传感器类型。
 * @param out 输出缓冲区。
 * @param out_size 输出缓冲区字节数。
 * @return 0 表示成功；负值表示失败。
 */
int SensorService::get_latest(platform::SensorType type, void* out, size_t out_size) noexcept {
  if (out == nullptr) {
    return -EINVAL;
  }

  k_mutex_lock(&mutex_, K_FOREVER);
  const int idx = find_cache_index(type);
  if (idx < 0) {
    k_mutex_unlock(&mutex_);
    return -ENOENT;
  }
  if (!cache_[idx].valid) {
    k_mutex_unlock(&mutex_);
    return -EAGAIN;
  }
  if (out_size < cache_[idx].sample_size) {
    k_mutex_unlock(&mutex_);
    return -ENOSPC;
  }
  (void)memcpy(out, cache_[idx].data, cache_[idx].sample_size);
  k_mutex_unlock(&mutex_);
  return 0;
}

/**
 * @brief 获取最新 INA226 样本。
 * @param out 输出样本。
 * @return 0 表示成功；负值表示失败。
 */
int SensorService::get_latest_ina226(platform::Ina226Sample& out) noexcept {
  return get_latest(platform::SensorType::Ina226, &out, sizeof(out));
}

/**
 * @brief 获取最新 AHT20 样本。
 * @param out 输出样本。
 * @return 0 表示成功；负值表示失败。
 */
int SensorService::get_latest_aht20(platform::Aht20Sample& out) noexcept {
  return get_latest(platform::SensorType::Aht20, &out, sizeof(out));
}

}  // namespace servers
