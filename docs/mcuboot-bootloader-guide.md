# MCUboot 接入指南(针对 `sky_board_zephyr_demo`)

## 1. 目标

本文给出一套可执行流程, 让当前项目支持 Bootloader 启动链, 并具备后续升级能力.  
推荐方案是 Zephyr 标准 `sysbuild + MCUboot`.

---

## 2. 当前工程现状(已核对)

### 2.1 已有条件

- 项目可稳定 `west build` 和 `west flash`.
- 目标板为 `lckfb_sky_board_stm32f407`.
- 主工程路径: `d:/zephyrproject/myproject/sky_board_zephyr_demo`.

### 2.2 还未接入的部分

- 根目录暂无 `sysbuild.conf`.
- 未定义 MCUboot 专用 flash 分区布局.
- 目前仍是直接启动 App, 不是 `MCUboot -> App` 链路.

### 2.3 容量基线(针对 `STM32F407VGT6`)

`STM32F407VGT6` 的标准内置 Flash 为 `1MB`(`0x100000`).  
因此本文默认使用 1MB 分区模板(A 方案).  
如果烧录工具偶发显示 512KB, 请先复核芯片丝印与实际型号, 再决定是否改用 512KB 布局.

---

## 3. 方案选择

## 3.1 推荐方案

使用 `sysbuild` 打包 MCUboot 与应用:

- `SB_CONFIG_BOOTLOADER_MCUBOOT=y`
- `SB_CONFIG_MCUBOOT_MODE_OVERWRITE_ONLY=y`

说明:

- `overwrite-only` 对分区要求更宽松, 比 swap 方案更容易先跑通.
- 跑通后再评估是否切到 swap 或外置 flash secondary slot.

## 3.2 为什么不先上复杂 OTA 方案

- 你当前应用体积约 198KB, 内部 flash 紧张.
- STM32F4 扇区大小不均匀, 双槽+回滚布局更容易踩坑.
- 先完成 "安全启动链 + 可升级基础能力", 风险最低.

---

## 4. 接入步骤

## 4.1 新增 `sysbuild.conf`(项目根目录)

文件: `d:/zephyrproject/myproject/sky_board_zephyr_demo/sysbuild.conf`

```conf
SB_CONFIG_BOOTLOADER_MCUBOOT=y
SB_CONFIG_MCUBOOT_MODE_OVERWRITE_ONLY=y
```

## 4.2 新增 MCUboot 子镜像配置

新建目录与文件:

- `d:/zephyrproject/myproject/sky_board_zephyr_demo/sysbuild/mcuboot.conf`

建议内容:

```conf
CONFIG_MCUBOOT_LOG_LEVEL_WRN=y
CONFIG_MCUBOOT_DOWNGRADE_PREVENTION=y
```

如需串口恢复模式(可选), 再加:

```conf
CONFIG_MCUBOOT_SERIAL=y
CONFIG_BOOT_SERIAL_UART=y
```

## 4.3 修改板级 overlay 分区

文件: `d:/zephyrproject/myproject/sky_board_zephyr_demo/boards/lckfb_sky_board_stm32f407.overlay`

在 `/ {}` 里增加:

```dts
/ {
 chosen {
  zephyr,code-partition = &slot0_partition;
 };
};
```

并在 `&flash0` 下增加 `fixed-partitions`.  
你当前型号是 `STM32F407VGT6`, 直接使用下面 A 模板.

### A. 若芯片实际是 1MB(0x100000)

```dts
&flash0 {
 partitions {
  compatible = "fixed-partitions";
  #address-cells = <1>;
  #size-cells = <1>;

  boot_partition: partition@0 {
   label = "mcuboot";
   reg = <0x00000000 0x00010000>;
   read-only;
  };

  slot0_partition: partition@10000 {
   label = "image-0";
   reg = <0x00010000 0x00070000>;
  };

  slot1_partition: partition@80000 {
   label = "image-1";
   reg = <0x00080000 0x00070000>;
  };
 };
};
```

### B. 若后续确认实际芯片是 512KB(0x80000)

先做最小可用链路, 重点是 MCUboot 能拉起 App.  
示例布局:

