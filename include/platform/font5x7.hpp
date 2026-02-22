/**
 * @file font5x7.hpp
 * @brief 5x7 ASCII 字库接口定义。
 */

#pragma once

#include <cstdint>

namespace platform::font5x7 {

/** @brief 字体宽度（像素）。 */
constexpr uint8_t kWidth = 5U;
/** @brief 字体高度（像素）。 */
constexpr uint8_t kHeight = 7U;
/** @brief 字符间隔（像素，缩放前）。 */
constexpr uint8_t kSpacing = 1U;

/**
 * @brief 根据 ASCII 字符获取 5x7 字模（按列存储，LSB 在上）。
 * @param c 输入字符。
 * @return 指向 5 字节字模数据的指针。
 */
const uint8_t *glyph(char c) noexcept;

} // namespace platform::font5x7

