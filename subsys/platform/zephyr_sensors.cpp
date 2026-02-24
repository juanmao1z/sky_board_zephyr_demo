/**
 * @file zephyr_sensors.cpp
 * @brief Zephyr 传感器适配实现：支持 N 驱动注册的 SensorHub。
 */

#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>

#include "platform/platform_sensors.hpp"

namespace {

#define INA226_NODE DT_NODELABEL(ina226_sensor)
#define AHT20_NODE DT_NODELABEL(aht20_sensor)

/** @brief 把 `sensor_value` 转为 milli 单位。 */
int32_t to_milli(const struct sensor_value& v) {
  return static_cast<int32_t>((static_cast<int64_t>(v.val1) * 1000LL) + (v.val2 / 1000));
}

/** @brief 把湿度 `sensor_value`（百分比）转为千分比。 */
int32_t humidity_percent_to_permille(const struct sensor_value& v) {
  return static_cast<int32_t>((static_cast<int64_t>(v.val1) * 10LL) + (v.val2 / 100000));
}

class ZephyrIna226Sensor final : public platform::IIna226Sensor {
 public:
  /** @brief 返回该驱动绑定的传感器类型。 */
  platform::SensorType type() const noexcept override { return platform::SensorType::Ina226; }

  /** @brief 检查设备是否就绪（幂等）。 */
  int init() noexcept override {
    if (ready_) {
      return 0;
    }
    if (dev_ == nullptr || !device_is_ready(dev_)) {
      return -ENODEV;
    }
    ready_ = true;
    return 0;
  }

  /** @brief 返回该驱动样本结构大小。 */
  size_t sample_size() const noexcept override { return sizeof(platform::Ina226Sample); }

  /**
   * @brief 读取 INA226 样本并完成单位换算。
   * @param out 输出样本（mV/mA/mW）。
   * @return 0 表示成功；负值表示失败。
   */
  int read(platform::Ina226Sample& out) noexcept override {
    /* 步骤 1：确保设备已就绪。 */
    int ret = init();
    if (ret < 0) {
      return ret;
    }

    /* 步骤 2：触发采样并拉取三路通道。 */
    ret = sensor_sample_fetch(dev_);
    if (ret < 0) {
      return ret;
    }

    struct sensor_value vbus = {};
    struct sensor_value current = {};
    struct sensor_value power = {};

    ret = sensor_channel_get(dev_, SENSOR_CHAN_VOLTAGE, &vbus);
    if (ret < 0) {
      return ret;
    }
    ret = sensor_channel_get(dev_, SENSOR_CHAN_CURRENT, &current);
    if (ret < 0) {
      return ret;
    }
    ret = sensor_channel_get(dev_, SENSOR_CHAN_POWER, &power);
    if (ret < 0) {
      return ret;
    }

    /* 步骤 3：转换为工程内部单位并补时间戳。 */
    out.bus_mv = to_milli(vbus);
    out.current_ma = to_milli(current);
    out.power_mw = to_milli(power);
    out.ts_ms = k_uptime_get();
    return 0;
  }

  /** @brief 通用缓冲区读接口，供 SensorHub 按类型统一调度。 */
  int read(void* out, size_t out_size) noexcept override {
    if (out == nullptr) {
      return -EINVAL;
    }
    if (out_size < sizeof(platform::Ina226Sample)) {
      return -ENOSPC;
    }
    return read(*static_cast<platform::Ina226Sample*>(out));
  }

 private:
#if DT_NODE_HAS_STATUS(INA226_NODE, okay)
  /** @brief INA226 设备实例。 */
  const struct device* dev_ = DEVICE_DT_GET(INA226_NODE);
#else
  /** @brief 当节点未启用时设备为空。 */
  const struct device* dev_ = nullptr;
#endif
  /** @brief 设备就绪标记。 */
  bool ready_ = false;
};

class ZephyrAht20Sensor final : public platform::IAht20Sensor {
 public:
  /** @brief 返回该驱动绑定的传感器类型。 */
  platform::SensorType type() const noexcept override { return platform::SensorType::Aht20; }

  /** @brief 检查设备是否就绪（幂等）。 */
  int init() noexcept override {
    if (ready_) {
      return 0;
    }
    if (dev_ == nullptr || !device_is_ready(dev_)) {
      return -ENODEV;
    }
    ready_ = true;
    return 0;
  }

