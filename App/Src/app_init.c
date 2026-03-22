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
#include "app_init.h"
#include "cmsis_os.h"
#include "usb_host.h"
#include "app_common.h"
#include "ui_palette.h"
#include "keypad.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "lvgl.h"
#include "stm32f429i_discovery_lcd.h"
#include "stm32f429i_discovery_sdram.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

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

/* defaultTaskHandle is created in main.c — extern here for vTaskSuspend/Resume */
extern osThreadId defaultTaskHandle;

/* Set true while entering/sleeping in Stop mode.
 * Volatile: read from ISR (HAL_GPIO_EXTI_Callback), written from CalcCoreTask. */
volatile bool g_sleeping = false;

/*---------------------------------------------------------------------------
 * Public functions
 *---------------------------------------------------------------------------*/

/**
 * @brief  Configures PE6 (KEYPAD_ON_PIN) as the ON button EXTI input.
 *
 * On a default DISC1 project PE6 is unconfigured. This runs after
 * MX_GPIO_Init() and sets PE6 as a pull-up input with a falling-edge EXTI.
 * Pin constants come from keypad.h so this function has no dependency on
 * CubeMX-generated main.h macros.
 *
 * Called from App_RTOS_Init() — entirely in App code, no generated file
 * modifications required.
 */
static void on_button_init(void)
{
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin  = KEYPAD_ON_PIN;        /* PE6 */
    gpio.Mode = GPIO_MODE_IT_FALLING; /* button shorts to GND on press */
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(KEYPAD_ON_PORT, &gpio);

    /* Priority 5 — at or below configMAX_SYSCALL_INTERRUPT_PRIORITY on F4,
     * required for xQueueSendFromISR to be safe */
    HAL_NVIC_SetPriority(EXTI9_5_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
}

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

    on_button_init();
}

/**
 * @brief  EXTI line[9:5] IRQ — shared handler for pins 5–9 on any port.
 *         Only PE6 (ON button) is configured on this line.
 *         Defined here rather than in the generated stm32f4xx_it.c so that
 *         no generated file needs modification.
 */
void EXTI9_5_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(KEYPAD_ON_PIN);
}

/**
 * @brief  EXTI callback — fires on PE6 falling edge (ON button pressed).
 *
 * Two cases:
 *  - Normal (awake): posts TOKEN_ON to the keypad queue so Execute_Token
 *    handles the save on the CalcCore task.
 *  - Wake from Stop mode (g_sleeping == true): the CPU has already exited
 *    WFI; Power_EnterStop() will continue executing.  Do NOT post TOKEN_ON —
 *    that would trigger a spurious save immediately after wake.
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin != KEYPAD_ON_PIN) { return; }
    if (g_sleeping) { return; }          /* waking from Stop — nothing to queue */
    if (keypadQueueHandle == NULL) { return; }

    Token_t token = TOKEN_ON;
    BaseType_t woken = pdFALSE;
    xQueueSendFromISR(keypadQueueHandle, &token, &woken);
    portYIELD_FROM_ISR(woken);
}

/**
 * @brief  Enters STM32 Stop mode (ultra-low power sleep) and returns after
 *         the ON button (PE6 EXTI) wakes the CPU.
 *
 * Called from Execute_Token() on the CalcCoreTask after 2nd+ON is pressed.
 * The caller must have already saved state via Persist_Save() before calling.
 *
 * Wake sequence (inline after HAL_PWR_EnterSTOPMode returns):
 *   1. Restore system PLL (SystemClock_Config)
 *   2. Restore PLLSAI for LTDC pixel clock
 *   3. Exit SDRAM self-refresh + restore refresh rate
 *   4. Re-enable LTDC output
 *   5. Turn display on, resume DefaultTask, clear flag
 */
