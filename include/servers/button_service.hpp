/**
 * @file button_service.hpp
 * @brief 按键后台服务声明.
 */

#pragma once

#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>

#include <cstdint>

#include "platform/ilogger.hpp"
#include "platform/platform_button.hpp"

namespace servers {

/**
 * @brief 按键后台服务.
 * @note 基于 input 事件流统计短按/长按次数, 并输出日志.
 */
class ButtonService {
 public:
  /**
   * @brief 按键事件回调函数类型.
   * @param id 按键标识.
   * @param pressed true 表示按下事件, false 表示释放事件.
   * @param long_press true 表示该次释放被判定为长按.
   * @param ts_ms 事件时间戳, 单位毫秒.
   * @param hold_ms 按住时长, 仅释放事件有效.
   * @param user 用户私有指针.
   */
  using ButtonCallback = void (*)(platform::ButtonId id, bool pressed, bool long_press,
                                  int64_t ts_ms, int64_t hold_ms, void* user);

  /**
   * @brief 构造按键服务.
   * @param log 日志接口引用, 生命周期需覆盖服务运行期.
   */
  explicit ButtonService(platform::ILogger& log) : log_(log) {}

  /**
   * @brief 启动服务线程. 幂等.
   * @return 0 表示成功或已在运行. 负值表示失败.
   */
  int run() noexcept;

  /**
   * @brief 请求停止服务线程.
   */
  void stop() noexcept;

  /**
   * @brief 获取最新按键事件.
   * @param out 输出事件.
   * @return 0 表示成功. -EAGAIN 表示暂无事件.
   */
  int get_latest(platform::ButtonEvent& out) noexcept;

  /**
   * @brief 获取指定按键短按计数.
   * @param id 按键标识.
   * @param out 输出计数值.
   * @return 0 表示成功. 负值表示失败.
   */
  int get_press_count(platform::ButtonId id, uint32_t& out) noexcept;

  /**
   * @brief 获取指定按键长按计数.
   * @param id 按键标识.
   * @param out 输出计数值.
   * @return 0 表示成功. 负值表示失败.
   */
  int get_long_press_count(platform::ButtonId id, uint32_t& out) noexcept;

  /**
   * @brief 注册按键事件回调.
   * @param cb 回调函数指针, 传 nullptr 可取消注册.
   * @param user 用户私有指针.
   * @return 0 表示成功.
   */
  int set_callback(ButtonCallback cb, void* user) noexcept;

 private:
  /** @brief 服务线程栈大小. */
  static constexpr size_t kStackSize = 1024;
  /** @brief 服务线程优先级. */
  static constexpr int kPriority = K_LOWEST_APPLICATION_THREAD_PRIO;
  /** @brief 等待按键事件超时周期. */
  static constexpr int32_t kEventWaitMs = 1000;
  /** @brief 长按判定阈值, 单位毫秒. */
  static constexpr int64_t kLongPressThresholdMs = 800;

  /**
   * @brief 线程入口静态适配.
   */
  static void threadEntry(void* p1, void* p2, void* p3);

  /**
   * @brief 线程主循环.
   */
  void threads() noexcept;

  /**
   * @brief 默认按键回调: 输出按键日志.
   * @param id 按键标识.
   * @param pressed true 表示按下事件.
   * @param long_press true 表示当前释放事件被判定为长按.
   * @param ts_ms 事件时间戳.
   * @param hold_ms 按住时长.
   * @param user 用户指针, 约定为 ButtonService*.
   */
  static void default_callback(platform::ButtonId id, bool pressed, bool long_press, int64_t ts_ms,
                               int64_t hold_ms, void* user);

  /**
   * @brief KEY1 短按处理函数.
   * @param ts_ms 事件时间戳.
   * @param hold_ms 按住时长.
   * @param user 用户指针, 约定为 ButtonService*.
   */
  static void key1_short(int64_t ts_ms, int64_t hold_ms, void* user);

  /**
   * @brief KEY1 长按处理函数.
   * @param ts_ms 事件时间戳.
   * @param hold_ms 按住时长.
   * @param user 用户指针, 约定为 ButtonService*.
   */
  static void key1_long(int64_t ts_ms, int64_t hold_ms, void* user);

  /**
   * @brief KEY2 短按处理函数.
   * @param ts_ms 事件时间戳.
   * @param hold_ms 按住时长.
   * @param user 用户指针, 约定为 ButtonService*.
   */
  static void key2_short(int64_t ts_ms, int64_t hold_ms, void* user);

  /**
   * @brief KEY2 长按处理函数.
   * @param ts_ms 事件时间戳.
   * @param hold_ms 按住时长.
   * @param user 用户指针, 约定为 ButtonService*.
   */
  static void key2_long(int64_t ts_ms, int64_t hold_ms, void* user);

  /**
   * @brief KEY3 短按处理函数.
   * @param ts_ms 事件时间戳.
   * @param hold_ms 按住时长.
   * @param user 用户指针, 约定为 ButtonService*.
   */
  static void key3_short(int64_t ts_ms, int64_t hold_ms, void* user);

  /**
   * @brief KEY3 长按处理函数.
   * @param ts_ms 事件时间戳.
   * @param hold_ms 按住时长.
   * @param user 用户指针, 约定为 ButtonService*.
   */
  static void key3_long(int64_t ts_ms, int64_t hold_ms, void* user);

  /** @brief 日志接口. */
  platform::ILogger& log_;
  /** @brief Zephyr 线程控制块. */
  struct k_thread thread_;
  /** @brief Zephyr 线程栈. */
  K_KERNEL_STACK_MEMBER(stack_, kStackSize);
  /** @brief 线程 ID, 未运行时为 nullptr. */
  k_tid_t thread_id_ = nullptr;
  /** @brief 运行标志. */
  atomic_t running_ = ATOMIC_INIT(0);
  /** @brief 停止请求标志. */
  atomic_t stop_requested_ = ATOMIC_INIT(0);
  /** @brief 保护最新事件和计数. */
  struct k_mutex mutex_{};
  /** @brief 最新按键事件. */
  platform::ButtonEvent latest_ = {};
  /** @brief 最新事件有效标志. */
  bool latest_valid_ = false;
  /** @brief KEY1/KEY2/KEY3 短按累计次数. */
  uint32_t press_count_[3] = {0U, 0U, 0U};
  /** @brief KEY1/KEY2/KEY3 长按累计次数. */
  uint32_t long_press_count_[3] = {0U, 0U, 0U};
  /** @brief KEY1/KEY2/KEY3 是否处于按下态. */
  bool key_down_[3] = {false, false, false};
  /** @brief KEY1/KEY2/KEY3 按下起始时间戳, 单位毫秒. */
  int64_t press_start_ms_[3] = {0, 0, 0};
  /** @brief 按键事件回调函数. */
  ButtonCallback callback_ = nullptr;
  /** @brief 回调用户私有指针. */
  void* callback_user_ = nullptr;
};

}  // namespace servers
