/**
 * @file time_service.cpp
 * @brief 北京时间获取服务实现：启动阶段通过 TCP/HTTP 获取并打印一次时间。
 */

#include "servers/time_service.hpp"

#include "platform/platform_logger.hpp"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/rtc.h>
#include <zephyr/net/ethernet.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/socket.h>
#include <zephyr/sys/timeutil.h>

namespace {

/**
 * @brief 关闭 socket 并重置 fd。
 * @param fd 文件描述符引用。
 */
void close_fd(int &fd)
{
	if (fd >= 0) {
		(void)zsock_close(fd);
		fd = -1;
	}
}

/**
 * @brief 把英文月份缩写转换为月份索引。
 * @param month_abbr 三字母月份缩写（如 "Jan"）。
 * @return 0~11 表示有效月份；-1 表示无效。
 */
int month_to_index(const char *month_abbr)
{
	static const char *const kMonths[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
					       "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

	for (size_t i = 0; i < (sizeof(kMonths) / sizeof(kMonths[0])); ++i) {
		if (strncmp(month_abbr, kMonths[i], 3) == 0) {
			return static_cast<int>(i);
		}
	}

	return -1;
}

} // namespace

namespace servers {

/**
 * @brief 线程入口静态适配函数。
 * @param p1 TimeService 对象指针。
 * @param p2 未使用。
 * @param p3 未使用。
 */
void TimeService::threadEntry(void *p1, void *, void *)
{
	static_cast<TimeService *>(p1)->threads();
}

/**
 * @brief 服务线程主循环。
 * @note 在启动窗口内等待 IPv4 并执行时间探测，成功或达到最大重试次数后退出。
 */
void TimeService::threads() noexcept
{
	/*
	 * 执行步骤：
	 * 1) 线程启动后进入循环，持续检查停止标志与探测完成标志。
	 * 2) 每轮触发一次时间探测状态机。
	 * 3) 若未完成则短暂休眠，避免线程空转占满 CPU。
	 * 4) 退出时回收运行状态并记录停止日志。
	 */

	log_.info("time service starting");

	/* 步骤 1：服务循环，直到被请求停止或探测完成。 */
	while (atomic_get(&stop_requested_) == 0 && !time_probe_done_) {
		/* 步骤 2：执行一次探测状态机。 */
		maybe_probe_beijing_time_once();
		if (!time_probe_done_) {
			/* 步骤 3：等待下一轮检查。 */
			k_sleep(K_MSEC(1000));
		}
	}

	/* 步骤 4：线程退出收尾。 */
	atomic_set(&running_, 0);
	thread_id_ = nullptr;
	log_.info("time service stopped");
}

/**
 * @brief 在启动阶段尝试获取并打印北京时间。
 * @note 失败后延迟 10 秒重试，最多尝试 3 次；达到上限后结束线程。
 */
void TimeService::maybe_probe_beijing_time_once() noexcept
{
	/*
	 * 执行步骤：
	 * 1) 初始化本次状态：读取当前时间并建立启动窗口。
	 * 2) 网络门控：IPv4 未就绪则等待或在超时后结束。
	 * 3) 重试门控：检查最大次数与重试冷却时间。
	 * 4) 发起 HTTP Date 获取并按结果更新状态。
	 * 5) 成功后写 RTC、打印北京时间并结束探测。
	 */

	if (time_probe_done_) {
		return;
	}

	/* 步骤 1：初始化启动窗口基准时间。 */
	const int64_t now_ms = k_uptime_get();
	if (time_probe_deadline_ms_ == 0) {
		time_probe_deadline_ms_ = now_ms + kTimeProbeWaitMs;
	}

	/* 步骤 2：IPv4 未就绪时继续等待，超时则停止探测。 */
	if (!is_ipv4_ready()) {
		if (now_ms >= time_probe_deadline_ms_) {
			log_.info("[time] Beijing sync skipped: IPv4 not ready within 30s");
			time_probe_done_ = true;
		}
		return;
	}

	/* 步骤 3.1：达到最大尝试次数则停止探测。 */
	if (probe_attempt_count_ >= kMaxProbeAttempts) {
		char msg[80];
		(void)snprintf(msg, sizeof(msg), "[time] Beijing sync stopped: reached max attempts (%u)",
			       static_cast<unsigned int>(kMaxProbeAttempts));
		log_.info(msg);
		time_probe_done_ = true;
		return;
	}

	/* 步骤 3.2：仍在重试冷却期时本轮直接返回。 */
	if (next_retry_after_ms_ != 0 && now_ms < next_retry_after_ms_) {
		return;
	}

	/* 步骤 4：执行一次 HTTP 时间获取。 */
	time_t utc_epoch_sec = 0;
	++probe_attempt_count_;
	const int ret = fetch_utc_epoch_from_http_date(utc_epoch_sec);
	if (ret < 0) {
		if (probe_attempt_count_ >= kMaxProbeAttempts) {
			char msg[96];
			(void)snprintf(msg, sizeof(msg), "[time] Beijing sync failed: err=%d, attempts=%u/%u", ret,
				       static_cast<unsigned int>(probe_attempt_count_),
				       static_cast<unsigned int>(kMaxProbeAttempts));
			log_.info(msg);
			time_probe_done_ = true;
		} else {
			next_retry_after_ms_ = now_ms + kRetryDelayMs;
			char msg[96];
			(void)snprintf(msg, sizeof(msg), "[time] Beijing sync failed: err=%d, retry in 10s (%u/%u)", ret,
				       static_cast<unsigned int>(probe_attempt_count_),
				       static_cast<unsigned int>(kMaxProbeAttempts));
			log_.info(msg);
		}
		return;
	}

	/* 步骤 5：获取成功后写入 RTC 并输出北京时间。 */
	const int rtc_ret = write_beijing_time_to_rtc(utc_epoch_sec);
	if (rtc_ret < 0) {
		log_.error("failed to write beijing time to rtc", rtc_ret);
	} else {
		log_.info("[time] RTC updated with Beijing time");
		const int ts_ret = platform::logger_enable_rtc_timestamp();
		if (ts_ret < 0) {
			log_.error("failed to switch log timestamp to rtc", ts_ret);
		} else {
			log_.info("[time] log timestamp switched to rtc");
		}
	}

	print_beijing_time(utc_epoch_sec);
	time_probe_done_ = true;
}

/**
 * @brief 检查以太网接口是否已有可用 IPv4 地址。
 * @return true 表示 IPv4 已就绪；false 表示未就绪。
 */
bool TimeService::is_ipv4_ready() const noexcept
{
	/* 步骤 1：定位以太网 L2 对应接口。 */
	struct net_if *iface = net_if_get_first_by_type(&NET_L2_GET_NAME(ETHERNET));
	if (iface == nullptr) {
		return false;
	}

	/* 步骤 2：优先检查 preferred 地址，其次检查 tentative 地址。 */
	struct net_in_addr *addr = net_if_ipv4_get_global_addr(iface, NET_ADDR_PREFERRED);
	if (addr == nullptr) {
		addr = net_if_ipv4_get_global_addr(iface, NET_ADDR_TENTATIVE);
	}

	return addr != nullptr;
}

/**
 * @brief 通过 HTTP Date 头获取 UTC 时间。
 * @param out_epoch_sec 输出 UTC epoch 秒。
 * @return 0 表示成功；负值表示失败。
 */
int TimeService::fetch_utc_epoch_from_http_date(time_t &out_epoch_sec) noexcept
{
	/*
	 * 执行步骤：
	 * 1) 准备 socket 与目标地址并建立连接。
	 * 2) 发送 HEAD 请求。
	 * 3) 接收响应头直到 \r\n\r\n。
	 * 4) 解析 Date 字段并转换为 UTC epoch。
	 */

	int sock = -1;
	int ret = 0;
	char date_value[64] = {};

	/* 步骤 1：配置目标地址并建立 TCP 连接。 */
	struct sockaddr_in server_addr = {};
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(kHttpTimePort);
	if (zsock_inet_pton(AF_INET, kHttpTimeIp, &server_addr.sin_addr) != 1) {
		return -EINVAL;
	}

	sock = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock < 0) {
		return -errno;
	}

