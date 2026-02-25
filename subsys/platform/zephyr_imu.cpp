/**
 * @file zephyr_imu.cpp
 * @brief ICM42688 平台化实现：设备初始化与单次采样读取。
 */

#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>

#include "platform/platform_imu.hpp"

namespace {

#define ICM42688_NODE DT_NODELABEL(icm42688_sensor)

#if DT_NODE_HAS_STATUS(ICM42688_NODE, okay)
/** @brief ICM42688 设备实例。 */
const struct device* g_imu_dev = DEVICE_DT_GET(ICM42688_NODE);
#else
/** @brief 当节点未启用时设备为空。 */
const struct device* g_imu_dev = nullptr;
#endif

/** @brief 设备就绪标记。 */
bool g_ready = false;

/** @brief 把 `sensor_value` 转为 milli 单位。 */
int32_t to_milli(const struct sensor_value& v) {
  return static_cast<int32_t>((static_cast<int64_t>(v.val1) * 1000LL) + (v.val2 / 1000));
}

/** @brief 把角速度 `sensor_value`（rad/s）转为 mdps。 */
int32_t rad_per_sec_to_mdps(const struct sensor_value& v) {
  constexpr int64_t kPiScaled = 3141592LL;
  constexpr int64_t kDegMilliPerRad = 180000LL;
  const int64_t micro_rad = (static_cast<int64_t>(v.val1) * 1000000LL) + v.val2;
  const int64_t numerator = micro_rad * kDegMilliPerRad;
  if (numerator >= 0) {
    return static_cast<int32_t>((numerator + (kPiScaled / 2)) / kPiScaled);
  }
  return static_cast<int32_t>((numerator - (kPiScaled / 2)) / kPiScaled);
}

}  // namespace

namespace platform {

int imu_init() noexcept {
  if (g_ready) {
    return 0;
  }
  if (g_imu_dev == nullptr || !device_is_ready(g_imu_dev)) {
    return -ENODEV;
  }
  g_ready = true;
  return 0;
}

int imu_read_once(ImuSample& out) noexcept {
  int ret = imu_init();
  if (ret < 0) {
    return ret;
  }

  ret = sensor_sample_fetch(g_imu_dev);
  if (ret < 0) {
    return ret;
  }

  struct sensor_value accel[3] = {};
  struct sensor_value gyro[3] = {};
  struct sensor_value temp = {};

  ret = sensor_channel_get(g_imu_dev, SENSOR_CHAN_ACCEL_XYZ, accel);
  if (ret < 0) {
    return ret;
  }
  ret = sensor_channel_get(g_imu_dev, SENSOR_CHAN_GYRO_XYZ, gyro);
  if (ret < 0) {
    return ret;
  }
  ret = sensor_channel_get(g_imu_dev, SENSOR_CHAN_DIE_TEMP, &temp);
  if (ret < 0) {
    return ret;
  }

  out.accel_x_mg = sensor_ms2_to_mg(&accel[0]);
  out.accel_y_mg = sensor_ms2_to_mg(&accel[1]);
  out.accel_z_mg = sensor_ms2_to_mg(&accel[2]);
  out.gyro_x_mdps = rad_per_sec_to_mdps(gyro[0]);
  out.gyro_y_mdps = rad_per_sec_to_mdps(gyro[1]);
  out.gyro_z_mdps = rad_per_sec_to_mdps(gyro[2]);
  out.temp_mc = to_milli(temp);
  out.ts_ms = k_uptime_get();
  return 0;
}

}  // namespace platform
