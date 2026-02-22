/**
 * @file app_Init.hpp
 * @brief 应用初始化接口声明。
 */

#pragma once

namespace app {

/**
 * @brief 初始化应用并启动核心服务。
 * @return 0 表示成功；负值表示失败。
 */
int app_Init() noexcept;

} // namespace app
