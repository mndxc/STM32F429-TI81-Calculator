# Power Management Spec — Custom PCB Stop Mode Sleep/Wake

> [!WARNING]
> **TARGET HARDWARE SPECIFICATION**
> This document outlines the true Stop Mode sleep strategy, which is the final target for a **custom PCB**.
>
> On the STM32F429I-DISC1 Discovery Board, this exact sleep sequence results in the display fading to bright white due to hardware limitations (RGB interface with no internal framebuffer holding black). 
> Therefore, this implementation is currently bypassed in `calculator_core.c` via the `Power_DisplayBlankAndMessage()` function, which acts as a software mock to simulate power-off without the white fade.
>
> **When transitioning to custom hardware:** Switch the function call in `Execute_Token()` back to `Power_EnterStop()` to engage this full sleep sequence.

**Status:** Implemented (but bypassed on Discovery Board)
**See also:** [docs/PCB_DESIGN.md](PCB_DESIGN.md) — target hardware BOM and power architecture (RT9471, RT8059, VBAT supply, battery monitoring).
**Date:** 2026-03-18
**Gestures:** `2nd+ON` = power down | `ON` (while sleeping) = wake

---

## 1. Design Overview

### State Machine

```
  [AWAKE] ──── 2nd+ON ────► save state ──► SDRAM self-refresh ──► Stop mode ──► [SLEEPING]
  [SLEEPING] ── ON press ──► EXTI fires ──► WFI returns ──► reinit ──► [AWAKE]
```

### Key Design Decisions

| Decision | Rationale |
|---|---|
| Stop mode (not Standby) | SRAM retained; EXTI can wake; no full reboot needed |
| SDRAM self-refresh before Stop | Preserves framebuffer + graph_buf; no redraw needed on wake |
| 2nd+ON to sleep, ON to wake | Matches TI-81 convention; plain ON = save only (existing behavior) |
| `g_sleeping` flag in EXTI ISR | Prevents ISR from queueing TOKEN_ON during wake transition |
| Suspend DefaultTask before Stop | Stops LVGL render loop; avoids race on LVGL mutex during sleep |
| PLLSAI explicit reinit on wake | `SystemClock_Config()` only restores main PLL; LTDC pixel clock (PLLSAI) must be re-enabled separately |

---

## 2. Hardware Context (Already Wired)

- **ON button:** PE6, EXTI falling-edge, pull-up, `EXTI9_5_IRQn`
- **ISR:** `EXTI9_5_IRQHandler()` → `HAL_GPIO_EXTI_Callback()` in `app_init.c`
- **Token:** `TOKEN_ON` already in `keypad_map.h`
- **Handler:** Early-exit TOKEN_ON handler already in `calculator_core.c` (lines ~1848–1860)

No hardware changes needed.

---

## 3. PLLSAI Values (from `Core/Src/stm32f4xx_hal_msp.c`)

```c
PeriphClkInitStruct.PLLSAI.PLLSAIN  = 100;
PeriphClkInitStruct.PLLSAI.PLLSAIR  = 3;
PeriphClkInitStruct.PLLSAIDivR      = RCC_PLLSAIDIVR_2;
```

These must be restored on wake since `SystemClock_Config()` does not touch PLLSAI.

---

## 4. SDRAM Refresh Rate

- SDRAM bank: `FMC_SDRAM_CMD_TARGET_BANK2`
- FMC SDRAM clock: 168 MHz / SDClockPeriod(2) = 84 MHz → period ≈ 11.9 ns
- IS42S16400J: 4096 rows, 64 ms refresh cycle → row interval = 15.625 µs
- Refresh count: `(15625 ns / 11.9 ns) − 20 = 1293 − 20 = 1273`
  *(use 1272 to match BSP default; verify against `BSP_SDRAM_Init` source)*

---

## 5. Implementation Steps

### Step 1 — Add `App_SystemClock_Reinit()` wrapper in `Core/Src/main.c`

