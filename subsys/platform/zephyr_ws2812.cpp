/**
 * @file zephyr_ws2812.cpp
 * @brief 基于 STM32 TIM5 CH4 + DMA 的 WS2812 平台驱动实现.
 */

#include <errno.h>
#include <stm32f4xx_hal.h>
#include <string.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>

#include <cstdint>

#include "platform/platform_ws2812.hpp"

namespace {

#define WS2812_NODE DT_ALIAS(ws2812_0)

enum class ColorOrder : uint8_t {
  kGrb = 0,
  kRgb = 1,
  kBrg = 2,
};

constexpr uint32_t kDefaultResetUs = 80U;

/* WS2812 位时序: 周期 1.25us, 0 码高电平约 0.4us, 1 码高电平约 0.8us. */
constexpr uint32_t kWs2812SymbolHz = 800000U;
constexpr uint32_t kDuty0Permille = 320U;
constexpr uint32_t kDuty1Permille = 640U;

/* STM32F407 固定映射: TIM5 CH4 -> PA3, DMA1 Stream1 Channel6. */
constexpr uint16_t kDataPin = GPIO_PIN_3;
constexpr uint32_t kTimerChannel = TIM_CHANNEL_4;
constexpr uint32_t kDmaTimeoutMs = 50U;
constexpr uint32_t kDmaRecoverWaitUs = 1000U;
constexpr size_t kBitsPerPixel = 24U;

#if DT_NODE_HAS_STATUS(WS2812_NODE, okay)
constexpr size_t kChainLength = DT_PROP(WS2812_NODE, chain_length);
constexpr uint32_t kResetUs = DT_PROP_OR(WS2812_NODE, reset_us, kDefaultResetUs);
constexpr int32_t kPixelOffset = DT_PROP_OR(WS2812_NODE, pixel_offset, 0);
constexpr char kColorOrderString[] = DT_PROP(WS2812_NODE, color_order);
constexpr size_t kPixelStorage = (kChainLength == 0U) ? 1U : kChainLength;
#else
constexpr size_t kChainLength = 0U;
constexpr uint32_t kResetUs = kDefaultResetUs;
constexpr int32_t kPixelOffset = 0;
constexpr char kColorOrderString[] = "grb";
constexpr size_t kPixelStorage = 1U;
#endif

constexpr size_t kResetSymbolCount = ((kResetUs * 100U) / 125U) + 1U;
constexpr size_t kDataSymbolCount = ((kChainLength == 0U) ? 1U : (kChainLength * kBitsPerPixel));
constexpr size_t kPulseBufferSize = kResetSymbolCount + kDataSymbolCount + kResetSymbolCount;
static_assert(kPulseBufferSize > 0U, "WS2812 pulse buffer size must be non-zero");

/**
 * @brief 解析设备树中的颜色顺序字符串.
 * @return 颜色顺序枚举.
 */
ColorOrder parse_color_order() noexcept {
  if (strcmp(kColorOrderString, "rgb") == 0) {
    return ColorOrder::kRgb;
  }
  if (strcmp(kColorOrderString, "brg") == 0) {
    return ColorOrder::kBrg;
  }
  return ColorOrder::kGrb;
}

/**
 * @brief 计算 TIM5 实际输入时钟频率.
 * @return TIM5 时钟频率, 单位 Hz.
 */
uint32_t timer5_clock_hz() noexcept {
  const uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();
  if ((RCC->CFGR & RCC_CFGR_PPRE1) == RCC_HCLK_DIV1) {
    return pclk1;
  }
  return pclk1 * 2U;
}

/**
 * @brief 将逻辑像素索引映射到物理索引.
 * @param logical_index 逻辑索引.
 * @return 物理索引.
 * @note 支持设备树中 pixel-offset 的正负偏移.
 */
size_t map_logical_to_physical(const size_t logical_index) noexcept {
  if (kChainLength == 0U) {
    return 0U;
  }

  const int32_t n = static_cast<int32_t>(kChainLength);
  int32_t normalized_offset = kPixelOffset % n;
  if (normalized_offset < 0) {
    normalized_offset += n;
  }

  const size_t offset = static_cast<size_t>(normalized_offset);
  return (logical_index + offset) % kChainLength;
}

class ZephyrWs2812 final : public platform::IWs2812 {
 public:
  /**
   * @brief 初始化驱动资源.
   * @return 0 表示成功, 负值表示失败.
   */
  int init() noexcept override;
  /**
   * @brief 获取灯珠数量.
   * @return 灯珠数量.
   */
  size_t size() const noexcept override { return kChainLength; }
  /**
   * @brief 设置单个像素颜色.
   * @param index 像素索引.
   * @param color RGB 颜色.
   * @return 0 表示成功, 负值表示失败.
   */
  int set_pixel(size_t index, const platform::Ws2812Rgb& color) noexcept override;
  /**
   * @brief 填充全部像素颜色.
   * @param color RGB 颜色.
   * @return 0 表示成功, 负值表示失败.
   */
  int fill(const platform::Ws2812Rgb& color) noexcept override;
  /**
   * @brief 下发一帧数据到灯带.
   * @return 0 表示成功, 负值表示失败.
   */
  int show() noexcept override;
  /**
   * @brief 清屏并立即下发.
   * @return 0 表示成功, 负值表示失败.
   */
  int clear_and_show() noexcept override;
  /**
   * @brief 设置全局亮度缩放值.
   * @param level 亮度值, 范围 0..255.
   * @return 0 表示成功.
   */
  int set_global_brightness(uint8_t level) noexcept override;