	struct timeval timeout = {};
	timeout.tv_sec = 3;
	timeout.tv_usec = 0;
	(void)zsock_setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
	(void)zsock_setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

	if (zsock_connect(sock, reinterpret_cast<struct sockaddr *>(&server_addr), sizeof(server_addr)) < 0) {
		ret = -errno;
		close_fd(sock);
		return ret;
	}

	/* 步骤 2：发送 HTTP HEAD 请求。 */
	static const char request[] =
		"HEAD / HTTP/1.1\r\n"
		"Host: 1.1.1.1\r\n"
		"Connection: close\r\n"
		"\r\n";

	size_t send_total = 0U;
	while (send_total < (sizeof(request) - 1U)) {
		const ssize_t sent = zsock_send(sock, request + send_total, (sizeof(request) - 1U) - send_total, 0);
		if (sent < 0) {
			ret = -errno;
			close_fd(sock);
			return ret;
		}

		send_total += static_cast<size_t>(sent);
	}

	/* 使用静态缓冲避免在线程栈上放置大对象。 */
	static char headers[1024];
	memset(headers, 0, sizeof(headers));
	size_t recv_total = 0U;
	bool header_complete = false;

	/* 步骤 3：读取响应头，直到头结束标记出现。 */
	while (recv_total < (sizeof(headers) - 1U)) {
		const ssize_t r = zsock_recv(sock, headers + recv_total, (sizeof(headers) - 1U) - recv_total, 0);
		if (r == 0) {
			break;
		}

		if (r < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				ret = -ETIMEDOUT;
			} else {
				ret = -errno;
			}
			close_fd(sock);
			return ret;
		}