```dts
&flash0 {
 partitions {
  compatible = "fixed-partitions";
  #address-cells = <1>;
  #size-cells = <1>;

  boot_partition: partition@0 {
   label = "mcuboot";
   reg = <0x00000000 0x00010000>;
   read-only;
  };

  slot0_partition: partition@10000 {
   label = "image-0";
   reg = <0x00010000 0x00070000>;
  };
 };
};
```

说明:

- 512KB 下建议先走单槽启动链路.
- 若要保留回滚能力, 建议后续把 `image-1` 放到外置 `W25Q128`(进阶方案).

## 4.4 应用侧配置

`sysbuild` 一般会自动为 App 处理 MCUboot 相关设置.  
若你希望显式声明, 可在 `prj.conf` 加:

```conf
CONFIG_BOOTLOADER_MCUBOOT=y
```

---

## 5. 构建与烧录

## 5.1 全新构建

```powershell
d:\zephyrproject\.venv\Scripts\west.exe build --sysbuild `
  -b lckfb_sky_board_stm32f407 `
  d:/zephyrproject/myproject/sky_board_zephyr_demo `
  -d d:/zephyrproject/myproject/sky_board_zephyr_demo/build/mcuboot `
  -p always
```

## 5.2 烧录

```powershell
d:\zephyrproject\.venv\Scripts\west.exe flash `
  -d d:/zephyrproject/myproject/sky_board_zephyr_demo/build/mcuboot
```

---

## 6. 验收清单

## 6.1 构建产物

确认以下至少存在:

- `mcuboot/zephyr/zephyr.bin`(或 `hex`)
- App 的 `zephyr.signed.bin`

可用命令:

```powershell
Get-ChildItem -Recurse d:/zephyrproject/myproject/sky_board_zephyr_demo/build/mcuboot `
  -Include *mcuboot*.bin,*signed*.bin,*signed*.hex
```

## 6.2 串口日志

启动时应看到 MCUboot 先启动, 然后跳转 App.  
即使日志级别较低, 也应能观察到"先短暂 Bootloader 阶段, 再进入你当前业务日志"的节奏.

## 6.3 功能回归

以下能力必须保持:

- 以太网, time service, tcp service.
- SD 卡挂载.
- PCA9555 流水灯与 DIP 读取.
- EEPROM, SPI flash, boot counter.

---

## 7. 升级流程基础(后续)

如果启用 `CONFIG_MCUBOOT_SERIAL=y`, 可用 `mcumgr` 走串口上传签名镜像:

```powershell
mcumgr --conntype serial --connstring "dev=COM9,baud=921600,mtu=512" image list
mcumgr --conntype serial --connstring "dev=COM9,baud=921600,mtu=512" image upload <path-to-signed-bin>
mcumgr --conntype serial --connstring "dev=COM9,baud=921600,mtu=512" reset
```

提示:

- `<path-to-signed-bin>` 请使用构建产物中的 `zephyr.signed.bin`.
- 若上传后不切换, 先核对 MCUboot 工作模式与分区定义.

---

## 8. 常见问题

## 8.1 烧录成功但设备不启动

- 优先检查 `zephyr,code-partition` 是否指向 `slot0_partition`.
- 检查分区是否越界.
- 检查 slot 起始是否与 flash 擦除粒度对齐.

## 8.2 报镜像头错误或签名错误

- 确认使用的是 `signed` 镜像, 不是 `unsigned` 镜像.
- 确认 MCUboot 与 App 使用同一签名策略.

## 8.3 512KB 机型空间不够

- 先使用单槽最小方案跑通.
- 减小应用体积(关闭非必要模块).
- 或把 `image-1` 放外置 `W25Q128` 实现更完整升级策略.

---

## 9. 建议的执行顺序

1. 使用 1MB 模板(A 方案)更新 overlay 分区.
1. 新增 `sysbuild.conf` 与 `sysbuild/mcuboot.conf`.
1. 执行 `--sysbuild` 全新构建与烧录.
1. 通过串口和功能回归做验收.
1. 最后再接入 `mcumgr` 升级链路.
