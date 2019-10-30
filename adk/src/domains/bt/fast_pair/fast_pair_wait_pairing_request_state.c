/*!
\copyright  Copyright (c) 2008 - 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       fast_pair_wait_pairing_request_state.c
\brief      Fast Pair Wait for Pairing Request State Event handling
*/


#include "fast_pair_wait_pairing_request_state.h"
#include "fast_pair_pairing_if.h"
#include "fast_pair_gfps.h"
#include "fast_pair_events.h"

static bool fastPair_SendKbPResponse(fast_pair_state_event_crypto_encrypt_args_t* args)
{
    bool status = FALSE;

    DEBUG_LOG("fastPair_SendKbPResponse");

    if(args->crypto_encrypt_cfm->status == success)
    {
        fastPair_SendFPNotification(FAST_PAIR_KEY_BASED_PAIRING, (uint8 *)args->crypto_encrypt_cfm->encrypted_data);
        
        fastPair_StartPairing();
        status = TRUE;
    }
    return status;
}

static bool fastPair_HandlePairingRequest(bool* args)
{
    bool status = FALSE;
    fastPairTaskData *theFastPair;
    
    DEBUG_LOG("fastPair_HandlePairingRequest");

    theFastPair = fastPair_GetTaskData();

    /* If TRUE, the Pairing request was receieved with I/O Capapbailities set to 
       DisplayKeyboard or DisplayYes/No. If False Remote I/O Capabalities was set
       to No Input No Output
     */
    if(*args == TRUE)
    {        
        fastPair_SetState(theFastPair, FAST_PAIR_STATE_WAIT_PASSKEY);
        status = TRUE;
    }
    else
    {    
        fastPair_SetState(theFastPair, FAST_PAIR_STATE_IDLE);
    }
    return status;
}

bool fastPair_StateWaitPairingRequestHandleEvent(fast_pair_state_event_t event)
{
    bool status = FALSE;
    fastPairTaskData *theFastPair;

    theFastPair = fastPair_GetTaskData();
    DEBUG_LOG("fastPair_StateWaitPairingRequestHandleEvent event [%d]", event.id);
    /* Return if event is related to handset connection allowed/disallowed and is handled */
    if(fastPair_HandsetConnectStatusChange(event.id))
    {
        return TRUE;
    }

    switch (event.id)
    {
        case fast_pair_state_event_timer_expire:
        {
            fastPair_SetState(theFastPair, FAST_PAIR_STATE_IDLE);
        }
        break;

        case fast_pair_state_event_crypto_encrypt:
        {
            if (event.args == NULL)
            {
                return FALSE;
            }
            status = fastPair_SendKbPResponse((fast_pair_state_event_crypto_encrypt_args_t *)event.args);
        }
        break;

        case fast_pair_state_event_pairing_request:
        {
            if(event.args == NULL)
            {
                return FALSE;
            }
            else
            {
                status = fastPair_HandlePairingRequest((bool *)event.args);
            }
        }
        break;

        case fast_pair_state_event_power_off:
        {
            fastPair_SetState(theFastPair, FAST_PAIR_STATE_IDLE);
        }
        break;
        
        default:
        {
            DEBUG_LOG("Unhandled event [%d]", event.id);
        }
        break;
    }
    return status;
}
