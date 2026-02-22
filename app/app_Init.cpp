/**
 * @file app_Init.cpp
 * @brief 应用初始化与服务启动实现。
 */

#include "app/app_Init.hpp"

#include "platform/platform_backlight.hpp"
#include "platform/platform_display.hpp"
#include "platform/platform_ethernet.hpp"
#include "platform/platform_logger.hpp"
#include "servers/hello_service.hpp"

namespace app {

/**
 * @brief 初始化应用并启动核心服务。
 * @return 0 表示初始化/启动成功；负值表示启动失败。
 * @note 使用静态服务对象保证后台线程运行期间对象生命周期有效。
 */
int app_Init() noexcept
{
	platform::IDisplay &display = platform::display();
	int ret = display.init();
	if (ret < 0) {
		platform::logger().error("failed to init display", ret);
		return ret;
	}

	ret = platform::backlight().set_brightness(100U);
	if (ret < 0) {
		platform::logger().error("failed to set backlight brightness", ret);
		return ret;
	}

	ret = display.show_boot_screen();
	if (ret < 0) {
		platform::logger().error("failed to draw display boot screen", ret);
		return ret;
	}

	platform::logger().info("display boot screen ready");

	ret = platform::ethernet_init();
	if (ret < 0) {
		platform::logger().error("failed to init ethernet", ret);
		return ret;
	}

	static servers::HelloService service(platform::logger());
	return service.run();
}
} // namespace app
