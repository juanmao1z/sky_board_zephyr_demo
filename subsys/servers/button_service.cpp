/**
 * @file button_service.cpp
 * @brief 按键后台服务实现.
 */

#include "servers/button_service.hpp"

#include <errno.h>

namespace servers {

/**
 * @brief 默认按键事件回调.
 * @param id 按键标识.
 * @param pressed true 表示按下事件, false 表示释放事件.
 * @param long_press true 表示当前释放事件被判定为长按.
 * @param ts_ms 事件时间戳, 单位毫秒.
 * @param hold_ms 按住时长, 仅释放事件有效.
 * @param user 用户上下文指针, 约定为 ButtonService*.
 * @note
 * 1) 默认输出 down/up 事件日志.
 * 2) 在释放事件里输出短按或长按统计日志.
 */
void ButtonService::default_callback(platform::ButtonId id, const bool pressed,
                                     const bool long_press, const int64_t ts_ms,
                                     const int64_t hold_ms, void* user) {
  auto* service = static_cast<ButtonService*>(user);
  if (service == nullptr) {
    return;
  }

  service->log_.infof("[btn] key=%u state=%s ts=%lld",
                      static_cast<unsigned int>(static_cast<uint8_t>(id) + 1U),
                      pressed ? "down" : "up", static_cast<long long>(ts_ms));

  if (pressed) {
    return;
  }

  switch (id) {
    case platform::ButtonId::kKey1:
      long_press ? key1_long(ts_ms, hold_ms, service) : key1_short(ts_ms, hold_ms, service);
      break;
    case platform::ButtonId::kKey2:
      long_press ? key2_long(ts_ms, hold_ms, service) : key2_short(ts_ms, hold_ms, service);
      break;
    case platform::ButtonId::kKey3:
      long_press ? key3_long(ts_ms, hold_ms, service) : key3_short(ts_ms, hold_ms, service);
      break;
    default:
      break;
  }
}

/**
 * @brief KEY1 短按业务处理.
 * @param ts_ms 事件时间戳, 单位毫秒.
 * @param hold_ms 按住时长, 单位毫秒.
 * @param user 用户上下文, 约定为 ButtonService*.
 */
void ButtonService::key1_short(const int64_t ts_ms, const int64_t hold_ms, void* user) {
  auto* service = static_cast<ButtonService*>(user);
  if (service == nullptr) {
    return;
  }

  uint32_t count = 0U;
  if (service->get_press_count(platform::ButtonId::kKey1, count) != 0) {
    return;
  }

  service->log_.infof("[btn] KEY1 short action ts=%lld hold=%lldms count=%lu",
                      static_cast<long long>(ts_ms), static_cast<long long>(hold_ms),
                      static_cast<unsigned long>(count));
}

/**
 * @brief KEY1 长按业务处理.
 * @param ts_ms 事件时间戳, 单位毫秒.
 * @param hold_ms 按住时长, 单位毫秒.
 * @param user 用户上下文, 约定为 ButtonService*.
 */
void ButtonService::key1_long(const int64_t ts_ms, const int64_t hold_ms, void* user) {
  auto* service = static_cast<ButtonService*>(user);
  if (service == nullptr) {
    return;
  }

  uint32_t count = 0U;
  if (service->get_long_press_count(platform::ButtonId::kKey1, count) != 0) {
    return;
  }

  service->log_.infof("[btn] KEY1 long action ts=%lld hold=%lldms count=%lu",
                      static_cast<long long>(ts_ms), static_cast<long long>(hold_ms),
                      static_cast<unsigned long>(count));
}

/**
 * @brief KEY2 短按业务处理.
 * @param ts_ms 事件时间戳, 单位毫秒.
 * @param hold_ms 按住时长, 单位毫秒.
 * @param user 用户上下文, 约定为 ButtonService*.
 */
void ButtonService::key2_short(const int64_t ts_ms, const int64_t hold_ms, void* user) {
  auto* service = static_cast<ButtonService*>(user);
  if (service == nullptr) {
    return;
  }

  uint32_t count = 0U;
  if (service->get_press_count(platform::ButtonId::kKey2, count) != 0) {
    return;
  }

  service->log_.infof("[btn] KEY2 short action ts=%lld hold=%lldms count=%lu",
                      static_cast<long long>(ts_ms), static_cast<long long>(hold_ms),
                      static_cast<unsigned long>(count));
}

/**
 * @brief KEY2 长按业务处理.
 * @param ts_ms 事件时间戳, 单位毫秒.
 * @param hold_ms 按住时长, 单位毫秒.
 * @param user 用户上下文, 约定为 ButtonService*.
 */
void ButtonService::key2_long(const int64_t ts_ms, const int64_t hold_ms, void* user) {
  auto* service = static_cast<ButtonService*>(user);
  if (service == nullptr) {
    return;
  }

  uint32_t count = 0U;
  if (service->get_long_press_count(platform::ButtonId::kKey2, count) != 0) {
    return;
  }

  service->log_.infof("[btn] KEY2 long action ts=%lld hold=%lldms count=%lu",
                      static_cast<long long>(ts_ms), static_cast<long long>(hold_ms),
                      static_cast<unsigned long>(count));
}

/**
 * @brief KEY3 短按业务处理.
 * @param ts_ms 事件时间戳, 单位毫秒.
 * @param hold_ms 按住时长, 单位毫秒.
 * @param user 用户上下文, 约定为 ButtonService*.
 */
void ButtonService::key3_short(const int64_t ts_ms, const int64_t hold_ms, void* user) {
  auto* service = static_cast<ButtonService*>(user);
  if (service == nullptr) {
    return;
  }

  uint32_t count = 0U;
  if (service->get_press_count(platform::ButtonId::kKey3, count) != 0) {
    return;
  }

  service->log_.infof("[btn] KEY3 short action ts=%lld hold=%lldms count=%lu",
                      static_cast<long long>(ts_ms), static_cast<long long>(hold_ms),
                      static_cast<unsigned long>(count));
}

/**
 * @brief KEY3 长按业务处理.
 * @param ts_ms 事件时间戳, 单位毫秒.
 * @param hold_ms 按住时长, 单位毫秒.
 * @param user 用户上下文, 约定为 ButtonService*.
 */
void ButtonService::key3_long(const int64_t ts_ms, const int64_t hold_ms, void* user) {
  auto* service = static_cast<ButtonService*>(user);
  if (service == nullptr) {
    return;
  }

  uint32_t count = 0U;
  if (service->get_long_press_count(platform::ButtonId::kKey3, count) != 0) {
    return;
  }

  service->log_.infof("[btn] KEY3 long action ts=%lld hold=%lldms count=%lu",
                      static_cast<long long>(ts_ms), static_cast<long long>(hold_ms),
                      static_cast<unsigned long>(count));
}

/**
 * @brief 线程入口静态适配函数.
 * @param p1 ButtonService 实例指针.
 * @param p2 未使用.
 * @param p3 未使用.
 */
void ButtonService::threadEntry(void* p1, void*, void*) {
  static_cast<ButtonService*>(p1)->threads();
}

/**
 * @brief 按键服务主线程.
 * @note
 * 1) 阻塞读取平台按键事件.
 * 2) 在释放沿计算按住时长并判定短按/长按.
 * 3) 更新内部状态缓存.
 * 4) 在锁外触发回调, 避免回调内潜在阻塞导致死锁.
 */
void ButtonService::threads() noexcept {
  log_.info("button service starting");
  uint32_t error_streak = 0U;

  while (atomic_get(&stop_requested_) == 0) {
    platform::ButtonEvent evt = {};
    const int ret = platform::button_read_event(evt, kEventWaitMs);
    if (ret == -EAGAIN) {
      continue;
    }
    if (ret < 0) {
      ++error_streak;
      if (error_streak == 1U || (error_streak % 10U) == 0U) {
        log_.error("button read event failed", ret);
      }
      continue;
    }
    error_streak = 0U;

    bool long_press_triggered = false;
    int64_t hold_ms = 0;
    ButtonCallback cb = nullptr;
    void* cb_user = nullptr;

    /* 临界区:
     * 1) 刷新 latest_ 缓存.
     * 2) 维护按下状态机并统计短按/长按.
     * 3) 复制当前回调配置到局部变量.
     */
    k_mutex_lock(&mutex_, K_FOREVER);
    latest_ = evt;
    latest_valid_ = true;
    const uint8_t idx = static_cast<uint8_t>(evt.id);
    if (idx < 3U) {
      if (evt.pressed) {
        if (!key_down_[idx]) {
          key_down_[idx] = true;
          press_start_ms_[idx] = evt.ts_ms;
        }
      } else {
        if (key_down_[idx]) {
          hold_ms = evt.ts_ms - press_start_ms_[idx];
          key_down_[idx] = false;
          press_start_ms_[idx] = 0;
          if (hold_ms >= kLongPressThresholdMs) {
            ++long_press_count_[idx];
            long_press_triggered = true;
          } else {
            ++press_count_[idx];
          }
        }
      }
    }
    cb = callback_;
    cb_user = callback_user_;
    k_mutex_unlock(&mutex_);
    /* 锁外回调, 避免回调重入服务接口时发生锁竞争. */
    if (cb != nullptr) {
      cb(evt.id, evt.pressed, long_press_triggered, evt.ts_ms, hold_ms, cb_user);
    }
  }

  atomic_set(&running_, 0);
  thread_id_ = nullptr;
  log_.info("button service stopped");
}

/**
 * @brief 请求停止按键服务线程.
 * @note 仅设置停止标志并唤醒线程, 不阻塞等待线程退出.
 */
void ButtonService::stop() noexcept {
  if (atomic_get(&running_) == 0) {
    return;
  }
  atomic_set(&stop_requested_, 1);
  if (thread_id_ != nullptr) {
    k_wakeup(thread_id_);
  }
}

/**
 * @brief 启动按键服务线程.
 * @return 0 表示成功或已运行, 负值表示失败.
 * @note
 * 1) 完成平台按键初始化.
 * 2) 重置内部缓存和计数状态.
 * 3) 默认注册 default_callback 以输出按键日志.
 */
int ButtonService::run() noexcept {
  if (!atomic_cas(&running_, 0, 1)) {
    log_.info("button service already running");
    return 0;
  }

  int ret = platform::button_init();
  if (ret < 0) {
    atomic_set(&running_, 0);
    log_.error("failed to init button platform", ret);
    return ret;
  }

  k_mutex_init(&mutex_);
  /* 线程启动前清空运行期状态, 确保每次 run() 行为一致. */
  k_mutex_lock(&mutex_, K_FOREVER);
  latest_ = {};
  latest_valid_ = false;
  press_count_[0] = 0U;
  press_count_[1] = 0U;
  press_count_[2] = 0U;
  long_press_count_[0] = 0U;
  long_press_count_[1] = 0U;
  long_press_count_[2] = 0U;
  key_down_[0] = false;
  key_down_[1] = false;
  key_down_[2] = false;
  press_start_ms_[0] = 0;
  press_start_ms_[1] = 0;
  press_start_ms_[2] = 0;
  callback_ = default_callback;
  callback_user_ = this;
  k_mutex_unlock(&mutex_);

  atomic_set(&stop_requested_, 0);
  thread_id_ = k_thread_create(&thread_, stack_, K_THREAD_STACK_SIZEOF(stack_), threadEntry, this,
                               nullptr, nullptr, kPriority, 0, K_NO_WAIT);
  if (thread_id_ == nullptr) {
    atomic_set(&running_, 0);
    log_.error("failed to create button service thread", -1);
    return -1;
  }

  k_thread_name_set(thread_id_, "button_service");
  return 0;
}

/**
 * @brief 获取最近一次按键事件.
 * @param out 输出事件.
 * @return 0 表示成功, -EAGAIN 表示暂无有效事件.
 */
int ButtonService::get_latest(platform::ButtonEvent& out) noexcept {
  k_mutex_lock(&mutex_, K_FOREVER);
  if (!latest_valid_) {
    k_mutex_unlock(&mutex_);
    return -EAGAIN;
  }
  out = latest_;
  k_mutex_unlock(&mutex_);
  return 0;
}

/**
 * @brief 获取指定按键短按计数.
 * @param id 按键标识.
 * @param out 输出计数.
 * @return 0 表示成功, -EINVAL 表示按键标识非法.
 */
int ButtonService::get_press_count(const platform::ButtonId id, uint32_t& out) noexcept {
  const uint8_t idx = static_cast<uint8_t>(id);
  if (idx >= 3U) {
    return -EINVAL;
  }

  k_mutex_lock(&mutex_, K_FOREVER);
  out = press_count_[idx];
  k_mutex_unlock(&mutex_);
  return 0;
}

/**
 * @brief 获取指定按键长按计数.
 * @param id 按键标识.
 * @param out 输出计数.
 * @return 0 表示成功, -EINVAL 表示按键标识非法.
 */
int ButtonService::get_long_press_count(const platform::ButtonId id, uint32_t& out) noexcept {
  const uint8_t idx = static_cast<uint8_t>(id);
  if (idx >= 3U) {
    return -EINVAL;
  }

  k_mutex_lock(&mutex_, K_FOREVER);
  out = long_press_count_[idx];
  k_mutex_unlock(&mutex_);
  return 0;
}

/**
 * @brief 设置按键事件回调.
 * @param cb 回调函数指针, 传 nullptr 表示取消回调.
 * @param user 用户私有指针.
 * @return 0 表示成功.
 */
int ButtonService::set_callback(ButtonCallback cb, void* user) noexcept {
  k_mutex_lock(&mutex_, K_FOREVER);
  callback_ = cb;
  callback_user_ = user;
  k_mutex_unlock(&mutex_);
  return 0;
}

}  // namespace servers