  /** @brief 返回该驱动样本结构大小。 */
  size_t sample_size() const noexcept override { return sizeof(platform::Aht20Sample); }

  /**
   * @brief 读取 AHT20 样本并完成单位换算。
   * @param out 输出样本（milli-Celsius / 千分比湿度）。
   * @return 0 表示成功；负值表示失败。
   */
  int read(platform::Aht20Sample& out) noexcept override {
    /* 步骤 1：确保设备已就绪。 */
    int ret = init();
    if (ret < 0) {
      return ret;
    }

    /* 步骤 2：触发采样并读取温湿度通道。 */
    ret = sensor_sample_fetch(dev_);
    if (ret < 0) {
      return ret;
    }

    struct sensor_value temp = {};
    struct sensor_value humidity = {};

    ret = sensor_channel_get(dev_, SENSOR_CHAN_AMBIENT_TEMP, &temp);
    if (ret < 0) {
      return ret;
    }
    ret = sensor_channel_get(dev_, SENSOR_CHAN_HUMIDITY, &humidity);
    if (ret < 0) {
      return ret;
    }

    /* 步骤 3：转换为工程内部单位并补时间戳。 */
    out.temp_mc = to_milli(temp);
    out.rh_mpermille = humidity_percent_to_permille(humidity);
    out.ts_ms = k_uptime_get();
    return 0;
  }

  /** @brief 通用缓冲区读接口，供 SensorHub 按类型统一调度。 */
  int read(void* out, size_t out_size) noexcept override {
    if (out == nullptr) {
      return -EINVAL;
    }
    if (out_size < sizeof(platform::Aht20Sample)) {
      return -ENOSPC;
    }
    return read(*static_cast<platform::Aht20Sample*>(out));
  }

 private:
#if DT_NODE_HAS_STATUS(AHT20_NODE, okay)
  /** @brief AHT20 设备实例。 */
  const struct device* dev_ = DEVICE_DT_GET(AHT20_NODE);
#else
  /** @brief 当节点未启用时设备为空。 */
  const struct device* dev_ = nullptr;
#endif
  /** @brief 设备就绪标记。 */
  bool ready_ = false;
};

/** @brief 内置 INA226 驱动实例。 */
ZephyrIna226Sensor g_ina226_sensor;
/** @brief 内置 AHT20 驱动实例。 */
ZephyrAht20Sensor g_aht20_sensor;

}  // namespace

