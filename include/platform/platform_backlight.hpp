/**
 * @file platform_backlight.hpp
 * @brief 平台背光实例访问接口。
 */

#pragma once

#include "platform/ibacklight.hpp"

namespace platform {

/**
 * @brief 获取全局背光实例。
 * @return IBacklight 引用，生命周期贯穿整个程序运行期。
 */
IBacklight &backlight();

} // namespace platform

