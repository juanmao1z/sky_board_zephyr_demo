# Zephyr大型项目架构快速上手指南（C++版）

> 目标：在 Zephyr 中使用 C++ 面向对象思想，但**不使用 `iostream`、异常、RTTI**，并保持高可维护、低资源占用、快上手。

---

## 1. 先定三条硬约束

本项目约束：

1. 禁用 `iostream`（不用 `std::cout/std::cin`）  
2. 禁用异常（`throw/try/catch`）  
3. 禁用 RTTI（`dynamic_cast/typeid`）

推荐理由：

1. 显著降低 Flash/RAM 占用  
2. 减少不可预测运行时开销  
3. 让错误路径、实时行为更可控

---

## 2. 对应的 Zephyr 配置（可直接抄）

`prj.conf` 建议：

```conf
CONFIG_CPP=y
CONFIG_STD_CPP17=y

# 关键：异常与 RTTI 关闭
# CONFIG_CPP_EXCEPTIONS is not set
# CONFIG_CPP_RTTI is not set

# 建议：保持轻量 C++ 库
CONFIG_MINIMAL_LIBCPP=y

# 日志
CONFIG_LOG=y
CONFIG_LOG_MODE_DEFERRED=y
CONFIG_LOG_DEFAULT_LEVEL=3
```

说明：

1. 在 Zephyr 中，只要不启用 `CONFIG_CPP_EXCEPTIONS/CONFIG_CPP_RTTI`，构建系统会为 C++ 自动加入禁用标志。  
2. 你仍可在 CMake 再加一层“防误开”保护（见下节）。

---

## 3. CMake 防误开保护（建议）

在应用 `CMakeLists.txt` 增加：

```cmake
target_compile_options(app PRIVATE
  $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
  $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
  $<$<COMPILE_LANGUAGE:CXX>:-fno-threadsafe-statics>
)
```

说明：

1. `-fno-threadsafe-statics` 可减少静态局部对象初始化的额外开销（按项目需求决定）。  
2. `-fno-exceptions/-fno-rtti` 与 Kconfig 形成双保险。

---

## 4. 架构分层（面向对象但贴合 Zephyr）

推荐 4 层：

1. **BSP层**：DTS/overlay、引脚、时钟、board 差异  
2. **Platform层**：GPIO/UART/SPI 封装、存储、时间、通信  
3. **Domain层**：业务对象、状态机、策略  
4. **App层**：系统启动、线程调度、模块装配

依赖方向：

1. `App -> Domain -> Platform -> BSP`  
2. 禁止反向依赖（比如 Domain 直接 include board 私有头）

---

## 5. 目录模板（C++工程化）

```text
threads/
├─ app/
│  ├─ main.cpp
│  ├─ app_Init.cpp
│  └─ app_threads.cpp
├─ include/
│  ├─ platform/
│  │  ├─ igpio.hpp
│  │  └─ ilogger.hpp
│  ├─ servers/
│  │  └─ led_blink_service.hpp
│  └─ app/
│     └─ app_context.hpp
├─ subsys/
│  ├─ platform/
│  │  ├─ zephyr_gpio.cpp
│  │  └─ zephyr_logger.cpp
│  └─ servers/
│     └─ led_blink_service.cpp
├─ conf/
│  ├─ prj_debug.conf
│  └─ prj_release.conf
├─ prj.conf
└─ CMakeLists.txt
```

---

## 6. 编码规则（核心）

### 6.1 错误处理规则（替代异常）

1. 接口返回 `int`（0 成功，负值失败）  
2. 关键函数返回错误码并写日志  
3. 不在构造函数做复杂失败逻辑（构造尽量 `noexcept`，复杂初始化放 `init()`）

### 6.2 多态规则（禁 RTTI 仍可 OOP）

可以使用：

1. 抽象接口（纯虚类）  
2. 虚函数分发

禁止使用：

1. `dynamic_cast`  
2. `typeid`

### 6.3 日志规则（替代 iostream）

统一使用：

1. `LOG_INF/LOG_ERR`（优先）  
2. `printk`（早期启动或极简路径）  
3. `printf`（需要 libc 格式化时）

### 6.4 注释规则（标准化，强制执行）

总体原则：

1. 注释写“意图/约束/副作用”，不要只复述代码字面意思  
2. 对外接口（`include/`）必须有标准注释块  
3. 线程函数、硬件操作、状态机分支必须写关键注释  
4. 修改代码时，同步更新注释，禁止注释与实现不一致

文件头注释模板：

```cpp
/**
 * @file hello_service.cpp
 * @brief 心跳服务实现：周期日志 + LED 翻转
 * @note 运行于 Zephyr 线程上下文，不在 ISR 中调用
 */
```