 private:
  /**
   * @brief 初始化底层 TIM, GPIO, DMA.
   * @return 0 表示成功, 负值表示失败.
   */
  int init_impl() noexcept;
  /**
   * @brief 启动 DMA 并等待本帧发送完成.
   * @param symbol_count 本次发送的符号个数.
   * @return 0 表示成功, 负值表示失败.
   */
  int start_dma_and_wait_impl(size_t symbol_count) noexcept;
  /**
   * @brief 强制停止 PWM DMA 输出.
   */
  void force_stop_impl() noexcept;
  /**
   * @brief 应用全局亮度到单通道值.
   * @param value 原始通道值.
   * @return 亮度缩放后的通道值.
   */
  uint8_t apply_brightness(uint8_t value) const noexcept;
  /**
   * @brief 将一个像素编码到脉冲缓冲区.
   * @param out_index 输出写指针.
   * @param color 待编码 RGB 颜色.
   */
  void encode_pixel_impl(size_t& out_index, const platform::Ws2812Rgb& color) noexcept;

  bool initialized_ = false;

  ColorOrder color_order_ = ColorOrder::kGrb;
  uint8_t brightness_ = 255U;
  uint32_t timer_period_ticks_ = 104U;
  uint32_t pulse_0_ticks_ = 34U;
  uint32_t pulse_1_ticks_ = 67U;

  TIM_HandleTypeDef htim5_{};
  DMA_HandleTypeDef hdma_tim5_ch4_{};
  bool dma_linked_ = false;

  platform::Ws2812Rgb pixels_[kPixelStorage] = {};
  uint32_t pulse_buffer_[kPulseBufferSize] = {};
};

/**
 * @brief 初始化驱动, 实际工作委托给 init_impl.
 * @return 0 表示成功, 负值表示失败.
 */
int ZephyrWs2812::init() noexcept { return init_impl(); }

/**
 * @brief 初始化 WS2812 底层外设资源.
 * @return 0 表示成功, 负值表示失败.
 */
int ZephyrWs2812::init_impl() noexcept {
  if (initialized_) {
    return 0;
  }

#if !DT_NODE_HAS_STATUS(WS2812_NODE, okay)
  return -ENODEV;
#else
  if (kChainLength == 0U) {
    return -EINVAL;
  }

  color_order_ = parse_color_order();

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_TIM5_CLK_ENABLE();
  __HAL_RCC_DMA1_CLK_ENABLE();

  GPIO_InitTypeDef gpio_init{};
  gpio_init.Pin = kDataPin;
  gpio_init.Mode = GPIO_MODE_AF_PP;
  gpio_init.Pull = GPIO_NOPULL;
  gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  gpio_init.Alternate = GPIO_AF2_TIM5;
  HAL_GPIO_Init(GPIOA, &gpio_init);

  const uint32_t tim_clk = timer5_clock_hz();
  if (tim_clk < kWs2812SymbolHz) {
    return -EINVAL;
  }
  timer_period_ticks_ = (tim_clk / kWs2812SymbolHz) - 1U;
  pulse_0_ticks_ = ((timer_period_ticks_ + 1U) * kDuty0Permille) / 1000U;
  pulse_1_ticks_ = ((timer_period_ticks_ + 1U) * kDuty1Permille) / 1000U;

  htim5_ = {};
  htim5_.Instance = TIM5;
  htim5_.Init.Prescaler = 0U;
  htim5_.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim5_.Init.Period = timer_period_ticks_;
  htim5_.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim5_.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim5_) != HAL_OK) {
    return -EIO;
  }

  TIM_OC_InitTypeDef oc{};
  oc.OCMode = TIM_OCMODE_PWM1;
  oc.Pulse = 0U;
  oc.OCPolarity = TIM_OCPOLARITY_HIGH;
  oc.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim5_, &oc, kTimerChannel) != HAL_OK) {
    return -EIO;
  }

  hdma_tim5_ch4_ = {};
  hdma_tim5_ch4_.Instance = DMA1_Stream1;
  hdma_tim5_ch4_.Init.Channel = DMA_CHANNEL_6;
  hdma_tim5_ch4_.Init.Direction = DMA_MEMORY_TO_PERIPH;
  hdma_tim5_ch4_.Init.PeriphInc = DMA_PINC_DISABLE;
  hdma_tim5_ch4_.Init.MemInc = DMA_MINC_ENABLE;
  hdma_tim5_ch4_.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
  hdma_tim5_ch4_.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
  hdma_tim5_ch4_.Init.Mode = DMA_NORMAL;
  hdma_tim5_ch4_.Init.Priority = DMA_PRIORITY_HIGH;
  hdma_tim5_ch4_.Init.FIFOMode = DMA_FIFOMODE_ENABLE;
  hdma_tim5_ch4_.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;
  hdma_tim5_ch4_.Init.MemBurst = DMA_MBURST_SINGLE;
  hdma_tim5_ch4_.Init.PeriphBurst = DMA_PBURST_SINGLE;
  /* 重新初始化 DMA 流, 避免上次运行后残留状态影响当前帧发送. */
  (void)HAL_DMA_DeInit(&hdma_tim5_ch4_);
  if (HAL_DMA_Init(&hdma_tim5_ch4_) != HAL_OK) {
    return -EIO;
  }

  __HAL_LINKDMA(&htim5_, hdma[TIM_DMA_ID_CC4], hdma_tim5_ch4_);
  dma_linked_ = true;
  initialized_ = true;
  return 0;
