/**
 * @file    keypad.h
 * @brief   TI-81 keypad matrix driver interface.
 *
 * Defines the hardware key IDs for the 7x8 matrix, the key definition
 * structure used in the lookup table, and the scan function prototype.
 *
 * Key ID formula: (row * 7) + column, matching the Keypad_Scan() output.
 * IDs marked UNUSED correspond to unpopulated matrix positions on the
 * original TI-81 hardware.
 */

#ifndef KEYPAD_H
#define KEYPAD_H

#ifndef HOST_TEST
#include "stm32f4xx_hal.h"
#endif
#include <stdint.h>

/*---------------------------------------------------------------------------
 * Key structure
 *--------------------------------------------------------------------------*/

/**
 * @brief Defines the three token layers for a single physical key.
 *
 * Each key on the TI-81 has up to three functions depending on the
 * current input mode (normal, 2nd, alpha).
 */
typedef struct {
    uint32_t normal; /* Primary function */
    uint32_t second; /* 2nd function (yellow label) */
    uint8_t  alpha;  /* Alpha function (letter) */
} KeyDefinition_t;

/*---------------------------------------------------------------------------
 * Hardware key IDs
 *--------------------------------------------------------------------------*/

/**
 * @brief Electrical matrix positions — not the physical key layout.
 *
 * Named as B(row)_A(column) matching the matrix wiring.
 * IDs start at 1 since ID 0 is reserved for "no key pressed".
 */
enum {
    ID_B1_A2 = 1,  /* 0          */
    ID_B1_A3,      /* .          */
    ID_B1_A4,      /* (-)        */
    ID_B1_A5,      /* ENTER      */
    ID_B1_A6,      /* Down arrow */
    ID_B1_A7,      /* GRAPH      */

    ID_B2_A1,      /* STO        */
    ID_B2_A2,      /* 1          */
    ID_B2_A3,      /* 2          */
    ID_B2_A4,      /* 3          */
    ID_B2_A5,      /* +          */
    ID_B2_A6,      /* Left arrow */
    ID_B2_A7,      /* TRACE      */

    ID_B3_A1,      /* LN         */
    ID_B3_A2,      /* 4          */
    ID_B3_A3,      /* 5          */
    ID_B3_A4,      /* 6          */
    ID_B3_A5,      /* -          */
    ID_B3_A6,      /* Right arrow*/
    ID_B3_A7,      /* ZOOM       */

    ID_B4_A1,      /* LOG        */
    ID_B4_A2,      /* 7          */
    ID_B4_A3,      /* 8          */
    ID_B4_A4,      /* 9          */
    ID_B4_A5,      /* *          */
    ID_B4_A6,      /* Up arrow   */
    ID_B4_A7,      /* RANGE      */

    ID_B5_A1,      /* x squared  */
    ID_B5_A2,      /* EE         */
    ID_B5_A3,      /* (          */
    ID_B5_A4,      /* )          */
    ID_B5_A5,      /* /          */
    ID_B5_A6,      /* UNUSED     */
    ID_B5_A7,      /* Y=         */

    ID_B6_A1,      /* x^-1       */
    ID_B6_A2,      /* SIN        */
    ID_B6_A3,      /* COS        */
    ID_B6_A4,      /* TAN        */
    ID_B6_A5,      /* ^          */
    ID_B6_A6,      /* UNUSED     */
    ID_B6_A7,      /* 2nd        */

    ID_B7_A1,      /* MATH       */
    ID_B7_A2,      /* MATRIX     */
    ID_B7_A3,      /* PRGM       */
    ID_B7_A4,      /* VARS       */
    ID_B7_A5,      /* CLEAR      */
    ID_B7_A6,      /* UNUSED     */
    ID_B7_A7,      /* INS        */

    ID_B8_A1,      /* ALPHA      */
    ID_B8_A2,      /* X|T        */
    ID_B8_A3,      /* MODE       */
    ID_B8_A4,      /* UNUSED     */
    ID_B8_A5,      /* UNUSED     */
    ID_B8_A6,      /* UNUSED     */
    ID_B8_A7,      /* DEL        */

