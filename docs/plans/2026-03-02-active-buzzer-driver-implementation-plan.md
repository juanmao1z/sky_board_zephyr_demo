# Active Buzzer Driver Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add a PA6 timer-based active buzzer platform driver with `buzzer_init()`, `buzzer_on(freq_hz, duty_percent)`, and `buzzer_off()` APIs, including out-of-range parameter clipping.

**Architecture:** Configure buzzer PWM in board devicetree and expose it via a platform abstraction, similar to existing backlight/ws2812 platform modules. Keep feature scope driver-only: no dip-switch read and no ButtonService integration. Implement clipping in `buzzer_on()` so invalid inputs are coerced into safe ranges.

**Tech Stack:** Zephyr RTOS, devicetree, PWM driver API (`pwm_dt_spec`, `pwm_set_dt`), C++17, west build.

---

### Task 1: Add Board-Level Buzzer PWM Alias (PA6)

**Files:**
- Modify: `boards/lckfb_sky_board_stm32f407.overlay`
- Verify: `boards/st/lckfb_sky_board_stm32f407/lckfb_sky_board_stm32f407.dts`
- Test: build output from `west build`

**Step 1: Write the failing test**

Add temporary compile-time usage in new buzzer source:

```cpp
#define BUZZER_PWM_NODE DT_ALIAS(buzzer_pwm0)
static_assert(DT_NODE_HAS_STATUS(BUZZER_PWM_NODE, okay), "Missing buzzer-pwm0 alias");
```

**Step 2: Run test to verify it fails**

Run: `west build -b lckfb_sky_board_stm32f407 . -p always`  
Expected: FAIL mentioning missing `buzzer_pwm0` alias or node status.

**Step 3: Write minimal implementation**

Add buzzer alias and PA6 timer PWM binding in overlay (matching available STM32 timer channel on PA6).

**Step 4: Run test to verify it passes**

Run: `west build -b lckfb_sky_board_stm32f407 . -p always`  
Expected: PASS at devicetree generation and compile stages.

**Step 5: Commit**

```bash
git add boards/lckfb_sky_board_stm32f407.overlay
git commit -m "feat: add PA6 buzzer pwm alias in board overlay"
```

### Task 2: Add Platform Buzzer API Header

**Files:**
- Create: `include/platform/platform_buzzer.hpp`
- Test: build output from `west build`

**Step 1: Write the failing test**

Add temporary call in `app/app_Init.cpp`:

```cpp
(void)platform::buzzer_init();
```

**Step 2: Run test to verify it fails**

Run: `west build -b lckfb_sky_board_stm32f407 .`  
Expected: FAIL with unknown symbol/declaration for `platform::buzzer_init`.

**Step 3: Write minimal implementation**

Create header:

```cpp
#pragma once
#include <cstdint>
namespace platform {
int buzzer_init() noexcept;
int buzzer_on(uint32_t freq_hz, uint8_t duty_percent) noexcept;
int buzzer_off() noexcept;
}
```

**Step 4: Run test to verify it passes**

Run: `west build -b lckfb_sky_board_stm32f407 .`  
Expected: compile moves forward (link may still fail before Task 3).

**Step 5: Commit**

```bash
git add include/platform/platform_buzzer.hpp app/app_Init.cpp
git commit -m "feat: add platform buzzer api declarations"
```

### Task 3: Implement Zephyr Buzzer Backend with Clipping

**Files:**
- Create: `subsys/platform/zephyr_buzzer.cpp`
- Modify: `CMakeLists.txt`
- Test: build output from `west build`

**Step 1: Write the failing test**

Keep API declarations but no implementation source added to build.

**Step 2: Run test to verify it fails**

Run: `west build -b lckfb_sky_board_stm32f407 .`  
Expected: FAIL at link stage for unresolved `platform::buzzer_*`.

**Step 3: Write minimal implementation**

Implement with clipping constants and PWM conversion:

```cpp
constexpr uint32_t kMinFreqHz = 100U;
constexpr uint32_t kMaxFreqHz = 5000U;
constexpr uint8_t kMinDuty = 5U;
constexpr uint8_t kMaxDuty = 95U;

const uint32_t freq = std::clamp(freq_hz, kMinFreqHz, kMaxFreqHz);
const uint8_t duty = std::clamp(duty_percent, kMinDuty, kMaxDuty);
const uint32_t period_ns = 1000000000U / freq;
const uint32_t pulse_ns = (period_ns * duty) / 100U;
```

And expose:
- `buzzer_init()`: readiness check
- `buzzer_on(...)`: clipped PWM output
- `buzzer_off()`: `pulse=0`

**Step 4: Run test to verify it passes**

Run: `west build -b lckfb_sky_board_stm32f407 .`  
Expected: PASS compile and link.

**Step 5: Commit**

```bash
git add subsys/platform/zephyr_buzzer.cpp CMakeLists.txt
git commit -m "feat: implement active buzzer zephyr backend with parameter clipping"
```

### Task 4: Add Minimal Manual Invocation Path

**Files:**
- Modify: `app/app_Init.cpp`
- Test: build output and runtime log behavior

**Step 1: Write the failing test**

Add temporary call expecting compiler access to API:

```cpp
ret = platform::buzzer_on(2000U, 50U);
```

**Step 2: Run test to verify it fails**

Run: `west build -b lckfb_sky_board_stm32f407 .`  
Expected: FAIL if include/integration wiring is incomplete.

**Step 3: Write minimal implementation**

Add guarded manual test path in init:

```cpp
ret = platform::buzzer_init();
if (ret == 0) {
  (void)platform::buzzer_on(2000U, 50U);
  k_msleep(80);
  (void)platform::buzzer_off();
}
```

If startup beep is not desired, use debug-only conditional and keep as manual developer hook.

**Step 4: Run test to verify it passes**

Run:
- `west build -b lckfb_sky_board_stm32f407 .`
- Flash and confirm audible on/off control works.

Expected: PASS build and expected audible behavior.

**Step 5: Commit**

```bash
git add app/app_Init.cpp
git commit -m "feat: wire minimal active buzzer invocation path"
```

### Task 5: Validate OpenSpec and Final Behavior

**Files:**
- Modify: `openspec/changes/add-buzzer-driver-pa6-timer/tasks.md` (mark completed)
- Verify: `openspec/changes/add-buzzer-driver-pa6-timer/specs/buzzer-driver/spec.md`
- Test: strict openspec validation + build

**Step 1: Write the failing test**

Run strict spec validation before final sync.

**Step 2: Run test to verify it fails**

Run: `openspec validate --type change add-buzzer-driver-pa6-timer --strict --no-interactive`  
Expected: FAIL if artifacts drift from implemented behavior.

**Step 3: Write minimal implementation**

Sync artifacts with delivered behavior (driver-only scope, clipping semantics, no ButtonService integration).

**Step 4: Run test to verify it passes**

Run:
- `openspec validate --type change add-buzzer-driver-pa6-timer --strict --no-interactive`
- `west build -b lckfb_sky_board_stm32f407 . -p always`

Expected: both PASS.

**Step 5: Commit**

```bash
git add openspec/changes/add-buzzer-driver-pa6-timer
git commit -m "docs: align openspec artifacts with active buzzer driver implementation"
```
