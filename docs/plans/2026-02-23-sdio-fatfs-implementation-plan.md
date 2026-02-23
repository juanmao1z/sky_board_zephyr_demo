# SDIO FATFS Bring-up Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 在 `lckfb_sky_board_stm32f407` 上完成 SDIO SDCard + FATFS 可用化，并在应用启动阶段完成一次最小读写验证，且不回退 ETH/TCP/显示功能。

**Architecture:** 使用“板级 DTS 定义硬件事实 + `prj.conf` 打开能力 + 应用服务执行挂载读写验证”的三层结构。板级只负责 `sdmmc1`、引脚和 CD(PD3)；应用层新增 `SdcardService` 负责挂载与文件 I/O 验证并打印日志。所有失败均降级处理，不阻塞主业务服务。

**Tech Stack:** Zephyr DTS/Kconfig、STM32 SDMMC(SDIO 4-bit)、FatFS(Elm)、Zephyr FS API、C++17、west。

---

### Task 1: 板级 DTS 启用 SDIO + Card Detect

**Files:**

- Modify: `myproject/sky_board_zephyr_demo/boards/st/lckfb_sky_board_stm32f407/lckfb_sky_board_stm32f407.dts`
- Modify: `myproject/sky_board_zephyr_demo/boards/lckfb_sky_board_stm32f407.overlay`（仅保留“已迁移到板级 DTS”的说明）

**Step 1: 基线失败检查（当前应未启用 SDIO）**

Run:

```powershell
rg -n "&sdmmc1|sdhc0|cd-gpios|disk-name" d:/zephyrproject/myproject/sky_board_zephyr_demo/boards/st/lckfb_sky_board_stm32f407/lckfb_sky_board_stm32f407.dts
```

Expected: 无 `&sdmmc1` 配置块或无 `sdhc0` alias（表示需求尚未实现）。

**Step 2: 修改板级 DTS（最小可用配置）**

Add/modify:

```dts
/ {
    aliases {
        sdhc0 = &sdmmc1;
    };
};

&sdmmc1 {
    status = "okay";
    pinctrl-0 = <&sdio_d0_pc8 &sdio_d1_pc9
                 &sdio_d2_pc10 &sdio_d3_pc11
                 &sdio_ck_pc12 &sdio_cmd_pd2>;
    pinctrl-names = "default";
    cd-gpios = <&gpiod 3 GPIO_ACTIVE_LOW>;
    disk-name = "SD";
};
```

Note: `cd-gpios` 极性先按 `GPIO_ACTIVE_LOW`，若现场测试“反相”则仅切换 ACTIVE_LOW/HIGH。

**Step 3: 执行 build 生成最终 DTS**

Run:

```powershell
d:\zephyrproject\.venv\Scripts\west.exe build \
  -b lckfb_sky_board_stm32f407 \
  d:/zephyrproject/myproject/sky_board_zephyr_demo \
  -d d:/zephyrproject/myproject/sky_board_zephyr_demo/build/lckfb_sky_board_stm32f407 \
  -p auto
```

Expected: Build 成功，DTS 编译通过。

**Step 4: 验证生成树中 SDIO 已生效**

Run:

```powershell
rg -n "sdmmc1|cd-gpios|disk-name|sdhc0" d:/zephyrproject/myproject/sky_board_zephyr_demo/build/lckfb_sky_board_stm32f407/zephyr/zephyr.dts
```

Expected: 能看到 `sdmmc1 status = "okay"`、`cd-gpios = <...pd3...>`、`disk-name = "SD"`、`sdhc0`。

**Step 5: Commit**

```powershell
git -C d:/zephyrproject/myproject/sky_board_zephyr_demo add \
  boards/st/lckfb_sky_board_stm32f407/lckfb_sky_board_stm32f407.dts \
  boards/lckfb_sky_board_stm32f407.overlay
git -C d:/zephyrproject/myproject/sky_board_zephyr_demo commit -m "feat(board): enable sdmmc1 and pd3 card detect"
```

---