		recv_total += static_cast<size_t>(r);
		headers[recv_total] = '\0';

		if (strstr(headers, "\r\n\r\n") != nullptr) {
			header_complete = true;
			break;
		}
	}

	close_fd(sock);

	if (!header_complete) {
		return -EBADMSG;
	}

	ret = extract_http_date_header(headers, date_value, sizeof(date_value));
	if (ret < 0) {
		return ret;
	}

	/* 步骤 4：把 Date 文本解析成 UTC epoch 秒。 */
	return parse_rfc7231_date_to_epoch(date_value, out_epoch_sec);
}

/**
 * @brief 从 HTTP 头中提取 Date 字段。
 * @param headers HTTP 响应头字符串。
 * @param out_date 输出 Date 字段内容缓存。
 * @param out_date_len 输出缓存长度。
 * @return 0 表示成功；负值表示失败。
 */
int TimeService::extract_http_date_header(const char *headers, char *out_date, size_t out_date_len) const noexcept
{
	/* 步骤 1：校验输入缓冲是否合法。 */
	if (headers == nullptr || out_date == nullptr || out_date_len == 0U) {
		return -EINVAL;
	}

	/* 步骤 2：按 CRLF 分行扫描，定位 "Date:" 头。 */
	const char *line = headers;
	while (*line != '\0') {
		const char *line_end = strstr(line, "\r\n");
		if (line_end == nullptr) {
			break;
		}

		const size_t line_len = static_cast<size_t>(line_end - line);
		if (line_len >= 5U && strncmp(line, "Date:", 5) == 0) {
			const char *value = line + 5;
			while (*value == ' ' || *value == '\t') {
				++value;
			}

			/* 步骤 3：拷贝 Date 值到输出缓冲。 */
			const size_t value_len = static_cast<size_t>(line_end - value);
			if (value_len + 1U > out_date_len) {
				return -ENOMEM;
			}

			memcpy(out_date, value, value_len);
			out_date[value_len] = '\0';
			return 0;
		}

		if (line_len == 0U) {
			break;
		}

		line = line_end + 2;
	}

	return -ENOENT;
}

/**
 * @brief 把 RFC7231 Date 字符串解析为 UTC epoch 秒。
 * @param date_value Date 字段值（如 "Tue, 18 Feb 2025 02:31:18 GMT"）。
 * @param out_epoch_sec 输出 UTC epoch 秒。
 * @return 0 表示成功；负值表示失败。
 */
int TimeService::parse_rfc7231_date_to_epoch(const char *date_value, time_t &out_epoch_sec) const noexcept
{
	/*
	 * 执行步骤：
	 * 1) 用 sscanf 拆分 RFC7231 字段。
	 * 2) 校验时区与时间边界。
	 * 3) 组装 tm 并转换为 UTC epoch。
	 */

	/* 步骤 1：校验输入并解析日期文本。 */
	if (date_value == nullptr) {
		return -EINVAL;
	}

	char weekday[4] = {};
	char month[4] = {};
	char zone[4] = {};
	int day = 0;
	int year = 0;
	int hour = 0;
	int minute = 0;
	int second = 0;

	const int matched = sscanf(date_value, "%3[^,], %d %3s %d %d:%d:%d %3s", weekday, &day, month, &year, &hour,
				  &minute, &second, zone);
	if (matched != 8) {
		return -EINVAL;
	}

	/* 步骤 2：校验 GMT 标识与取值范围。 */
	if (strcmp(zone, "GMT") != 0) {
		return -EINVAL;
	}

	const int month_index = month_to_index(month);
	if (month_index < 0) {
		return -EINVAL;
	}

	if (day < 1 || day > 31 || hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 ||
	    second > 60) {
		return -EINVAL;
	}

	struct tm tm_utc = {};
	tm_utc.tm_year = year - 1900;
	tm_utc.tm_mon = month_index;
	tm_utc.tm_mday = day;
	tm_utc.tm_hour = hour;
	tm_utc.tm_min = minute;
	tm_utc.tm_sec = second;
	tm_utc.tm_isdst = 0;

	/* 步骤 3：把 UTC tm 转成 epoch 秒。 */
	errno = 0;
	const time_t epoch_sec = timeutil_timegm(&tm_utc);
	if (epoch_sec == static_cast<time_t>(-1) && errno == ERANGE) {
		return -ERANGE;
	}

	out_epoch_sec = epoch_sec;
	return 0;
}

/**
 * @brief 打印北京时间（UTC+8）。
 * @param utc_epoch_sec UTC epoch 秒。
 */
