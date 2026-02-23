/**
 * @file platform_ethernet.hpp
 * @brief 平台以太网启动接口。
 */

#pragma once

namespace platform {

/**
 * @brief 初始化以太网接口并启动 DHCPv4。
 * @return 0 表示成功；负值表示失败。
 */
int ethernet_init() noexcept;

}  // namespace platform
