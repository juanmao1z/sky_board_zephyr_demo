/**
 * @file hello_service.hpp
 * @brief 心跳服务声明：启动线程、周期日志、LED 翻转。
 */

#pragma once

#include "platform/ilogger.hpp"

#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>

namespace servers {

/**
 * @brief 心跳服务。
 * @note 该服务启动后在独立 Zephyr 线程中运行，每 5 秒执行一次心跳逻辑。
 */
class HelloService {
public:
	/**
	 * @brief 构造心跳服务。
	 * @param log 日志接口引用，必须在服务生命周期内保持有效。
	 */
	explicit HelloService(platform::ILogger &log) : log_(log) {}

	/**
	 * @brief 启动服务线程（幂等）。
	 * @return 0 表示成功或已在运行；负值表示启动失败。
	 */
	int run() noexcept;

	/**
	 * @brief 请求停止服务线程。
	 * @note 该函数仅发出停止请求并唤醒线程，不阻塞等待线程退出。
	 */
	void stop() noexcept;

private:
	/** @brief 服务线程栈大小（字节）。 */
	static constexpr size_t kStackSize = 1024;
	/** @brief 服务线程优先级。 */
	static constexpr int kPriority = K_LOWEST_APPLICATION_THREAD_PRIO;

	/**
	 * @brief 线程入口静态适配函数。
	 * @param p1 HelloService 对象指针。
	 * @param p2 未使用。
	 * @param p3 未使用。
	 */
	static void threadEntry(void *p1, void *p2, void *p3);

	/**
	 * @brief 服务线程主循环。
	 */
	void threads() noexcept;

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
};

} // namespace servers
