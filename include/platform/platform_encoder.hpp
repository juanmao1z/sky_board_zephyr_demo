/**
 * @file platform_encoder.hpp
 * @brief Platform encoder access interface for EC11 (QDEC).
 */

#pragma once

#include <cstdint>

namespace platform {

/**
 * @brief One encoder sample.
 */
struct EncoderSample {
  /** @brief Absolute position in degrees from driver reset point. */
  int32_t position_deg = 0;
  /** @brief Sample timestamp from system uptime in milliseconds. */
  int64_t ts_ms = 0;
};

/**
 * @brief Initialize encoder device (idempotent).
 * @return 0 on success; negative errno on failure.
 */
int encoder_init() noexcept;

/**
 * @brief Fetch one encoder sample.
 * @param out Output sample.
 * @return 0 on success; negative errno on failure.
 */
int encoder_read_once(EncoderSample& out) noexcept;

}  // namespace platform