void Power_EnterStop(void)
{
    extern SDRAM_HandleTypeDef hsdram1;
    FMC_SDRAM_CommandTypeDef cmd = {0};

    /* 1. Signal ISR to skip TOKEN_ON on wake */
    g_sleeping = true;

    /* 2. Suspend DefaultTask to stop the LVGL render loop */
    vTaskSuspend(defaultTaskHandle);

    /* 3. Fill the LTDC framebuffer with black so LTDC actively drives black pixels.
     *    In RGB interface mode the ILI9341 has no internal frame buffer — when the
     *    pixel clock stops the panel capacitors bleed off to white.  Writing black
     *    here means the panel shows black for the osDelay window, so by the time
     *    LTDC is disabled the display is already dark rather than fading. */
    memset((void *)LCD_FRAMEBUFFER_ADDR, 0,
           LCD_WIDTH * LCD_HEIGHT * LCD_BYTES_PER_PIXEL);
    osDelay(20);  /* hold black for at least one full frame scan (~17 ms at 60 Hz) */

    /* 4. Disable LTDC — display is already showing black, no capacitor bleed */
    LTDC->GCR &= ~LTDC_GCR_LTDCEN;

    /* 5a. Tell ILI9341 to power off its output stage (belt-and-suspenders) */
    BSP_LCD_DisplayOff();

    /* 5b. Put SDRAM into self-refresh — chip refreshes itself; data is preserved.
     *     LTDC is now stopped so it cannot generate AHB reads against SDRAM while
     *     the SDRAM is in self-refresh (which caused the random pixel output). */
    cmd.CommandMode            = FMC_SDRAM_CMD_SELFREFRESH_MODE;
    cmd.CommandTarget          = FMC_SDRAM_CMD_TARGET_BANK2;
    cmd.AutoRefreshNumber      = 1;
    cmd.ModeRegisterDefinition = 0;
    HAL_SDRAM_SendCommand(&hsdram1, &cmd, HAL_MAX_DELAY);

    /* 6. Suspend HAL SysTick (prevents spurious tick IRQs from waking the core) */
    HAL_SuspendTick();

    /* 7. Enter Stop mode — CPU halts here until PE6 EXTI fires (ON button) */
    HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);

    /* ===== CPU WAKES HERE — execution resumes on HSI at reduced speed ===== */

    /* 8. Restore system PLL (Stop mode stops HSE + PLL; HSI is the wake clock).
     *    FLASH is accessible from HSI so this call from FLASH is safe. */
    App_SystemClock_Reinit();

    /* 9. Resume HAL SysTick */
    HAL_ResumeTick();

    /* 10. Restore PLLSAI for LTDC pixel clock.
     *     SystemClock_Config only restores the main PLL; PLLSAI must be
     *     re-enabled explicitly with the values from HAL_LTDC_MspInit()
     *     (stm32f4xx_hal_msp.c: PLLSAIN=100, PLLSAIR=3, PLLSAIDivR=2). */
    RCC_PeriphCLKInitTypeDef periphClk = {0};
    periphClk.PeriphClockSelection = RCC_PERIPHCLK_LTDC;
    periphClk.PLLSAI.PLLSAIN       = 176;
    periphClk.PLLSAI.PLLSAIR       = 4;
    periphClk.PLLSAIDivR            = RCC_PLLSAIDIVR_8;
    HAL_RCCEx_PeriphCLKConfig(&periphClk);

    /* 11. Exit SDRAM self-refresh — FMC clock restored above, data intact */
    cmd.CommandMode = FMC_SDRAM_CMD_NORMAL_MODE;
    HAL_SDRAM_SendCommand(&hsdram1, &cmd, HAL_MAX_DELAY);
    /* Restore refresh rate: IS42S16400J, 4096 rows, 64 ms cycle.
     * At 84 MHz FMC SDRAM clock: (15625 ns / 11.9 ns) - 20 = 1273 counts. */
    HAL_SDRAM_ProgramRefreshRate(&hsdram1, 1272);

    /* 12. Re-enable LTDC output */
    LTDC->GCR |= LTDC_GCR_LTDCEN;

    /* 13. Turn display back on */
    BSP_LCD_DisplayOn();

    /* 14. Resume DefaultTask — LVGL render loop resumes */
    vTaskResume(defaultTaskHandle);

    /* 15. Clear flag — subsequent ON presses post TOKEN_ON normally */
    g_sleeping = false;
}

/**
 * @brief  Prototype stand-in for Stop mode sleep.
 *
 * On the STM32F429I-DISC1 the ILI9341 runs in RGB interface mode and has no
 * internal frame buffer.  Once LTDC stops clocking, the panel capacitors
 * discharge and the display fades to white — there is no hardware path to hold
 * it black.  Rather than enter actual Stop mode (invisible, then white fade),
 * this function shows a full-screen black LVGL overlay with a dim centred
 * "Powered off" label and blocks until the ON button is pressed again.
 *
 * The call site in Execute_Token is identical to where Power_EnterStop() would
 * be called, so replacing this with the real implementation on a custom PCB
 * only requires changing this function body.
 */