    KEYPAD_KEY_COUNT
};

/*---------------------------------------------------------------------------
 * Hardware pin assignments
 *
 * Defined here so keypad.c and app_init.c are independent of the
 * CubeMX-generated main.h Matrix* macros. A fresh CubeMX project needs no
 * keypad GPIO entries in the .ioc — Keypad_GPIO_Init() handles all of it.
 *--------------------------------------------------------------------------*/

#ifndef HOST_TEST
/* A-lines: column driver outputs (driven HIGH one at a time during scan)
 * Wiring: MatrixA1=PE5, A2=PE4, A3=PE3, A4=PE2, A5=PB7, A6=PB4, A7=PB3 */
#define KEYPAD_A1_PORT  GPIOE
#define KEYPAD_A1_PIN   GPIO_PIN_5
#define KEYPAD_A2_PORT  GPIOE
#define KEYPAD_A2_PIN   GPIO_PIN_4
#define KEYPAD_A3_PORT  GPIOE
#define KEYPAD_A3_PIN   GPIO_PIN_3
#define KEYPAD_A4_PORT  GPIOE
#define KEYPAD_A4_PIN   GPIO_PIN_2
#define KEYPAD_A5_PORT  GPIOB
#define KEYPAD_A5_PIN   GPIO_PIN_7
#define KEYPAD_A6_PORT  GPIOB
#define KEYPAD_A6_PIN   GPIO_PIN_4
#define KEYPAD_A7_PORT  GPIOB
#define KEYPAD_A7_PIN   GPIO_PIN_3

/* B-lines: row sense inputs (pull-down; read HIGH when key pressed)
 * Wiring: MatrixB1=PG9, B2=PD7, B3=PC11, B4=PC8, B5=PC3, B6=PA5, B7=PG2, B8=PG3 */
#define KEYPAD_B1_PORT  GPIOG
#define KEYPAD_B1_PIN   GPIO_PIN_9
#define KEYPAD_B2_PORT  GPIOD
#define KEYPAD_B2_PIN   GPIO_PIN_7
#define KEYPAD_B3_PORT  GPIOC
#define KEYPAD_B3_PIN   GPIO_PIN_11
#define KEYPAD_B4_PORT  GPIOC
#define KEYPAD_B4_PIN   GPIO_PIN_8
#define KEYPAD_B5_PORT  GPIOC
#define KEYPAD_B5_PIN   GPIO_PIN_3
#define KEYPAD_B6_PORT  GPIOA
#define KEYPAD_B6_PIN   GPIO_PIN_5
#define KEYPAD_B7_PORT  GPIOG
#define KEYPAD_B7_PIN   GPIO_PIN_2
#define KEYPAD_B8_PORT  GPIOG
#define KEYPAD_B8_PIN   GPIO_PIN_3

/* ON button — PE6, falling-edge EXTI, configured by on_button_init() in
 * app_init.c. Not part of the scan; listed here for a single source of truth. */
#define KEYPAD_ON_PORT  GPIOE
#define KEYPAD_ON_PIN   GPIO_PIN_6

/*---------------------------------------------------------------------------
 * Function prototypes
 *--------------------------------------------------------------------------*/

/**
 * @brief Initialises all keypad matrix GPIO pins.
 *
 * A-lines configured as push-pull outputs; B-lines as pull-down inputs.
 * The ON button (PE6) is NOT configured here — on_button_init() in
 * app_init.c sets it up as a falling-edge EXTI.
 *
 * Called once from StartKeypadTask() before the scan loop starts.
 */
void Keypad_GPIO_Init(void);

/**
 * @brief Scans the 7x8 matrix and returns the ID of the first pressed key.
 * @return Key ID in range [1, 55], or 0xFF if no key is pressed.
 */
uint8_t Keypad_Scan(void);

#endif /* HOST_TEST */

#endif /* KEYPAD_H */