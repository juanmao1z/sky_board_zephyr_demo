/**
 * @file idisplay.hpp
 * @brief 显示设备抽象接口定义。
 */

#pragma once

#include <cstdint>

namespace platform {

/**
 * @brief 显示设备接口。
 */
class IDisplay {
public:
	virtual ~IDisplay() = default;

	/**
	 * @brief 初始化显示设备。
	 * @return 0 表示成功；负值表示初始化失败。
	 */
	virtual int init() noexcept = 0;

	/**
	 * @brief 获取屏幕宽度（像素）。
	 * @return 屏幕宽度；未初始化时返回 0。
	 */
	virtual uint16_t width() const noexcept = 0;

	/**
	 * @brief 获取屏幕高度（像素）。
	 * @return 屏幕高度；未初始化时返回 0。
	 */
	virtual uint16_t height() const noexcept = 0;

	/**
	 * @brief 清屏（填充全屏单色）。
	 * @param color_rgb565 RGB565 颜色值。
	 * @return 0 表示成功；负值表示失败。
	 */
	virtual int clear(uint16_t color_rgb565) noexcept = 0;

	/**
	 * @brief 填充矩形区域（单色）。
	 * @param x 左上角 X 坐标。
	 * @param y 左上角 Y 坐标。
	 * @param w 矩形宽度。
	 * @param h 矩形高度。
	 * @param color_rgb565 RGB565 颜色值。
	 * @return 0 表示成功；负值表示失败。
	 */
	virtual int fill_rect(uint16_t x,
			      uint16_t y,
			      uint16_t w,
			      uint16_t h,
			      uint16_t color_rgb565) noexcept = 0;

	/**
	 * @brief 绘制单个 5x7 字符（可缩放）。
	 * @param x 左上角 X 坐标。
	 * @param y 左上角 Y 坐标。
	 * @param c 待绘制字符（支持 ASCII 0x20~0x7E，其他字符按 '?' 处理）。
	 * @param fg_rgb565 前景色（字形颜色）。
	 * @param bg_rgb565 背景色。
	 * @param scale 缩放倍数，0 按 1 处理。
	 * @return 0 表示成功；负值表示失败。
	 */
	virtual int draw_char(uint16_t x,
			      uint16_t y,
			      char c,
			      uint16_t fg_rgb565,
			      uint16_t bg_rgb565,
			      uint8_t scale) noexcept = 0;

	/**
	 * @brief 绘制字符串（5x7 字体，可缩放，支持 '\n' 换行）。
	 * @param x 起始 X 坐标。
	 * @param y 起始 Y 坐标。
	 * @param text 字符串指针。
	 * @param fg_rgb565 前景色（字形颜色）。
	 * @param bg_rgb565 背景色。
	 * @param scale 缩放倍数，0 按 1 处理。
	 * @return 0 表示成功；负值表示失败。
	 */
	virtual int draw_text(uint16_t x,
			      uint16_t y,
			      const char *text,
			      uint16_t fg_rgb565,
			      uint16_t bg_rgb565,
			      uint8_t scale) noexcept = 0;

	/**
	 * @brief 绘制有符号整数。
	 * @param x 起始 X 坐标。
	 * @param y 起始 Y 坐标。
	 * @param value 待显示整数值。
	 * @param fg_rgb565 前景色（字形颜色）。
	 * @param bg_rgb565 背景色。
	 * @param scale 缩放倍数，0 按 1 处理。
	 * @return 0 表示成功；负值表示失败。
	 */
	virtual int draw_int(uint16_t x,
			     uint16_t y,
			     int32_t value,
			     uint16_t fg_rgb565,
			     uint16_t bg_rgb565,
			     uint8_t scale) noexcept = 0;

	/**
	 * @brief 绘制启动测试画面。
	 * @return 0 表示成功；负值表示绘制失败。
	 */
	virtual int show_boot_screen() noexcept = 0;
};

} // namespace platform