### Task 2: 打开 FATFS/SDMMC 所需 Kconfig

**Files:**

- Modify: `myproject/sky_board_zephyr_demo/prj.conf`

**Step 1: 基线失败检查（配置缺失）**

Run:

```powershell
rg -n "^CONFIG_(DISK_ACCESS|FILE_SYSTEM|FAT_FILESYSTEM_ELM|DISK_DRIVER_SDMMC)=y" d:/zephyrproject/myproject/sky_board_zephyr_demo/prj.conf
```

Expected: 至少缺少一项。

**Step 2: 添加最小可用配置**

Add:

```conf
CONFIG_DISK_ACCESS=y
CONFIG_FILE_SYSTEM=y
CONFIG_FAT_FILESYSTEM_ELM=y
CONFIG_DISK_DRIVER_SDMMC=y
```

**Step 3: 重新构建**

Run:

```powershell
d:\zephyrproject\.venv\Scripts\west.exe build \
  -b lckfb_sky_board_stm32f407 \
  d:/zephyrproject/myproject/sky_board_zephyr_demo \
  -d d:/zephyrproject/myproject/sky_board_zephyr_demo/build/lckfb_sky_board_stm32f407 \
  -p auto
```

Expected: 配置生效，编译继续推进。

**Step 4: 验证 `.config`**

Run:

```powershell
rg -n "^CONFIG_(DISK_ACCESS|FILE_SYSTEM|FAT_FILESYSTEM_ELM|DISK_DRIVER_SDMMC)=y" d:/zephyrproject/myproject/sky_board_zephyr_demo/build/lckfb_sky_board_stm32f407/zephyr/.config
```

Expected: 四项均为 `=y`。

**Step 5: Commit**

```powershell
git -C d:/zephyrproject/myproject/sky_board_zephyr_demo add prj.conf
git -C d:/zephyrproject/myproject/sky_board_zephyr_demo commit -m "feat(config): enable fatfs and sdmmc stack"
```

---

### Task 3: 新增 `SdcardService` 接口与骨架

**Files:**

- Create: `myproject/sky_board_zephyr_demo/include/servers/sdcard_service.hpp`
- Create: `myproject/sky_board_zephyr_demo/subsys/servers/sdcard_service.cpp`

**Step 1: 基线失败检查（服务尚不存在）**

Run:

```powershell
rg -n "SdcardService|sdcard_service" d:/zephyrproject/myproject/sky_board_zephyr_demo/include d:/zephyrproject/myproject/sky_board_zephyr_demo/subsys
```

Expected: 无结果。

**Step 2: 创建头文件（接口）**

```cpp
#pragma once

#include "platform/ilogger.hpp"

namespace servers {

class SdcardService {
public:
    explicit SdcardService(platform::ILogger &log) : log_(log) {}
    int run() noexcept;

private:
    platform::ILogger &log_;
};

} // namespace servers
```

**Step 3: 创建源文件骨架（先可编译）**

```cpp
#include "servers/sdcard_service.hpp"

namespace servers {

int SdcardService::run() noexcept
{
    log_.info("sdcard service stub");
    return 0;
}

} // namespace servers
```

**Step 4: 构建验证骨架可编译**

Run:

```powershell
d:\zephyrproject\.venv\Scripts\west.exe build \
  -b lckfb_sky_board_stm32f407 \
  d:/zephyrproject/myproject/sky_board_zephyr_demo \
  -d d:/zephyrproject/myproject/sky_board_zephyr_demo/build/lckfb_sky_board_stm32f407 \
  -p auto
```

Expected: 通过（尚未集成调用）。

**Step 5: Commit**

```powershell
git -C d:/zephyrproject/myproject/sky_board_zephyr_demo add \
  include/servers/sdcard_service.hpp \
  subsys/servers/sdcard_service.cpp
git -C d:/zephyrproject/myproject/sky_board_zephyr_demo commit -m "feat(sd): add sdcard service skeleton"
```

---

### Task 4: 实现挂载 + 最小读写回读验证

