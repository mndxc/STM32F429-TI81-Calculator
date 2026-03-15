/**
 * @file    lv_port_disp.c
 * @brief   LVGL display port for the STM32F429I-DISC1 board.
 *
 * Connects LVGL's rendering pipeline to the ILI9341 LCD controller via the
 * STM32 LTDC peripheral. Pixel data is written to the SDRAM framebuffer
 * with a software 90-degree counterclockwise rotation to achieve landscape
 * orientation, since the LTDC RGB interface bypasses the ILI9341's internal
 * MADCTL rotation register.
 *
 * Physical display: 240x320 portrait
 * Logical canvas:   320x240 landscape
 * Framebuffer:      SDRAM at 0xD0000000, RGB565
 */

#include "lv_port_disp.h"
#include <string.h>

/*---------------------------------------------------------------------------
 * Defines
 *--------------------------------------------------------------------------*/

#define PHYS_W          240     /* Physical display width  (portrait) */
#define PHYS_H          320     /* Physical display height (portrait) */
#define LOG_W           320     /* Logical canvas width    (landscape) */
#define LOG_H           240     /* Logical canvas height   (landscape) */

#define LCD_FRAMEBUFFER_ADDR    ((uint16_t *)0xD0000000)
#define BYTE_PER_PIXEL          (LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_RGB565))

/*---------------------------------------------------------------------------
 * Static prototypes
 *--------------------------------------------------------------------------*/

static void disp_flush(lv_display_t *disp, const lv_area_t *area,
                       uint8_t *px_map);

/*---------------------------------------------------------------------------
 * Global functions
 *--------------------------------------------------------------------------*/

/**
 * @brief Initialises the LVGL display driver.
 *
 * Creates an LVGL display instance sized to the logical landscape canvas
 * and registers the flush callback. Uses a partial render buffer of 10
 * logical rows held in internal SRAM. The LTDC and SDRAM must already be
 * initialised before calling this.
 */
void lv_port_disp_init(void)
{
    lv_display_t *disp = lv_display_create(LOG_W, LOG_H);
    lv_display_set_flush_cb(disp, disp_flush);

    /* Partial render buffer — 10 logical rows in internal SRAM */
    static uint8_t buf[LOG_W * 10 * BYTE_PER_PIXEL];
    lv_display_set_buffers(disp, buf, NULL, sizeof(buf),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
}

/*---------------------------------------------------------------------------
 * Static functions
 *--------------------------------------------------------------------------*/

/**
 * @brief LVGL flush callback — writes a rendered strip to the LTDC framebuffer
 *        with 90-degree counterclockwise rotation.
 *
 * Translates each logical pixel coordinate to its physical framebuffer
 * position. The LTDC continuously scans the physical framebuffer and the
 * rotation is invisible to the rest of the application.
 *
 * Rotation formula (counterclockwise):
 *   logical  (x, y) on 320x240
 *   physical (y, 319-x) on 240x320
 *
 * @param disp    LVGL display handle.
 * @param area    Logical screen region to update.
 * @param px_map  Rendered pixel data in RGB565 format.
 */
static void disp_flush(lv_display_t *disp, const lv_area_t *area,
                       uint8_t *px_map)
{
    uint16_t *fb  = LCD_FRAMEBUFFER_ADDR;
    uint16_t *src = (uint16_t *)px_map;

    for (int32_t y = area->y1; y <= area->y2; y++) {
        for (int32_t x = area->x1; x <= area->x2; x++) {
            fb[(PHYS_H - 1 - x) * PHYS_W + y] = *src++;
        }
    }

    lv_display_flush_ready(disp);
}