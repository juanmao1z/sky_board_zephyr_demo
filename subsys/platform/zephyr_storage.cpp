/**
 * @file zephyr_storage.cpp
 * @brief 基于 Zephyr FATFS 的存储平台实现.
 */

#include <errno.h>
#include <ff.h>
#include <stdint.h>
#include <stdio.h>
#include <zephyr/fs/fs.h>
#include <zephyr/kernel.h>
#include <zephyr/storage/disk_access.h>

#include "platform/platform_logger.hpp"
#include "platform/platform_storage.hpp"

namespace {

/** @brief FATFS 挂载点。 */
constexpr char kMountPoint[] = "/SD:";
/** @brief SD 初始化最大重试次数。 */
constexpr int kMaxInitAttempts = 4;
/** @brief 初始化重试间隔。 */
constexpr k_timeout_t kRetryDelay = K_MSEC(300);
/** @brief 上电后等待 SD 电源稳定时间。 */
constexpr k_timeout_t kPowerSettleDelay = K_MSEC(220);

/**
 * @brief 基于 Zephyr FATFS 的存储实现。
 */
class ZephyrStorage final : public platform::IStorage {
 public:
  /** @brief 初始化并挂载 SD 文件系统。 */
  int init() noexcept override;
  /** @brief 写文件（支持覆盖或追加）。 */
  int write_file(const char* path, const void* data, size_t len,
                 bool append = false) noexcept override;
  /** @brief 读文件到调用方缓冲区。 */
  int read_file(const char* path, void* buffer, size_t buffer_size,
                size_t& out_len) noexcept override;
  /** @brief 预留异步写接口，当前未实现。 */
  int enqueue_write(const char* path, const void* data, size_t len,
                    bool append = false) noexcept override;

 private:
  /** @brief 在持锁状态下执行底层磁盘初始化与挂载。 */
  int init_and_mount_locked() noexcept;
  /** @brief 在持锁状态下检查是否可读写。 */
  bool is_ready_locked() const noexcept { return initialized_ && is_mounted_; }