`SystemClock_Config()` is `static` so it cannot be called from `app_init.c`.
Add a non-static wrapper in a `USER CODE` section (CubeMX preserves USER CODE blocks):

```c
/* USER CODE BEGIN 4 */
void App_SystemClock_Reinit(void) {
    SystemClock_Config();
}
/* USER CODE END 4 */
```

---

### Step 2 — Declare `App_SystemClock_Reinit()` in `App/Inc/app_init.h`

```c
void App_SystemClock_Reinit(void);
```

---

### Step 3 — Add global `g_sleeping` flag and `defaultTaskHandle` extern to `App/Src/app_init.c`

At the top of `app_init.c`, alongside the existing extern declarations:

```c
volatile bool g_sleeping = false;         /* read from ISR — must be volatile */
extern osThreadId defaultTaskHandle;      /* created in main.c freertos section */
```

Also expose `g_sleeping` in `App/Inc/app_init.h` (for use in `calculator_core.c`):

```c
extern volatile bool g_sleeping;
```

---

### Step 4 — Modify `HAL_GPIO_EXTI_Callback()` in `App/Src/app_init.c`

Current code posts `TOKEN_ON` unconditionally. Add sleep-guard:

```c
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == MatrixA0_Pin) {
        if (g_sleeping) {
            /* Wake from Stop mode — WFI returns naturally; do not queue TOKEN_ON */
            return;
        }
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        Token_t tok = TOKEN_ON;
        xQueueSendFromISR(keypadQueueHandle, &tok, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}
```

---

### Step 5 — Add `Power_EnterStop()` to `App/Src/app_init.c`

Add the full sleep/wake function. This runs in CalcCoreTask context.

```c
void Power_EnterStop(void)
{
    /* 1. Set flag before suspending DefaultTask to avoid a race where the
          render loop runs one more frame after g_sleeping is true */
    g_sleeping = true;

    /* 2. Suspend DefaultTask — stops LVGL render loop */
    vTaskSuspend(defaultTaskHandle);

    /* 3. Turn off display (ILI9341 command over SPI) */
    BSP_LCD_DisplayOff();

    /* 4. Put SDRAM into self-refresh — preserves framebuffer data.
          Must be done while FMC clock is still running (before Stop mode). */
    extern SDRAM_HandleTypeDef hsdram1;
    FMC_SDRAM_CommandTypeDef cmd = {0};
    cmd.CommandMode            = FMC_SDRAM_CMD_SELFREFRESH_MODE;
    cmd.CommandTarget          = FMC_SDRAM_CMD_TARGET_BANK2;
    cmd.AutoRefreshNumber      = 1;
    cmd.ModeRegisterDefinition = 0;
    HAL_SDRAM_SendCommand(&hsdram1, &cmd, HAL_MAX_DELAY);

    /* 5. Disable LTDC output — stops bus traffic to SDRAM during Stop mode */
    LTDC->GCR &= ~LTDC_GCR_LTDCEN;

    /* 6. Suspend HAL SysTick (prevents spurious tick IRQs waking the core) */
    HAL_SuspendTick();

    /* 7. Enter Stop mode — CPU stops here until EXTI fires (ON button) */
    HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);

    /* ===== CPU WAKES HERE — ON button EXTI has fired ===== */

    /* 8. Restore system PLL (HSE + PLL are stopped in Stop mode; HSI is the
          wake clock so execution from FLASH is fine immediately after WFI) */
    App_SystemClock_Reinit();   /* restores 168 MHz */

    /* 9. Resume HAL SysTick */
    HAL_ResumeTick();

    /* 10. Restore PLLSAI for LTDC pixel clock (not restored by SystemClock_Config) */
    RCC_PeriphCLKInitTypeDef periphClk = {0};
    periphClk.PeriphClockSelection  = RCC_PERIPHCLK_LTDC;
    periphClk.PLLSAI.PLLSAIN        = 100;
    periphClk.PLLSAI.PLLSAIR        = 3;
    periphClk.PLLSAIDivR            = RCC_PLLSAIDIVR_2;
    HAL_RCCEx_PeriphCLKConfig(&periphClk);

    /* 11. Exit SDRAM self-refresh — FMC clock restored above; SDRAM data intact */
    cmd.CommandMode = FMC_SDRAM_CMD_NORMAL_MODE;
    HAL_SDRAM_SendCommand(&hsdram1, &cmd, HAL_MAX_DELAY);
    /* Restore refresh rate (1272 counts @ 84 MHz FMC clock ≈ 15.6 µs row period) */
    HAL_SDRAM_ProgramRefreshRate(&hsdram1, 1272);

    /* 12. Re-enable LTDC */
    LTDC->GCR |= LTDC_GCR_LTDCEN;

    /* 13. Turn display back on */
    BSP_LCD_DisplayOn();

    /* 14. Resume DefaultTask — LVGL render loop resumes */
    vTaskResume(defaultTaskHandle);

    /* 15. Clear flag — subsequent ON presses will queue TOKEN_ON normally */
    g_sleeping = false;
}
```

