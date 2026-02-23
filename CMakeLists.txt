# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sky_board_zephyr_demo)

target_include_directories(app PRIVATE include)

target_sources(app PRIVATE
  app/main.cpp
  app/app_Init.cpp
  subsys/platform/font5x7.cpp
  subsys/platform/zephyr_backlight.cpp
  subsys/platform/zephyr_display.cpp
  subsys/platform/zephyr_ethernet.cpp
  subsys/servers/hello_service.cpp
  subsys/servers/sdcard_service.cpp
  subsys/servers/time_service.cpp
  subsys/servers/tcp_service.cpp
  subsys/platform/zephyr_logger.cpp
)

target_compile_options(app PRIVATE
  $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
  $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
  $<$<COMPILE_LANGUAGE:CXX>:-fno-threadsafe-statics>
)
