/**
 * @file sensor_service.cpp
 * @brief 传感器后台采样服务实现：按 SensorHub 注册表泛化采样与缓存。
 */

#include "servers/sensor_service.hpp"

#include <errno.h>
#include <stdio.h>
#include <string.h>

namespace servers {

void SensorService::threadEntry(void* p1, void*, void*) {
  static_cast<SensorService*>(p1)->threads();
}

int SensorService::find_cache_index(platform::SensorType type) const noexcept {
  for (size_t i = 0; i < cache_count_; ++i) {
    if (cache_[i].type == type) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

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

void SensorService::threads() noexcept {
  /*
   * 执行步骤：
   * 1) 线程循环按注册表遍历所有已注册传感器。
   * 2) 调用 Hub 的通用 read(type, ...) 获取样本。
   * 3) 成功则更新缓存，失败则记录限频错误日志。
   * 4) 到达日志周期后输出当前缓存快照。
   * 5) 停止请求到达后退出线程。
   */

  log_.info("sensor service starting");
  next_log_ms_ = k_uptime_get() + kLogPeriodMs;

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

    maybe_log_snapshot(k_uptime_get());
    k_sleep(K_MSEC(kSamplePeriodMs));
  }

  atomic_set(&running_, 0);
  thread_id_ = nullptr;
  log_.info("sensor service stopped");
}

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
          const auto& ina = *reinterpret_cast<const platform::Ina226Sample*>(sample);
          (void)snprintf(msg, sizeof(msg), "[sensor] INA226: V=%ldmV I=%ldmA P=%ldmW",
                         static_cast<long>(ina.bus_mv), static_cast<long>(ina.current_ma),
                         static_cast<long>(ina.power_mw));
          log_.info(msg);
        }
        break;
      }
      case platform::SensorType::Aht20: {
        if (sample_size >= sizeof(platform::Aht20Sample)) {
          const auto& aht = *reinterpret_cast<const platform::Aht20Sample*>(sample);
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

void SensorService::stop() noexcept {
  if (atomic_get(&running_) == 0) {
    return;
  }

  atomic_set(&stop_requested_, 1);
  if (thread_id_ != nullptr) {
    k_wakeup(thread_id_);
  }
}

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

int SensorService::get_latest_ina226(platform::Ina226Sample& out) noexcept {
  return get_latest(platform::SensorType::Ina226, &out, sizeof(out));
}

int SensorService::get_latest_aht20(platform::Aht20Sample& out) noexcept {
  return get_latest(platform::SensorType::Aht20, &out, sizeof(out));
}

}  // namespace servers
