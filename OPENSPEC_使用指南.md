# OpenSpec 使用指南（快速上手）

本文只覆盖“如何使用”，不包含介绍与安装内容。

## 1. 初始化到你的项目

进入你的项目目录后执行：

```bash
openspec init
```

如果你要非交互式指定 AI 工具：

```bash
openspec init --tools codex,claude
```

可用 `--tools` 值支持：

- `all`
- `none`
- 或逗号分隔的工具列表（如 `codex,claude,cursor`）

常用参数：

- `--tools <tools>`: 非交互配置 AI 工具
- `--force`: 自动清理 legacy 文件（无提示）
- `--profile <profile>`: 指定配置 profile

## 2. 推荐工作流（从想法到归档）

### 第一步：创建变更

```bash
openspec new change <change-name>
```

示例：

```bash
openspec new change add-user-audit-log
```

### 第二步：查看和管理变更

```bash
openspec list
openspec change show <change-name>
openspec status --change <change-name>
```

### 第三步：校验规范

```bash
openspec validate --changes
openspec validate --specs
# 或全部校验
openspec validate --all
```

常用增强参数：

- `--strict`: 严格校验
- `--json`: 机器可读输出
- `--no-interactive`: 禁用交互

### 第四步：归档已完成变更

```bash
openspec archive <change-name>
```

常用参数：

- `-y, --yes`: 跳过确认
- `--skip-specs`: 跳过 spec 更新（适合文档/工具类改动）
- `--no-validate`: 跳过验证（不推荐）

## 3. 常用命令速查

### 基础命令

```bash
openspec --help
openspec --version
```

### 项目初始化与更新

```bash
openspec init
openspec update
```

- `openspec update --force`: 强制更新指令文件

### 列表与查看

```bash
openspec list
openspec list --specs
openspec show <item-name>
openspec spec list
openspec spec show <spec-id>
```

### 变更管理

```bash
openspec new change <name>
openspec change show <name>
openspec change validate <name>
openspec archive <name>
```

### 配置管理

```bash
openspec config path
openspec config list
openspec config get <key>
openspec config set <key> <value>
openspec config unset <key>
openspec config profile
```

### 诊断与自动化

```bash
openspec status --change <id>
openspec instructions --change <id>
openspec schemas
openspec validate --json
```

## 4. 一个最小可执行示例

```bash
# 1) 在项目内初始化
openspec init --tools codex

# 2) 新建一个变更
openspec new change add-login-rate-limit

# 3) 查看当前变更状态
openspec status --change add-login-rate-limit

# 4) 执行校验
openspec validate --all

# 5) 完成后归档
openspec archive add-login-rate-limit -y
```

## 5. 常见问题

### Q1：升级后项目里的指令没有同步

在项目目录执行：

```bash
openspec update
```

### Q2：校验失败怎么排查

优先执行：

```bash
openspec validate --all --strict
```

再用：

```bash
openspec show <item-name>
```

定位具体条目。

## 6. 建议实践

- 每个需求独立一个 change，避免混合多个目标。
- 变更过程中频繁使用 `openspec status` 和 `openspec validate`。
- 合并前至少执行一次 `openspec validate --all`。
- 升级 CLI 后，在每个项目里跑一次 `openspec update`。