类/函数注释模板：

```cpp
/**
 * @brief 启动服务线程（幂等）
 * @return 0 成功或已在运行；负值表示失败
 */
int run() noexcept;

/**
 * @brief 请求停止服务线程
 * @note 本函数仅设置停止标志并唤醒线程，非阻塞等待
 */
void stop() noexcept;
```

参数/返回值规则：

1. 有参数必须写 `@param`  
2. 有返回值必须写 `@return` 或 `@retval`  
3. 可能失败的接口必须说明失败条件

---

## 7. 并发模型（适合大型项目）

建议：

1. 少量长期线程（2~4 个）  
2. 大量短任务用 `k_work`  
3. 模块间通信用 `k_msgq`/`k_fifo`/`zbus`

示例分工：

1. `io_thread`：外设收发  
2. `logic_thread`：业务事件处理  
3. `monitor_thread`：状态/健康监控

ISR 规则：

1. ISR 只投递事件，不做复杂业务

---

## 8. C++接口模板（无异常/无RTTI）

```cpp
// include/platform/ilogger.hpp
#pragma once
#include <cstdint>

class ILogger {
public:
  virtual ~ILogger() = default;
  virtual void info(const char* msg) = 0;
  virtual void error(const char* msg, int err) = 0;
};
```

```cpp
// include/platform/igpio.hpp
#pragma once
#include <cstdint>

class IGpio {
public:
  virtual ~IGpio() = default;
  virtual int init() = 0;
  virtual int set(uint8_t id, bool on) = 0;
};
```

```cpp
// include/servers/led_blink_service.hpp
#pragma once
#include "platform/igpio.hpp"
#include "platform/ilogger.hpp"

class LedBlinkService {
public:
  LedBlinkService(IGpio& gpio, ILogger& log) : gpio_(gpio), log_(log) {}
  int init() noexcept;
  int tick() noexcept;   // 周期调用，不抛异常

private:
  IGpio& gpio_;
  ILogger& log_;
  uint32_t cnt_{0};
};
```

---

## 9. 日志实现示例（不用 iostream）

```cpp
// subsys/platform/zephyr_logger.cpp
#include <zephyr/logging/log.h>
#include "platform/ilogger.hpp"

LOG_MODULE_REGISTER(app_log, LOG_LEVEL_INF);

class ZephyrLogger final : public ILogger {
public:
  void info(const char* msg) override {
    LOG_INF("%s", msg);
  }

  void error(const char* msg, int err) override {
    LOG_ERR("%s err=%d", msg, err);
  }
};
```

---

## 10. 从你当前 `threads` 迁移的最短路径

### 第一步：语言切换

1. `main.c -> main.cpp`  
2. 保留 Zephyr C API 调用（正常做法）

### 第二步：把示例函数收进类

1. `blink()` 迁移到 `LedBlinkService`  
2. `uart_out()` 迁移到 `TelemetryService`

### 第三步：线程变“调度器”

1. 线程函数只做取事件 + 调服务对象  
2. 业务逻辑不直接写在线程函数里

### 第四步：固化禁用项

1. `prj.conf` 固定禁用异常/RTTI  
2. CMake 增加 compile options 双保险

---

## 11. 常见坑与规避

1. **误引入 `<iostream>`**：IDE 能补全不代表适合 MCU。  
2. **在构造函数里做复杂失败流程**：改成 `init()` 返回错误码。  
3. **使用 `dynamic_cast` 解决设计问题**：改接口拆分或状态机分派。  
4. **滥用动态内存**：优先静态对象、对象池、`k_mem_slab`。  
5. **线程太多**：先收敛为“少线程 + 消息队列”。

---

## 12. 质量检查清单（提交前）

1. 编译命令包含 `-fno-exceptions -fno-rtti`  
2. 项目中无 `iostream` / `throw` / `typeid` / `dynamic_cast`  
3. 模块边界清晰（接口在 `include/`，实现在 `subsys/`）  
4. 关键路径有日志和错误码  
5. 注释符合 6.4 规范（接口、线程、硬件路径）  
6. RAM/Flash 与上版本有对比记录

---

## 13. 一页结论

你可以在 Zephyr 里放心使用 C++ 的面向对象思想。  
正确做法不是“回到纯 C”，而是：

1. 用 C++ 做架构和抽象  
2. 用 Zephyr C API 做底层调用  
3. 明确禁用 `iostream`、异常、RTTI  
4. 以事件驱动 + 少线程模型保证可维护与实时性

这套方式兼顾了**工程质量、资源占用和团队可协作性**。