Declare in `App/Inc/app_init.h`:

```c
void Power_EnterStop(void);
```

---

### Step 6 — Update `TOKEN_ON` handler in `App/Src/calculator_core.c`

The existing early-exit handler (before all mode-specific handlers) checks for `TOKEN_ON`.
Extend it to branch on whether 2ND is active:

```c
if (t == TOKEN_ON) {
    if (current_mode == MODE_2ND) {
        /* 2nd+ON — save state then enter Stop mode */
        PersistBlock_t block;
        Calc_BuildPersistBlock(&block);
        Persist_Save(&block);
        current_mode = MODE_NORMAL;
        return_mode  = MODE_NORMAL;
        Power_EnterStop();          /* blocks until ON button wakes the CPU */
        /* After wake: force LVGL full redraw (SDRAM intact but LVGL state may need sync) */
        lvgl_lock();
        lv_obj_invalidate(lv_scr_act());
        lvgl_unlock();
        return;
    }
    /* Plain ON — save state only (discovery board; no true power-off) */
    PersistBlock_t block;
    Calc_BuildPersistBlock(&block);
    Persist_Save(&block);
    current_mode = MODE_NORMAL;
    return_mode  = MODE_NORMAL;
    return;
}
```

Add `#include "app_init.h"` at the top of `calculator_core.c` if not already present.

---

### Step 7 — Fix heartbeat LED (bonus — same edit session)

The heartbeat LED (GPIOG pin 14) is toggled every 5 ms → 100 Hz, appearing as a dim glow.
Change to a 1 Hz blink by counting iterations in `App_DefaultTask_Run()`:

```c
/* Replace: */
HAL_GPIO_TogglePin(HEARTBEAT_PORT, HEARTBEAT_PIN);
osDelay(5);

/* With: */
static uint32_t blink_count = 0;
if (++blink_count >= 100) {          /* 100 × 5 ms = 500 ms half-period → 1 Hz */
    blink_count = 0;
    HAL_GPIO_TogglePin(HEARTBEAT_PORT, HEARTBEAT_PIN);
}
osDelay(5);
```

---

## 6. Files Changed Summary

| File | Change |
|---|---|
| `Core/Src/main.c` | Add `App_SystemClock_Reinit()` wrapper in USER CODE BEGIN 4 |
| `App/Inc/app_init.h` | Declare `Power_EnterStop()`, `App_SystemClock_Reinit()`, `extern volatile bool g_sleeping` |
| `App/Src/app_init.c` | Add `g_sleeping` flag; modify EXTI callback; add `Power_EnterStop()` |
| `App/Src/calculator_core.c` | Extend TOKEN_ON handler to branch on MODE_2ND |

