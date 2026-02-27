/**
 * @file ilogger.hpp
 * @brief 日志抽象接口定义。
 */

#pragma once

#include <stdarg.h>

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
  virtual void info(const char* msg) = 0;

  /**
   * @brief 输出错误级日志。
   * @param msg 错误消息字符串。
   * @param err 错误码。
   */
  virtual void error(const char* msg, int err) = 0;

  /**
   * @brief 输出格式化信息级日志.
   * @param fmt printf 风格格式串.
   * @param ... 可变参数.
   */
  void infof(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vinfof(fmt, args);
    va_end(args);
  }

  /**
   * @brief 输出格式化错误级日志.
   * @param fmt printf 风格格式串.
   * @param ... 可变参数.
   */
  void errorf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    verrorf(fmt, args);
    va_end(args);
  }

  /**
   * @brief 输出 va_list 形式的信息级日志.
   * @param fmt printf 风格格式串.
   * @param args 参数列表.
   */
  virtual void vinfof(const char* fmt, va_list args) = 0;

  /**
   * @brief 输出 va_list 形式的错误级日志.
   * @param fmt printf 风格格式串.
   * @param args 参数列表.
   */
  virtual void verrorf(const char* fmt, va_list args) = 0;
};

}  // namespace platform
