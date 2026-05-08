#pragma once
#include <stdbool.h>
#include "app_common.h"  /* CalcMode_t, MODE_STO, MODE_COUNT */

/**
 * Returns true if assigning `to` to current_mode is a legal operation.
 *
 * MODE_STO is a synthetic rendering value — it is derived on the fly from
 * sto_pending and is never stored in current_mode.  Out-of-range values
 * (>= MODE_COUNT) are always illegal.
 *
 * The `from` parameter is reserved for future from→to pair validation.
 */
static inline bool CalcMode_IsValidTransition(CalcMode_t from, CalcMode_t to)
{
    (void)from;
    return (int)to < (int)MODE_COUNT && to != MODE_STO;
}