void Power_DisplayBlankAndMessage(void)
{
    /* 1. Create a full-screen black overlay so every active screen below
     *    is completely hidden.  The overlay is the last child of lv_scr_act()
     *    so LVGL draws it on top of everything else. */
    xSemaphoreTake(xLVGL_Mutex, portMAX_DELAY);
    lv_obj_t *overlay = lv_obj_create(lv_scr_act());
    lv_obj_set_size(overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(overlay, 0, 0);
    lv_obj_set_style_bg_color(overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_set_style_pad_all(overlay, 0, 0);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl = lv_label_create(overlay);
    lv_label_set_text(lbl, "Powered off");
    lv_obj_set_style_text_color(lbl, lv_color_hex(COLOR_GREY_DARK), 0);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
    xSemaphoreGive(xLVGL_Mutex);

    /* 2. Let DefaultTask render the overlay for at least one full frame */
    osDelay(20);

    /* 3. Drain the queue and block until the ON button is pressed.
     *    Any other tokens that arrive while the screen is "off" are discarded. */
    Token_t tok;
    do {
        xQueueReceive(keypadQueueHandle, &tok, portMAX_DELAY);
    } while (tok != TOKEN_ON);

    /* 4. Tear down the overlay — screens beneath are unchanged */
    xSemaphoreTake(xLVGL_Mutex, portMAX_DELAY);
    lv_obj_del(overlay);
    lv_obj_invalidate(lv_scr_act());
    xSemaphoreGive(xLVGL_Mutex);
}

/**
 * @brief  Application body of the default FreeRTOS task.
 *         Called from StartDefaultTask() in main.c after MX_USB_HOST_Init().
 *         Initialises hardware and LVGL, then runs the LVGL timer loop.
 */
void App_DefaultTask_Run(void)
{
    /* 1. Ensure the display clock is set to the stable 5.5 MHz (approx. 60 Hz).
     *    We do this here (not just in stm32f4xx_hal_msp.c) so that it persists
     *    even if CubeMX boilerplate is regenerated with different defaults. */
    RCC_PeriphCLKInitTypeDef periphClk = {0};
    periphClk.PeriphClockSelection = RCC_PERIPHCLK_LTDC;
    periphClk.PLLSAI.PLLSAIN       = 176;
    periphClk.PLLSAI.PLLSAIR       = 4;
    periphClk.PLLSAIDivR            = RCC_PLLSAIDIVR_8;
    HAL_RCCEx_PeriphCLKConfig(&periphClk);

    /* 2. Bring up external SDRAM — framebuffer lives here */
    BSP_SDRAM_Init();

    /* Initialise the ILI9341 display controller over SPI.
     * LTDC is disabled at this point (disabled in MX_LTDC_Init after CubeMX
     * config) so the ILI9341 SPI init sequence is not corrupted by garbage
     * RGB pixel data from an uninitialised framebuffer. */
    BSP_LCD_Init();
    BSP_LCD_LayerDefaultInit(LCD_BACKGROUND_LAYER, LCD_FRAME_BUFFER);
    BSP_LCD_SelectLayer(LCD_BACKGROUND_LAYER);

    /* Clear framebuffer to black BEFORE enabling LTDC and turning the display
     * on — this guarantees the first frame the ILI9341 sees is solid black. */
    memset((void *)LCD_FRAMEBUFFER_ADDR, 0,
           LCD_WIDTH * LCD_HEIGHT * LCD_BYTES_PER_PIXEL);

    /* Re-enable LTDC now the framebuffer is black and ILI9341 is initialised. */
    LTDC->GCR |= LTDC_GCR_LTDCEN;

    BSP_LCD_DisplayOn();

    /* Initialise LVGL and connect tick source */
    lv_init();
    lv_tick_set_cb(HAL_GetTick);

    /* Register display and input device drivers */
    lv_port_disp_init();
    lv_port_indev_init();

    /* Signal CalcCoreTask that LVGL is ready for UI creation */
    xSemaphoreGive(xLVGL_Ready);

    /* Seed the RNG — tick varies with OS/USB init duration, giving entropy */
    srand(HAL_GetTick());

    /* Verify float printf is functional (requires -u _printf_float linker flag).
     * If missing, all %.f/%.e/%.g format specifiers silently produce empty strings.
     * Signal the fault via fast heartbeat LED blink (10 Hz) and halt. */
    {
        char buf[8];
        snprintf(buf, sizeof(buf), "%.2f", 1.5f);
        while (buf[0] != '1') {
            HAL_GPIO_TogglePin(HEARTBEAT_PORT, HEARTBEAT_PIN);
            HAL_Delay(50);
        }
    }

    /* UI render loop — runs every 5 ms */
    for (;;) {
        if (xSemaphoreTake(xLVGL_Mutex, portMAX_DELAY) == pdTRUE) {
            lv_timer_handler();
            xSemaphoreGive(xLVGL_Mutex);
        }
        static uint32_t blink_count = 0;
        if (++blink_count >= 100) {   /* 100 × 5 ms = 500 ms half-period → 1 Hz */
            blink_count = 0;
            HAL_GPIO_TogglePin(HEARTBEAT_PORT, HEARTBEAT_PIN);
        }
        osDelay(5);
    }
}
