/**
 * @file zephyr_display.cpp
 * @brief IDisplay 的 Zephyr Display API 后端实现。
 */

#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>

#include "platform/font5x7.hpp"
#include "platform/platform_backlight.hpp"
#include "platform/platform_display.hpp"

namespace {

/** @brief 静态行缓冲支持的最大屏幕宽度。 */
constexpr uint16_t kMaxDisplayWidth = 320U;
/**
 * @brief 把 8-bit RGB 颜色转换为 RGB565。
 * @param r 红色分量。
 * @param g 绿色分量。
 * @param b 蓝色分量。
 * @return RGB565 格式颜色值。
 */
constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return static_cast<uint16_t>(((r & 0xF8U) << 8) | ((g & 0xFCU) << 3) | (b >> 3));
}
/**
 * @brief Zephyr Display API 的平台实现。
 */
class ZephyrDisplay final : public platform::IDisplay {
 public:
  /**
   * @brief 初始化显示设备。
   * @return 0 成功；负值失败。
   */
  int init() noexcept override;

  /**
   * @brief 获取屏幕宽度（像素）。
   * @return 已初始化时返回宽度；否则返回 0。
   */
  uint16_t width() const noexcept override;

  /**
   * @brief 获取屏幕高度（像素）。
   * @return 已初始化时返回高度；否则返回 0。
   */
  uint16_t height() const noexcept override;

  /**
   * @brief 全屏填充单色。
   * @param color_rgb565 RGB565 颜色值。
   * @return 0 成功；负值失败。
   */
  int clear(uint16_t color_rgb565) noexcept override;

  /**
   * @brief 填充矩形区域（单色）。
   * @param x 左上角 X。
   * @param y 左上角 Y。
   * @param w 宽度。
   * @param h 高度。
   * @param color_rgb565 RGB565 颜色值。
   * @return 0 成功；负值失败。
   */
  int fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                uint16_t color_rgb565) noexcept override;

  /**
   * @brief 绘制单个 5x7 字符（可缩放）。
   * @param x 左上角 X。
   * @param y 左上角 Y。
   * @param c 待绘制字符。
   * @param fg_rgb565 前景色。
   * @param bg_rgb565 背景色。
   * @param scale 缩放倍数，0 按 1 处理。
   * @return 0 成功；负值失败。
   */
  int draw_char(uint16_t x, uint16_t y, char c, uint16_t fg_rgb565, uint16_t bg_rgb565,
                uint8_t scale) noexcept override;

  /**
   * @brief 绘制字符串（支持 '\n'）。
   * @param x 起始 X。
   * @param y 起始 Y。
   * @param text 字符串。
   * @param fg_rgb565 前景色。
   * @param bg_rgb565 背景色。
   * @param scale 缩放倍数，0 按 1 处理。
   * @return 0 成功；负值失败。
   */
  int draw_text(uint16_t x, uint16_t y, const char* text, uint16_t fg_rgb565, uint16_t bg_rgb565,
                uint8_t scale) noexcept override;

  /**
   * @brief 绘制十进制有符号整数。
   * @param x 起始 X。
   * @param y 起始 Y。
   * @param value 待绘制整数。
   * @param fg_rgb565 前景色。
   * @param bg_rgb565 背景色。
   * @param scale 缩放倍数，0 按 1 处理。
   * @return 0 成功；负值失败。
   */
  int draw_int(uint16_t x, uint16_t y, int32_t value, uint16_t fg_rgb565, uint16_t bg_rgb565,
               uint8_t scale) noexcept override;

  /**
   * @brief 绘制启动测试画面。
   * @return 0 成功；负值失败。
   */
  int show_boot_screen() noexcept override;

  /**
   * @brief 获取显示关联的背光控制接口。
   * @return IBacklight 引用。
   */
  platform::IBacklight& backlight() noexcept override;

 private:
  /**
   * @brief 执行矩形区域写入（假设参数已完成校验/裁剪）。
   * @param x 左上角 X。
   * @param y 左上角 Y。
   * @param w 宽度。
   * @param h 高度。
   * @param color_rgb565 RGB565 颜色值。
   * @return 0 成功；负值失败。
   */
  int write_solid_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                       uint16_t color_rgb565) noexcept;

  /** @brief 显示设备句柄。 */
  const struct device* display_dev_ = nullptr;
  /** @brief 显示能力缓存。 */
  struct display_capabilities caps_{};
  /** @brief 初始化状态标志。 */
  bool initialized_ = false;
  /** @brief 单行 RGB565 缓冲。 */
  uint16_t line_buf_[kMaxDisplayWidth]{};
};

