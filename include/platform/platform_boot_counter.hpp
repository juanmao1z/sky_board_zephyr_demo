/**
 * @file platform_boot_counter.hpp
 * @brief 上电计数平台抽象接口。
 */

#pragma once

#include <cstdint>

namespace platform {

/**
 * @brief 上电计数状态快照。
 */
struct BootCounterStatus {
  uint32_t count = 0U;
  bool eeprom_ready = false;
  bool flash_ready = false;
};

/**
 * @brief 上电计数抽象接口。
 */
class IBootCounter {
 public:
  virtual ~IBootCounter() = default;

  /**
   * @brief 初始化并返回上电计数状态。
   * @param[out] out 计数与介质状态。
   * @return 0 表示成功；负值表示失败。
   */
  virtual int init_and_get_status(BootCounterStatus& out) noexcept = 0;
};

/**
 * @brief 获取全局上电计数实例。
 * @return IBootCounter 引用，生命周期贯穿整个程序运行期。
 */
IBootCounter& boot_counter() noexcept;

}  // namespace platform
