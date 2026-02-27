/**
 * @file platform_button.hpp
 * @brief 按键平台接口: 基于 gpio-keys + input 子系统.
 */

#pragma once

#include <cstdint>

namespace platform {

/**
 * @brief 按键标识.
 */
enum class ButtonId : uint8_t {
  kKey1 = 0,
  kKey2 = 1,
  kKey3 = 2,
  kUnknown = 0xFF,
};

/**
 * @brief 单次按键事件.
 */
struct ButtonEvent {
  /** @brief 按键标识. */
  ButtonId id = ButtonId::kUnknown;
  /** @brief true 表示按下, false 表示释放. */
  bool pressed = false;
  /** @brief Zephyr input 事件码. */
  uint32_t code = 0U;
  /** @brief 事件时间戳, 单位毫秒. */
  int64_t ts_ms = 0;
};

/**
 * @brief 当前按键状态快照.
 */
struct ButtonState {
  /** @brief KEY1 当前电平状态. */
  bool key1_pressed = false;
  /** @brief KEY2 当前电平状态. */
  bool key2_pressed = false;
  /** @brief KEY3 当前电平状态. */
  bool key3_pressed = false;
  /** @brief 事件队列满导致的丢包计数. */
  uint32_t dropped_events = 0U;
};

/**
 * @brief 初始化按键平台模块.
 * @return 0 表示成功. 负值表示失败.
 */
int button_init() noexcept;

/**
 * @brief 读取一条按键事件.
 * @param out 输出事件.
 * @param timeout_ms 超时毫秒. 小于 0 表示永久等待.
 * @return 0 表示成功. -EAGAIN 表示超时无事件.
 */
int button_read_event(ButtonEvent& out, int32_t timeout_ms) noexcept;

/**
 * @brief 获取当前按键状态快照.
 * @param out 输出状态.
 * @return 0 表示成功. 负值表示失败.
 */
int button_get_state(ButtonState& out) noexcept;

}  // namespace platform