/** @brief 全局显示实例。 */
ZephyrDisplay g_display;
/**
 * @brief 获取屏幕宽度。
 * @return 已初始化时返回宽度；否则返回 0。
 */
uint16_t ZephyrDisplay::width() const noexcept {
  if (!initialized_) {
    return 0U;
  }

  return caps_.x_resolution;
}

/**
 * @brief 获取屏幕高度。
 * @return 已初始化时返回高度；否则返回 0。
 */
uint16_t ZephyrDisplay::height() const noexcept {
  if (!initialized_) {
    return 0U;
  }

  return caps_.y_resolution;
}

/**
 * @brief 执行矩形区域单色写入。
 * @param x 左上角 X。
 * @param y 左上角 Y。
 * @param w 宽度。
 * @param h 高度。
 * @param color_rgb565 RGB565 颜色值。
 * @return 0 成功；负值失败。
 */
int ZephyrDisplay::write_solid_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                    uint16_t color_rgb565) noexcept {
  if (w == 0U || h == 0U) {
    return 0;
  }

  if (w > kMaxDisplayWidth) {
    return -ENOMEM;
  }

  for (uint16_t i = 0U; i < w; ++i) {
    line_buf_[i] = color_rgb565;
  }

  struct display_buffer_descriptor desc{};
  desc.width = w;
  desc.height = 1U;
  desc.pitch = w;
  desc.buf_size = static_cast<uint32_t>(w) * sizeof(line_buf_[0]);

  for (uint16_t row = 0U; row < h; ++row) {
    desc.frame_incomplete = (row + 1U) < h;
    int ret = display_write(display_dev_, x, y + row, &desc, line_buf_);
    if (ret < 0) {
      return ret;
    }
  }

  return 0;
}

/**
 * @brief 初始化显示设备。
 * @return 0 成功；负值失败。
 */
int ZephyrDisplay::init() noexcept {
  /* 幂等保护：已经初始化过则直接成功返回。 */
  if (initialized_) {
    return 0;
  }

  /* 从 devicetree 的 zephyr_display chosen 节点拿到显示设备。 */
  display_dev_ = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
  /* 设备未 ready 通常意味着驱动/时钟/引脚尚未完成初始化。 */
  if (!device_is_ready(display_dev_)) {
    return -ENODEV;
  }

  /* 读取分辨率、当前像素格式、支持像素格式等能力信息。 */
  display_get_capabilities(display_dev_, &caps_);

  /* 本实现固定使用 RGB565；如果硬件不支持则无法继续。 */
  if ((caps_.supported_pixel_formats & PIXEL_FORMAT_RGB_565) == 0U) {
    return -ENOTSUP;
  }

  /* 当前格式不是 RGB565 时，尝试切换并重新确认。 */
  if (caps_.current_pixel_format != PIXEL_FORMAT_RGB_565) {
    int ret = display_set_pixel_format(display_dev_, PIXEL_FORMAT_RGB_565);
    if (ret < 0) {
      return ret;
    }
    display_get_capabilities(display_dev_, &caps_);
    if (caps_.current_pixel_format != PIXEL_FORMAT_RGB_565) {
      return -ENOTSUP;
    }
  }

  /* 关闭 blanking，让面板真正开始显示像素内容。 */
  int ret = display_blanking_off(display_dev_);
  /* 某些驱动未实现该接口会返回 -ENOSYS，这里按可接受处理。 */
  if (ret < 0 && ret != -ENOSYS) {
    return ret;
  }

  /* 所有步骤完成后，标记初始化成功。 */
  initialized_ = true;
  return 0;
}

/**
 * @brief 全屏填充单色。
 * @param color_rgb565 RGB565 颜色值。
 * @return 0 成功；负值失败。
 */
int ZephyrDisplay::clear(uint16_t color_rgb565) noexcept {
  int ret = init();
  if (ret < 0) {
    return ret;
  }

  return write_solid_rect(0U, 0U, caps_.x_resolution, caps_.y_resolution, color_rgb565);
}

/**
 * @brief 填充矩形区域。
 * @param x 左上角 X。
 * @param y 左上角 Y。
 * @param w 宽度。
 * @param h 高度。
 * @param color_rgb565 RGB565 颜色值。
 * @return 0 成功；负值失败。
 */