#endif
}

/**
 * @brief 对单通道值执行全局亮度缩放.
 * @param value 原始值, 范围 0..255.
 * @return 缩放后值.
 */
uint8_t ZephyrWs2812::apply_brightness(const uint8_t value) const noexcept {
  const uint16_t scaled = static_cast<uint16_t>(value) * static_cast<uint16_t>(brightness_);
  return static_cast<uint8_t>((scaled + 127U) / 255U);
}

/**
 * @brief 设置单个像素缓冲值.
 * @param index 像素索引.
 * @param color RGB 颜色.
 * @return 0 表示成功, 负值表示失败.
 */
int ZephyrWs2812::set_pixel(const size_t index, const platform::Ws2812Rgb& color) noexcept {
  if (index >= kChainLength) {
    return -EINVAL;
  }

  pixels_[map_logical_to_physical(index)] = color;
  return 0;
}

/**
 * @brief 用同一颜色填充所有像素缓冲.
 * @param color RGB 颜色.
 * @return 0 表示成功.
 */
int ZephyrWs2812::fill(const platform::Ws2812Rgb& color) noexcept {
  for (size_t i = 0U; i < kChainLength; ++i) {
    pixels_[i] = color;
  }
  return 0;
}

/**
 * @brief 按配置颜色顺序把一个像素编码为 24bit 脉冲序列.
 * @param out_index 脉冲缓冲区当前写入位置.
 * @param color 待编码颜色.
 */
void ZephyrWs2812::encode_pixel_impl(size_t& out_index, const platform::Ws2812Rgb& color) noexcept {
  const uint8_t r = apply_brightness(color.r);
  const uint8_t g = apply_brightness(color.g);
  const uint8_t b = apply_brightness(color.b);

  uint8_t channels[3] = {};
  switch (color_order_) {
    case ColorOrder::kRgb:
      channels[0] = r;
      channels[1] = g;
      channels[2] = b;
      break;
    case ColorOrder::kBrg:
      channels[0] = b;
      channels[1] = r;
      channels[2] = g;
      break;
    case ColorOrder::kGrb:
    default:
      channels[0] = g;
      channels[1] = r;
      channels[2] = b;
      break;
  }

  for (size_t ch = 0U; ch < 3U; ++ch) {
    for (int bit = 7; bit >= 0; --bit) {
      pulse_buffer_[out_index++] =
          ((channels[ch] & (1U << bit)) != 0U) ? pulse_1_ticks_ : pulse_0_ticks_;
    }
  }
}

/**
 * @brief 启动 TIM5 PWM DMA 并等待整帧发送结束.
 * @param symbol_count 要发送的符号数量.
 * @return 0 表示成功, 负值表示失败.
 */
