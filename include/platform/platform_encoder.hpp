/**
 * @file platform_encoder.hpp
 * @brief 编码器平台接口: 封装 EC11 的 QDEC 单次采样访问.
 */

#pragma once

#include <cstdint>

namespace platform {

/**
 * @brief 单次编码器样本.
 */
struct EncoderSample {
  /** @brief 自驱动复位点起算的绝对角度, 单位度. */
  int32_t position_deg = 0;
  /** @brief 样本时间戳, 基于系统 uptime 毫秒. */
  int64_t ts_ms = 0;
};

/**
 * @brief 初始化编码器设备(幂等).
 * @return 0 表示成功. 负值表示失败.
 */
int encoder_init() noexcept;

/**
 * @brief 读取一次编码器样本.
 * @param out 输出样本.
 * @return 0 表示成功. 负值表示失败.
 */
int encoder_read_once(EncoderSample& out) noexcept;

}  // namespace platform
