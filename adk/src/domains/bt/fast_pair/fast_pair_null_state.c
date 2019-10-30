/*!
\copyright  Copyright (c) 2008 - 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       fast_pair_null_state.c
\brief      Fast Pair Null State Event handling
*/

#include "fast_pair_null_state.h"

bool fastPair_StateNullHandleEvent(fast_pair_state_event_t event)
{
    bool status = FALSE;
    fastPairTaskData *theFastPair;

    theFastPair = fastPair_GetTaskData();

    switch (event.id)
    {

        case fast_pair_state_event_timer_expire:
        case fast_pair_state_event_power_off:
        {
            fastPair_SetState(theFastPair, FAST_PAIR_STATE_IDLE);
        }
        break;

        default:
        {
            DEBUG_LOG("Unhandled event [%d]\n", event.id);
        }
        break;
    }
    return status;

}
