/**
 * @file platform_display.hpp
 * @brief 平台显示实例访问接口。
 */

#pragma once

#include "platform/idisplay.hpp"

namespace platform {

/**
 * @brief 获取全局显示实例。
 * @return IDisplay 引用，生命周期贯穿整个程序运行期。
 */
IDisplay &display();

} // namespace platform

