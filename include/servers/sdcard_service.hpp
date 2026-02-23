/**
 * @file sdcard_service.hpp
 * @brief SD 卡服务声明：执行 SD 初始化、挂载与文件读写。
 */

#pragma once

#include <ff.h>
#include <zephyr/fs/fs.h>
#include <zephyr/kernel.h>

#include <cstddef>

#include "platform/ilogger.hpp"

namespace servers {

/**
 * @brief SD 卡服务。
 * @note 首次运行会初始化磁盘并挂载 FATFS。
 */
class SdcardService {
 public:
  /**
   * @brief 构造 SD 卡服务。
   * @param log 日志接口引用，必须在服务生命周期内保持有效。
   */
  explicit SdcardService(platform::ILogger& log) noexcept;
  SdcardService(const SdcardService&) = delete;
  SdcardService& operator=(const SdcardService&) = delete;
  SdcardService(SdcardService&&) = delete;
  SdcardService& operator=(SdcardService&&) = delete;

  /**
   * @brief 运行 SD 卡服务。
   * @return 0 表示成功。
   */
  int run() noexcept;
  /**
   * @brief 设置服务初始化完成标志。
   * @param value true 表示已完成启动初始化；false 表示未初始化。
   */
  void set_initialized(bool value = true) noexcept;
  /**
   * @brief 向指定文件写入数据。
   * @param path 目标文件路径（例如 /SD:/DATA.BIN）。
   * @param data 待写入数据指针。
   * @param len 待写入字节数。
   * @param append true 追加写入；false 覆盖写入。
   * @return 0 表示成功；负值表示失败。
   */
  int write_file(const char* path, const void* data, size_t len, bool append = false) noexcept;
  /**
   * @brief 从指定文件读取数据到缓冲区。
   * @param path 源文件路径（例如 /SD:/DATA.BIN）。
   * @param buffer 输出缓冲区。
   * @param buffer_size 输出缓冲区大小（字节）。
   * @param[out] out_len 实际读取字节数。
   * @return 0 表示成功；负值表示失败（缓冲区不足返回 -ENOSPC）。
   */
  int read_file(const char* path, void* buffer, size_t buffer_size, size_t& out_len) noexcept;
  /**
   * @brief 异步写接口预留（当前仅占位，后续可接入队列线程）。
   * @param path 目标文件路径（例如 /SD:/DATA.BIN）。
   * @param data 待写入数据指针。
   * @param len 待写入字节数。
   * @param append true 追加写入；false 覆盖写入。
   * @return 当前固定返回 -ENOTSUP。
   */
  int enqueue_write(const char* path, const void* data, size_t len, bool append = false) noexcept;

 private:
  /**
   * @brief 若尚未挂载，则完成磁盘初始化与 FATFS 挂载（调用方需持有锁）。
   * @return 0 表示成功；负值表示初始化或挂载失败。
   */
  int init_and_mount_locked() noexcept;

  /** @brief 日志接口。 */
  platform::ILogger& log_;
  /** @brief FATFS 对象，作为挂载上下文的私有状态。 */
  FATFS fat_fs_{};
  /** @brief 挂载配置，绑定到当前服务实例。 */
  fs_mount_t mount_{};
  /** @brief 磁盘名（传递给 disk_access/fs 层）。 */
  char sd_disk_name_[3] = {'S', 'D', '\0'};
  /** @brief 当前是否已挂载。 */
  bool is_mounted_ = false;
  /** @brief 当前是否允许业务读写（由 app_Init 在 run 成功后置位）。 */
  bool initialized_ = false;
  /** @brief 服务内部互斥锁，用于串行化挂载与文件读写。 */
  struct k_mutex mutex_{};
};

}  // namespace servers
