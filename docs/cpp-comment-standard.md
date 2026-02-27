# C++ 注释规范

## 1. 目标

统一 `include/`, `app/`, `subsys/` 的 C++ 注释风格, 提高可读性和可维护性.

## 2. 基本规则

- 所有 `.hpp/.cpp` 文件必须有文件头注释.
- 公开接口, 线程入口, 状态机, 时序敏感逻辑必须有函数级注释.
- 注释描述 "为什么" 和 "约束", 不重复代码字面含义.
- 注释与实现同步更新, 禁止过期注释.
- 文档注释使用 Doxygen 风格 `/** ... */`.

## 3. 文件头模板

```cpp
/**
 * @file xxx.hpp
 * @brief 文件职责一句话说明.
 */
```

## 4. 函数注释模板

```cpp
/**
 * @brief 函数职责.
 * @param a 参数含义.
 * @param b 参数含义.
 * @return 返回值语义.
 * @note 调用约束, 时序要求, 线程安全说明.
 */
```

## 5. 成员注释要求

- 类成员需要说明用途和生命周期.
- 线程对象, 锁, 原子变量需要说明并发语义.
- 固定参数常量需要说明单位, 例如 ms, Hz, mA.

示例:

```cpp
/** @brief 采样周期, 单位毫秒. */
static constexpr int64_t kSamplePeriodMs = 20;
```

## 6. 推荐范围

- `include/` 下所有公共头文件: 全覆盖.
- `subsys/servers/`: 服务线程主循环和回调链路全覆盖.
- `subsys/platform/`: 设备初始化, 读写流程, 错误处理全覆盖.
- `app/`: 初始化主链路和主循环全覆盖.

## 7. 禁止项

- 禁止空泛注释, 如 "设置变量", "调用函数".
- 禁止与代码不一致的历史注释.
- 禁止大段注释解释显而易见的语句.

## 8. 落地检查

建议在提交前检查:

```powershell
cd d:/zephyrproject/myproject/sky_board_zephyr_demo
rg -n "@file|@brief" include app subsys
```

如新增文件缺少 `@file/@brief`, 先补注释再提交.
