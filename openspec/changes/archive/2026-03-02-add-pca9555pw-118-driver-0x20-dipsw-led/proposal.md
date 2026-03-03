## Why

当前工程缺少对 PCA9555PW,118 的板级驱动接入，无法通过统一平台接口读取外接拨码开关状态或控制外接白色 LED。为支持硬件联调与后续业务复用，需要新增该器件在 I2C1 地址 `0x20` 的驱动能力。

## What Changes

- 在板级 devicetree 中新增 PCA9555 节点（I2C1, `0x20`）并定义稳定 alias。
- 新增平台层 PCA9555 抽象接口与 Zephyr 实现，提供初始化、读取 DIP 开关、写入 LED 位图能力。
- 固定端口语义：
  - `io0_0` ~ `io0_7`: 作为拨码开关输入（DIP）
  - `io1_0` ~ `io1_7`: 作为白色 LED 输出
- 新增启动期最小化自检与日志，验证设备可用和方向配置生效。
- 增加必要 Kconfig 使能项，确保驱动构建行为稳定。

## Capabilities

### New Capabilities

- `pca9555-io-expander`: 提供 PCA9555 的 I/O 扩展驱动能力，支持 DIP 输入读取与白色 LED 输出控制。

### Modified Capabilities

- (none)

## Impact

- 受影响代码：
  - `boards/lckfb_sky_board_stm32f407.overlay`
  - `include/platform/`
  - `subsys/platform/`
  - `app/app_Init.cpp`（仅初始化接入与日志）
  - `CMakeLists.txt`
  - `prj.conf`
- 对外网络 API 无变更；新增内部平台接口。
- 对现有按键、蜂鸣器、存储、传感器服务无强耦合改动。