int ZephyrDisplay::fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                             uint16_t color_rgb565) noexcept {
  int ret = init();
  if (ret < 0) {
    return ret;
  }

  if (w == 0U || h == 0U) {
    return 0;
  }

  if (x >= caps_.x_resolution || y >= caps_.y_resolution) {
    return -EINVAL;
  }

  const uint16_t max_w = static_cast<uint16_t>(caps_.x_resolution - x);
  const uint16_t max_h = static_cast<uint16_t>(caps_.y_resolution - y);
  if (w > max_w) {
    w = max_w;
  }
  if (h > max_h) {
    h = max_h;
  }

  return write_solid_rect(x, y, w, h, color_rgb565);
}

/**
 * @brief 绘制单个 5x7 字符（可缩放）。
 * @param x 左上角 X。
 * @param y 左上角 Y。
 * @param c 待绘制字符。
 * @param fg_rgb565 前景色。
 * @param bg_rgb565 背景色。
 * @param scale 缩放倍数，0 按 1 处理。
 * @return 0 成功；负值失败。
 */
int ZephyrDisplay::draw_char(uint16_t x, uint16_t y, char c, uint16_t fg_rgb565, uint16_t bg_rgb565,
                             uint8_t scale) noexcept {
  int ret = init();
  if (ret < 0) {
    return ret;
  }

  if (scale == 0U) {
    scale = 1U;
  }

  if (x >= caps_.x_resolution || y >= caps_.y_resolution) {
    return 0;
  }

  const uint8_t* glyph = platform::font5x7::glyph(c);

  for (uint8_t col = 0U; col < platform::font5x7::kWidth; ++col) {
    const uint8_t line = glyph[col];
    for (uint8_t row = 0U; row < platform::font5x7::kHeight; ++row) {
      const uint32_t px = static_cast<uint32_t>(x) + static_cast<uint32_t>(col) * scale;
      const uint32_t py = static_cast<uint32_t>(y) + static_cast<uint32_t>(row) * scale;
      if (px >= caps_.x_resolution || py >= caps_.y_resolution) {
        continue;
      }

      const uint16_t color = ((line >> row) & 0x01U) != 0U ? fg_rgb565 : bg_rgb565;
      uint16_t w = scale;
      uint16_t h = scale;
      if (px + w > caps_.x_resolution) {
        w = static_cast<uint16_t>(caps_.x_resolution - px);
      }
      if (py + h > caps_.y_resolution) {
        h = static_cast<uint16_t>(caps_.y_resolution - py);
      }

      ret = write_solid_rect(static_cast<uint16_t>(px), static_cast<uint16_t>(py), w, h, color);
      if (ret < 0) {
        return ret;
      }
    }
  }

  /* 额外绘制 1 列字符间隔，避免字符粘连。 */
  const uint32_t gap_x =
      static_cast<uint32_t>(x) + static_cast<uint32_t>(platform::font5x7::kWidth) * scale;
  if (gap_x < caps_.x_resolution) {
    uint16_t gap_w = scale;
    uint16_t gap_h =
        static_cast<uint16_t>(static_cast<uint32_t>(platform::font5x7::kHeight) * scale);
    if (gap_x + gap_w > caps_.x_resolution) {
      gap_w = static_cast<uint16_t>(caps_.x_resolution - gap_x);
    }
    if (static_cast<uint32_t>(y) + gap_h > caps_.y_resolution) {
      gap_h = static_cast<uint16_t>(caps_.y_resolution - y);
    }

    ret = write_solid_rect(static_cast<uint16_t>(gap_x), y, gap_w, gap_h, bg_rgb565);
    if (ret < 0) {
      return ret;
    }
  }

  return 0;
}

/**
 * @brief 绘制字符串（支持 '\n'）。
 * @param x 起始 X。
 * @param y 起始 Y。
 * @param text 字符串。
 * @param fg_rgb565 前景色。
 * @param bg_rgb565 背景色。
 * @param scale 缩放倍数，0 按 1 处理。
 * @return 0 成功；负值失败。
 */
