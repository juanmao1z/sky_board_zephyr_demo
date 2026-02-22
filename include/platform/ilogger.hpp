/**
 * @file ilogger.hpp
 * @brief 日志抽象接口定义。
 */

#pragma once

namespace platform {

/**
 * @brief 日志接口。
 * @note Domain/Server 层只依赖该接口，不直接耦合 Zephyr LOG 宏。
 */
class ILogger {
public:
	virtual ~ILogger() = default;

	/**
	 * @brief 输出信息级日志。
	 * @param msg 日志消息字符串。
	 */
	virtual void info(const char *msg) = 0;

	/**
	 * @brief 输出错误级日志。
	 * @param msg 错误消息字符串。
	 * @param err 错误码。
	 */
	virtual void error(const char *msg, int err) = 0;
};

} // namespace platform
