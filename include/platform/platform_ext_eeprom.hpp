/**
 * @file platform_ext_eeprom.hpp
 * @brief 外置 EEPROM 平台抽象接口。
 */

#pragma once

#include <cstddef>

namespace platform {

/**
 * @brief 外置 EEPROM 抽象接口。
 */
class IExtEeprom {
 public:
  virtual ~IExtEeprom() = default;

  /**
   * @brief 初始化 EEPROM 驱动资源。
   * @return 0 表示成功；负值表示失败。
   */
  virtual int init() noexcept = 0;

  /**
   * @brief 从 EEPROM 指定偏移读取数据。
   * @param offset 读取偏移，单位字节。
   * @param buffer 输出缓冲区。
   * @param len 读取长度，单位字节。
   * @return 0 表示成功；负值表示失败。
   */
  virtual int read(size_t offset, void* buffer, size_t len) noexcept = 0;

  /**
   * @brief 向 EEPROM 指定偏移写入数据。
   * @param offset 写入偏移，单位字节。
   * @param data 输入数据指针。
   * @param len 写入长度，单位字节。
   * @return 0 表示成功；负值表示失败。
   */
  virtual int write(size_t offset, const void* data, size_t len) noexcept = 0;

  /**
   * @brief 获取 EEPROM 容量。
   * @param[out] out_size 容量，单位字节。
   * @return 0 表示成功；负值表示失败。
   */
  virtual int get_size(size_t& out_size) noexcept = 0;
};

/**
 * @brief 获取全局外置 EEPROM 驱动实例。
 * @return IExtEeprom 引用，生命周期贯穿整个程序运行期。
 */
IExtEeprom& ext_eeprom() noexcept;

}  // namespace platform