**Files:**

- Modify: `myproject/sky_board_zephyr_demo/subsys/servers/sdcard_service.cpp`

**Step 1: 失败用例定义（验收前置）**

Run (after flash current image):

```text
串口观察 10s
```

Expected: 当前不存在 `[sd] mounted` / `[sd] readback ok` 日志（功能尚未完成）。

**Step 2: 实现最小可用逻辑**

Implement (核心结构):

```cpp
#include "servers/sdcard_service.hpp"

#include <ff.h>
#include <string.h>

#include <zephyr/fs/fs.h>
#include <zephyr/storage/disk_access.h>

namespace {

FATFS g_fat_fs;
fs_mount_t g_mp = {
    .type = FS_FATFS,
    .fs_data = &g_fat_fs,
    .storage_dev = (void *)"SD",
    .mnt_point = "/SD:",
};

} // namespace

namespace servers {

int SdcardService::run() noexcept
{
    int ret = disk_access_ioctl("SD", DISK_IOCTL_CTRL_INIT, nullptr);
    if (ret != 0) {
        log_.error("[sd] disk init failed", ret);
        return ret;
    }

    ret = fs_mount(&g_mp);
    if (ret != 0) {
        log_.error("[sd] mount failed", ret);
        return ret;
    }

    log_.info("[sd] mounted /SD:");

    fs_file_t file;
    fs_file_t_init(&file);

    ret = fs_open(&file, "/SD:/boot.txt", FS_O_CREATE | FS_O_WRITE | FS_O_APPEND);
    if (ret != 0) {
        log_.error("[sd] open write failed", ret);
        return ret;
    }

    static const char msg[] = "sky_board boot\n";
    ret = fs_write(&file, msg, sizeof(msg) - 1U);
    (void)fs_close(&file);
    if (ret < 0) {
        log_.error("[sd] write failed", ret);
        return ret;
    }

    fs_file_t_init(&file);
    ret = fs_open(&file, "/SD:/boot.txt", FS_O_READ);
    if (ret != 0) {
        log_.error("[sd] open read failed", ret);
        return ret;
    }

    char buf[32] = {0};
    ret = fs_read(&file, buf, sizeof(buf) - 1U);
    (void)fs_close(&file);
    if (ret < 0) {
        log_.error("[sd] read failed", ret);
        return ret;
    }

    log_.info("[sd] readback ok");
    return 0;
}

} // namespace servers
```

**Step 3: build 验证**

Run:

```powershell
d:\zephyrproject\.venv\Scripts\west.exe build \
  -b lckfb_sky_board_stm32f407 \
  d:/zephyrproject/myproject/sky_board_zephyr_demo \
  -d d:/zephyrproject/myproject/sky_board_zephyr_demo/build/lckfb_sky_board_stm32f407 \
  -p auto
```

Expected: 编译通过，无未定义符号。

**Step 4: flash + 串口验证**

Run:

```powershell
d:\zephyrproject\.venv\Scripts\west.exe flash \
  -d d:/zephyrproject/myproject/sky_board_zephyr_demo/build/lckfb_sky_board_stm32f407
```

Expected: 串口出现 `[sd] mounted /SD:` 与 `[sd] readback ok`（有卡 FAT 场景）。

**Step 5: Commit**

```powershell
git -C d:/zephyrproject/myproject/sky_board_zephyr_demo add subsys/servers/sdcard_service.cpp
git -C d:/zephyrproject/myproject/sky_board_zephyr_demo commit -m "feat(sd): mount fatfs and perform boot readback check"
```

---

### Task 5: 集成到应用启动链路

**Files:**

- Modify: `myproject/sky_board_zephyr_demo/CMakeLists.txt`
- Modify: `myproject/sky_board_zephyr_demo/app/app_Init.cpp`

**Step 1: 失败用例定义（未集成前）**

Run:

```powershell
rg -n "sdcard_service|SdcardService" d:/zephyrproject/myproject/sky_board_zephyr_demo/CMakeLists.txt d:/zephyrproject/myproject/sky_board_zephyr_demo/app/app_Init.cpp
```

