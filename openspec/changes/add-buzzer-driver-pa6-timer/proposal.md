## Why

当前项目缺少板载蜂鸣器驱动能力，无法统一控制蜂鸣器输出。为支持硬件联调与后续业务复用，需要新增固定有源模式的蜂鸣器驱动，连接 PA6 并通过定时器输出。

## What Changes

- 新增固定有源蜂鸣器驱动模块，提供初始化、开启、关闭接口。
- 开启接口支持外部传入 `freq_hz` 与 `duty_percent` 参数。
- 对 `freq/duty` 越界值执行自动裁剪，避免参数错误导致调用失败。
- 新增设备树配置，使蜂鸣器通过 PA6 对应定时器通道输出 PWM。
- 在平台层暴露统一蜂鸣器访问入口，供上层服务调用。
- 本次不读取拨码开关，不做有源/无源自动识别与切换。
- 本次不接入 `ButtonService` 或其他业务触发逻辑，仅提供驱动能力。
- 增加故障日志与安全兜底，确保异常情况下蜂鸣器保持关闭。

## Capabilities

### New Capabilities

- `buzzer-driver`: 新增基于定时器 PWM 的固定有源蜂鸣器驱动能力，支持 PA6 输出与 `freq/duty` 控制。

### Modified Capabilities

- (none)

## Impact

- 受影响代码: `subsys/platform/`、`include/platform/`。
- 受影响板级配置: `boards/st/lckfb_sky_board_stm32f407/lckfb_sky_board_stm32f407.dts`（或 overlay）。
- 受影响运行机制: 无新增后台服务或按键联动逻辑。
- 无对外网络 API 变更；内部平台接口会新增蜂鸣器能力。
