# WS2812 TIM5 DMA Driver Framework Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add a production-ready WS2812 driver framework on `lckfb_sky_board_stm32f407` using `TIM5 + DMA + PA3`, with a service layer for animation control.

**Architecture:** Use a layered structure. Board resources and hardware wiring are declared in DTS. A platform driver translates RGB data into timer compare waveforms and pushes them through DMA. A service thread owns animation state and calls platform APIs only.

**Tech Stack:** Zephyr RTOS, STM32 TIM5 PWM, STM32 DMA, Devicetree, C++17 platform/service pattern, west build.

---

## Preconditions

1. Hardware route is fixed as `PA3 -> level shifter -> WS2812 DIN`.
2. LED count is fixed to `3` for initial bring-up.
3. Existing services (ETH, SD, IMU, sensors, time) must keep working.

## Task 1: Add board level WS2812 hardware description

**Files:**
- Modify: `myproject/sky_board_zephyr_demo/boards/st/lckfb_sky_board_stm32f407/lckfb_sky_board_stm32f407.dts`
- Optional sync: `zephyr/boards/st/lckfb_sky_board_stm32f407/lckfb_sky_board_stm32f407.dts`
- Create: `myproject/sky_board_zephyr_demo/dts/bindings/led/lckfb,ws2812-tim-dma.yaml`

**Step 1: Create devicetree binding for custom node**

Create binding with required properties:
- `chain-length` (int, required)
- `color-order` (string, enum: `grb`, `rgb`, `brg`)
- `timer` (phandle to PWM node)
- `channel` (int, timer channel index)
- `reset-us` (int, default 80)
- `status`

**Step 2: Enable TIM5 PWM on PA3**

In board dts:
- Enable `&timers5`.
- Enable child `pwm` node, create label like `pwm5`.
- Set pinctrl to `&tim5_ch4_pa3`.
- Keep current `pwm10` backlight unchanged.

**Step 3: Add WS2812 device node and alias**

Add root node, example shape:
- `compatible = "lckfb,ws2812-tim-dma"`
- `chain-length = <3>`
- `color-order = "grb"`
- `timer = <&pwm5>`
- `channel = <4>`
- `reset-us = <80>`

Add alias:
- `ws2812-0 = &ws2812_led0;`

**Step 4: Build dts compile check**

Run:
```bash
west build -b lckfb_sky_board_stm32f407 myproject/sky_board_zephyr_demo -p auto
```

Expected:
- DTS compiles with no missing binding errors.
- No conflict with existing `timers10` backlight setup.

## Task 2: Add Kconfig options for WS2812 stack

**Files:**
- Modify: `myproject/sky_board_zephyr_demo/prj.conf`

**Step 1: Add WS2812 framework flags**

Add guarded options:
- `CONFIG_PWM=y` (already on, keep)
- `CONFIG_DMA=y`
- `CONFIG_LOG=y` (already on, keep)
- Optional debug during bring-up:
  - `CONFIG_ASSERT=y`
  - `CONFIG_MAIN_STACK_SIZE` keep unchanged unless overflow appears

**Step 2: Keep non-regression**

Do not remove or weaken:
- ETH config
- SNTP config
- SD/FATFS config
- IMU/SENSOR config

**Step 3: Build config check**

Run:
```bash
west build -b lckfb_sky_board_stm32f407 myproject/sky_board_zephyr_demo -p auto
```

Expected:
- Kconfig resolves without unmet dependency warnings.

## Task 3: Add platform WS2812 interface

**Files:**
- Create: `myproject/sky_board_zephyr_demo/include/platform/platform_ws2812.hpp`

**Step 1: Define public data type**

Define:
- `struct Rgb { uint8_t r; uint8_t g; uint8_t b; };`

**Step 2: Define abstract interface**

Define `class IWs2812`:
- `virtual int init() noexcept = 0;`
- `virtual size_t size() const noexcept = 0;`
- `virtual int set_pixel(size_t index, const Rgb& c) noexcept = 0;`
- `virtual int fill(const Rgb& c) noexcept = 0;`
- `virtual int show() noexcept = 0;`
- `virtual int clear_and_show() noexcept = 0;`
- `virtual int set_global_brightness(uint8_t level) noexcept = 0;`

