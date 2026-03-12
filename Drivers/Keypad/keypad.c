/**
 * @file    keypad.c
 * @brief   TI-81 keypad matrix driver and FreeRTOS scan task.
 *
 * Implements a 7x8 matrix scan for the TI-81 calculator keypad.
 * Column lines (A-lines) are driven HIGH one at a time, and row lines
 * (B-lines) are read to detect keypresses. A unique key ID is generated
 * from the column and row indices and passed to the calculator core
 * via the token queue.
 *
 * Matrix layout:
 *   A-Lines (columns): 7 output pins driven HIGH during scan
 *   B-Lines (rows):    8 input pins read with pull-down resistors
 *   Key ID formula:    (row * 7) + column
 */

#include "app_common.h"
#include "keypad.h"
#include "main.h"

/*---------------------------------------------------------------------------
 * Matrix pin definitions
 *--------------------------------------------------------------------------*/

/** Column driver lines — driven HIGH one at a time during scan */
static GPIO_TypeDef *Matrix_A_Ports[] = {
    MatrixA1_GPIO_Port, MatrixA2_GPIO_Port, MatrixA3_GPIO_Port,
    MatrixA4_GPIO_Port, MatrixA5_GPIO_Port, MatrixA6_GPIO_Port,
    MatrixA7_GPIO_Port
};

static uint16_t Matrix_A_Pins[] = {
    MatrixA1_Pin, MatrixA2_Pin, MatrixA3_Pin,
    MatrixA4_Pin, MatrixA5_Pin, MatrixA6_Pin,
    MatrixA7_Pin
};

/** Row input lines — read with pull-down resistors */
static GPIO_TypeDef *Matrix_B_Ports[] = {
    MatrixB1_GPIO_Port, MatrixB2_GPIO_Port, MatrixB3_GPIO_Port,
    MatrixB4_GPIO_Port, MatrixB5_GPIO_Port, MatrixB6_GPIO_Port,
    MatrixB7_GPIO_Port, MatrixB8_GPIO_Port
};

static uint16_t Matrix_B_Pins[] = {
    MatrixB1_Pin, MatrixB2_Pin, MatrixB3_Pin, MatrixB4_Pin,
    MatrixB5_Pin, MatrixB6_Pin, MatrixB7_Pin, MatrixB8_Pin
};

#define MATRIX_COLS     7
#define MATRIX_ROWS     8
#define MATRIX_NO_KEY   0xFF

/*---------------------------------------------------------------------------
 * Global functions
 *--------------------------------------------------------------------------*/

/**
 * @brief Scans the full 7x8 keypad matrix.
 *
 * Drives each column HIGH in sequence and reads all rows.
 * Returns immediately when the first pressed key is found.
 *
 * @return Key ID in range [0, 55], or MATRIX_NO_KEY (0xFF) if no key pressed.
 */
uint8_t Keypad_Scan(void)
{
    for (uint8_t col = 0; col < MATRIX_COLS; col++) {

        /* Drive column HIGH */
        HAL_GPIO_WritePin(Matrix_A_Ports[col], Matrix_A_Pins[col],
                          GPIO_PIN_SET);

        /* Short settle delay for membrane capacitance */
        for (volatile int i = 0; i < 100; i++);

        for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
            if (HAL_GPIO_ReadPin(Matrix_B_Ports[row],
                                 Matrix_B_Pins[row]) == GPIO_PIN_SET) {

                /* Drive column LOW before returning */
                HAL_GPIO_WritePin(Matrix_A_Ports[col], Matrix_A_Pins[col],
                                  GPIO_PIN_RESET);
                return (row * MATRIX_COLS) + col;
            }
        }

        /* Drive column LOW before moving to next */
        HAL_GPIO_WritePin(Matrix_A_Ports[col], Matrix_A_Pins[col],
                          GPIO_PIN_RESET);
    }

    return MATRIX_NO_KEY;
}

/* Arrow key IDs that auto-repeat when held */
static const uint8_t ARROW_KEY_IDS[] = {
    ID_B1_A6,   /* Down  */
    ID_B2_A6,   /* Left  */
    ID_B3_A6,   /* Right */
    ID_B4_A6,   /* Up    */
};

/* Repeat timing (scan period = 20ms) */
#define KEY_REPEAT_DELAY_TICKS  20   /* 400ms before first repeat  */
#define KEY_REPEAT_RATE_TICKS    4   /* 80ms between repeats        */

/**
 * @brief Keypad scan task.
 *
 * Scans the matrix every 20ms. On a new keypress, passes the key ID
 * to the calculator core via Process_Hardware_Key(). Simple debounce
 * is achieved by requiring the key ID to change between scans.
 *
 * Arrow keys (UP/DOWN/LEFT/RIGHT) additionally auto-repeat when held:
 * first repeat fires after KEY_REPEAT_DELAY_TICKS, then every
 * KEY_REPEAT_RATE_TICKS thereafter.
 */
void StartKeypadTask(void const *argument)
{
    (void)argument;
    uint8_t  last_key   = MATRIX_NO_KEY;
    uint32_t hold_ticks = 0;

    for (;;) {
        uint8_t current_key = Keypad_Scan();

        if (current_key != MATRIX_NO_KEY) {
            if (current_key != last_key) {
                /* New keypress — fire immediately */
                HAL_GPIO_TogglePin(GPIOG, GPIO_PIN_13);
                Process_Hardware_Key(current_key);
                hold_ticks = 0;
            } else {
                /* Same key held — repeat only for arrow keys */
                bool is_arrow = false;
                for (int i = 0; i < 4; i++) {
                    if (current_key == ARROW_KEY_IDS[i]) { is_arrow = true; break; }
                }
                if (is_arrow) {
                    hold_ticks++;
                    uint32_t after = hold_ticks - KEY_REPEAT_DELAY_TICKS;
                    if (hold_ticks == KEY_REPEAT_DELAY_TICKS ||
                        (hold_ticks > KEY_REPEAT_DELAY_TICKS &&
                         after % KEY_REPEAT_RATE_TICKS == 0)) {
                        HAL_GPIO_TogglePin(GPIOG, GPIO_PIN_13);
                        Process_Hardware_Key(current_key);
                    }
                }
            }
        } else {
            hold_ticks = 0;
        }

        last_key = current_key;
        osDelay(20);
    }
}