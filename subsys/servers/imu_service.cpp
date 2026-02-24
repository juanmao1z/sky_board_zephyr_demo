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
 * @brief IMU 服务线程主循环。
 * @note 以 100Hz 周期执行一次采样，成功后更新缓存并按配置打印/发布。
 */
void ImuService::threads() noexcept {
  printk("[imu] service starting\n");

  int error_streak = 0;
  uint32_t sample_count = 0;
  while (atomic_get(&stop_requested_) == 0) {
    /* 步骤 1：从平台 IMU 驱动读取一次样本。 */
    platform::ImuSample sample = {};
    const int ret = platform::imu_read_once(sample);
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

    /* 步骤 2：更新最新样本缓存，并快照当前发布回调。 */
    k_mutex_lock(&mutex_, K_FOREVER);
    latest_ = sample;
    latest_valid_ = true;
    const ImuPublishCallback cb = publish_cb_;
    void* user = publish_user_;
    k_mutex_unlock(&mutex_);

    /* 步骤 3：若注册了发布回调，则把样本发布给上层。 */
    if (cb != nullptr) {
      cb(sample, user);
    }

    /* 步骤 4：按配置打印样本。 */
    if (kEnablePrint && ((sample_count % kPrintEveryNSamples) == 0U)) {
      printk("[imu] A=(%ld,%ld,%ld)mg G=(%ld,%ld,%ld)mdps T=%ld.%03ldC\n",
             static_cast<long>(sample.accel_x_mg), static_cast<long>(sample.accel_y_mg),
             static_cast<long>(sample.accel_z_mg), static_cast<long>(sample.gyro_x_mdps),
             static_cast<long>(sample.gyro_y_mdps), static_cast<long>(sample.gyro_z_mdps),
             static_cast<long>(sample.temp_mc / 1000), static_cast<long>(sample.temp_mc % 1000));
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
