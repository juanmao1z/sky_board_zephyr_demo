/**
 * @file platform_sensors.hpp
 * @brief 平台传感器访问接口：封装 INA226 与 AHT20 的单次读取。
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace platform {

/**
 * @brief INA226 电参样本。
 */
struct Ina226Sample {
	/** @brief 总线电压，单位 mV。 */
	int32_t bus_mv = 0;
	/** @brief 电流，单位 mA。 */
	int32_t current_ma = 0;
	/** @brief 功率，单位 mW。 */
	int32_t power_mw = 0;
	/** @brief 采样时间戳（系统启动毫秒）。 */
	int64_t ts_ms = 0;
};

/**
 * @brief AHT20 温湿度样本。
 */
struct Aht20Sample {
	/** @brief 温度，单位 milli-Celsius。 */
	int32_t temp_mc = 0;
	/** @brief 相对湿度，单位千分比（0..1000）。 */
	int32_t rh_mpermille = 0;
	/** @brief 采样时间戳（系统启动毫秒）。 */
	int64_t ts_ms = 0;
};

/**
 * @brief 传感器类型标识。
 */
enum class SensorType : uint8_t {
	/** @brief INA226 电流功率监测传感器。 */
	Ina226 = 0,
	/** @brief AHT20 温湿度传感器。 */
	Aht20 = 1,
};

/**
 * @brief 通用传感器驱动抽象接口。
 */
class ISensorDriver {
public:
	virtual ~ISensorDriver() = default;
	/**
	 * @brief 获取当前驱动绑定的传感器类型。
	 * @return 传感器类型枚举值。
	 */
	virtual SensorType type() const noexcept = 0;
	/**
	 * @brief 初始化传感器实例。
	 * @return 0 表示成功；负值表示失败。
	 */
	virtual int init() noexcept = 0;
	/**
	 * @brief 获取当前驱动输出样本结构大小。
	 * @return 样本结构大小（字节）。
	 */
	virtual size_t sample_size() const noexcept = 0;
	/**
	 * @brief 读取样本数据到调用方缓冲区。
	 * @param out 输出缓冲区指针。
	 * @param out_size 输出缓冲区字节数。
	 * @return 0 表示成功；负值表示失败。
	 */
	virtual int read(void *out, size_t out_size) noexcept = 0;
};

/**
 * @brief INA226 专用驱动抽象接口（可选类型化能力）。
 */
class IIna226Sensor : public ISensorDriver {
public:
	virtual ~IIna226Sensor() = default;
	using ISensorDriver::read;
	/**
	 * @brief 读取 INA226 样本。
	 * @param out 输出样本。
	 * @return 0 表示成功；负值表示失败。
	 */
	virtual int read(Ina226Sample &out) noexcept = 0;
};

/**
 * @brief AHT20 专用驱动抽象接口（可选类型化能力）。
 */
class IAht20Sensor : public ISensorDriver {
public:
	virtual ~IAht20Sensor() = default;
	using ISensorDriver::read;
	/**
	 * @brief 读取 AHT20 样本。
	 * @param out 输出样本。
	 * @return 0 表示成功；负值表示失败。
	 */
	virtual int read(Aht20Sample &out) noexcept = 0;
};

/**
 * @brief 传感器管理中心：支持注册 N 个驱动并统一管理初始化/采样。
 */
class SensorHub final {
public:
	/**
	 * @brief Hub 支持的最大驱动注册数量。
	 */
	static constexpr size_t kMaxDrivers = 8U;

	/**
	 * @brief 构造 SensorHub。
	 */
	SensorHub() noexcept = default;

	/**
	 * @brief 注册一个传感器驱动实例。
	 * @param driver 传感器驱动。
	 * @return 0 表示成功；-EALREADY 表示同类型已注册；-ENOSPC 表示容量不足。
	 */
	int register_driver(ISensorDriver &driver) noexcept;

	/**
	 * @brief 初始化全部传感器。
	 * @return 0 表示成功；负值表示失败。
	 */
	int init() noexcept;
	/**
	 * @brief 初始化全部传感器（与 init 同义）。
	 * @return 0 表示成功；负值表示失败。
	 */
	int init_all() noexcept;
	/**
	 * @brief 初始化指定类型传感器。
	 * @param type 传感器类型。
	 * @return 0 表示成功；负值表示失败。
	 */
	int init(SensorType type) noexcept;
	/**
	 * @brief 获取当前已注册驱动数量。
	 * @return 已注册驱动数量。
	 */
	size_t registered_count() const noexcept;
	/**
	 * @brief 按序号获取已注册传感器类型。
	 * @param index 驱动序号（0..registered_count-1）。
	 * @param[out] out_type 输出的传感器类型。
	 * @return 0 表示成功；-ENOENT 表示序号无效。
	 */
	int registered_type_at(size_t index, SensorType &out_type) const noexcept;
	/**
	 * @brief 获取指定类型传感器的样本大小。
	 * @param type 传感器类型。
	 * @param[out] out_size 样本大小（字节）。
	 * @return 0 表示成功；-ENOENT 表示该类型未注册。
	 */
	int sample_size(SensorType type, size_t &out_size) const noexcept;

	/**
	 * @brief 按类型读取样本到调用方缓冲区。
	 * @param type 传感器类型。
	 * @param out 输出缓冲区。
	 * @param out_size 输出缓冲区大小。
	 * @return 0 表示成功；负值表示失败。
	 */
	int read(SensorType type, void *out, size_t out_size) noexcept;

	/**
	 * @brief 读取 INA226 样本。
	 * @param out 输出样本。
	 * @return 0 表示成功；负值表示失败。
	 */
	int read_ina226_once(Ina226Sample &out) noexcept;

	/**
	 * @brief 读取 AHT20 样本。
	 * @param out 输出样本。
	 * @return 0 表示成功；负值表示失败。
	 */
	int read_aht20_once(Aht20Sample &out) noexcept;

private:
	/** @brief 已注册驱动槽位。 */
	struct DriverSlot {
		ISensorDriver *driver = nullptr;
		bool initialized = false;
	};

	/**
	 * @brief 查找指定类型驱动所在槽位。
	 * @param type 传感器类型。
	 * @return 槽位下标；未找到返回 -1。
	 */
	int find_slot(SensorType type) const noexcept;

	DriverSlot slots_[kMaxDrivers] = {};
	size_t driver_count_ = 0;
};

/**
 * @brief 获取全局 SensorHub 实例。
 * @return SensorHub 引用，生命周期贯穿整个程序运行期。
 */
SensorHub &sensor_hub() noexcept;

/**
 * @brief 初始化传感器设备句柄并检查就绪状态。
 * @return 0 表示成功；负值表示失败。
 */
int sensors_init() noexcept;

/**
 * @brief 执行一次 INA226 采样并输出工程内部单位。
 * @param out 输出样本。
 * @return 0 表示成功；负值表示失败。
 */
int read_ina226_once(Ina226Sample &out) noexcept;

/**
 * @brief 执行一次 AHT20 采样并输出工程内部单位。
 * @param out 输出样本。
 * @return 0 表示成功；负值表示失败。
 */
int read_aht20_once(Aht20Sample &out) noexcept;

} // namespace platform
