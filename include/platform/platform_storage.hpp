/**
 * @file platform_storage.hpp
 * @brief 存储平台抽象接口.
 */

#pragma once

#include <cstddef>

namespace platform {

/**
 * @brief 存储抽象接口.
 */
class IStorage {
 public:
  virtual ~IStorage() = default;

  /**
   * @brief 初始化存储设备并完成挂载.
   * @return 0 表示成功, 负值表示失败.
   */
  virtual int init() noexcept = 0;

  /**
   * @brief 向指定文件写入数据.
   * @param path 目标文件路径, 例如 /SD:/DATA.BIN.
   * @param data 待写入数据指针.
   * @param len 待写入字节数.
   * @param append true 表示追加写, false 表示覆盖写.
   * @return 0 表示成功, 负值表示失败.
   */
  virtual int write_file(const char* path, const void* data, size_t len,
                         bool append = false) noexcept = 0;

  /**
   * @brief 从指定文件读取数据到缓冲区.
   * @param path 源文件路径, 例如 /SD:/DATA.BIN.
   * @param buffer 输出缓冲区.
   * @param buffer_size 缓冲区大小, 单位字节.
   * @param[out] out_len 实际读取字节数.
   * @return 0 表示成功, 负值表示失败.
   */
  virtual int read_file(const char* path, void* buffer, size_t buffer_size,
                        size_t& out_len) noexcept = 0;

  /**
   * @brief 异步写接口预留.
   * @param path 目标文件路径.
   * @param data 待写入数据指针.
   * @param len 待写入字节数.
   * @param append true 表示追加写, false 表示覆盖写.
   * @return 当前固定返回 -ENOTSUP.
   */
  virtual int enqueue_write(const char* path, const void* data, size_t len,
                            bool append = false) noexcept = 0;
};

/**
 * @brief 获取全局存储实例.
 * @return 存储实例引用.
 */
IStorage& storage() noexcept;

}  // namespace platform
