/**
 * @file sdcard_service.cpp
 * @brief SD 卡服务实现：执行 SD 初始化、挂载与文件读写。
 */

#include "servers/sdcard_service.hpp"

#include <errno.h>
#include <ff.h>
#include <stdio.h>
#include <stdint.h>

#include <zephyr/fs/fs.h>
#include <zephyr/kernel.h>
#include <zephyr/storage/disk_access.h>

namespace {

const char k_mount_point[] = "/SD:";

} // namespace

namespace servers {

/**
 * @brief 构造 SD 卡服务并初始化挂载配置。
 * @param log 日志接口引用。
 */
SdcardService::SdcardService(platform::ILogger &log) noexcept : log_(log)
{
	/* 1) 固定挂载类型为 FATFS。 */
	mount_.type = FS_FATFS;
	/* 2) 绑定统一挂载点，业务侧始终通过 /SD: 访问。 */
	mount_.mnt_point = k_mount_point;
	/* 3) 绑定 FATFS 运行时上下文。 */
	mount_.fs_data = &fat_fs_;
	/* 4) 绑定底层磁盘设备名（disk_access 与 fs_mount 共用）。 */
	mount_.storage_dev = sd_disk_name_;
	/* 5) 初始化服务内部互斥锁。 */
	k_mutex_init(&mutex_);
}

/**
 * @brief 按需执行 SD 初始化与 FATFS 挂载（调用方需持有锁）。
 * @return 0 表示成功；负值表示失败。
 */
int SdcardService::init_and_mount_locked() noexcept
{
	if (is_mounted_) {
		return 0;
	}

	/* Step 1: 初始化 SD 磁盘控制器。 */
	int ret = disk_access_ioctl(sd_disk_name_, DISK_IOCTL_CTRL_INIT, nullptr);
	if (ret != 0) {
		log_.error("[sd] disk init failed", ret);
		return ret;
	}

	/* Step 2: 挂载 FATFS 到 /SD:。 */
	ret = fs_mount(&mount_);
	if (ret != 0) {
		log_.error("[sd] mount failed", ret);
		return ret;
	}

	/* Step 3: 挂载成功后记录状态。 */
	is_mounted_ = true;
	log_.info("[sd] mounted /SD:");
	return 0;
}

/**
 * @brief 运行 SD 卡服务。
 * @return 0 表示成功。
 * @note 调用约束：仅允许在启动阶段由单线程调用（例如 app_Init 中调用一次）。
 */
int SdcardService::run() noexcept
{
	if (is_mounted_) {
		return 0;
	}

	/* 启动期重试策略：最多 3 次，每次失败后延时重试。 */
	constexpr int k_max_attempts = 3;
	const k_timeout_t k_retry_delay = K_MSEC(280);
	const k_timeout_t k_power_settle_delay = K_MSEC(220);

	int last_err = 0;
	if (!is_mounted_) {
		/* 首轮初始化前给卡和总线预留稳定时间，降低上电瞬间失败概率。 */
		k_sleep(k_power_settle_delay);
	}

	/* 重试主循环：任一轮成功即返回，全部失败返回最后错误码。 */
	for (int attempt = 1; attempt <= k_max_attempts; ++attempt) {
		last_err = init_and_mount_locked();
		if (last_err == 0) {
			return 0;
		}
		if (attempt < k_max_attempts) {
			/* 非最后一轮失败时打印并延时，再进入下一轮。 */
			char msg[96] = {};
			(void)snprintf(msg, sizeof(msg), "[sd] retry %d/%d after err=%d",
				       attempt, k_max_attempts, last_err);
			log_.info(msg);
			k_sleep(k_retry_delay);
		}
	}

	return last_err;
}

/**
 * @brief 设置初始化完成标志。
 * @param value 标志值。
 */
void SdcardService::set_initialized(bool value) noexcept
{
	if (k_mutex_lock(&mutex_, K_FOREVER) != 0) {
		return;
	}
	initialized_ = value;
	k_mutex_unlock(&mutex_);
}

/**
 * @brief 向指定文件写入数据。
 * @param path 目标文件路径。
 * @param data 待写入数据指针。
 * @param len 待写入字节数。
 * @param append true 表示追加写；false 表示覆盖写。
 * @return 0 表示成功；负值表示失败。
 */
int SdcardService::write_file(const char *path, const void *data, size_t len, bool append) noexcept
{
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
	if (!initialized_ || !is_mounted_) {
		k_mutex_unlock(&mutex_);
		return -EACCES;
	}

	fs_file_t file;
	fs_file_t_init(&file);
	const fs_mode_t mode = append ? (FS_O_CREATE | FS_O_WRITE | FS_O_APPEND)
				      : (FS_O_CREATE | FS_O_WRITE | FS_O_TRUNC);
	ret = fs_open(&file, path, mode);
	if (ret != 0) {
		log_.error("[sd] file open write failed", ret);
		k_mutex_unlock(&mutex_);
		return ret;
	}

	const uint8_t *bytes = static_cast<const uint8_t *>(data);
	size_t written = 0U;
	while (written < len) {
		const ssize_t write_ret = fs_write(&file, bytes + written, len - written);
		if (write_ret < 0) {
			const int close_ret = fs_close(&file);
			if (close_ret != 0) {
				log_.error("[sd] file close write failed", close_ret);
			}
			log_.error("[sd] file write failed", static_cast<int>(write_ret));
			k_mutex_unlock(&mutex_);
			return static_cast<int>(write_ret);
		}
		if (write_ret == 0) {
			const int close_ret = fs_close(&file);
			if (close_ret != 0) {
				log_.error("[sd] file close write failed", close_ret);
			}
			k_mutex_unlock(&mutex_);
			return -EIO;
		}
		written += static_cast<size_t>(write_ret);
	}

	const int close_ret = fs_close(&file);
	if (close_ret != 0) {
		log_.error("[sd] file close write failed", close_ret);
		k_mutex_unlock(&mutex_);
		return close_ret;
	}

	k_mutex_unlock(&mutex_);
	return 0;
}

/**
 * @brief 从指定文件读取数据到缓冲区。
 * @param path 源文件路径。
 * @param buffer 输出缓冲区。
 * @param buffer_size 输出缓冲区大小（字节）。
 * @param[out] out_len 实际读取长度。
 * @return 0 表示成功；负值表示失败。
 */
int SdcardService::read_file(const char *path, void *buffer, size_t buffer_size, size_t &out_len) noexcept
{
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
	if (!initialized_ || !is_mounted_) {
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

	uint8_t *bytes = static_cast<uint8_t *>(buffer);
	while (out_len < buffer_size) {
		const ssize_t read_ret = fs_read(&file, bytes + out_len, buffer_size - out_len);
		if (read_ret < 0) {
			const int close_ret = fs_close(&file);
			if (close_ret != 0) {
				log_.error("[sd] file close read failed", close_ret);
			}
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
			const int close_ret = fs_close(&file);
			if (close_ret != 0) {
				log_.error("[sd] file close read failed", close_ret);
			}
			log_.error("[sd] file read failed", static_cast<int>(extra_ret));
			k_mutex_unlock(&mutex_);
			return static_cast<int>(extra_ret);
		}
		if (extra_ret > 0) {
			const int close_ret = fs_close(&file);
			if (close_ret != 0) {
				log_.error("[sd] file close read failed", close_ret);
			}
			k_mutex_unlock(&mutex_);
			return -ENOSPC;
		}
	}

	const int close_ret = fs_close(&file);
	if (close_ret != 0) {
		log_.error("[sd] file close read failed", close_ret);
		k_mutex_unlock(&mutex_);
		return close_ret;
	}

	k_mutex_unlock(&mutex_);
	return 0;
}

/**
 * @brief 异步写接口预留（当前仅占位）。
 * @param path 目标文件路径。
 * @param data 待写入数据指针。
 * @param len 待写入字节数。
 * @param append true 追加写；false 覆盖写。
 * @return 当前固定返回 -ENOTSUP。
 */
int SdcardService::enqueue_write(const char *path, const void *data, size_t len, bool append) noexcept
{
	(void)path;
	(void)data;
	(void)len;
	(void)append;
	return -ENOTSUP;
}

} // namespace servers