**Step 3: Provide singleton accessor**

Define:
- `IWs2812& ws2812() noexcept;`

## Task 4: Implement Zephyr TIM DMA backend

**Files:**
- Create: `myproject/sky_board_zephyr_demo/subsys/platform/zephyr_ws2812.cpp`

**Step 1: Read DT config at compile time**

Use DT macros against `ws2812-0` alias and binding properties.

**Step 2: Build frame encoder**

Implement encoder:
- Input: pixel array in RGB.
- Output: CCR duty tick buffer in WS2812 bit order.
- One LED = 24 bits.
- Add reset tail slots to ensure latch time (`reset-us`).

**Step 3: Implement timer + DMA transaction**

Flow:
- Configure timer period for 800 kHz symbol rate (1.25 us).
- Start PWM channel.
- Start DMA memory-to-peripheral to timer CCR.
- Wait completion with timeout.
- Stop DMA and PWM cleanly.

**Step 4: Implement API with thread safety**

Use mutex in write path:
- `set_pixel`, `fill`, `show`, `clear_and_show`.

**Step 5: Add robust error handling**

Return negative errno style codes:
- `-ENODEV`, `-EINVAL`, `-EIO`, `-ETIMEDOUT`, `-EALREADY`.

## Task 5: Add WS2812 service layer

**Files:**
- Create: `myproject/sky_board_zephyr_demo/include/servers/ws2812_service.hpp`
- Create: `myproject/sky_board_zephyr_demo/subsys/servers/ws2812_service.cpp`

**Step 1: Define service API**

Define:
- `run()`, `stop()`
- `set_mode(...)`
- `set_static_color(...)`

**Step 2: Implement worker loop**

Thread periodic update, modes:
- `Off`
- `Static`
- `Breath`
- `Rainbow`

Start with default mode `Static` low brightness white for safe bring-up.

**Step 3: Keep service independent**

No register access in service. Only call `platform::ws2812()`.

## Task 6: Integrate into app startup and build

**Files:**
- Modify: `myproject/sky_board_zephyr_demo/CMakeLists.txt`
- Modify: `myproject/sky_board_zephyr_demo/app/app_Init.cpp`

**Step 1: Add new sources**

Add:
- `subsys/platform/zephyr_ws2812.cpp`
- `subsys/servers/ws2812_service.cpp`

**Step 2: Start service in app init chain**

In `app_Init.cpp`:
- Create static `Ws2812Service`.
- Run after basic logger/display init.
- Handle errors with current logger style.

**Step 3: Non-regression boot order**

Ensure no behavior regression for:
- TimeService
- TcpService
- SdcardService
- SensorService
- ImuService

## Task 7: Verification checklist

**Files:**
- Optional update: `myproject/sky_board_zephyr_demo/docs/README` sections if behavior changed

**Step 1: Build verification**

Run:
```bash
west build -b lckfb_sky_board_stm32f407 myproject/sky_board_zephyr_demo -p auto
```

Expected:
- Build and link succeed.

**Step 2: Flash and boot log verification**

Run:
```bash
west flash
```

Expected:
- Boot succeeds with no fault.
- WS2812 service start log appears once.

**Step 3: Functional verification**

Check:
- LED pattern visible on 3 LEDs.
- No random flicker at idle.
- Mode switch works if command path exists.

**Step 4: Signal verification with scope**

Check PA3:
- Bit period about 1.25 us.
- High pulse width for 0/1 within WS2812 tolerance.
- Reset low interval larger than 50 us.

**Step 5: Full system regression**

Check existing features:
- ETH DHCP and TCP 8000
- SNTP/RTC path
- SD mount/read-write path
- INA226/AHT20 and IMU output

## Task 8: Delivery and commit strategy

**Step 1: Commit in small checkpoints**

Suggested commit sequence:
1. `feat(dts): add ws2812 tim5 pa3 board description and binding`
2. `feat(platform): add ws2812 tim dma backend and api`
3. `feat(service): add ws2812 service and app integration`
4. `chore: verify build flash and ws2812 bring-up logs`

**Step 2: Final evidence block**

Before completion, collect and present:
- build command output summary
- flash result summary
- boot log snippet with ws2812 service line
- regression checklist status

