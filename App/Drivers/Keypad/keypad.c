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
    KEYPAD_A1_PORT, KEYPAD_A2_PORT, KEYPAD_A3_PORT,
    KEYPAD_A4_PORT, KEYPAD_A5_PORT, KEYPAD_A6_PORT,
    KEYPAD_A7_PORT
};

static uint16_t Matrix_A_Pins[] = {
    KEYPAD_A1_PIN, KEYPAD_A2_PIN, KEYPAD_A3_PIN,
    KEYPAD_A4_PIN, KEYPAD_A5_PIN, KEYPAD_A6_PIN,
    KEYPAD_A7_PIN
};

/** Row input lines — read with pull-down resistors */
static GPIO_TypeDef *Matrix_B_Ports[] = {
    KEYPAD_B1_PORT, KEYPAD_B2_PORT, KEYPAD_B3_PORT,
    KEYPAD_B4_PORT, KEYPAD_B5_PORT, KEYPAD_B6_PORT,
    KEYPAD_B7_PORT, KEYPAD_B8_PORT
};

static uint16_t Matrix_B_Pins[] = {
    KEYPAD_B1_PIN, KEYPAD_B2_PIN, KEYPAD_B3_PIN, KEYPAD_B4_PIN,
    KEYPAD_B5_PIN, KEYPAD_B6_PIN, KEYPAD_B7_PIN, KEYPAD_B8_PIN
};

#define MATRIX_COLS     7
#define MATRIX_ROWS     8
#define MATRIX_NO_KEY   0xFF

/*---------------------------------------------------------------------------
 * Global functions
 *--------------------------------------------------------------------------*/

/**
 * @brief Initialises all keypad matrix GPIO pins.
 *
 * A-lines (column drivers): push-pull output, no pull, low speed.
 * B-lines (row inputs):     input with pull-down.
 *
 * Clock enables are emitted for every port used; redundant enables are
 * harmless and avoid silent failures if a port's clock was not already on.
 *
 * PE6 (ON button) is NOT configured here — on_button_init() in app_init.c
 * sets it up as a falling-edge EXTI with pull-up.
 */
void Keypad_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    /* Enable GPIO clocks for all ports used by the keypad matrix */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    /* A-lines: push-pull output, all driven LOW initially */
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;

    gpio.Pin = KEYPAD_A1_PIN; HAL_GPIO_Init(KEYPAD_A1_PORT, &gpio);
    gpio.Pin = KEYPAD_A2_PIN; HAL_GPIO_Init(KEYPAD_A2_PORT, &gpio);
    gpio.Pin = KEYPAD_A3_PIN; HAL_GPIO_Init(KEYPAD_A3_PORT, &gpio);
    gpio.Pin = KEYPAD_A4_PIN; HAL_GPIO_Init(KEYPAD_A4_PORT, &gpio);
    gpio.Pin = KEYPAD_A5_PIN; HAL_GPIO_Init(KEYPAD_A5_PORT, &gpio);
    gpio.Pin = KEYPAD_A6_PIN; HAL_GPIO_Init(KEYPAD_A6_PORT, &gpio);
    gpio.Pin = KEYPAD_A7_PIN; HAL_GPIO_Init(KEYPAD_A7_PORT, &gpio);

    /* B-lines: input with pull-down */
    gpio.Mode  = GPIO_MODE_INPUT;
    gpio.Pull  = GPIO_PULLDOWN;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;

    gpio.Pin = KEYPAD_B1_PIN; HAL_GPIO_Init(KEYPAD_B1_PORT, &gpio);
    gpio.Pin = KEYPAD_B2_PIN; HAL_GPIO_Init(KEYPAD_B2_PORT, &gpio);
    gpio.Pin = KEYPAD_B3_PIN; HAL_GPIO_Init(KEYPAD_B3_PORT, &gpio);
    gpio.Pin = KEYPAD_B4_PIN; HAL_GPIO_Init(KEYPAD_B4_PORT, &gpio);
    gpio.Pin = KEYPAD_B5_PIN; HAL_GPIO_Init(KEYPAD_B5_PORT, &gpio);
    gpio.Pin = KEYPAD_B6_PIN; HAL_GPIO_Init(KEYPAD_B6_PORT, &gpio);
    gpio.Pin = KEYPAD_B7_PIN; HAL_GPIO_Init(KEYPAD_B7_PORT, &gpio);
    gpio.Pin = KEYPAD_B8_PIN; HAL_GPIO_Init(KEYPAD_B8_PORT, &gpio);
}

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

    /* Configure all matrix GPIO pins before first scan */
    Keypad_GPIO_Init();

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