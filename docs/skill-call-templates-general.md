# Skill 调用模板（通用版）

## 1) 方案设计（先澄清再做）

```text
请使用 brainstorming，先澄清需求并给出可选方案，不要直接改代码。
需求：<你的需求>
约束：<资源/时间/兼容性>
```

## 2) 写实施计划

```text
请使用 writing-plans，基于以下需求输出分步骤实施计划（含风险与验证点），先不要改代码。
需求：<你的需求>
```

## 3) 按计划执行

```text
请使用 executing-plans，执行这份计划并在关键检查点汇报进度。
计划：<粘贴计划内容>
```

## 4) 新功能开发（TDD）

```text
请使用 test-driven-development，实现以下功能：<功能描述>。
要求：先写失败测试，再实现，再回归验证。
```

## 5) Bug 排查

```text
请使用 systematic-debugging，定位并修复这个问题。
现象：<日志/报错>
复现步骤：<步骤>
期望结果：<期望>
```

## 6) 并行拆分任务

```text
请使用 dispatching-parallel-agents，把这个需求拆成可并行子任务并并行推进。
需求：<大需求描述>
```

## 7) 子代理执行开发

```text
请使用 subagent-driven-development，按子任务分工实现并集成结果。
任务清单：<子任务列表>
```

## 8) 完成前强校验

```text
请使用 verification-before-completion，先运行必要验证并给出证据，再声明完成。
验证范围：<构建/测试/静态检查>
```

## 9) 发起代码评审

```text
请使用 requesting-code-review，对本次改动做正式代码评审，按严重级别列出问题和建议。
```

## 10) 处理评审意见

```text
请使用 receiving-code-review，逐条分析以下评审意见，区分“应修改/可讨论/拒绝并说明理由”。
评审意见：<粘贴内容>
```

## 11) 分支收尾

```text
请使用 finishing-a-development-branch，基于当前状态给出合并/PR/清理的最佳收尾方案。
```

## 12) 新开隔离工作树

```text
请使用 using-git-worktrees，为这个功能创建隔离 worktree 并开始开发。
功能：<功能名>
```