void TimeService::print_beijing_time(time_t utc_epoch_sec) const noexcept
{
	/* 步骤 1：UTC +8 小时得到北京时间。 */
	time_t beijing_epoch_sec = utc_epoch_sec + (8 * 3600);
	struct tm beijing_tm = {};

	/* 步骤 2：转换为可读日期结构并格式化输出。 */
	if (gmtime_r(&beijing_epoch_sec, &beijing_tm) == nullptr) {
		log_.info("[time] Beijing format failed");
		return;
	}

	char msg[96];
	(void)snprintf(msg, sizeof(msg), "[time] Beijing: %04d-%02d-%02d %02d:%02d:%02d (UTC+8)",
		       beijing_tm.tm_year + 1900, beijing_tm.tm_mon + 1, beijing_tm.tm_mday, beijing_tm.tm_hour,
		       beijing_tm.tm_min, beijing_tm.tm_sec);
	log_.info(msg);
}

/**
 * @brief 把北京时间写入片上 RTC。
 * @param utc_epoch_sec UTC epoch 秒。
 * @return 0 表示成功；负值表示失败。
 */
int TimeService::write_beijing_time_to_rtc(time_t utc_epoch_sec) noexcept
{
	/*
	 * 执行步骤：
	 * 1) 获取并检查 RTC 设备是否就绪。
	 * 2) 把 UTC 转为北京时间结构体。
	 * 3) 映射到 rtc_time 并调用 rtc_set_time()。
	 */

	/* 步骤 1：获取 RTC 设备句柄并检查可用性。 */
	const struct device *rtc_dev = DEVICE_DT_GET_OR_NULL(DT_NODELABEL(rtc));
	if (rtc_dev == nullptr || !device_is_ready(rtc_dev)) {
		return -ENODEV;
	}

	/* 步骤 2：生成北京时间 tm。 */
	time_t beijing_epoch_sec = utc_epoch_sec + (8 * 3600);
	struct tm beijing_tm = {};
	if (gmtime_r(&beijing_epoch_sec, &beijing_tm) == nullptr) {
		return -EINVAL;
	}

	/* 步骤 3：映射到 rtc_time 并写入硬件 RTC。 */
	struct rtc_time rtc_tm = {};
	rtc_tm.tm_sec = beijing_tm.tm_sec;
	rtc_tm.tm_min = beijing_tm.tm_min;
	rtc_tm.tm_hour = beijing_tm.tm_hour;
	rtc_tm.tm_mday = beijing_tm.tm_mday;
	rtc_tm.tm_mon = beijing_tm.tm_mon;
	rtc_tm.tm_year = beijing_tm.tm_year;
	rtc_tm.tm_wday = beijing_tm.tm_wday;
	rtc_tm.tm_yday = beijing_tm.tm_yday;
	rtc_tm.tm_isdst = -1;
	rtc_tm.tm_nsec = 0;

	return rtc_set_time(rtc_dev, &rtc_tm);
}

/**
 * @brief 请求停止时间服务线程。
 * @note 仅设置停止标志并尝试唤醒线程，不阻塞等待退出。
 */
void TimeService::stop() noexcept
{
	/* 步骤 1：未运行则直接返回。 */
	if (atomic_get(&running_) == 0) {
		return;
	}

	/* 步骤 2：置位停止标志并唤醒线程。 */
	atomic_set(&stop_requested_, 1);
	if (thread_id_ != nullptr) {
		k_wakeup(thread_id_);
	}
}

/**
 * @brief 启动时间服务线程（幂等）。
 * @return 0 表示成功或已在运行；负值表示失败。
 */
int TimeService::run() noexcept
{
	/*
	 * 执行步骤：
	 * 1) 幂等检查，避免重复启动。
	 * 2) 重置探测状态机变量。
	 * 3) 创建线程并设置线程名。
	 */

	/* 步骤 1：若已运行则直接返回。 */
	if (!atomic_cas(&running_, 0, 1)) {
		log_.info("time service already running");
		return 0;
	}

	/* 步骤 2：重置状态机。 */
	atomic_set(&stop_requested_, 0);
	time_probe_done_ = false;
	time_probe_deadline_ms_ = 0;
	probe_attempt_count_ = 0;
	next_retry_after_ms_ = 0;

	/* 步骤 3：创建服务线程并命名。 */
	thread_id_ = k_thread_create(&thread_, stack_, K_THREAD_STACK_SIZEOF(stack_), threadEntry, this, nullptr,
				     nullptr, kPriority, 0, K_NO_WAIT);
	if (thread_id_ == nullptr) {
		atomic_set(&running_, 0);
		log_.error("failed to create time service thread", -1);
		return -1;
	}

	k_thread_name_set(thread_id_, "time_service");
	return 0;
}

} // namespace servers
