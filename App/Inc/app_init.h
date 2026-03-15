/**
 * @file    app_init.h
 * @brief   Application initialisation — RTOS objects, hardware bring-up, UI loop.
 *
 * Call App_RTOS_Init() from main() before osKernelStart() to create all
 * application RTOS objects and tasks.
 */

#ifndef APP_INIT_H
#define APP_INIT_H

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

#endif /* APP_INIT_H */
