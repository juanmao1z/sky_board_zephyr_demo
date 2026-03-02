# OpenSpec 给 Codex 的可复制命令模板

本文只写“可以直接粘贴给 Codex 的文本”。

## 0. 先创建一个新 change（openspec new）

把下面整段发给 Codex：

```text
请先创建一个新的 OpenSpec change：<CHANGE_NAME>

要求：
1) 运行 openspec new change <CHANGE_NAME>
2) 运行 openspec status --change "<CHANGE_NAME>" --json
3) 输出新建结果和下一步建议
```

手动命令示例：

```bash
openspec new change add-boot-counter
```

## 1. 让 Codex 执行某个 change（最常用）

把下面整段发给 Codex：

```text
请按 OpenSpec 工作流实现 change：<CHANGE_NAME>

要求：
1) 先运行 openspec status --change "<CHANGE_NAME>" --json
2) 再运行 openspec instructions apply --change "<CHANGE_NAME>" --json
3) 读取 contextFiles
4) 按 tasks 逐项实现，完成一项就把 tasks.md 对应项打勾
5) 如果遇到阻塞，先汇报阻塞点和建议方案
6) 最后汇报：本次完成项、剩余项、下一步建议
```

## 2. 不知道 change 名字时

把下面整段发给 Codex：

```text
请先帮我选定要执行的 OpenSpec change：
1) 运行 openspec list --json
2) 如果只有一个 active change，就直接使用它
3) 如果有多个，请列出候选并让我选择
4) 选定后继续执行 apply 流程
```

## 3. 继续上次未做完的实现

把下面整段发给 Codex：

```text
继续上次的 OpenSpec change：<CHANGE_NAME>

要求：
1) 运行 openspec status --change "<CHANGE_NAME>" --json
2) 运行 openspec instructions apply --change "<CHANGE_NAME>" --json
3) 只做未完成任务
4) 每完成一项立刻更新 tasks.md 勾选状态
5) 输出最新进度（完成/总数）
```

## 4. 只做校验，不改代码

把下面整段发给 Codex：

```text
请仅做 OpenSpec 校验，不修改任何代码：
1) openspec validate --all --strict
2) 如果失败，按错误逐条解释原因和修复建议
3) 给出可复制的修复命令
```

## 5. 归档已完成 change

把下面整段发给 Codex：

```text
请归档 OpenSpec change：<CHANGE_NAME>

步骤：
1) 先确认任务是否已全部完成（status + tasks）
2) 运行 openspec archive "<CHANGE_NAME>" -y
3) 输出归档结果和关键日志
```

## 6. CLI 原生命令速查（手动执行用）

```bash
openspec new change <CHANGE_NAME>
openspec list --json
openspec status --change "<CHANGE_NAME>" --json
openspec instructions apply --change "<CHANGE_NAME>" --json
openspec validate --all --strict
openspec archive "<CHANGE_NAME>" -y
```

## 7. 说明

- OpenSpec CLI 没有一级 `apply` 命令。
- 正确写法是：`openspec instructions apply --change "<CHANGE_NAME>" --json`
- `/opsx:apply` 这类写法属于外层代理快捷指令，不是 OpenSpec CLI 原生命令。
