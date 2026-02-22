/**
 * @file ibacklight.hpp
 * @brief 背光抽象接口定义。
 */

#pragma once

#include <cstdint>

namespace platform {

/**
 * @brief 背光控制接口。
 */
class IBacklight {
public:
	virtual ~IBacklight() = default;

	/**
	 * @brief 设置背光开关状态。
	 * @param on true 打开；false 关闭。
	 * @return 0 表示成功；负值表示失败。
	 */
	virtual int set_enabled(bool on) noexcept = 0;

	/**
	 * @brief 设置背光亮度百分比。
	 * @param percent 亮度百分比，范围 0~100；超过范围按边界处理。
	 * @return 0 表示成功；负值表示失败。
	 */
	virtual int set_brightness(uint8_t percent) noexcept = 0;
};

} // namespace platform

