/**
 * @file imu_service.cpp
 * @brief IMU 后台服务实现：100Hz 采样 ICM42688，并打印或发布样本。
 */

#include "servers/imu_service.hpp"

#include <errno.h>
#include <string.h>
#include <zephyr/sys/printk.h>

namespace servers {

/**
 * @brief 线程入口静态适配函数。
 * @param p1 ImuService 对象指针。
 * @param p2 未使用。
 * @param p3 未使用。
 */
void ImuService::threadEntry(void* p1, void*, void*) { static_cast<ImuService*>(p1)->threads(); }

/**
 * @brief 上电静止校准陀螺零偏。
 * @note 在服务线程启动后立即执行，期间仍可响应 stop 请求。
 */
void ImuService::calibrate_gyro_bias() noexcept {
  gyro_bias_x_mdps_ = 0;
  gyro_bias_y_mdps_ = 0;
  gyro_bias_z_mdps_ = 0;
  gyro_bias_valid_ = false;

  printk("[imu] gyro bias calibration start (%lld ms)\n", static_cast<long long>(kGyroBiasCalibMs));

  /* 先等待第一帧有效样本，避免上电初期设备未就绪导致整段校准无效。 */
  const int64_t wait_first_deadline_ms = k_uptime_get() + 10000;
  platform::ImuSample first_sample = {};
  bool first_ready = false;
  while (atomic_get(&stop_requested_) == 0 && k_uptime_get() < wait_first_deadline_ms) {
    const int ret = platform::imu_read_once(first_sample);
    if (ret == 0) {
      first_ready = true;
      break;
    }
    k_sleep(K_MSEC(kSamplePeriodMs));
  }
  if (!first_ready) {
    printk("[imu] gyro bias calibration skipped, no valid sample in startup window\n");
    return;
  }

  const int64_t deadline_ms = k_uptime_get() + kGyroBiasCalibMs;
  int64_t sum_x = 0;
  int64_t sum_y = 0;
  int64_t sum_z = 0;
  uint32_t valid_samples = 0U;
  uint32_t read_failures = 0U;

  /* 把第一帧有效样本计入校准统计。 */
  sum_x += first_sample.gyro_x_mdps;
  sum_y += first_sample.gyro_y_mdps;
  sum_z += first_sample.gyro_z_mdps;
  ++valid_samples;

  while (atomic_get(&stop_requested_) == 0 && k_uptime_get() < deadline_ms) {
    platform::ImuSample sample = {};
    const int ret = platform::imu_read_once(sample);
    if (ret == 0) {
      sum_x += sample.gyro_x_mdps;
      sum_y += sample.gyro_y_mdps;
      sum_z += sample.gyro_z_mdps;
      ++valid_samples;
    } else {
      ++read_failures;
    }
    k_sleep(K_MSEC(kSamplePeriodMs));
  }

  if (valid_samples == 0U) {
    printk("[imu] gyro bias calibration skipped, valid=0 fail=%lu\n",
           static_cast<unsigned long>(read_failures));
    return;
  }

  if (valid_samples < kGyroBiasMinSamples) {
    printk("[imu] gyro bias calibration degraded, valid=%lu fail=%lu (<%lu), still applying\n",
           static_cast<unsigned long>(valid_samples), static_cast<unsigned long>(read_failures),
           static_cast<unsigned long>(kGyroBiasMinSamples));
  }

  gyro_bias_x_mdps_ = static_cast<int32_t>(sum_x / static_cast<int64_t>(valid_samples));
  gyro_bias_y_mdps_ = static_cast<int32_t>(sum_y / static_cast<int64_t>(valid_samples));
  gyro_bias_z_mdps_ = static_cast<int32_t>(sum_z / static_cast<int64_t>(valid_samples));
  gyro_bias_valid_ = true;

  printk("[imu] gyro bias ready: (%ld,%ld,%ld)mdps, samples=%lu fail=%lu\n",
         static_cast<long>(gyro_bias_x_mdps_), static_cast<long>(gyro_bias_y_mdps_),
         static_cast<long>(gyro_bias_z_mdps_), static_cast<unsigned long>(valid_samples),
         static_cast<unsigned long>(read_failures));
}

/**
 * @brief IMU 服务线程主循环。
 * @note 以 100Hz 周期执行一次采样，成功后更新缓存并按配置打印/发布。
 */
void ImuService::threads() noexcept {
  printk("[imu] service starting\n");
  calibrate_gyro_bias();
  still_streak_ = 0U;
  online_bias_updates_ = 0U;

  int error_streak = 0;
  uint32_t sample_count = 0;
  while (atomic_get(&stop_requested_) == 0) {
    /* 步骤 1：从平台 IMU 驱动读取一次样本。 */
    platform::ImuSample sample_raw = {};
    const int ret = platform::imu_read_once(sample_raw);
    if (ret < 0) {
      ++error_streak;
      if (error_streak == 1 || (error_streak % 10) == 0) {
        printk("[imu] read failed: %d\n", ret);
      }
      k_sleep(K_MSEC(kSamplePeriodMs));
      continue;
    }
    error_streak = 0;
    ++sample_count;

    /* 步骤 2：对陀螺数据应用零偏，得到 corrected 样本。 */
    platform::ImuSample sample_corrected = sample_raw;
    if (gyro_bias_valid_) {
      sample_corrected.gyro_x_mdps = sample_raw.gyro_x_mdps - gyro_bias_x_mdps_;
      sample_corrected.gyro_y_mdps = sample_raw.gyro_y_mdps - gyro_bias_y_mdps_;
      sample_corrected.gyro_z_mdps = sample_raw.gyro_z_mdps - gyro_bias_z_mdps_;
    }

    /* 步骤 2.5：静止时在线慢速更新零偏，抵消温漂。 */
    if (gyro_bias_valid_) {
      const auto abs_i32 = [](const int32_t v) -> int32_t { return (v >= 0) ? v : -v; };

      const int64_t ax = sample_raw.accel_x_mg;
      const int64_t ay = sample_raw.accel_y_mg;
      const int64_t az = sample_raw.accel_z_mg;
      const int64_t acc_norm_sq = (ax * ax) + (ay * ay) + (az * az);
      const int64_t acc_low = static_cast<int64_t>(1000 - kOnlineBiasAccelNormTolMg) *
                              static_cast<int64_t>(1000 - kOnlineBiasAccelNormTolMg);
      const int64_t acc_high = static_cast<int64_t>(1000 + kOnlineBiasAccelNormTolMg) *
                               static_cast<int64_t>(1000 + kOnlineBiasAccelNormTolMg);

      const bool accel_still = (acc_norm_sq >= acc_low) && (acc_norm_sq <= acc_high);
      const bool gyro_still =
          (abs_i32(sample_corrected.gyro_x_mdps) <= kOnlineBiasGyroStillThrMdps) &&
          (abs_i32(sample_corrected.gyro_y_mdps) <= kOnlineBiasGyroStillThrMdps) &&
          (abs_i32(sample_corrected.gyro_z_mdps) <= kOnlineBiasGyroStillThrMdps);

      if (accel_still && gyro_still) {
        ++still_streak_;
        if (still_streak_ >= kOnlineBiasStreakSamples) {
          const int32_t dx = sample_raw.gyro_x_mdps - gyro_bias_x_mdps_;
          const int32_t dy = sample_raw.gyro_y_mdps - gyro_bias_y_mdps_;
          const int32_t dz = sample_raw.gyro_z_mdps - gyro_bias_z_mdps_;

          gyro_bias_x_mdps_ += (dx >= 0) ? ((dx + (kOnlineBiasIirDiv / 2)) / kOnlineBiasIirDiv)
                                         : ((dx - (kOnlineBiasIirDiv / 2)) / kOnlineBiasIirDiv);
          gyro_bias_y_mdps_ += (dy >= 0) ? ((dy + (kOnlineBiasIirDiv / 2)) / kOnlineBiasIirDiv)
                                         : ((dy - (kOnlineBiasIirDiv / 2)) / kOnlineBiasIirDiv);
          gyro_bias_z_mdps_ += (dz >= 0) ? ((dz + (kOnlineBiasIirDiv / 2)) / kOnlineBiasIirDiv)
                                         : ((dz - (kOnlineBiasIirDiv / 2)) / kOnlineBiasIirDiv);

          ++online_bias_updates_;

          /* 在线更新后刷新 corrected，保证发布与日志使用最新零偏。 */
          sample_corrected.gyro_x_mdps = sample_raw.gyro_x_mdps - gyro_bias_x_mdps_;
          sample_corrected.gyro_y_mdps = sample_raw.gyro_y_mdps - gyro_bias_y_mdps_;
          sample_corrected.gyro_z_mdps = sample_raw.gyro_z_mdps - gyro_bias_z_mdps_;

          if ((online_bias_updates_ % 200U) == 0U) {
            printk("[imu] gyro bias online update: (%ld,%ld,%ld)mdps\n",
                   static_cast<long>(gyro_bias_x_mdps_), static_cast<long>(gyro_bias_y_mdps_),
                   static_cast<long>(gyro_bias_z_mdps_));
          }
        }
      } else {
        still_streak_ = 0U;
      }
    }

    /* 步骤 3：更新最新样本缓存，并快照当前发布回调。 */
    k_mutex_lock(&mutex_, K_FOREVER);
    latest_ = sample_corrected;
    latest_valid_ = true;
    const ImuPublishCallback cb = publish_cb_;
    void* user = publish_user_;
    k_mutex_unlock(&mutex_);

    /* 步骤 4：若注册了发布回调，则把 corrected 样本发布给上层。 */
    if (cb != nullptr) {
      cb(sample_corrected, user);
    }

    /* 步骤 5：按配置打印样本，同时输出 raw 与 corrected 便于确认校准效果。 */
    if (kEnablePrint && ((sample_count % kPrintEveryNSamples) == 0U)) {
      printk(
          "[imu] A=(%ld,%ld,%ld)mg Graw=(%ld,%ld,%ld)mdps Gcorr=(%ld,%ld,%ld)mdps T=%ld.%03ldC\n",
          static_cast<long>(sample_raw.accel_x_mg), static_cast<long>(sample_raw.accel_y_mg),
          static_cast<long>(sample_raw.accel_z_mg), static_cast<long>(sample_raw.gyro_x_mdps),
          static_cast<long>(sample_raw.gyro_y_mdps), static_cast<long>(sample_raw.gyro_z_mdps),
          static_cast<long>(sample_corrected.gyro_x_mdps),
          static_cast<long>(sample_corrected.gyro_y_mdps),
          static_cast<long>(sample_corrected.gyro_z_mdps),
          static_cast<long>(sample_raw.temp_mc / 1000),
          static_cast<long>(sample_raw.temp_mc % 1000));
    }

    k_sleep(K_MSEC(kSamplePeriodMs));
  }

  atomic_set(&running_, 0);
  thread_id_ = nullptr;
  printk("[imu] service stopped\n");
}

/**
 * @brief 请求停止 IMU 服务线程。
 * @note 仅设置停止标志并尝试唤醒线程，不阻塞等待退出。
 */
void ImuService::stop() noexcept {
  if (atomic_get(&running_) == 0) {
    return;
  }

  atomic_set(&stop_requested_, 1);
  if (thread_id_ != nullptr) {
    k_wakeup(thread_id_);
  }
}

/**
 * @brief 启动 IMU 服务线程（幂等）。
 * @return 0 表示成功或已在运行；负值表示启动失败。
 */
int ImuService::run() noexcept {
  if (!atomic_cas(&running_, 0, 1)) {
    printk("[imu] service already running\n");
    return 0;
  }

  const int ret = platform::imu_init();
  if (ret < 0) {
    atomic_set(&running_, 0);
    printk("[imu] init failed: %d\n", ret);
    return ret;
  }

  k_mutex_init(&mutex_);
  k_mutex_lock(&mutex_, K_FOREVER);
  latest_ = {};
  latest_valid_ = false;
  k_mutex_unlock(&mutex_);

  atomic_set(&stop_requested_, 0);
  thread_id_ = k_thread_create(&thread_, stack_, K_THREAD_STACK_SIZEOF(stack_), threadEntry, this,
                               nullptr, nullptr, kPriority, 0, K_NO_WAIT);
  if (thread_id_ == nullptr) {
    atomic_set(&running_, 0);
    printk("[imu] failed to create thread\n");
    return -1;
  }

  k_thread_name_set(thread_id_, "imu_service");
  return 0;
}

/**
 * @brief 设置样本发布回调。
 * @param cb 发布回调，可为 nullptr。
 * @param user 回调用户参数。
 */
void ImuService::set_publish_callback(ImuPublishCallback cb, void* user) noexcept {
  k_mutex_lock(&mutex_, K_FOREVER);
  publish_cb_ = cb;
  publish_user_ = user;
  k_mutex_unlock(&mutex_);
}

/**
 * @brief 获取最新 IMU 样本。
 * @param out 输出样本。
 * @return 0 表示成功；-EAGAIN 表示暂无有效样本。
 */
int ImuService::get_latest(platform::ImuSample& out) noexcept {
  k_mutex_lock(&mutex_, K_FOREVER);
  if (!latest_valid_) {
    k_mutex_unlock(&mutex_);
    return -EAGAIN;
  }
  out = latest_;
  k_mutex_unlock(&mutex_);
  return 0;
}

}  // namespace servers