int ZephyrDisplay::draw_text(uint16_t x, uint16_t y, const char* text, uint16_t fg_rgb565,
                             uint16_t bg_rgb565, uint8_t scale) noexcept {
  if (text == nullptr) {
    return -EINVAL;
  }

  int ret = init();
  if (ret < 0) {
    return ret;
  }

  if (scale == 0U) {
    scale = 1U;
  }

  uint16_t cursor_x = x;
  uint16_t cursor_y = y;
  const uint16_t step_x =
      static_cast<uint16_t>((platform::font5x7::kWidth + platform::font5x7::kSpacing) * scale);
  const uint16_t step_y =
      static_cast<uint16_t>((platform::font5x7::kHeight + platform::font5x7::kSpacing) * scale);

  for (const char* p = text; *p != '\0'; ++p) {
    if (*p == '\n') {
      cursor_x = x;
      cursor_y = static_cast<uint16_t>(cursor_y + step_y);
      continue;
    }

    ret = draw_char(cursor_x, cursor_y, *p, fg_rgb565, bg_rgb565, scale);
    if (ret < 0) {
      return ret;
    }
    cursor_x = static_cast<uint16_t>(cursor_x + step_x);
  }

  return 0;
}

/**
 * @brief 绘制十进制有符号整数。
 * @param x 起始 X。
 * @param y 起始 Y。
 * @param value 待绘制整数。
 * @param fg_rgb565 前景色。
 * @param bg_rgb565 背景色。
 * @param scale 缩放倍数，0 按 1 处理。
 * @return 0 成功；负值失败。
 */
int ZephyrDisplay::draw_int(uint16_t x, uint16_t y, int32_t value, uint16_t fg_rgb565,
                            uint16_t bg_rgb565, uint8_t scale) noexcept {
  char buf[16]{};
  uint8_t len = 0U;

  uint32_t abs_value = 0U;
  if (value < 0) {
    buf[len++] = '-';
    /* 兼容 INT32_MIN：先 +1 再取反并补 1。 */
    abs_value = static_cast<uint32_t>(-(value + 1)) + 1U;
  } else {
    abs_value = static_cast<uint32_t>(value);
  }

  char digits[11]{};
  uint8_t dlen = 0U;
  do {
    digits[dlen++] = static_cast<char>('0' + (abs_value % 10U));
    abs_value /= 10U;
  } while (abs_value != 0U && dlen < sizeof(digits));

  while (dlen > 0U) {
    buf[len++] = digits[--dlen];
  }
  buf[len] = '\0';

  return draw_text(x, y, buf, fg_rgb565, bg_rgb565, scale);
}

/**
 * @brief 绘制启动演示画面（字符/字符串/数字，含缩放）。
 * @return 0 成功；负值失败。
 */
int ZephyrDisplay::show_boot_screen() noexcept {
  int ret = init();
  if (ret < 0) {
    return ret;
  }

  ret = clear(rgb565(0U, 0U, 0U));
  if (ret < 0) {
    return ret;
  }

  ret = draw_text(8U, 8U, "SKY BOARD", rgb565(255U, 230U, 0U), rgb565(0U, 0U, 0U), 2U);
  if (ret < 0) {
    return ret;
  }

  ret = draw_text(8U, 34U, "Display Driver", rgb565(120U, 220U, 255U), rgb565(0U, 0U, 0U), 1U);
  if (ret < 0) {
    return ret;
  }

  ret = draw_text(8U, 50U, "5x7 text x1", rgb565(180U, 255U, 180U), rgb565(0U, 0U, 0U), 1U);
  if (ret < 0) {
    return ret;
  }

  ret = draw_text(8U, 66U, "Scale x2", rgb565(255U, 160U, 80U), rgb565(0U, 0U, 0U), 2U);
  if (ret < 0) {
    return ret;
  }

  ret = draw_text(8U, 96U, "Number:", rgb565(255U, 255U, 255U), rgb565(0U, 0U, 0U), 1U);
  if (ret < 0) {
    return ret;
  }

  ret = draw_int(56U, 96U, 2026, rgb565(255U, 80U, 80U), rgb565(0U, 0U, 0U), 1U);
  if (ret < 0) {
    return ret;
  }

  ret = draw_text(8U, 112U, "Char:", rgb565(255U, 255U, 255U), rgb565(0U, 0U, 0U), 1U);
  if (ret < 0) {
    return ret;
  }

  ret = draw_char(44U, 108U, 'A', rgb565(255U, 0U, 255U), rgb565(0U, 0U, 0U), 2U);
  if (ret < 0) {
    return ret;
  }

  return 0;
}

/**
 * @brief 获取显示关联的背光控制接口。
 * @return IBacklight 引用。
 */
platform::IBacklight& ZephyrDisplay::backlight() noexcept { return platform::backlight(); }
}  // namespace

namespace platform {

/**
 * @brief 获取全局显示实例。
 * @return IDisplay 引用。
 */
IDisplay& display() { return g_display; }

}  // namespace platform
