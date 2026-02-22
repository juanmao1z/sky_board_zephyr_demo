/**
 * @file zephyr_logger.cpp
 * @brief ILogger 的 Zephyr LOG 后端实现。
 */

#include "platform/platform_logger.hpp"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(sky_board_demo, LOG_LEVEL_INF);

namespace {

/**
 * @brief 基于 Zephyr LOG 宏的日志实现。
 */
class ZephyrLogger final : public platform::ILogger {
public:
	/**
	 * @brief 输出信息级日志。
	 * @param msg 日志消息字符串。
	 */
	void info(const char *msg) override
	{
		LOG_INF("%s", msg);
	}

	/**
	 * @brief 输出错误级日志。
	 * @param msg 错误消息字符串。
	 * @param err 错误码。
	 */
	void error(const char *msg, int err) override
	{
		LOG_ERR("%s err=%d", msg, err);
	}
};

/** @brief 全局日志对象实例。 */
ZephyrLogger g_logger;

} // namespace

namespace platform {

/**
 * @brief 获取全局日志实例。
 * @return ILogger 引用。
 */
ILogger &logger()
{
	return g_logger;
}

} // namespace platform