  /** @brief 日志接口。 */
  platform::ILogger& log_ = platform::logger();
  /** @brief FATFS 文件系统对象。 */
  FATFS fat_fs_{};
  /** @brief Zephyr 挂载描述结构。 */
  fs_mount_t mount_{};
  /** @brief SD 设备名（传给 disk_access_ioctl）。 */
  char sd_disk_name_[3] = {'S', 'D', '\0'};
  /** @brief 挂载状态。 */
  bool is_mounted_ = false;
  /** @brief 初始化状态。 */
  bool initialized_ = false;
  /** @brief 互斥锁，保护挂载与文件访问流程。 */
  struct k_mutex mutex_{};
};

/**
 * @brief 在持锁状态下完成 SD 设备初始化并挂载文件系统。
 * @return 0 成功；负值失败。
 */
int ZephyrStorage::init_and_mount_locked() noexcept {
  if (is_mounted_) {
    return 0;
  }

  int ret = disk_access_ioctl(sd_disk_name_, DISK_IOCTL_CTRL_INIT, nullptr);
  if (ret != 0) {
    log_.error("[sd] disk init failed", ret);
    return ret;
  }

  ret = fs_mount(&mount_);
  if (ret != 0) {
    log_.error("[sd] mount failed", ret);
    return ret;
  }

  is_mounted_ = true;
  log_.info("[sd] mounted /SD:");
  return 0;
}

/**
 * @brief 初始化 SD 存储并执行挂载（含重试）。
 * @return 0 成功；负值失败。
 */
int ZephyrStorage::init() noexcept {
  int ret = k_mutex_lock(&mutex_, K_FOREVER);
  if (ret != 0) {
    return ret;
  }

  if (initialized_) {
    k_mutex_unlock(&mutex_);
    return 0;
  }

  mount_.type = FS_FATFS;
  mount_.mnt_point = kMountPoint;
  mount_.fs_data = &fat_fs_;
  mount_.storage_dev = sd_disk_name_;

  k_sleep(kPowerSettleDelay);

  int last_err = 0;
  for (int attempt = 1; attempt <= kMaxInitAttempts; ++attempt) {
    last_err = init_and_mount_locked();
    if (last_err == 0) {
      initialized_ = true;
      k_mutex_unlock(&mutex_);
      return 0;
    }
    if (attempt < kMaxInitAttempts) {
      char msg[96] = {};
      (void)snprintf(msg, sizeof(msg), "[sd] retry %d/%d after err=%d", attempt, kMaxInitAttempts,
                     last_err);
      log_.info(msg);
      k_sleep(kRetryDelay);
    }
  }

  k_mutex_unlock(&mutex_);
  return last_err;
}

/**
 * @brief 向文件写入数据。
 * @param path 目标文件路径。
 * @param data 待写入数据。
 * @param len 数据长度。
 * @param append true 追加；false 覆盖重写。
 * @return 0 成功；负值失败。
 */
int ZephyrStorage::write_file(const char* path, const void* data, const size_t len,
                              const bool append) noexcept {
  if (path == nullptr || path[0] == '\0') {
    return -EINVAL;
  }
  if (len > 0U && data == nullptr) {
    return -EINVAL;
  }

  int ret = k_mutex_lock(&mutex_, K_FOREVER);
  if (ret != 0) {
    return ret;
  }
  if (!is_ready_locked()) {
    k_mutex_unlock(&mutex_);
    return -EACCES;
  }

  fs_file_t file;
  fs_file_t_init(&file);
  const fs_mode_t mode =
      append ? (FS_O_CREATE | FS_O_WRITE | FS_O_APPEND) : (FS_O_CREATE | FS_O_WRITE | FS_O_TRUNC);
  ret = fs_open(&file, path, mode);
  if (ret != 0) {
    log_.error("[sd] file open write failed", ret);
    k_mutex_unlock(&mutex_);
    return ret;
  }

  const uint8_t* bytes = static_cast<const uint8_t*>(data);
  size_t written = 0U;
  while (written < len) {
    const ssize_t write_ret = fs_write(&file, bytes + written, len - written);
    if (write_ret < 0) {
      (void)fs_close(&file);
      log_.error("[sd] file write failed", static_cast<int>(write_ret));
      k_mutex_unlock(&mutex_);
      return static_cast<int>(write_ret);
    }
    if (write_ret == 0) {
      (void)fs_close(&file);
      k_mutex_unlock(&mutex_);
      return -EIO;
    }
    written += static_cast<size_t>(write_ret);
  }

  ret = fs_close(&file);
  if (ret != 0) {
    log_.error("[sd] file close write failed", ret);
    k_mutex_unlock(&mutex_);
    return ret;
  }

  k_mutex_unlock(&mutex_);
  return 0;
}

/**
 * @brief 从文件读取数据到缓冲区。
 * @param path 源文件路径。
 * @param buffer 输出缓冲区。
 * @param buffer_size 缓冲区大小。
 * @param[out] out_len 实际读取字节数。
 * @return 0 成功；负值失败。
 */
int ZephyrStorage::read_file(const char* path, void* buffer, const size_t buffer_size,
                             size_t& out_len) noexcept {
  out_len = 0U;
  if (path == nullptr || path[0] == '\0') {
    return -EINVAL;
  }
  if (buffer_size > 0U && buffer == nullptr) {
    return -EINVAL;
  }

  int ret = k_mutex_lock(&mutex_, K_FOREVER);
  if (ret != 0) {
    return ret;
  }
  if (!is_ready_locked()) {
    k_mutex_unlock(&mutex_);
    return -EACCES;
  }

  fs_file_t file;
  fs_file_t_init(&file);
  ret = fs_open(&file, path, FS_O_READ);
  if (ret != 0) {
    log_.error("[sd] file open read failed", ret);
    k_mutex_unlock(&mutex_);
    return ret;
  }

  uint8_t* bytes = static_cast<uint8_t*>(buffer);
  while (out_len < buffer_size) {
    const ssize_t read_ret = fs_read(&file, bytes + out_len, buffer_size - out_len);
    if (read_ret < 0) {
      (void)fs_close(&file);
      log_.error("[sd] file read failed", static_cast<int>(read_ret));
      k_mutex_unlock(&mutex_);
      return static_cast<int>(read_ret);
    }
    if (read_ret == 0) {
      break;
    }
    out_len += static_cast<size_t>(read_ret);
  }

  if (out_len == buffer_size) {
    uint8_t extra = 0U;
    const ssize_t extra_ret = fs_read(&file, &extra, 1U);
    if (extra_ret < 0) {
      (void)fs_close(&file);
      log_.error("[sd] file read failed", static_cast<int>(extra_ret));
      k_mutex_unlock(&mutex_);
      return static_cast<int>(extra_ret);
    }
    if (extra_ret > 0) {
      (void)fs_close(&file);
      k_mutex_unlock(&mutex_);
      return -ENOSPC;
    }
  }

  ret = fs_close(&file);
  if (ret != 0) {
    log_.error("[sd] file close read failed", ret);
    k_mutex_unlock(&mutex_);
    return ret;
  }

  k_mutex_unlock(&mutex_);
  return 0;
}

/**
 * @brief 异步写接口占位实现。
 * @return 始终返回 -ENOTSUP。
 */
int ZephyrStorage::enqueue_write(const char* path, const void* data, const size_t len,
                                 const bool append) noexcept {
  (void)path;
  (void)data;
  (void)len;
  (void)append;
  return -ENOTSUP;
}

/** @brief 全局存储实例。 */
ZephyrStorage g_storage;

}  // namespace

namespace platform {

/**
 * @brief 获取全局存储实例。
 * @return IStorage 引用。
 */
IStorage& storage() noexcept { return g_storage; }

}  // namespace platform
