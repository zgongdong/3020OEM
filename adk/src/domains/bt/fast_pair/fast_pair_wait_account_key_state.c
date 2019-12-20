/*!
\copyright  Copyright (c) 2008 - 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       fast_pair_wait_account_key_state.c
\brief      Fast Pair Wait for Account Key State Event handling
*/


#include "fast_pair_wait_account_key_state.h"
#include "fast_pair_bloom_filter.h"
#include "fast_pair_session_data.h"
#include "fast_pair_account_key_sync.h"
#include "fast_pair_events.h"

static bool fastPair_ValidateAccountKey(fast_pair_state_event_crypto_decrypt_args_t* args)
{
    bool status = FALSE;
    fastPairTaskData *theFastPair;

    theFastPair = fastPair_GetTaskData();
    
    if (args->crypto_decrypt_cfm->status == success)
    {
        fastPair_StoreAccountKey((uint8 *)args->crypto_decrypt_cfm->decrypted_data);
        /*! Account Key Sharing */
        fastPair_AccountKeySync_Sync();

        /* Regenerate New Account Key Filter */
        fastPair_AccountKeyAdded();

        /* Set Fast Pair state to state to Idle */
        fastPair_SetState(theFastPair, FAST_PAIR_STATE_IDLE);
        status = TRUE;
    }

    return status;

}
static bool fastpair_AccountKeyWriteEventHandler(fast_pair_state_event_account_key_write_args_t* args)
{
    bool status = FALSE;
    fastPairTaskData *theFastPair;

    theFastPair = fastPair_GetTaskData();
    
    if(args->enc_data != NULL)
    {
        /* Decrypt passkey Block */
        ConnectionDecryptBlockAes(&theFastPair->task, (uint16 *)args->enc_data, theFastPair->session_data.aes_key);
        status = TRUE;
    }

    return status;
}

static bool fastPair_HandleAdvBloomFilterCalc(fast_pair_state_event_crypto_hash_args_t* args)
{
    DEBUG_LOG("fastPair_HandleAdvBloomFilterCalc");

    if(args->crypto_hash_cfm->status == success)
    {
        fastPair_AdvHandleHashCfm(args->crypto_hash_cfm);
        return TRUE;
    }
    return FALSE;
}

bool fastPair_StateWaitAccountKeyHandleEvent(fast_pair_state_event_t event)
{
    bool status = FALSE;
    fastPairTaskData *theFastPair;

    theFastPair = fastPair_GetTaskData();
    DEBUG_LOG("fastPair_StateWaitPasskeyHandleEvent event [%d]", event.id);
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
            status = TRUE;
        }
        break;
        
        case fast_pair_state_event_crypto_decrypt:
        {
            if (event.args == NULL)
            {
                return FALSE;
            }
            status = fastPair_ValidateAccountKey((fast_pair_state_event_crypto_decrypt_args_t *)event.args);
        }
        break;
        

        case fast_pair_state_event_account_key_write:
        {
            if (event.args == NULL)
            {
                return FALSE;
            }
            status = fastpair_AccountKeyWriteEventHandler((fast_pair_state_event_account_key_write_args_t *)event.args);
        }
        break;

        case fast_pair_state_event_crypto_hash:
        {
            status = fastPair_HandleAdvBloomFilterCalc((fast_pair_state_event_crypto_hash_args_t *)event.args);
        }
        break;

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