namespace platform {

/**
 * @brief 注册一个驱动到 Hub。
 * @note 同类型仅允许注册一次；注册数量受 kMaxDrivers 限制。
 */
int SensorHub::register_driver(ISensorDriver& driver) noexcept {
  if (find_slot(driver.type()) >= 0) {
    return -EALREADY;
  }
  if (driver_count_ >= kMaxDrivers) {
    return -ENOSPC;
  }

  slots_[driver_count_].driver = &driver;
  slots_[driver_count_].initialized = false;
  ++driver_count_;
  return 0;
}

/**
 * @brief 查找指定类型驱动槽位。
 * @param type 传感器类型。
 * @return 槽位下标；未找到返回 -1。
 */
int SensorHub::find_slot(SensorType type) const noexcept {
  for (size_t i = 0; i < driver_count_; ++i) {
    if (slots_[i].driver != nullptr && slots_[i].driver->type() == type) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

/**
 * @brief 兼容入口：初始化全部驱动。
 * @return 0 表示成功；负值表示失败。
 */
int SensorHub::init() noexcept { return init_all(); }

/**
 * @brief 初始化所有已注册驱动。
 * @return 0 表示成功；负值表示失败。
 */
int SensorHub::init_all() noexcept {
  /* 逐个初始化未完成初始化的驱动，任一失败直接返回。 */
  for (size_t i = 0; i < driver_count_; ++i) {
    if (slots_[i].driver == nullptr || slots_[i].initialized) {
      continue;
    }

    const int ret = slots_[i].driver->init();
    if (ret < 0) {
      return ret;
    }
    slots_[i].initialized = true;
  }
  return 0;
}

/**
 * @brief 按类型初始化指定驱动。
 * @param type 传感器类型。
 * @return 0 表示成功；负值表示失败。
 */
int SensorHub::init(SensorType type) noexcept {
  /* 按类型懒初始化，避免无关设备影响当前读取路径。 */
  const int idx = find_slot(type);
  if (idx < 0) {
    return -ENOENT;
  }
  if (slots_[idx].initialized) {
    return 0;
  }

  const int ret = slots_[idx].driver->init();
  if (ret < 0) {
    return ret;
  }
  slots_[idx].initialized = true;
  return 0;
}

/**
 * @brief 获取已注册驱动数量。
 * @return 当前驱动数量。
 */
size_t SensorHub::registered_count() const noexcept { return driver_count_; }

/**
 * @brief 按序号查询已注册传感器类型。
 * @param index 驱动序号。
 * @param[out] out_type 输出类型。
 * @return 0 表示成功；-ENOENT 表示序号无效。
 */
int SensorHub::registered_type_at(size_t index, SensorType& out_type) const noexcept {
  if (index >= driver_count_ || slots_[index].driver == nullptr) {
    return -ENOENT;
  }
  out_type = slots_[index].driver->type();
  return 0;
}

/**
 * @brief 查询指定类型驱动样本大小。
 * @param type 传感器类型。
 * @param[out] out_size 样本大小（字节）。
 * @return 0 表示成功；-ENOENT 表示该类型未注册。
 */
int SensorHub::sample_size(SensorType type, size_t& out_size) const noexcept {
  const int idx = find_slot(type);
  if (idx < 0 || slots_[idx].driver == nullptr) {
    return -ENOENT;
  }
  out_size = slots_[idx].driver->sample_size();
  return 0;
}

/**
 * @brief 按类型读取单次样本。
 * @param type 传感器类型。
 * @param out 输出缓冲区。
 * @param out_size 输出缓冲区大小。
 * @return 0 表示成功；负值表示失败。
 */
int SensorHub::read(SensorType type, void* out, size_t out_size) noexcept {
  /* 统一读取入口：参数检查 -> 定位驱动 -> 懒初始化 -> 读样本。 */
  if (out == nullptr) {
    return -EINVAL;
  }

  const int idx = find_slot(type);
  if (idx < 0 || slots_[idx].driver == nullptr) {
    return -ENOENT;
  }
  if (out_size < slots_[idx].driver->sample_size()) {
    return -ENOSPC;
  }

  int ret = init(type);
  if (ret < 0) {
    return ret;
  }
  return slots_[idx].driver->read(out, out_size);
}

/**
 * @brief 兼容接口：读取 INA226 样本。
 * @param out 输出样本。
 * @return 0 表示成功；负值表示失败。
 */
int SensorHub::read_ina226_once(Ina226Sample& out) noexcept {
  return read(SensorType::Ina226, &out, sizeof(out));
}

/**
 * @brief 兼容接口：读取 AHT20 样本。
 * @param out 输出样本。
 * @return 0 表示成功；负值表示失败。
 */
int SensorHub::read_aht20_once(Aht20Sample& out) noexcept {
  return read(SensorType::Aht20, &out, sizeof(out));
}

/**
 * @brief 获取全局 SensorHub 单例。
 * @return SensorHub 引用。
 */
SensorHub& sensor_hub() noexcept {
  /* 全局单例：集中完成内置驱动注册，后续可扩展注册更多驱动。 */
  static SensorHub hub;

  /* 幂等注册内置驱动，便于后续继续注册更多传感器实现。 */
  const int ina_ret = hub.register_driver(g_ina226_sensor);
  if (ina_ret < 0 && ina_ret != -EALREADY) {
    /* ignore: 延迟到实际 init/read 时通过返回码暴露问题 */
  }
  const int aht_ret = hub.register_driver(g_aht20_sensor);
  if (aht_ret < 0 && aht_ret != -EALREADY) {
    /* ignore: 延迟到实际 init/read 时通过返回码暴露问题 */
  }
  return hub;
}

/**
 * @brief 兼容接口：初始化全部传感器。
 * @return 0 表示成功；负值表示失败。
 */
int sensors_init() noexcept { return sensor_hub().init_all(); }

/**
 * @brief 兼容接口：读取 INA226 一次。
 * @param out 输出样本。
 * @return 0 表示成功；负值表示失败。
 */
int read_ina226_once(Ina226Sample& out) noexcept { return sensor_hub().read_ina226_once(out); }

/**
 * @brief 兼容接口：读取 AHT20 一次。
 * @param out 输出样本。
 * @return 0 表示成功；负值表示失败。
 */
int read_aht20_once(Aht20Sample& out) noexcept { return sensor_hub().read_aht20_once(out); }

}  // namespace platform