int ZephyrWs2812::start_dma_and_wait_impl(const size_t symbol_count) noexcept {
  if (!dma_linked_) {
    return -ENODEV;
  }
  if (symbol_count == 0U) {
    return -EINVAL;
  }

  HAL_StatusTypeDef st = HAL_TIM_PWM_Start_DMA(&htim5_, kTimerChannel, pulse_buffer_,
                                               static_cast<uint16_t>(symbol_count));
  if (st != HAL_OK) {
    /* 遇到 HAL 忙或异常时先强制停机, 再做一次短延迟重试. */
    force_stop_impl();
    k_busy_wait(kDmaRecoverWaitUs);
    st = HAL_TIM_PWM_Start_DMA(&htim5_, kTimerChannel, pulse_buffer_,
                               static_cast<uint16_t>(symbol_count));
    if (st != HAL_OK) {
      return (st == HAL_BUSY) ? -EBUSY : -EIO;
    }
  }

  st = HAL_DMA_PollForTransfer(&hdma_tim5_ch4_, HAL_DMA_FULL_TRANSFER, kDmaTimeoutMs);
  if (st != HAL_OK) {
    force_stop_impl();
    return (st == HAL_TIMEOUT) ? -ETIMEDOUT : -EIO;
  }

  force_stop_impl();
  __HAL_TIM_SET_COMPARE(&htim5_, kTimerChannel, 0U);
  k_busy_wait(kResetUs);
  return 0;
}

/**
 * @brief 强制停止 PWM DMA 并拉低输出比较值.
 */
void ZephyrWs2812::force_stop_impl() noexcept {
  /* 停止路径统一使用 HAL_TIM_PWM_Stop_DMA, 并拉低比较值. */
  (void)HAL_TIM_PWM_Stop_DMA(&htim5_, kTimerChannel);
  __HAL_TIM_SET_COMPARE(&htim5_, kTimerChannel, 0U);
}

/**
 * @brief 组帧并发送当前像素缓冲.
 * @return 0 表示成功, 负值表示失败.
 */
int ZephyrWs2812::show() noexcept {
  const int init_ret = init_impl();
  if (init_ret < 0) {
    return init_ret;
  }

  /* 组帧: 前置复位低电平 + 像素编码数据 + 尾部复位低电平. */
  size_t out_idx = 0U;
  for (size_t i = 0U; i < kResetSymbolCount; ++i) {
    pulse_buffer_[out_idx++] = 0U;
  }
  for (size_t i = 0U; i < kChainLength; ++i) {
    encode_pixel_impl(out_idx, pixels_[i]);
  }
  while (out_idx < kPulseBufferSize) {
    pulse_buffer_[out_idx++] = 0U;
  }

  /* 发送整帧, 若遇到瞬时 HAL 异常则执行一次恢复重试. */
  int ret = start_dma_and_wait_impl(kPulseBufferSize);
  if (ret == -EBUSY || ret == -EIO) {
    /* 针对瞬时 busy 或 IO 异常, 强制停机后仅重试一帧. */
    force_stop_impl();
    k_sleep(K_MSEC(1));
    ret = start_dma_and_wait_impl(kPulseBufferSize);
  }
  return ret;
}

/**
 * @brief 清空像素缓冲并立即下发.
 * @return 0 表示成功, 负值表示失败.
 */
int ZephyrWs2812::clear_and_show() noexcept {
  for (size_t i = 0U; i < kChainLength; ++i) {
    pixels_[i] = {};
  }
  return show();
}

/**
 * @brief 设置全局亮度缩放值.
 * @param level 亮度值, 范围 0..255.
 * @return 0 表示成功.
 */
int ZephyrWs2812::set_global_brightness(const uint8_t level) noexcept {
  brightness_ = level;
  return 0;
}

ZephyrWs2812 g_ws2812;

}  // namespace

#undef WS2812_NODE

namespace platform {

IWs2812& ws2812() noexcept { return g_ws2812; }

Ws2812Rgb ws2812_wheel(uint8_t pos) noexcept {
  pos = static_cast<uint8_t>(255U - pos);
  if (pos < 85U) {
    return {static_cast<uint8_t>(255U - pos * 3U), 0U, static_cast<uint8_t>(pos * 3U)};
  }
  if (pos < 170U) {
    pos = static_cast<uint8_t>(pos - 85U);
    return {0U, static_cast<uint8_t>(pos * 3U), static_cast<uint8_t>(255U - pos * 3U)};
  }
  pos = static_cast<uint8_t>(pos - 170U);
  return {static_cast<uint8_t>(pos * 3U), static_cast<uint8_t>(255U - pos * 3U), 0U};
}

int ws2812_wheel_show(IWs2812& ws, const uint8_t phase) noexcept {
  const size_t count = ws.size();
  if (count == 0U) {
    return 0;
  }

  for (size_t i = 0U; i < count; ++i) {
    const uint8_t p =
        static_cast<uint8_t>((static_cast<uint16_t>(phase) + (i * 256U / count)) & 0xFFU);
    const int set_ret = ws.set_pixel(i, ws2812_wheel(p));
    if (set_ret < 0) {
      return set_ret;
    }
  }
  return ws.show();
}

}  // namespace platform
