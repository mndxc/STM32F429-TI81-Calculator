/**
 * @file    app_init.h
 * @brief   Application initialisation — RTOS objects, hardware bring-up, UI loop.
 *
 * Call App_RTOS_Init() from main() before osKernelStart() to create all
 * application RTOS objects and tasks.
 */

#ifndef APP_INIT_H
#define APP_INIT_H

#include <stdbool.h>

/**
 * @brief  Creates application RTOS objects (mutex, semaphore, queue, tasks).
 *         Must be called from main() before osKernelStart().
 */
void App_RTOS_Init(void);

/**
 * @brief  Application body of the default FreeRTOS task.
 *         Called from StartDefaultTask() in main.c after MX_USB_HOST_Init().
 */
void App_DefaultTask_Run(void);

/**
 * @brief  Saves state, suspends tasks, and enters STM32 Stop mode.
 *         Returns after the ON button EXTI wakes the CPU.
 *         Restores all clocks, SDRAM, LTDC, and the render task before returning.
 */
void Power_EnterStop(void);

/**
 * @brief  Wrapper around the static SystemClock_Config() so App code can
 *         call it after waking from Stop mode.  Defined in main.c USER CODE.
 */
void App_SystemClock_Reinit(void);

/** Set to true while the CPU is entering / in Stop mode.
 *  Read from ISR context — must be volatile.
 *  The EXTI callback checks this flag to skip posting TOKEN_ON during wake. */
extern volatile bool g_sleeping;

#endif /* APP_INIT_H */
