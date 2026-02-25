/**
 * @file zephyr_encoder.cpp
 * @brief Zephyr QDEC platform adapter for EC11.
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
/** @brief QDEC sensor device instance from devicetree alias `qdec0`. */
const struct device* g_qdec_dev = DEVICE_DT_GET(QDEC_NODE);
#else
/** @brief Null device when qdec0 alias is missing or disabled. */
const struct device* g_qdec_dev = nullptr;
#endif

/** @brief Device ready flag for idempotent initialization. */
bool g_ready = false;

}  // namespace

namespace platform {

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
