/*!
\copyright  Copyright (c) 2017 - 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       kymera_a2dp.c
\brief      Kymera A2DP message handling functions.
*/

#include "earbud_config.h"
#include "earbud_init.h"
#include "kymera_private.h"
#include "kymera_a2dp_private.h"
#include "kymera_config.h"
#include "av.h"
#include "a2dp_profile_config.h"

static void appKymeraPreStartSanity(kymeraTaskData *theKymera)
{
    /* Can only start streaming if we're currently idle */
    PanicFalse(appKymeraGetState() == KYMERA_STATE_IDLE);

    /* Ensure there are no audio chains already */
    PanicNotNull(theKymera->chain_input_handle);
    PanicNotNull(theKymera->chainu.output_vol_handle);
}

bool appKymeraHandleInternalA2dpStart(const KYMERA_INTERNAL_A2DP_START_T *msg)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    uint8 seid = msg->codec_settings.seid;
    uint32 rate = msg->codec_settings.rate;

    DEBUG_LOG("appKymeraHandleInternalA2dpStart, state %u, seid %u, rate %u", appKymeraGetState(), seid, rate);

    if (appKymeraGetState() == KYMERA_STATE_TONE_PLAYING)
    {
        /* If there is a tone still playing at this point,
         * it must be an interruptable tone, so cut it off */
        appKymeraTonePromptStop();
    }

    if (appA2dpIsSeidNonTwsSink(seid))
    {
        switch (appKymeraGetState())
        {
            case KYMERA_STATE_IDLE:
            {
                appKymeraPreStartSanity(theKymera);
                theKymera->output_rate = rate;
                theKymera->a2dp_seid = seid;
                appKymeraSetState(KYMERA_STATE_A2DP_STARTING_A);
            }
            // fall-through
            case KYMERA_STATE_A2DP_STARTING_A:
            case KYMERA_STATE_A2DP_STARTING_B:
            case KYMERA_STATE_A2DP_STARTING_C:
            {
                if (!appKymeraA2dpStartMaster(&msg->codec_settings, msg->volume_in_db))
                {
                    appKymeraSetState(appKymeraGetState() + 1);
                    return FALSE;
                }
                /* Startup is complete, now streaming */
                appKymeraSetState(KYMERA_STATE_A2DP_STREAMING);
            }
            break;
            default:
                Panic();
            break;
        }
    }
    else if (appA2dpIsSeidTwsSink(seid))
    {
        appKymeraPreStartSanity(theKymera);
        appKymeraA2dpStartSlave(&msg->codec_settings, msg->volume_in_db);
        theKymera->a2dp_seid = seid;
        theKymera->output_rate = rate;
        appKymeraSetState(KYMERA_STATE_A2DP_STREAMING);
    }
    else if (appA2dpIsSeidSource(seid))
    {
        /* Ignore attempts to start forwarding in the wrong state */
        if (appKymeraGetState() == KYMERA_STATE_A2DP_STREAMING)
        {
            appKymeraA2dpStartForwarding(&msg->codec_settings);
            appKymeraSetState(KYMERA_STATE_A2DP_STREAMING_WITH_FORWARDING);
        }
        else
        {
            /* Ignore attempts to start forwarding when not streaming */
            DEBUG_LOGF("appKymeraHandleInternalA2dpStart, ignoring start forwarding in state %u", appKymeraGetState());
        }            
    }
    else
    {
        /* Unsupported SEID, control should never reach here */
        Panic();
    }
    return TRUE;
}

void appKymeraHandleInternalA2dpStop(const KYMERA_INTERNAL_A2DP_STOP_T *msg)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    uint8 seid = msg->seid;

    DEBUG_LOGF("appKymeraHandleInternalA2dpStop, state %u, seid %u", appKymeraGetState(), seid);

    if (appA2dpIsSeidNonTwsSink(seid) || appA2dpIsSeidTwsSink(seid))
    {
        switch (appKymeraGetState())
        {
            case KYMERA_STATE_A2DP_STREAMING_WITH_FORWARDING:
                appKymeraA2dpStopForwarding();
                // Fall-through

            case KYMERA_STATE_A2DP_STREAMING:
                /* Common stop code for master/slave */
                appKymeraA2dpCommonStop(msg->source);
                theKymera->output_rate = 0;
                theKymera->a2dp_seid = AV_SEID_INVALID;
                appKymeraSetState(KYMERA_STATE_IDLE);
            break;

            case KYMERA_STATE_IDLE:
            break;

            default:
                // Report, but ignore attempts to stop in invalid states
                DEBUG_LOGF("appKymeraHandleInternalA2dpStop, invalid state %u", appKymeraGetState());
            break;
        }
    }
    else if (appA2dpIsSeidSource(seid))
    {
        if (appKymeraGetState() == KYMERA_STATE_A2DP_STREAMING_WITH_FORWARDING)
        {
            appKymeraA2dpStopForwarding();
            appKymeraSetState(KYMERA_STATE_A2DP_STREAMING);
        }
        else
        {
            /* Ignore attempts to stop forwarding when not forwarding */
            DEBUG_LOGF("appKymeraHandleInternalA2dpStop, ignoring stop forwarding in state %u", appKymeraGetState());
        }                    
    }
    else
    {
        /* Unsupported SEID, control should never reach here */
        Panic();
    }
}

void appKymeraHandleInternalA2dpSetVolume(int16 volume_in_db)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    DEBUG_LOGF("appKymeraHandleInternalA2dpSetVolume, vol %d", volume_in_db);

    switch (appKymeraGetState())
    {
        case KYMERA_STATE_A2DP_STREAMING:
        case KYMERA_STATE_A2DP_STREAMING_WITH_FORWARDING:
            appKymeraSetMainVolume(theKymera->chainu.output_vol_handle, volume_in_db);
            break;

        default:
            break;
    }
}

void appKymeraSetStereoLeftRightMix(bool stereo_lr_mix)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    DEBUG_LOG("appKymeraSetStereoLeftRightMix, %d", stereo_lr_mix);

    theKymera->enable_left_right_mix = stereo_lr_mix;

    switch (appKymeraGetState())
    {
        case KYMERA_STATE_A2DP_STREAMING:
        case KYMERA_STATE_A2DP_STREAMING_WITH_FORWARDING:
            appKymeraSetLeftRightMixerMode(theKymera->chain_input_handle,
                                           stereo_lr_mix, appConfigIsLeft());
            break;

        default:
            break;
    }
}