---

## 7. Known Gotchas

1. **`SystemClock_Config()` is static** — cannot be called directly from app_init.c; must use the USER CODE wrapper added in Step 1. CubeMX preserves USER CODE sections on regeneration.

2. **PLLSAI is not in `SystemClock_Config()`** — It lives in `HAL_LTDC_MspInit()` (stm32f4xx_hal_msp.c). After Stop mode, PLLSAI is stopped and LTDC has no pixel clock until `HAL_RCCEx_PeriphCLKConfig()` is called with the PLLSAI values. Without this, the display stays blank after wake.

3. **SDRAM self-refresh requires FMC clock running** — Issue the self-refresh command *before* entering Stop mode. After wake, exit self-refresh *after* `App_SystemClock_Reinit()` restores the PLL (and therefore the FMC clock).

4. **Refresh rate must be restored after self-refresh exit** — `HAL_SDRAM_SendCommand(NORMAL_MODE)` alone does not re-enable the FMC refresh counter. Call `HAL_SDRAM_ProgramRefreshRate()` explicitly. Value 1272 is calculated for 84 MHz FMC clock; verify against BSP source if BSP uses a different value.

5. **LTDC must be disabled before Stop** — Otherwise LTDC continuously issues AHB reads against SDRAM-in-self-refresh, which either stalls the bus or generates spurious traffic. Disable via `LTDC->GCR &= ~LTDC_GCR_LTDCEN` before Stop; re-enable after SDRAM normal mode is restored.

6. **`g_sleeping` must be `volatile`** — Read from ISR context, written from task context. Compiler must not cache it in a register.

7. **`defaultTaskHandle` scoping** — The handle is created in main.c's FreeRTOS section. If it is not exposed in a header, `extern osThreadId defaultTaskHandle;` in app_init.c is sufficient. Do not attempt to suspend it via a queue mechanism — `vTaskSuspend()` from another task is the correct approach.

8. **FreeRTOS tick is NOT suspended** — `HAL_SuspendTick()` only suspends the HAL millisecond tick. FreeRTOS uses its own SysTick ISR (`xPortSysTickHandler`). In Stop mode, SysTick stops automatically because the system clock stops. No explicit `vTaskSuspendAll()` is needed — the CPU is halted until the EXTI fires.

9. **Wake latency** — After `HAL_PWR_EnterSTOPMode` returns, the CPU is running on HSI (16 MHz). `App_SystemClock_Reinit()` switches to the 168 MHz PLL. Until that call returns, FLASH wait states should be kept at 5 (they were set by the initial `SystemClock_Config()` before Stop; FLASH latency register is retained in Stop mode).

10. **BSP_LCD_DisplayOff / DisplayOn** — These send SPI commands to the ILI9341 controller. SPI5 clock is derived from APB2 (84 MHz / prescaler). After wake, APB2 is restored by `App_SystemClock_Reinit()`, so these calls in steps 13/14 are safe. The SPI peripheral itself retains its configuration (Stop mode preserves peripheral registers).

11. **Testing on discovery board** — The board has no battery, so pressing `2nd+ON` will sleep and `ON` will wake but the board must remain USB-powered. Verify wake by observing display re-enabling. Use the debugger to step through the wake sequence if the display stays blank (usually means PLLSAI not restored correctly).

---

## 8. Test Checklist

- [ ] Press `2nd+ON` → display turns off; heartbeat LED stops
- [ ] Press `ON` → display turns back on; calculator state intact (variables, equations, mode)
- [ ] After wake, calculator input works normally
- [ ] After wake, graphing works (proves SDRAM data survived self-refresh)
- [ ] Press `2nd+ON` multiple times in succession — no hang, no assert
- [ ] Power cycle after `2nd+ON` save → state reloads correctly on next boot (persist layer)
- [ ] Plain `ON` (not 2nd) → saves state but does NOT sleep
