# Codex 项目约定（Runbook）

## 1. 目的

统一本项目的构建、烧录、日志与协作约定，减少重复确认，提高 Codex 执行效率。

## 2. 固定工程信息

- 工作区根目录：`d:\zephyrproject`
- 应用目录：`d:\zephyrproject\myproject\sky_board_zephyr_demo`
- 目标板：`lckfb_sky_board_stm32f407`
- 默认构建目录：`d:\zephyrproject\myproject\sky_board_zephyr_demo\build\lckfb_sky_board_stm32f407`
- West 可执行文件：`d:\zephyrproject\.venv\Scripts\west.exe`
- Git 远程仓库：`origin https://github.com/juanmao1z/sky_board_zephyr_demo.git`

## 3. 默认提交与推送命令

### 3.1 查看状态与远程

```powershell
git -C d:/zephyrproject/myproject/sky_board_zephyr_demo status --short --branch
git -C d:/zephyrproject/myproject/sky_board_zephyr_demo remote -v
```

### 3.2 提交并推送到 main

```powershell
git -C d:/zephyrproject/myproject/sky_board_zephyr_demo add -A
git -C d:/zephyrproject/myproject/sky_board_zephyr_demo commit -m "<提交标题>" -m "<提交说明>"
git -C d:/zephyrproject/myproject/sky_board_zephyr_demo push origin main
```

### 3.3 创建并推送标签（可选）

```powershell
git -C d:/zephyrproject/myproject/sky_board_zephyr_demo tag -a <tag-name> -m "<tag标题>" -m "<详细说明>"
git -C d:/zephyrproject/myproject/sky_board_zephyr_demo push origin <tag-name>
```

## 4. 默认构建命令

```powershell
d:\zephyrproject\.venv\Scripts\west.exe build `
  -b lckfb_sky_board_stm32f407 `
  d:/zephyrproject/myproject/sky_board_zephyr_demo `
  -d d:/zephyrproject/myproject/sky_board_zephyr_demo/build/lckfb_sky_board_stm32f407 `
  -p auto
```

## 5. 默认烧录命令

```powershell
d:\zephyrproject\.venv\Scripts\west.exe flash `
  -d d:/zephyrproject/myproject/sky_board_zephyr_demo/build/lckfb_sky_board_stm32f407
```

## 6. 日志观察约定

- 目标串口波特率默认：`115200`
- 重点日志关键字：
  - `tcp service listening on port 8000`
  - `[time] Beijing`
  - `RTC updated with Beijing time`

## 7. Codex 执行规则（本项目）

- 默认使用本文件中的固定路径与命令，不重复询问 `west` 用法。
- 若命令失败，先按“路径/环境/设备连接”顺序排查，再回报错误与修复动作。
- 宣告“完成/修复”前，至少执行一次构建验证并给出结果摘要。
- 涉及功能变更时，优先同步更新 `docs/` 下对应文档。

## 8. 常用请求模板

```text
按 docs/codex-runbook.md 执行 build
```

```text
按 docs/codex-runbook.md 执行 build + flash，并给出串口关键日志检查项
```

```text
按 docs/codex-runbook.md 排查构建失败，先给根因再给修复
```

```text
按 docs/codex-runbook.md 执行提交并推送到 origin/main，提交信息由你生成
```

```text
按 docs/codex-runbook.md 执行提交+推送+打标签，标签写详细说明
```

## 9. 维护约定

- 当板卡、构建目录、west 路径发生变化时，第一时间更新此文件。
- 优先保持命令可直接复制执行，不写模糊路径。
