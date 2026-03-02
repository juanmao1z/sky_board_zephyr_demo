/**
 * @file platform_buzzer.hpp
 * @brief 蜂鸣器平台抽象接口。
 */

#pragma once

#include <cstdint>

namespace platform {

/**
 * @brief 蜂鸣器驱动抽象接口。
 */
class IBuzzer {
 public:
  virtual ~IBuzzer() = default;

  /**
   * @brief 初始化蜂鸣器驱动资源。
   * @return 0 表示成功；负值表示失败。
   */
  virtual int init() noexcept = 0;

  /**
   * @brief 开启蜂鸣器输出。
   * @param freq_hz 目标频率（Hz）。
   * @param duty_percent 占空比百分比（0..100）。
   * @return 0 表示成功；负值表示失败。
   */
  virtual int on(uint32_t freq_hz, uint8_t duty_percent) noexcept = 0;

  /**
   * @brief 关闭蜂鸣器输出。
   * @return 0 表示成功；负值表示失败。
   */
  virtual int off() noexcept = 0;
};

/**
 * @brief 获取全局蜂鸣器驱动实例。
 * @return IBuzzer 引用，生命周期贯穿整个程序运行期。
 */
IBuzzer& buzzer() noexcept;

}  // namespace platform
