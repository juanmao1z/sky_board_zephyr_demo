/**
 * @file zephyr_encoder.cpp
 * @brief Zephyr QDEC 平台适配实现: 面向 EC11 编码器.
 */

#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>

#include "platform/platform_encoder.hpp"

namespace {

#define QDEC_NODE DT_ALIAS(qdec0)

#if DT_NODE_HAS_STATUS(QDEC_NODE, okay)
/** @brief 从 devicetree 别名 `qdec0` 获取的 QDEC 设备实例. */
const struct device* g_qdec_dev = DEVICE_DT_GET(QDEC_NODE);
#else
/** @brief 当 qdec0 缺失或禁用时, 设备指针为 nullptr. */
const struct device* g_qdec_dev = nullptr;
#endif

/** @brief 设备就绪标记, 用于幂等初始化. */
bool g_ready = false;

}  // namespace

namespace platform {

/**
 * @brief 初始化编码器设备.
 * @return 0 表示成功. 负值表示失败.
 */
int encoder_init() noexcept {
  if (g_ready) {
    return 0;
  }
  if (g_qdec_dev == nullptr || !device_is_ready(g_qdec_dev)) {
    return -ENODEV;
  }
  g_ready = true;
  return 0;
}

/**
 * @brief 读取一次编码器样本.
 * @param out 输出样本.
 * @return 0 表示成功. 负值表示失败.
 */
int encoder_read_once(EncoderSample& out) noexcept {
  int ret = encoder_init();
  if (ret < 0) {
    return ret;
  }

  ret = sensor_sample_fetch(g_qdec_dev);
  if (ret < 0) {
    return ret;
  }

  struct sensor_value val = {};
  ret = sensor_channel_get(g_qdec_dev, SENSOR_CHAN_ROTATION, &val);
  if (ret < 0) {
    return ret;
  }

  out.position_deg = val.val1;
  out.ts_ms = k_uptime_get();
  return 0;
}

}  // namespace platform
