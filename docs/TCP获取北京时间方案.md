# ETH TCP 获取北京时间并串口打印实施方案

## 1. 功能目标
- 设备上电后，通过以太网 `TCP/HTTP` 从固定地址 `1.1.1.1:80` 获取服务器响应头中的 `Date`（UTC）时间。
- 将 UTC 时间换算为北京时间（`UTC+8`）并通过串口打印。
- 保持现有 `tcp_service` 在 `8000` 端口的 TCP 回传（echo）功能不受影响。

## 2. 设计约束
- 时间源固定为 `1.1.1.1:80`，不使用 DNS。
- 仅在启动阶段探测一次，不做周期同步。
- 失败不阻塞系统，不影响 `8000` 回传服务。
- 北京时间按固定 `UTC+8` 计算，不处理夏令时。
- 串口打印优先使用 `printk`，保证调试可见性。

## 3. 时序流程
1. 系统启动后，`TcpService` 线程进入主循环。
2. 每轮循环调用一次“时间探测门控”逻辑。
3. 若 IPv4 未就绪：
   - 在 30 秒窗口内继续等待；
   - 超过 30 秒则打印“跳过时间同步”，并标记本次探测结束。
4. 若 IPv4 已就绪且尚未探测：
   - 建立到 `1.1.1.1:80` 的 TCP 连接；
   - 发送 `HEAD / HTTP/1.1` 请求；
   - 接收响应头直到 `\r\n\r\n` 或超时；
   - 解析 `Date:` 头并转为 UTC epoch；
   - 加 `8*3600` 转为北京时间并格式化打印。
5. 无论成功或失败，都将探测标志置为完成，不再重复执行。
6. `tcp_service` 持续监听 `0.0.0.0:8000` 并执行回传。

## 4. 实现清单
### 4.1 代码改动
- `include/servers/tcp_service.hpp`
  - 新增一次性探测状态字段：
    - `time_probe_done_`
    - `time_probe_deadline_ms_`
  - 新增私有函数声明：
    - `maybe_probe_beijing_time_once()`
    - `is_ipv4_ready()`
    - `fetch_utc_epoch_from_http_date()`
    - `extract_http_date_header()`
    - `parse_rfc7231_date_to_epoch()`
    - `print_beijing_time()`

- `subsys/servers/tcp_service.cpp`
  - 在服务主循环中增加一次性探测调用。
  - 增加 HTTP Date 抓取与解析逻辑：
    - TCP 连接、发送请求、读取响应头。
    - 解析 `Date` 字段为 `struct tm`。
    - 使用 `timeutil_timegm()` 转 UTC epoch。
    - 转换北京时间并 `printk` 输出。
  - 失败处理全部非致命化（仅日志，不中断服务）。

### 4.2 配置变更
- `prj.conf` 保持不变。
- 当前能力依赖已存在配置：`CONFIG_NET_TCP=y`、`CONFIG_NET_SOCKETS=y`。

## 5. 失败与降级策略
- TCP 连接失败：打印失败原因，标记探测完成，继续提供 `8000` 服务。
- 发送或接收超时：打印失败原因，标记探测完成。
- 响应头缺失 `Date`：打印解析失败，标记探测完成。
- `Date` 格式异常：打印解析失败，标记探测完成。
- IPv4 超时未就绪（30 秒）：打印跳过信息，标记探测完成。

## 6. 测试步骤与预期日志
### 6.1 正常联网场景
1. 上电启动，确保 DHCP 可获取 IPv4。
2. 观察串口日志应出现一次：
   - `"[time] Beijing: YYYY-MM-DD HH:MM:SS (UTC+8)"`
3. 使用 TCP 客户端连接板端 `8000`，发送测试字符串。
4. 期望收到原样回传。

### 6.2 DHCP 未就绪或无网场景
1. 上电后不接入可用网络。
2. 约 30 秒后串口应出现：
   - `"[time] Beijing sync skipped: IPv4 not ready within 30s"`
3. `8000` 服务仍可在网络恢复后正常工作。

### 6.3 HTTP 异常场景
1. 网络可达但响应异常（无 `Date` 或格式异常）。
2. 串口应出现一次失败日志：
   - `"[time] Beijing sync failed: err=..."`
3. 系统不崩溃、不重启，`8000` 服务持续可用。

### 6.4 回归场景
- 检查显示初始化、背光设置、以太网启动、其他服务行为无回退。

## 7. 构建验证
- 参考命令：
  - `west build -b lckfb_sky_board_stm32f407 ... -p auto`
- 验收要求：
  1. 编译链接通过。
  2. 启动后时间探测日志最多一次（成功或失败）。
  3. `8000` TCP 回传功能正常。
