/**
 * @file platform_spi_flash.hpp
 * @brief 外置 SPI NOR Flash 平台抽象接口。
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <sys/types.h>

namespace platform {

/**
 * @brief 外置 SPI NOR Flash 抽象接口。
 */
class ISpiFlash {
 public:
  virtual ~ISpiFlash() = default;

  /**
   * @brief 初始化 SPI Flash 驱动资源。
   * @return 0 表示成功；负值表示失败。
   */
  virtual int init() noexcept = 0;

  /**
   * @brief 读取 SPI Flash 数据。
   * @param offset 读取偏移，单位字节。
   * @param buffer 输出缓冲区。
   * @param len 读取长度，单位字节。
   * @return 0 表示成功；负值表示失败。
   */
  virtual int read(off_t offset, void* buffer, size_t len) noexcept = 0;

  /**
   * @brief 写入 SPI Flash 数据。
   * @param offset 写入偏移，单位字节。
   * @param data 输入数据指针。
   * @param len 写入长度，单位字节。
   * @return 0 表示成功；负值表示失败。
   * @note 不执行隐式擦除，调用方需先对目标区域调用 erase。
   */
  virtual int write(off_t offset, const void* data, size_t len) noexcept = 0;

  /**
   * @brief 擦除 SPI Flash 指定区域。
   * @param offset 擦除偏移，单位字节。
   * @param len 擦除长度，单位字节。
   * @return 0 表示成功；负值表示失败。
   */
  virtual int erase(off_t offset, size_t len) noexcept = 0;

  /**
   * @brief 获取 SPI Flash 容量。
   * @param[out] out_size 容量，单位字节。
   * @return 0 表示成功；负值表示失败。
   */
  virtual int get_size(uint64_t& out_size) noexcept = 0;
};

/**
 * @brief 获取全局外置 SPI Flash 驱动实例。
 * @return ISpiFlash 引用，生命周期贯穿整个程序运行期。
 */
ISpiFlash& spi_flash_ext() noexcept;

}  // namespace platform
