# Active Buzzer PA6 Design

## Context

目标板需要新增有源蜂鸣器提示能力。硬件连接固定为 `PA6`，并要求通过定时器输出控制。交互策略已确定为: 按键按下时触发单短响 `80ms`。

当前代码已具备:
- 平台层设备抽象 (`include/platform`, `subsys/platform`)
- `ButtonService` 事件线程与回调处理
- PWM 使用范例（背光）

## Design

### Architecture

采用三层最小改动方案:
1. 板级 DTS/overlay 新增蜂鸣器 PWM 节点，绑定 `PA6` 对应定时器通道与 alias。
2. 平台层新增蜂鸣器接口与 Zephyr 实现，提供 `init/on/off`。
3. `ButtonService` 在按下事件触发蜂鸣器 `on`，并用 `k_work_delayable` 在 `80ms` 后 `off`。

### Data/Control Flow

1. 系统启动后，`ButtonService::run()` 调用蜂鸣器 `init`。
2. 收到 `pressed=true` 按键事件时，调用 `buzzer_on()`。
3. 重新提交延迟工作（`80ms`）以执行 `buzzer_off()`。
4. 如果连续按下，关闭时刻以最近一次按下为准。

### Error Handling

- 设备未就绪: `init` 返回错误并记录日志，后续触发直接忽略蜂鸣器动作。
- `on/off` 失败: 记录日志并尝试确保输出关闭。
- 延迟工作执行异常: 不影响按键主流程。

## Scope

### In Scope

- 有源蜂鸣器基础驱动（PA6 + timer PWM）
- 按键按下短响 `80ms`
- 非阻塞自动关闭

### Out of Scope

- 旋律/音高编排
- 独立蜂鸣器服务
- 其他业务模块联动策略

## Verification

- 编译验证: `west build -b lckfb_sky_board_stm32f407 .`
- 运行验证: 实机按键确认每次按下响 `80ms` 左右，连续按键无阻塞
- 规约验证: `openspec validate --type change add-buzzer-driver-pa6-timer --strict --no-interactive`
