/**
 * @file zephyr_button.cpp
 * @brief 按键平台适配: 对接 Zephyr gpio-keys + input 事件流.
 */

#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>

#include "platform/platform_button.hpp"

namespace {

/** @brief 按键事件缓冲深度. */
constexpr size_t kEventQueueDepth = 32U;

/** @brief 按键事件消息队列. */
K_MSGQ_DEFINE(g_button_event_msgq, sizeof(platform::ButtonEvent), kEventQueueDepth, 4);

/** @brief 初始化标志. */
atomic_t g_initialized = ATOMIC_INIT(0);
/** @brief 实时按键状态位图. bit0/1/2 分别对应 KEY1/KEY2/KEY3. */
atomic_t g_state_bits = ATOMIC_INIT(0);
/** @brief 事件队列丢包计数. */
atomic_t g_drop_count = ATOMIC_INIT(0);

/**
 * @brief 将 input 事件码映射为业务按键标识.
 * @param code Zephyr input 键值编码.
 * @return 对应业务按键标识. 未识别返回 kUnknown.
 */
platform::ButtonId map_code_to_button_id(const uint16_t code) noexcept {
  switch (code) {
    case INPUT_KEY_0:
      return platform::ButtonId::kKey1;
    case INPUT_KEY_1:
      return platform::ButtonId::kKey2;
    case INPUT_KEY_2:
      return platform::ButtonId::kKey3;
    default:
      return platform::ButtonId::kUnknown;
  }
}

/**
 * @brief input 子系统回调, 负责过滤并投递按键事件.
 * @param evt input 原始事件指针.
 * @param user 用户参数, 当前实现未使用.
 * @note 仅处理 INPUT_EV_KEY 且 sync=1 的按键事件.
 */
void on_button_input_event(struct input_event* evt, void*) {
  if (evt == nullptr || evt->sync == 0 || evt->type != INPUT_EV_KEY) {
    return;
  }

  const platform::ButtonId id = map_code_to_button_id(evt->code);
  if (id == platform::ButtonId::kUnknown) {
    return;
  }

  const uint8_t idx = static_cast<uint8_t>(id);
  const bool pressed = (evt->value != 0);
  if (pressed) {
    (void)atomic_or(&g_state_bits, BIT(idx));
  } else {
    (void)atomic_and(&g_state_bits, ~BIT(idx));
  }

  platform::ButtonEvent out = {};
  out.id = id;
  out.pressed = pressed;
  out.code = evt->code;
  out.ts_ms = k_uptime_get();

  if (k_msgq_put(&g_button_event_msgq, &out, K_NO_WAIT) != 0) {
    (void)atomic_inc(&g_drop_count);
  }
}

#define GPIO_KEYS_NODE DT_PATH(gpio_keys)

#if DT_NODE_EXISTS(GPIO_KEYS_NODE)
INPUT_CALLBACK_DEFINE(DEVICE_DT_GET(GPIO_KEYS_NODE), on_button_input_event, NULL);
#else
INPUT_CALLBACK_DEFINE(NULL, on_button_input_event, NULL);
#endif

}  // namespace

namespace platform {

/**
 * @brief 初始化按键平台模块.
 * @return 0 表示初始化成功. -ENODEV 表示未启用 gpio-keys 节点.
 */
int button_init() noexcept {
#if DT_HAS_COMPAT_STATUS_OKAY(gpio_keys)
  atomic_set(&g_initialized, 1);
  return 0;
#else
  return -ENODEV;
#endif
}

/**
 * @brief 从按键事件队列读取一条事件.
 * @param[out] out 输出事件.
 * @param timeout_ms 超时毫秒. 小于 0 表示永久等待.
 * @return 0 表示读取成功. 其他负值表示错误或超时.
 */
int button_read_event(ButtonEvent& out, const int32_t timeout_ms) noexcept {
  if (atomic_get(&g_initialized) == 0) {
    const int ret = button_init();
    if (ret < 0) {
      return ret;
    }
  }

  const k_timeout_t timeout = (timeout_ms < 0) ? K_FOREVER : K_MSEC(timeout_ms);
  return k_msgq_get(&g_button_event_msgq, &out, timeout);
}

/**
 * @brief 获取按键状态快照.
 * @param[out] out 输出状态结构.
 * @return 0 表示成功. 其他负值表示失败.
 */
int button_get_state(ButtonState& out) noexcept {
  if (atomic_get(&g_initialized) == 0) {
    const int ret = button_init();
    if (ret < 0) {
      return ret;
    }
  }

  const atomic_val_t bits = atomic_get(&g_state_bits);
  out.key1_pressed = ((bits & BIT(0)) != 0);
  out.key2_pressed = ((bits & BIT(1)) != 0);
  out.key3_pressed = ((bits & BIT(2)) != 0);
  out.dropped_events = static_cast<uint32_t>(atomic_get(&g_drop_count));
  return 0;
}

}  // namespace platform
