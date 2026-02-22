# Skill 调用模板（Zephyr/嵌入式专用）

## 1) 板级需求澄清与技术方案

```text
请使用 brainstorming，先给出 Zephyr 板级开发方案，不要直接改代码。
目标板：<board/soc>
需求：<功能>
约束：<RAM/FLASH/功耗/外设>
```

## 2) DTS/Overlay 设计与改动计划

```text
请使用 writing-plans，输出 devicetree 修改计划（dts/overlay/pinctrl/kconfig 影响分析）。
目标：<如 ETH/RTC/SPI/LCD>
现状文件：<路径列表>
```

## 3) 执行 DTS/驱动接入计划

```text
请使用 executing-plans，按计划完成 Zephyr 外设接入并分阶段汇报。
计划：<粘贴计划>
```

## 4) 外设驱动功能开发（TDD）

```text
请使用 test-driven-development，实现 <驱动/服务名>。
要求：先写失败测试（或最小可验证用例），再实现，再回归。
```

## 5) 网络问题系统化排查（ETH/TCP/DHCP）

```text
请使用 systematic-debugging，排查 Zephyr 网络异常。
现象：<日志>
场景：<DHCP/TCP连接失败/丢包>
期望：<应有行为>
```

## 6) HardFault/UsageFault 排查

```text
请使用 systematic-debugging，定位并修复 MCU fault。
错误：<寄存器日志/pc/lr/sp>
构建产物：<elf路径>
目标：给出根因与最小修复方案。
```

## 7) 资源优化（RAM/FLASH）

```text
请使用 brainstorming，给出 Zephyr 工程 RAM/FLASH 优化方案（2-3种）并推荐。
当前占用：<贴内存报告>
要求：优先不影响核心功能。
```

## 8) 多任务并行改造（可并行子任务）

```text
请使用 dispatching-parallel-agents，将以下嵌入式改造拆分并并行推进。
任务：<如 dts整理/服务拆分/日志改造/文档补全>
```

## 9) 子代理并行实施

```text
请使用 subagent-driven-development，按子任务实现并集成。
子任务清单：<列表>
验收标准：<列表>
```

## 10) 完成前验证（构建+运行证据）

```text
请使用 verification-before-completion，在宣告完成前执行并展示证据。
验证项：
1) west build
2) 烧录/启动日志
3) 关键功能回归（<列表>）
```

## 11) 请求正式代码评审

```text
请使用 requesting-code-review，对本次 Zephyr 改动做评审。
关注点：行为回归、并发安全、内存/栈风险、配置一致性。
```

## 12) 处理评审意见并回归

```text
请使用 receiving-code-review，逐条处理以下评审意见并说明采纳理由。
评审内容：<粘贴>
要求：修改后给出回归验证结果。
```
