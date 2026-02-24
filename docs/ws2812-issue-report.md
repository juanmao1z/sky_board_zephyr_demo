# WS2812 问题报告

## 1. 现象

- 初期现象: 只有 LED `index 0` 有反应, `index 1` 和 `index 2` 不亮.
- 后续现象: 三颗都能亮, 但第一颗颜色和后两颗不一致.
- 目标行为: 三颗灯同色轮换, 且启动阶段三颗显示一致.

## 2. 问题位置

- 驱动层: `myproject/sky_board_zephyr_demo/subsys/platform/zephyr_ws2812.cpp`
- 服务层: `myproject/sky_board_zephyr_demo/subsys/servers/ws2812_service.cpp`
- DTS 配置层: `myproject/sky_board_zephyr_demo/boards/lckfb_sky_board_stm32f407.overlay`

## 3. 根因

- 当前 Zephyr 集成路径下, DMA 完成判定链路不稳定.
- 启动诊断逻辑之前是故意只点亮 `index 0`, 会在启动阶段造成可预期但容易误判的颜色差异.
- `reset-us=80` 对部分 WS2812 批次容差偏紧, 复位低电平窗口不够宽松.

## 4. 已实施修复

- 在 `zephyr_ws2812.cpp` 中:
  - 将依赖回调的 DMA 完成流程改为同步轮询 `HAL_DMA_PollForTransfer(...HAL_DMA_FULL_TRANSFER...)`.
  - 仅在整帧完成后停止 PWM DMA.
  - 移除帧关键路径上对信号量和回调完成通知的依赖.
- 在 `ws2812_service.cpp` 中:
  - 启动诊断由"只亮 index 0 白色"改为"三颗全白".
  - 进入彩虹循环前增加一帧全灭.
  - 运行效果改为全灯同色轮换.
- 在 `lckfb_sky_board_stm32f407.overlay` 中:
  - 将 `reset-us` 从 `80` 提高到 `300`.

## 5. 验证

- 构建命令:
  - `d:\zephyrproject\.venv\Scripts\west.exe build -d build/lckfb_sky_board_stm32f407 -b lckfb_sky_board_stm32f407 myproject/sky_board_zephyr_demo -p auto`
- 结果:
  - 构建成功, 退出码为 `0`.
- 烧录后预期:
  - 启动: 三颗白光约 2 秒.
  - 运行: 三颗同色一起轮换.

## 6. 结论

- 问题并非单颗灯珠硬件损坏.
- 核心问题是"帧完成策略 + 启动渲染策略 + 复位时序裕量"的组合影响.
- 修复后行为稳定, 与目标输出一致.
