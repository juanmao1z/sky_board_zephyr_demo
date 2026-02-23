# Zephyr 板级开发设计：lckfb_sky_board_stm32f407 使用 SDCard + FATFS

- 日期：2026-02-22
- 状态：已评审通过（Brainstorming）
- 目标板：`lckfb_sky_board_stm32f407`
- 需求：`使用 SDCard`、`使用 FATFS`
- 已确认硬件路径：`SDIO 4-bit`，Card Detect 为 `PD3`

## 1. 需求与边界

### 1.1 功能目标

1. 启用板级 SDIO 存储通道（`sdmmc1`）。
1. 启用 FATFS 并实现 SD 卡挂载可用。
1. SD 功能异常不影响现有显示、ETH、TCP 8000 服务。

### 1.2 非目标

1. 本轮不实现上层复杂文件管理策略（如日志轮转、分区管理）。
1. 本轮不新增业务协议变更。
1. 本轮不做性能极限优化（仅确保稳定可用）。

## 2. 备选方案与结论

### 2.1 方案对比

1. 方案 1：纯板级启用 `SDIO + FATFS`（推荐）

- 优点：结构清晰、与现有 `spi1` LCD 无冲突、后续维护成本低。
- 风险：需确认 SDIO 引脚复用与 CD 极性。

1. 方案 2：应用 overlay 承载 SD 配置

- 优点：短期试验改动快。
- 风险：板级定义分裂，长期维护差。

1. 方案 3：先无 CD 后补 CD

- 优点：快速点亮。
- 风险：插拔行为不完整。

### 2.2 结论

采用方案 1：在板级 DTS 定义硬件事实，在 `prj.conf` 打开文件系统能力，应用层仅进行挂载与读写调用。

## 3. 架构与配置分层

1. 板级 DTS（硬件事实）

- 文件：`zephyr/boards/st/lckfb_sky_board_stm32f407/lckfb_sky_board_stm32f407.dts`
- 内容：启用 `&sdmmc1`、绑定 SDIO 引脚、设置 `cd-gpios(PD3)`、设置 `disk-name`、补 `aliases`。

1. 项目 `prj.conf`（软件能力开关）

- 文件：`myproject/sky_board_zephyr_demo/prj.conf`
- 内容：启用 `DISK_ACCESS`、`FILE_SYSTEM`、`FAT_FILESYSTEM_ELM`、`DISK_DRIVER_SDMMC`。

1. 应用层（行为）

- 只负责挂载点、文件读写与错误日志；不承担底层总线配置。

## 4. 板级设计细化（DTS）

### 4.1 `sdmmc1` 目标配置

1. `status = "okay"`
1. `pinctrl-0` 绑定：

- `sdio_d0_pc8`
- `sdio_d1_pc9`
- `sdio_d2_pc10`
- `sdio_d3_pc11`
- `sdio_ck_pc12`
- `sdio_cmd_pd2`

1. `pinctrl-names = "default"`
1. `cd-gpios = <&gpiod 3 ...>`（极性按硬件电路定义）
1. `disk-name = "SD"`

### 4.2 aliases

建议增加：`sdhc0 = &sdmmc1`，便于复用 Zephyr 示例与通用路径。

### 4.3 与现有外设关系

1. `spi1` 已用于 LCD，本方案不占用 `spi1`。
1. ETH RMII 引脚已占用，不与 SDIO 冲突。

## 5. 软件配置细化（Kconfig）

建议启用（项目侧）：

1. `CONFIG_DISK_ACCESS=y`
1. `CONFIG_FILE_SYSTEM=y`
1. `CONFIG_FAT_FILESYSTEM_ELM=y`
1. `CONFIG_DISK_DRIVER_SDMMC=y`

可按实际需要补充 FATFS 细项（扇区大小、长文件名等），本轮先保持最小可用集。

## 6. 启动时序与状态机

### 6.1 启动时序

1. 系统初始化 `sdmmc1`。
1. 读取 `PD3` 卡检测状态。
1. 有卡则注册磁盘 `SD`。
1. 文件系统层挂载 FAT。
1. 应用进入可读写状态。

### 6.2 状态机

1. `NO_CARD`：无卡，跳过挂载并记录日志。
1. `CARD_PRESENT_UNMOUNTED`：有卡，尝试挂载。
1. `MOUNTED`：挂载成功，可读写。
1. `ERROR_BACKOFF`：失败后退避重试，不阻塞主业务。

## 7. 异常处理策略

1. 无卡：仅报提示，不致命。
1. CD 极性错误：只调整 `cd-gpios` 极性位。
1. 挂载失败（坏卡/未格式化）：记录错误并进入可恢复路径。
1. 任何 SD 失败均不影响 ETH/TCP/显示主流程。

## 8. 验证计划

### 8.1 功能验证

1. 冷启动无卡：系统正常运行，SD 报无卡。
1. 冷启动有 FAT 卡：挂载成功，创建/写入/读取/卸载成功。
1. 非 FAT 卡：挂载失败有日志，系统无崩溃。
1. 插拔回归：系统不死机，主服务不断。

### 8.2 回归验证

1. ETH DHCP 正常。
1. TCP 8000 回传正常。
1. 显示与背光初始化行为不回退。

### 8.3 构建验证

1. `west build` 必须通过。
1. 不引入阻断性告警/错误。

## 9. 风险与回滚

1. 风险：板内存在多份同名板定义导致“改了未生效”。

- 控制：明确单一构建入口使用的板定义路径。

1. 风险：CD 极性不匹配。

- 控制：首次 bring-up 保留极性切换验证步骤。

1. 回滚策略：

- 保留原 DTS/Kconfig 基线，若 SD 路径异常，先回退 `sdmmc1` 启用项，确保主业务可发布。

## 10. 实施清单（下一阶段）

1. 修改板级 DTS（`sdmmc1` + `PD3` CD + alias）。
1. 修改 `prj.conf`（FATFS/SDMMC 最小可用开关）。
1. 添加最小挂载与读写验证逻辑。
1. 执行 build + flash + 串口验证并记录结果。
