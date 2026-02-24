/**
 * @file encoder_service.cpp
 * @brief EC11 encoder background service implementation.
 */

#include "servers/encoder_service.hpp"

#include <errno.h>
#include <stdio.h>

namespace servers {

namespace {

/**
 * @brief Convert linear angle difference to circular difference in [-180, 180].
 */
int32_t circular_delta_deg(int32_t now_deg, int32_t prev_deg) noexcept {
  int32_t delta = now_deg - prev_deg;
  if (delta > 180) {
    delta -= 360;
  } else if (delta < -180) {
    delta += 360;
  }
  return delta;
}

}  // namespace

void EncoderService::threadEntry(void* p1, void*, void*) {
  static_cast<EncoderService*>(p1)->threads();
}

void EncoderService::threads() noexcept {
  log_.info("encoder service starting");

  int32_t last_position = 0;
  bool have_last_position = false;
  int32_t residual_deg = 0;
  uint32_t error_streak = 0;

  while (atomic_get(&stop_requested_) == 0) {
    platform::EncoderSample sample = {};
    const int ret = platform::encoder_read_once(sample);
    if (ret < 0) {
      ++error_streak;
      if (error_streak == 1U || (error_streak % 10U) == 0U) {
        log_.error("encoder read failed", ret);
      }
      k_sleep(K_MSEC(kSamplePeriodMs));
      continue;
    }
    error_streak = 0;

    if (!have_last_position || sample.position_deg != last_position) {
      const int32_t delta =
          have_last_position ? circular_delta_deg(sample.position_deg, last_position) : 0;
      residual_deg += delta;
      const int32_t step_delta = residual_deg / kDegPerStep;
      residual_deg -= (step_delta * kDegPerStep);

      int64_t count_snapshot = 0;
      k_mutex_lock(&mutex_, K_FOREVER);
      latest_ = sample;
      latest_valid_ = true;
      count_ += step_delta;
      count_snapshot = count_;
      k_mutex_unlock(&mutex_);

      char msg[96];
      (void)snprintf(msg, sizeof(msg), "[enc] pos=%ld deg delta=%ld deg count=%lld",
                     static_cast<long>(sample.position_deg), static_cast<long>(delta),
                     static_cast<long long>(count_snapshot));
      log_.info(msg);
      last_position = sample.position_deg;
      have_last_position = true;
    } else {
      k_mutex_lock(&mutex_, K_FOREVER);
      latest_ = sample;
      latest_valid_ = true;
      k_mutex_unlock(&mutex_);
    }

    k_sleep(K_MSEC(kSamplePeriodMs));
  }

  atomic_set(&running_, 0);
  thread_id_ = nullptr;
  log_.info("encoder service stopped");
}

void EncoderService::stop() noexcept {
  if (atomic_get(&running_) == 0) {
    return;
  }

  atomic_set(&stop_requested_, 1);
  if (thread_id_ != nullptr) {
    k_wakeup(thread_id_);
  }
}

int EncoderService::run() noexcept {
  if (!atomic_cas(&running_, 0, 1)) {
    log_.info("encoder service already running");
    return 0;
  }

  int ret = platform::encoder_init();
  if (ret < 0) {
    atomic_set(&running_, 0);
    log_.error("failed to init encoder", ret);
    return ret;
  }

  k_mutex_init(&mutex_);
  k_mutex_lock(&mutex_, K_FOREVER);
  latest_ = {};
  latest_valid_ = false;
  count_ = 0;
  k_mutex_unlock(&mutex_);

  atomic_set(&stop_requested_, 0);
  thread_id_ = k_thread_create(&thread_, stack_, K_THREAD_STACK_SIZEOF(stack_), threadEntry, this,
                               nullptr, nullptr, kPriority, 0, K_NO_WAIT);
  if (thread_id_ == nullptr) {
    atomic_set(&running_, 0);
    log_.error("failed to create encoder service thread", -1);
    return -1;
  }

  k_thread_name_set(thread_id_, "encoder_service");
  return 0;
}

int EncoderService::get_latest(platform::EncoderSample& out) noexcept {
  k_mutex_lock(&mutex_, K_FOREVER);
  if (!latest_valid_) {
    k_mutex_unlock(&mutex_);
    return -EAGAIN;
  }
  out = latest_;
  k_mutex_unlock(&mutex_);
  return 0;
}

int EncoderService::get_count(int64_t& out) noexcept {
  k_mutex_lock(&mutex_, K_FOREVER);
  out = count_;
  k_mutex_unlock(&mutex_);
  return 0;
}

}  // namespace servers