Expected: 未命中。

**Step 2: 在 CMake 中注册源文件**

Add to `target_sources(app PRIVATE ...)`:

```cmake
subsys/servers/sdcard_service.cpp
```

**Step 3: 在 app_Init 启动服务**

Add include and boot call:

```cpp
#include "servers/sdcard_service.hpp"

static servers::SdcardService sdcard_service(platform::logger());
ret = sdcard_service.run();
if (ret < 0) {
    platform::logger().error("failed to start sdcard service", ret);
    return ret;
}
```

**Step 4: build + flash 验证集成路径**

Run:

```powershell
d:\zephyrproject\.venv\Scripts\west.exe build \
  -b lckfb_sky_board_stm32f407 \
  d:/zephyrproject/myproject/sky_board_zephyr_demo \
  -d d:/zephyrproject/myproject/sky_board_zephyr_demo/build/lckfb_sky_board_stm32f407 \
  -p auto

d:\zephyrproject\.venv\Scripts\west.exe flash \
  -d d:/zephyrproject/myproject/sky_board_zephyr_demo/build/lckfb_sky_board_stm32f407
```

Expected: 上电后自动执行 SD 挂载验证日志。

**Step 5: Commit**

```powershell
git -C d:/zephyrproject/myproject/sky_board_zephyr_demo add CMakeLists.txt app/app_Init.cpp
git -C d:/zephyrproject/myproject/sky_board_zephyr_demo commit -m "feat(app): integrate sdcard service into boot sequence"
```

---

### Task 6: 关键功能回归 + 文档同步

**Files:**

- Modify: `myproject/sky_board_zephyr_demo/docs/codex-runbook.md`
- Modify: `myproject/sky_board_zephyr_demo/docs/plans/2026-02-22-sdio-fatfs-design.md`（状态更新为已进入实施）

**Step 1: 执行回归验证清单**

Run/Check:

```text
1) 串口包含: [sd] mounted /SD:, [sd] readback ok
2) 串口包含: tcp service listening on port 8000
3) 串口包含: [time] RTC updated with Beijing time 或重试/失败日志
4) TCP echo: 向 8000 端口发送数据可原样回传
```

Expected: SD 成功时不影响 TCP/Time/Display 主流程。

**Step 2: 更新 runbook 日志关键字**

Add keywords:

```text
[sd] mounted /SD:
[sd] readback ok
```

**Step 3: 最终 build 证据（verification-before-completion）**

Run:

```powershell
d:\zephyrproject\.venv\Scripts\west.exe build \
  -b lckfb_sky_board_stm32f407 \
  d:/zephyrproject/myproject/sky_board_zephyr_demo \
  -d d:/zephyrproject/myproject/sky_board_zephyr_demo/build/lckfb_sky_board_stm32f407 \
  -p auto
```

Expected: 成功。

**Step 4: Commit**

```powershell
git -C d:/zephyrproject/myproject/sky_board_zephyr_demo add docs/codex-runbook.md docs/plans/2026-02-22-sdio-fatfs-design.md
git -C d:/zephyrproject/myproject/sky_board_zephyr_demo commit -m "docs: update runbook and mark sdio-fatfs implementation status"
```

---

## 执行注意事项

1. 若 `CD` 极性错误，优先只改 `cd-gpios` 的 ACTIVE_LOW/HIGH，不改代码逻辑。
1. 若无卡或坏卡，`SdcardService` 允许失败返回，但需明确日志且不影响主服务。
1. 不修改 Zephyr 主仓 `zephyr/boards/...` 同名板定义，避免双源漂移；本项目以 `myproject/.../boards/st/...` 为单一真源。

## 完成判定（DoD）

1. `west build` 通过。
1. 启动日志出现 SD 挂载与读写回读成功（有卡 FAT 场景）。
1. TCP 8000 回传与时间服务行为不回退。
1. Runbook 已更新 SD 关键日志项。
