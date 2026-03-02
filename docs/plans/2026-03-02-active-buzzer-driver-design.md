# Active Buzzer Driver Design (PA6, Fixed Active Mode)

## Context

项目需要新增蜂鸣器驱动能力，硬件连接为 `PA6`，并通过定时器 PWM 输出。当前决策为固定有源蜂鸣器模式，不读取拨码开关，不做有源/无源自动识别，也不接入 `ButtonService` 或其他业务触发路径。

## Goals / Non-Goals

**Goals:**
- 在平台层新增蜂鸣器驱动接口: `buzzer_init()`, `buzzer_on(freq_hz, duty_percent)`, `buzzer_off()`
- 使用 PA6 对应定时器通道输出 PWM，接口可由上层任意调用
- 对 `freq_hz` 与 `duty_percent` 越界值自动裁剪
- 错误路径保证蜂鸣器关闭

**Non-Goals:**
- 不读取拨码开关
- 不支持有源/无源动态切换
- 不接入 ButtonService、告警服务、开机提示音等业务策略

## Architecture

1. 板级 devicetree 新增蜂鸣器 PWM 节点与 alias（PA6 + timer channel）。
2. 平台层新增 `platform_buzzer` 头文件与 Zephyr 实现。
3. `buzzer_on(freq_hz, duty_percent)` 在驱动内部执行参数裁剪并计算 PWM period/pulse。
4. `buzzer_off()` 统一下发 `pulse=0` 实现静音。

## Parameter Strategy

- `freq_hz` 裁剪到 `[kMinFreqHz, kMaxFreqHz]`（由实现定义安全范围）
- `duty_percent` 裁剪到 `[kMinDutyPercent, kMaxDutyPercent]`（建议 `5~95`）
- 裁剪后继续执行，不因越界参数直接失败

## Failure Handling

- PWM 设备未就绪: `buzzer_init()` 返回 `-ENODEV`
- `pwm_set_dt` 失败: 返回驱动错误码并尝试保持关闭输出
- 失败记录日志，便于硬件联调

## Verification

- 编译验证: `west build -b lckfb_sky_board_stm32f407 .`
- 运行验证:
  - `buzzer_on(2000, 50)` 发声
  - `buzzer_off()` 静音
  - `buzzer_on(1, 0)`、`buzzer_on(1000000, 100)` 等极值可运行且表现为裁剪后输出
- OpenSpec 验证: `openspec validate --type change add-buzzer-driver-pa6-timer --strict --no-interactive`
