/**
 * @file platform_imu.hpp
 * @brief 平台 IMU 访问接口：封装 ICM42688 单次读取。
 */

#pragma once

#include <cstdint>

namespace platform {

/**
 * @brief ICM42688 IMU 样本。
 */
struct ImuSample {
  /** @brief 加速度 X 轴，单位 mg。 */
  int32_t accel_x_mg = 0;
  /** @brief 加速度 Y 轴，单位 mg。 */
  int32_t accel_y_mg = 0;
  /** @brief 加速度 Z 轴，单位 mg。 */
  int32_t accel_z_mg = 0;
  /** @brief 角速度 X 轴，单位 mdps。 */
  int32_t gyro_x_mdps = 0;
  /** @brief 角速度 Y 轴，单位 mdps。 */
  int32_t gyro_y_mdps = 0;
  /** @brief 角速度 Z 轴，单位 mdps。 */
  int32_t gyro_z_mdps = 0;
  /** @brief 温度，单位 milli-Celsius。 */
  int32_t temp_mc = 0;
  /** @brief 采样时间戳（系统启动毫秒）。 */
  int64_t ts_ms = 0;
};

/**
 * @brief 初始化 ICM42688 设备（幂等）。
 * @return 0 表示成功；负值表示失败。
 */
int imu_init() noexcept;

/**
 * @brief 执行一次 ICM42688 采样并输出工程内部单位。
 * @param out 输出样本。
 * @return 0 表示成功；负值表示失败。
 */
int imu_read_once(ImuSample& out) noexcept;

}  // namespace platform
