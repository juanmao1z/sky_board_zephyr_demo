/**
 * @file platform_pca9555.hpp
 * @brief PCA9555 I/O 扩展器平台抽象接口。
 */

#pragma once

#include <cstdint>

namespace platform {

/**
 * @brief PCA9555 平台抽象接口。
 * @note 固定语义:
 * - io0_0 ~ io0_7: DIP 开关输入
 * - io1_0 ~ io1_7: 白色 LED 输出
 */
class IPca9555 {
 public:
  virtual ~IPca9555() = default;

  /**
   * @brief 初始化 PCA9555 并配置固定端口方向。
   * @return 0 表示成功；负值表示失败。
   */
  virtual int init() noexcept = 0;

  /**
   * @brief 读取 DIP 输入位图（Port0）。
   * @param[out] out_mask bit0..bit7 对应 io0_0..io0_7。
   * @return 0 表示成功；负值表示失败。
   */
  virtual int read_dipsw(uint8_t& out_mask) noexcept = 0;

  /**
   * @brief 设置白色 LED 输出位图（Port1）。
   * @param led_mask bit0..bit7 对应 io1_0..io1_7。
   * @return 0 表示成功；负值表示失败。
   */
  virtual int set_leds(uint8_t led_mask) noexcept = 0;
};

/**
 * @brief 获取全局 PCA9555 平台实例。
 * @return IPca9555 引用，生命周期贯穿整个程序运行期。
 */
IPca9555& pca9555() noexcept;

}  // namespace platform
