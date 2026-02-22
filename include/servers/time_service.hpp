/**
 * @file time_service.hpp
 * @brief 北京时间获取服务声明：启动阶段通过 TCP/HTTP 获取时间并打印串口。
 */

#pragma once

#include "platform/ilogger.hpp"

#include <time.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>

namespace servers {

/**
 * @brief 北京时间获取服务。
 * @note 该服务在独立线程中运行，失败后延迟重试，最多尝试固定次数后退出。
 */
class TimeService {
public:
	/**
	 * @brief 构造时间服务。
	 * @param log 日志接口引用，必须在服务生命周期内保持有效。
	 */
	explicit TimeService(platform::ILogger &log) : log_(log) {}

	/**
	 * @brief 启动服务线程（幂等）。
	 * @return 0 表示成功或已在运行；负值表示失败。
	 */
	int run() noexcept;

	/**
	 * @brief 请求停止服务线程。
	 * @note 仅发出停止请求，不阻塞等待线程退出。
	 */
	void stop() noexcept;

private:
	/** @brief 服务线程栈大小（字节）。 */
	static constexpr size_t kStackSize = 3072;
	/** @brief 服务线程优先级。 */
	static constexpr int kPriority = K_LOWEST_APPLICATION_THREAD_PRIO;
	/** @brief HTTP 时间源端口。 */
	static constexpr uint16_t kHttpTimePort = 80U;
	/** @brief HTTP 时间源固定 IP。 */
	static constexpr const char *kHttpTimeIp = "1.1.1.1";
	/** @brief 上电后等待 IPv4 就绪的最大时间（毫秒）。 */
	static constexpr int64_t kTimeProbeWaitMs = 30000;
	/** @brief 获取失败后的重试间隔（毫秒）。 */
	static constexpr int64_t kRetryDelayMs = 10000;
	/** @brief 最多获取尝试次数。 */
	static constexpr uint8_t kMaxProbeAttempts = 3;

	/**
	 * @brief 线程入口静态适配函数。
	 * @param p1 TimeService 对象指针。
	 * @param p2 未使用。
	 * @param p3 未使用。
	 */
	static void threadEntry(void *p1, void *p2, void *p3);

	/**
	 * @brief 服务线程主循环。
	 */
	void threads() noexcept;

	/**
	 * @brief 在启动阶段尝试获取并打印北京时间。
	 * @note 仅在 IPv4 就绪后执行；失败会延迟重试，最多尝试 kMaxProbeAttempts 次。
	 */
	void maybe_probe_beijing_time_once() noexcept;

	/**
	 * @brief 检查以太网接口是否已有可用 IPv4 地址。
	 * @return true 表示 IPv4 已就绪；false 表示未就绪。
	 */
	bool is_ipv4_ready() const noexcept;

	/**
	 * @brief 通过 HTTP Date 头获取 UTC 时间。
	 * @param out_epoch_sec 输出 UTC epoch 秒。
	 * @return 0 表示成功；负值表示失败。
	 */
	int fetch_utc_epoch_from_http_date(time_t &out_epoch_sec) noexcept;

	/**
	 * @brief 从 HTTP 头中提取 Date 字段。
	 * @param headers HTTP 响应头字符串。
	 * @param out_date 输出 Date 字段内容缓存。
	 * @param out_date_len 输出缓存长度。
	 * @return 0 表示成功；负值表示失败。
	 */
	int extract_http_date_header(const char *headers, char *out_date, size_t out_date_len) const noexcept;

	/**
	 * @brief 把 RFC7231 Date 字符串解析为 UTC epoch 秒。
	 * @param date_value Date 字段值（如 "Tue, 18 Feb 2025 02:31:18 GMT"）。
	 * @param out_epoch_sec 输出 UTC epoch 秒。
	 * @return 0 表示成功；负值表示失败。
	 */
	int parse_rfc7231_date_to_epoch(const char *date_value, time_t &out_epoch_sec) const noexcept;

	/**
	 * @brief 打印北京时间（UTC+8）。
	 * @param utc_epoch_sec UTC epoch 秒。
	 */
	void print_beijing_time(time_t utc_epoch_sec) const noexcept;

	/**
	 * @brief 把北京时间写入片上 RTC。
	 * @param utc_epoch_sec UTC epoch 秒。
	 * @return 0 表示成功；负值表示失败。
	 */
	int write_beijing_time_to_rtc(time_t utc_epoch_sec) noexcept;

	/** @brief 日志接口。 */
	platform::ILogger &log_;
	/** @brief Zephyr 线程控制块。 */
	struct k_thread thread_;
	/** @brief Zephyr 线程栈。 */
	K_KERNEL_STACK_MEMBER(stack_, kStackSize);
	/** @brief 线程 ID，未运行时为 nullptr。 */
	k_tid_t thread_id_ = nullptr;
	/** @brief 运行状态标志：1 运行中，0 未运行。 */
	atomic_t running_ = ATOMIC_INIT(0);
	/** @brief 停止请求标志：1 请求停止，0 继续运行。 */
	atomic_t stop_requested_ = ATOMIC_INIT(0);
	/** @brief 时间探测是否已完成（成功或失败都视为完成）。 */
	bool time_probe_done_ = false;
	/** @brief 启动后时间探测截止时刻（毫秒），0 表示未初始化。 */
	int64_t time_probe_deadline_ms_ = 0;
	/** @brief 已执行的时间获取尝试次数。 */
	uint8_t probe_attempt_count_ = 0;
	/** @brief 下一次允许重试的时间戳（毫秒），0 表示立即可试。 */
	int64_t next_retry_after_ms_ = 0;
};

} // namespace servers
