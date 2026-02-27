/**
 * @file app_Init.cpp
 * @brief 应用初始化与服务启动实现。
 */

#include "app/app_Init.hpp"

#include "platform/platform_display.hpp"
#include "platform/platform_ethernet.hpp"
#include "platform/platform_logger.hpp"
#include "platform/platform_storage.hpp"
#include "platform/platform_ws2812.hpp"
#include "servers/button_service.hpp"
#include "servers/encoder_service.hpp"
#include "servers/hello_service.hpp"
#include "servers/imu_service.hpp"
#include "servers/sensor_service.hpp"
#include "servers/tcp_service.hpp"
#include "servers/time_service.hpp"

namespace app {

/**
 * @brief 初始化应用并启动核心服务。
 * @return 0 表示初始化/启动成功；负值表示启动失败。
 * @note 使用静态服务对象保证后台线程运行期间对象生命周期有效。
 */
int app_Init() noexcept {
  int ret = 0;
  platform::IDisplay& display = platform::display();
  ret = display.init();
  if (ret < 0) {
    platform::logger().error("failed to init display", ret);
    return ret;
  }
  ret = display.backlight().set_brightness(100U);
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

  ret = platform::ws2812().init();
  if (ret < 0) {
    platform::logger().error("failed to init ws2812", ret);
    return ret;
  }
  ret = platform::ethernet_init();
  if (ret < 0) {
    platform::logger().error("failed to init ethernet", ret);
    return ret;
  }
  static servers::TimeService time_service(platform::logger());
  ret = time_service.run();
  if (ret < 0) {
    platform::logger().error("failed to start time service", ret);
    return ret;
  }
  static servers::HelloService hello_service(platform::logger());
  ret = hello_service.run();
  if (ret < 0) {
    platform::logger().error("failed to start hello service", ret);
    return ret;
  }
  static servers::TcpService tcp_service(platform::logger());
  ret = tcp_service.run();
  if (ret < 0) {
    platform::logger().error("failed to start tcp service", ret);
    return ret;
  }

  ret = time_service.wait_first_sync(45000);
  if (ret < 0) {
    platform::logger().error("failed waiting first beijing rtc sync", ret);
    return ret;
  }
  platform::logger().info("[time] first beijing rtc sync ready");

  ret = platform::storage().init();
  if (ret < 0) {
    platform::logger().error("failed to init storage", ret);
    return ret;
  }

  static servers::SensorService sensor_service(platform::logger());
  ret = sensor_service.run();
  if (ret < 0) {
    platform::logger().error("failed to start sensor service", ret);
    return ret;
  }

  static servers::EncoderService encoder_service(platform::logger());
  ret = encoder_service.run();
  if (ret < 0) {
    platform::logger().error("failed to start encoder service", ret);
    return ret;
  }

  static servers::ButtonService button_service(platform::logger());
  ret = button_service.run();
  if (ret < 0) {
    platform::logger().error("failed to start button service", ret);
    return ret;
  }

  // static servers::ImuService imu_service(platform::logger());
  // ret = imu_service.run();
  // if (ret < 0) {
  //   platform::logger().error("failed to start imu service", ret);
  //   return ret;
  // }
  return 0;
}

}  // namespace app
