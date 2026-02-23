Sky Board Zephyr Demo (C++)
###########################

项目概述
========

`sky_board_zephyr_demo` 是一个基于 Zephyr 的 C++ 固件项目, 目标板为
`lckfb_sky_board_stm32f407`. 项目采用分层架构, 运行显示, 网络, 时间同步,
SD 卡和传感器等多个服务.


当前功能
========

- 启用 C++17(禁用异常与 RTTI)
- 显示初始化与开机画面
- 以太网启动与 DHCPv4
- TCP 服务(端口 `8000`)
- 时间服务:
  - 通过 HTTP 获取 UTC 时间
  - 转换为北京时间(UTC+8)
  - 写入 RTC 并切换日志时间戳
- SD 卡服务(SDIO + FATFS)
- 泛化传感器框架:
  - `SensorHub` 支持注册 N 个驱动
  - `SensorService` 按注册表统一轮询采样
  - 内置传感器:
    - INA226(地址 `0x40`)
    - AHT20(地址 `0x38`)


目录结构
========

- `app/`: 应用启动与服务编排
- `include/`: 接口与抽象定义
- `subsys/`: 平台与服务实现
- `boards/`: 板级 DTS 配置
- `docs/`: runbook, 设计文档与接入指南


构建与烧录
==========

推荐命令以 `docs/codex-runbook.md` 为准.

快速构建:

.. code-block:: powershell

  d:\zephyrproject\.venv\Scripts\west.exe build `
    -b lckfb_sky_board_stm32f407 `
    d:/zephyrproject/myproject/sky_board_zephyr_demo `
    -d d:/zephyrproject/myproject/sky_board_zephyr_demo/build/lckfb_sky_board_stm32f407 `
    -p auto

快速烧录:

.. code-block:: powershell

  d:\zephyrproject\.venv\Scripts\west.exe flash `
    -d d:/zephyrproject/myproject/sky_board_zephyr_demo/build/lckfb_sky_board_stm32f407


关键日志
========

启动后可在串口(`115200`)观察以下关键日志:

- `display boot screen ready`
- `ethernet dhcpv4 started`
- `tcp service listening on port 8000`
- `[time] Beijing: ...`
- `[sd] mounted /SD:`
- `[sensor] ...`


传感器扩展
==========

新增传感器(如 BH1750, SHT4x)请参考:

- `docs/传感器接入指南.md`

该文档覆盖 DTS, Kconfig, `SensorHub` 驱动注册和 `SensorService` 验证流程.


格式化与规范
============

C++ 格式规范:

- 使用 `.clang-format`(Google 风格)
- 见 `docs/codex-runbook.md` 中"C++ 格式规范(Google)"章节

Markdown 文档规范:

- 使用 `prettier` + `markdownlint-cli2`
- 配置文件:
  - `.prettierrc.json`
  - `.markdownlint.json`


注意事项
========

- 板级 DTS 在 Zephyr 板仓库与项目板级目录中都可能存在, 修改硬件节点时需保持一致.
- 推送代码前请至少执行一次构建验证.

