/**
 * @file    app_common.h
 * @brief   Shared types, handles and function declarations for the calculator.
 *
 * This header is included by all application modules to provide a common
 * interface. It intentionally keeps dependencies minimal — only types and
 * declarations that are genuinely shared across multiple modules belong here.
 */

#ifndef APP_COMMON_H
#define APP_COMMON_H

#include "cmsis_os.h"   /* FreeRTOS queue and task types */
#include "keypad_map.h" /* Token_t and KeyDefinition_t */
#include <stdint.h>
#include <stdbool.h>

/*---------------------------------------------------------------------------
 * Shared types
 *--------------------------------------------------------------------------*/

/**
 * @brief Calculator input mode — controls which token layer is active.
 *
 * MODE_NORMAL: Standard key function
 * MODE_2ND:    2nd function layer (sticky, resets after one keypress)
 * MODE_ALPHA:  Alpha character layer (sticky, resets after one keypress)
 */
typedef enum {
    MODE_NORMAL,
    MODE_2ND,
    MODE_ALPHA
} CalcMode_t;

/*---------------------------------------------------------------------------
 * Graph state
 *--------------------------------------------------------------------------*/
/**
 * @brief Holds all state for the graphing subsystem.
 *
 * Initialised to ZStandard defaults (±10 range).
 */
typedef struct {
    char    equation[64];   /* Y= equation string in terms of x */
    float   x_min;          /* Left edge of graph window */
    float   x_max;          /* Right edge of graph window */
    float   y_min;          /* Bottom edge of graph window */
    float   y_max;          /* Top edge of graph window */
    float   x_scl;          /* X axis tick spacing */
    float   y_scl;          /* Y axis tick spacing */
    bool    active;         /* True when in graph mode */
} GraphState_t;

/** Global graph state — owned by calculator_core.c */
extern GraphState_t graph_state;

/*---------------------------------------------------------------------------
 * Shared handles
 *--------------------------------------------------------------------------*/

/** Queue for passing keypad tokens from the keypad task to the core task. */
extern osMessageQId keypadQueueHandle;

/*---------------------------------------------------------------------------
 * Function declarations
 *--------------------------------------------------------------------------*/

/** FreeRTOS task entry points */
void StartCalcCoreTask(void const *argument);
void StartKeypadTask(void const *argument);

/** Translates a raw hardware key ID into a token and posts it to the queue */
void Process_Hardware_Key(uint8_t key_id);

/** Processes a single token — updates internal calculator state */
void Execute_Token(Token_t t);

/** Refreshes the LVGL display label from the current input buffer */
void Update_Calculator_Display(void);

#endif /* APP_COMMON_H */