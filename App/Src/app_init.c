/**
 * @file    app_init.c
 * @brief   Application initialisation — RTOS objects, hardware bring-up, UI loop.
 *
 * Owns all custom startup code that was previously embedded in the
 * CubeMX-generated main.c USER CODE sections. main.c now calls
 * App_RTOS_Init() from one USER CODE block and is otherwise untouched
 * generated code.
 */

#include "main.h"
#include "cmsis_os.h"
#include "usb_host.h"
#include "app_common.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "lvgl.h"
#include "stm32f429i_discovery_lcd.h"
#include "stm32f429i_discovery_sdram.h"
#include <string.h>

/*---------------------------------------------------------------------------
 * Private defines
 *---------------------------------------------------------------------------*/
#define LCD_FRAMEBUFFER_ADDR   0xD0000000U
#define LCD_WIDTH              240
#define LCD_HEIGHT             320
#define LCD_BYTES_PER_PIXEL    2            /* RGB565 */
#define HEARTBEAT_PIN          GPIO_PIN_14
#define HEARTBEAT_PORT         GPIOG

/*---------------------------------------------------------------------------
 * RTOS object definitions
 * (extern declarations live in app_common.h)
 *---------------------------------------------------------------------------*/
osThreadId        keypadTaskHandle;
osThreadId        calcTaskHandle;
osMessageQId      keypadQueueHandle;
SemaphoreHandle_t xLVGL_Mutex;
SemaphoreHandle_t xLVGL_Ready;

/*---------------------------------------------------------------------------
 * Public functions
 *---------------------------------------------------------------------------*/

/**
 * @brief  Creates all application RTOS objects and tasks.
 *         Call from main() before osKernelStart().
 */
void App_RTOS_Init(void)
{
    xLVGL_Mutex = xSemaphoreCreateMutex();
    xLVGL_Ready = xSemaphoreCreateBinary();

    osMessageQDef(keypadQueue, 16, uint16_t);
    keypadQueueHandle = osMessageCreate(osMessageQ(keypadQueue), NULL);

    osThreadDef(keypadTask, StartKeypadTask, osPriorityNormal, 0, 1024 * 2);
    keypadTaskHandle = osThreadCreate(osThread(keypadTask), NULL);

    osThreadDef(calcCore, StartCalcCoreTask, osPriorityNormal, 0, 1024 * 2);
    calcTaskHandle = osThreadCreate(osThread(calcCore), NULL);
}

/**
 * @brief  Printf retarget to USART1 for debug output.
 */
int _write(int file, char *ptr, int len)
{
    extern UART_HandleTypeDef huart1;
    HAL_UART_Transmit(&huart1, (uint8_t *)ptr, len, HAL_MAX_DELAY);
    return len;
}

/**
 * @brief  Application body of the default FreeRTOS task.
 *         Called from StartDefaultTask() in main.c after MX_USB_HOST_Init().
 *         Initialises hardware and LVGL, then runs the LVGL timer loop.
 */
void App_DefaultTask_Run(void)
{
    /* Bring up external SDRAM — framebuffer lives here */
    BSP_SDRAM_Init();

    /* Initialise the ILI9341 display controller over SPI */
    BSP_LCD_Init();
    BSP_LCD_LayerDefaultInit(LCD_BACKGROUND_LAYER, LCD_FRAME_BUFFER);
    BSP_LCD_SelectLayer(LCD_BACKGROUND_LAYER);
    BSP_LCD_DisplayOn();

    /* Clear framebuffer to black before LVGL takes over */
    memset((void *)LCD_FRAMEBUFFER_ADDR, 0,
           LCD_WIDTH * LCD_HEIGHT * LCD_BYTES_PER_PIXEL);

    /* Initialise LVGL and connect tick source */
    lv_init();
    lv_tick_set_cb(HAL_GetTick);

    /* Register display and input device drivers */
    lv_port_disp_init();
    lv_port_indev_init();

    /* Signal CalcCoreTask that LVGL is ready for UI creation */
    xSemaphoreGive(xLVGL_Ready);

    /* UI render loop — runs every 5 ms */
    for (;;) {
        if (xSemaphoreTake(xLVGL_Mutex, portMAX_DELAY) == pdTRUE) {
            lv_timer_handler();
            xSemaphoreGive(xLVGL_Mutex);
        }
        HAL_GPIO_TogglePin(HEARTBEAT_PORT, HEARTBEAT_PIN);
        osDelay(5);
    }
}
