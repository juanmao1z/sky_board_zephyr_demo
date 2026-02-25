/**
 * @file main.cpp
 * @brief 应用主入口：把控制权交给 app 初始化流程。
 */

#include <zephyr/kernel.h>

#include "app/app_Init.hpp"
#include "platform/platform_ws2812.hpp"

/**
 * @brief 应用入口函数。
 * @return app 层初始化结果码，0 表示成功，负值表示失败。
 */
int main(void) {
  const int ret = app::app_Init();
  if (ret < 0) {
    return ret;
  }
  // 主线程彩虹流水灯: 每帧整体相位递增, 灯带颜色呈流动效果.
  platform::IWs2812& ws = platform::ws2812();
  (void)ws.set_global_brightness(255U);
  uint8_t phase = 0U;
  while (true) {
    const size_t count = ws.size();
    if (count == 0U) {
      k_sleep(K_MSEC(500));
      continue;
    }
    (void)platform::ws2812_wheel_show(ws, phase);
    ++phase;
    k_sleep(K_MSEC(2));
  }
}
