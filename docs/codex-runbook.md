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

## 8. 维护约定

- 当板卡、构建目录、west 路径发生变化时，第一时间更新此文件。
- 优先保持命令可直接复制执行，不写模糊路径。

## 9. 文档格式化与规范（Google 风格落地）

### 9.0 文档标点规则

- 文档统一使用英文半角标点，不使用中文全角符号。
- 禁用示例：`，。；：、“”‘’（）【】《》？！、`
- 适用范围：`docs/**/*.md`、`README.rst` 及项目根目录 Markdown 文档。

快速检查命令：

```powershell
cd d:/zephyrproject/myproject/sky_board_zephyr_demo
rg -n "[，。；：、“”‘’（）【】《》？！、]" docs README.rst *.md
```

若命中结果非空，则需要先修正文档标点后再提交。

### 9.1 首次安装工具

```powershell
cd d:/zephyrproject/myproject/sky_board_zephyr_demo
npm install
```

### 9.2 自动格式化文档

```powershell
cd d:/zephyrproject/myproject/sky_board_zephyr_demo
npm run docs:format
```

### 9.3 规范检查

```powershell
cd d:/zephyrproject/myproject/sky_board_zephyr_demo
npm run docs:lint
```

### 9.4 提交前检查（推荐）

```powershell
cd d:/zephyrproject/myproject/sky_board_zephyr_demo
npm run docs:check
```

### 9.5 已配置文件

- `package.json`: 文档格式化/检查脚本
- `.prettierrc.json`: Markdown 自动换行与宽度规则
- `.markdownlint.json`: markdownlint 规则（含 `MD029: one`）

## 10. C++ 格式规范（Google）

### 10.1 规则文件

- 项目根目录 `.clang-format` 已配置为 `BasedOnStyle: Google`（C++17）。

### 10.2 格式化全部 C++ 文件

```powershell
cd d:/zephyrproject/myproject/sky_board_zephyr_demo
Get-ChildItem app,include,subsys,tests -Recurse -Include *.cpp,*.hpp -File | `
  ForEach-Object { & "D:/LLVM/bin/clang-format.exe" -i $_.FullName }
```

### 10.3 检查格式（不改文件）

```powershell
cd d:/zephyrproject/myproject/sky_board_zephyr_demo
Get-ChildItem app,include,subsys,tests -Recurse -Include *.cpp,*.hpp -File | `
  ForEach-Object { & "D:/LLVM/bin/clang-format.exe" --dry-run --Werror $_.FullName }
```

### 10.4 推荐流程

1. 改代码后先执行 10.2 自动格式化。
1. 再执行 10.3 确认无格式违规。
1. 最后执行 `west build` 做编译验证。

## 11. 常用请求模板

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

```text
按 docs/codex-runbook.md 执行 build + flash + 串口检查，并按关键日志清单给结论
```

```text
按 docs/codex-runbook.md 先执行 C++ Google 格式化，再 build 验证
```

```text
按 docs/codex-runbook.md 执行文档格式化与 lint（docs:format + docs:lint）
```

```text
按 docs/codex-runbook.md 检查文档是否含中文全角标点，并修复后再 lint
```

```text
更新 README.rst 到当前实现状态，并确保不使用中文全角符号
```

```text
按 docs/传感器接入指南.md 接入 <传感器名>，完成 DTS + Kconfig + 驱动注册 + build 验证
```

```text
基于 SensorHub 新增 <传感器名> 驱动，保持 SensorService 无需改采样主循环
```

```text
按 docs/codex-runbook.md 执行一次完整发布流程：build -> flash -> 日志验证 -> commit -> push -> tag
```

```text
按 docs/codex-runbook.md 做回归检查：TCP(8000)、TimeService、platform::storage、SensorService
```
