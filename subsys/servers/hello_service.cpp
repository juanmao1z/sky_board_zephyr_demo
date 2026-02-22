/**
 * @file hello_service.cpp
 * @brief 心跳服务实现：后台线程、日志心跳、PB2(led0) 翻转。
 */

#include "servers/hello_service.hpp"

#include <zephyr/autoconf.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

namespace servers {

/** @brief 板级 led0 别名节点（本板映射到 PB2）。 */
#define LED0_NODE DT_ALIAS(led0)

#if !DT_NODE_HAS_STATUS(LED0_NODE, okay)
#error "Unsupported board: led0 devicetree alias is not defined"
#endif

/**
 * @brief 线程入口静态适配函数。
 * @param p1 HelloService 对象指针。
 * @param p2 未使用。
 * @param p3 未使用。
 */
void HelloService::threadEntry(void *p1, void *, void *)
{
	static_cast<HelloService *>(p1)->threads();
}

/**
 * @brief 服务线程主循环。
 * @note 每 5 秒输出一次心跳日志并翻转一次 led0。
 */
void HelloService::threads() noexcept
{
	const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
	bool led_ready = false;
	bool led_on = false;
	int ret = 0;

	/* 线程启动时一次性初始化 LED。 */
	if (!gpio_is_ready_dt(&led)) {
		log_.error("led0 gpio device not ready", -1);
	} else {
		ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
		if (ret < 0) {
			log_.error("failed to configure led0", ret);
		} else {
			led_ready = true;
		}
	}

	log_.info("sky_board_zephyr_demo starting");
	log_.info("hello service started");

	while (atomic_get(&stop_requested_) == 0) {
		/* 心跳动作：翻转 led0，用于快速观测系统活性。 */
		if (led_ready) {
			led_on = !led_on;
			ret = gpio_pin_set_dt(&led, led_on ? 1 : 0);
			if (ret < 0) {
				log_.error("failed to set led0", ret);
				led_ready = false;
			}
		}

		log_.info("heartbeat: system alive");
		k_sleep(K_SECONDS(5));
	}

	/* 退出前把 LED 拉低，避免服务停止后维持未知状态。 */
	if (led_ready) {
		(void)gpio_pin_set_dt(&led, 0);
	}

	atomic_set(&running_, 0);
	thread_id_ = nullptr;
	log_.info("hello service task stopped");
}

/**
 * @brief 请求停止服务线程。
 * @note 设置停止标志，并唤醒线程以缩短退出等待时间。
 */
void HelloService::stop() noexcept
{
	if (atomic_get(&running_) == 0) {
		return;
	}

	atomic_set(&stop_requested_, 1);
	if (thread_id_ != nullptr) {
		k_wakeup(thread_id_);
	}
}

/**
 * @brief 启动服务线程（幂等）。
 * @return 0 表示成功或已在运行；-1 表示线程创建失败。
 */
int HelloService::run() noexcept
{
	if (!atomic_cas(&running_, 0, 1)) {
		log_.info("hello service task already running");
		return 0;
	}
	atomic_set(&stop_requested_, 0);
	thread_id_ = k_thread_create(&thread_, stack_, K_THREAD_STACK_SIZEOF(stack_), threadEntry, this, nullptr,
				     nullptr, kPriority, 0, K_NO_WAIT);
	if (thread_id_ == nullptr) {
		atomic_set(&running_, 0);
		log_.error("failed to create hello service task", -1);
		return -1;
	}
	k_thread_name_set(thread_id_, "hello_service");
	return 0;
}

} // namespace servers
