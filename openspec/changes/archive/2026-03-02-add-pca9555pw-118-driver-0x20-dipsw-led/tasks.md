## 1. Board and Configuration

- [x] 1.1 Add PCA9555 node on `&i2c1` in `boards/lckfb_sky_board_stm32f407.overlay` with `compatible = "nxp,pca9555"` and `reg = <0x20>`
- [x] 1.2 Add stable alias for PCA9555 runtime lookup (for example `gpio-expander-0`)
- [x] 1.3 Enable required Kconfig options for GPIO expander driver in `prj.conf` (including explicit PCA series option if needed)

## 2. Platform Driver Interface

- [x] 2.1 Add `include/platform/platform_pca9555.hpp` with fixed-semantic APIs: `init`, `read_dipsw`, `set_leds`
- [x] 2.2 Define DIP/LED bit-width contract as 8-bit masks and document pin mapping (`io0_x` for DIP, `io1_x` for LED)
- [x] 2.3 Expose global accessor `platform::pca9555()` consistent with existing platform modules

## 3. Zephyr Implementation

- [x] 3.1 Add `subsys/platform/zephyr_pca9555.cpp` and resolve device by alias
- [x] 3.2 Implement initialization flow that configures Port0 as input and Port1 as output with all LEDs off
- [x] 3.3 Implement `read_dipsw` to return Port0 8-bit snapshot
- [x] 3.4 Implement `set_leds` to apply Port1 8-bit output mask
- [x] 3.5 Add mutex protection and diagnostic logs for init/read/write failure paths

## 4. Integration and Verification

- [x] 4.1 Register new source file in `CMakeLists.txt`
- [x] 4.2 Call PCA9555 initialization in `app/app_Init.cpp` and print startup status log
- [x] 4.3 Build firmware for `lckfb_sky_board_stm32f407` and resolve all compile/dts/kconfig issues
- [x] 4.4 Verify runtime behavior on board: DIP readback changes with switch state and LED outputs follow written mask
  - Verified on hardware with serial log and functional checks: DIP toggling changes readback, LED outputs follow written bitmap.
