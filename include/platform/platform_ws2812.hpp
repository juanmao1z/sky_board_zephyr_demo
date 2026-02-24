/**
 * @file platform_ws2812.hpp
 * @brief WS2812 平台抽象接口.
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace platform {

/**
 * @brief RGB 颜色结构.
 */
struct Ws2812Rgb {
  uint8_t r = 0U;
  uint8_t g = 0U;
  uint8_t b = 0U;
};

/**
 * @brief WS2812 驱动抽象类.
 */
class IWs2812 {
 public:
  virtual ~IWs2812() = default;

  /**
   * @brief 初始化驱动资源.
   * @return 0 表示成功, 负值表示失败.
   */
  virtual int init() noexcept = 0;

  /**
   * @brief 获取灯珠数量.
   * @return 灯带像素个数.
   */
  virtual size_t size() const noexcept = 0;

  /**
   * @brief 设置本地缓冲区中的单个像素.
   * @param index 像素索引.
   * @param color RGB 颜色.
   * @return 0 表示成功, 负值表示失败.
   */
  virtual int set_pixel(size_t index, const Ws2812Rgb& color) noexcept = 0;

  /**
   * @brief 填充本地缓冲区所有像素.
   * @param color RGB 颜色.
   * @return 0 表示成功, 负值表示失败.
   */
  virtual int fill(const Ws2812Rgb& color) noexcept = 0;

  /**
   * @brief 将本地缓冲区下发到灯带.
   * @return 0 表示成功, 负值表示失败.
   */
  virtual int show() noexcept = 0;

  /**
   * @brief 清屏并立即下发.
   * @return 0 表示成功, 负值表示失败.
   */
  virtual int clear_and_show() noexcept = 0;

  /**
   * @brief 设置全局亮度缩放系数.
   * @param level 亮度值, 范围 0..255.
   * @return 0 表示成功, 负值表示失败.
   */
  virtual int set_global_brightness(uint8_t level) noexcept = 0;
};

/**
 * @brief 获取全局 WS2812 驱动实例.
 * @return 驱动实例引用.
 */
IWs2812& ws2812() noexcept;

/**
 * @brief 颜色轮函数, 根据相位生成 RGB 颜色.
 * @param pos 相位值, 范围 0..255.
 * @return 对应 RGB 颜色.
 */
Ws2812Rgb ws2812_wheel(uint8_t pos) noexcept;

/**
 * @brief 渲染一帧彩虹流水灯效果.
 * @param ws WS2812 驱动实例.
 * @param phase 当前全局相位.
 * @return 0 表示成功, 负值表示失败.
 */
int ws2812_wheel_show(IWs2812& ws, uint8_t phase) noexcept;

}  // namespace platform
