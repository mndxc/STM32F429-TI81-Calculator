/**
 * @file    lv_port_indev.c
 * @brief   LVGL input device port for the TI-81 keypad matrix.
 *
 * Registers the keypad matrix as an LVGL keypad input device.
 * Key scanning and token generation are handled by the keypad task —
 * this module only provides the LVGL registration glue.
 *
 * Note: Touchpad, mouse, encoder and button input devices are not used
 * on this hardware and have been intentionally omitted.
 */

#include "lv_port_indev.h"

/*---------------------------------------------------------------------------
 * Static variables
 *--------------------------------------------------------------------------*/

lv_indev_t *indev_keypad;

/*---------------------------------------------------------------------------
 * Static prototypes
 *--------------------------------------------------------------------------*/

static void keypad_read(lv_indev_t *indev, lv_indev_data_t *data);

/*---------------------------------------------------------------------------
 * Global functions
 *--------------------------------------------------------------------------*/

/**
 * @brief Registers the keypad matrix as an LVGL input device.
 */
void lv_port_indev_init(void)
{
    indev_keypad = lv_indev_create();
    lv_indev_set_type(indev_keypad, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(indev_keypad, keypad_read);
}

/*---------------------------------------------------------------------------
 * Static functions
 *--------------------------------------------------------------------------*/

/**
 * @brief LVGL keypad read callback.
 *
 * This calculator uses a queue-based token system rather than direct LVGL
 * key injection. Key events are processed by the calculator core task via
 * the keypad queue, so this callback always reports no key activity to LVGL.
 *
 * @param indev  LVGL input device handle.
 * @param data   Input state to populate.
 */
static void keypad_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    data->state = LV_INDEV_STATE_RELEASED;
    data->key   = 0;
}