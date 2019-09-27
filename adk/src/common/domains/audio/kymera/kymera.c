/*!
\copyright  Copyright (c) 2017-2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       kymera.c
\brief      Kymera Manager
*/

#include "kymera_private.h"
#include "kymera_config.h"
#include "av.h"
#include "a2dp_profile.h"
#include "scofwd_profile_config.h"
#include "kymera.h"
#include "usb_common.h"

/*!< State data for the DSP configuration */
kymeraTaskData  app_kymera;

#include "chains/chain_sco_nb.h"
#include "chains/chain_sco_wb.h"
#include "chains/chain_sco_swb.h"
#include "chains/chain_micfwd_wb.h"
#include "chains/chain_micfwd_nb.h"
#include "chains/chain_scofwd_wb.h"
#include "chains/chain_scofwd_nb.h"

#include "chains/chain_sco_nb_2mic.h"
#include "chains/chain_sco_wb_2mic.h"
#include "chains/chain_micfwd_wb_2mic.h"
#include "chains/chain_micfwd_nb_2mic.h"
#include "chains/chain_scofwd_wb_2mic.h"
#include "chains/chain_scofwd_nb_2mic.h"

#include "chains/chain_micfwd_send.h"
#include "chains/chain_scofwd_recv.h"
#include "chains/chain_micfwd_send_2mic.h"
#include "chains/chain_scofwd_recv_2mic.h"

/*! Macro for creating messages */
#define MAKE_KYMERA_MESSAGE(TYPE) \
    TYPE##_T *message = PanicUnlessNew(TYPE##_T);

const appKymeraScoChainInfo appKymeraScoChainTable[] =
{
  /* sco_mode sco_fwd mic_fwd cvc_2_mic chain                        rate */
  { SCO_NB,   FALSE,  FALSE,  FALSE,    &chain_sco_nb_config,          8000 },
  { SCO_WB,   FALSE,  FALSE,  FALSE,    &chain_sco_wb_config,         16000 },
  { SCO_SWB,  FALSE,  FALSE,  FALSE,    &chain_sco_swb_config,        32000 },
  { SCO_NB,   TRUE,   FALSE,  FALSE,    &chain_scofwd_nb_config,       8000 },
  { SCO_WB,   TRUE,   FALSE,  FALSE,    &chain_scofwd_wb_config,      16000 },
  { SCO_NB,   TRUE,   TRUE,   FALSE,    &chain_micfwd_nb_config,       8000 },
  { SCO_WB,   TRUE,   TRUE,   FALSE,    &chain_micfwd_wb_config,      16000 },

  { SCO_NB,   FALSE,  FALSE,  TRUE,     &chain_sco_nb_2mic_config,     8000 },
  { SCO_WB,   FALSE,  FALSE,  TRUE,     &chain_sco_wb_2mic_config,    16000 },
/*  { SCO_SWB,  FALSE,  FALSE,  TRUE,     &chain_sco_swb_2mic_config,   32000 },*/
  { SCO_NB,   TRUE,   FALSE,  TRUE,     &chain_scofwd_nb_2mic_config,  8000 },
  { SCO_WB,   TRUE,   FALSE,  TRUE,     &chain_scofwd_wb_2mic_config, 16000 },
  { SCO_NB,   TRUE,   TRUE,   TRUE,     &chain_micfwd_nb_2mic_config,  8000 },
  { SCO_WB,   TRUE,   TRUE,   TRUE,     &chain_micfwd_wb_2mic_config, 16000 },
  { NO_SCO }
};

const appKymeraScoChainInfo appKymeraScoSlaveChainTable[] =
{
  /* sco_mode sco_fwd mic_fwd cvc_2_mic chain                              rate */
  { SCO_WB,   FALSE,  FALSE,  FALSE,    &chain_scofwd_recv_config,         16000 },
  { SCO_WB,   FALSE,  TRUE,   FALSE,    &chain_micfwd_send_config,         16000 },    
  { SCO_WB,   FALSE,  FALSE,  TRUE,     &chain_scofwd_recv_2mic_config,    16000 },
  { SCO_WB,   FALSE,  TRUE,   TRUE,     &chain_micfwd_send_2mic_config,    16000 },    
  { NO_SCO }
};

static const capability_bundle_t capability_bundle[] =
{
#ifdef DOWNLOAD_SWITCHED_PASSTHROUGH
    {
        "download_switched_passthrough_consumer.edkcs",
        capability_bundle_available_p0
    },
#endif    
#ifdef DOWNLOAD_APTX_CLASSIC_DEMUX
    {
        "download_aptx_demux.edkcs",
        capability_bundle_available_p0
    },
#endif    
#ifdef DOWNLOAD_AEC_REF
    {
        "download_aec_reference.edkcs",
        capability_bundle_available_p0
    },
#endif    
#ifdef DOWNLOAD_APTX_ADAPTIVE_DECODE
    {
        "download_aptx_adaptive_decode.edkcs",
        capability_bundle_available_p0
    },
#endif    
#if defined(DOWNLOAD_ASYNC_WBS_DEC) || defined(DOWNLOAD_ASYNC_WBS_ENC)
    /*  Chains for SCO forwarding.
        Likely to update to use the downloadable AEC regardless
        as offers better TTP support (synchronisation) and other
        extensions */
    {
        "download_async_wbs.edkcs",
        capability_bundle_available_p0
    },
#endif
    {
        0, 0
    }
};

static const capability_bundle_config_t bundle_config = {capability_bundle, ARRAY_DIM(capability_bundle) - 1};


void appKymeraPromptPlay(FILE_INDEX prompt, promptFormat format, uint32 rate,
                         bool interruptible, uint16 *client_lock, uint16 client_lock_mask)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    DEBUG_LOGF("appKymeraPromptPlay, queue prompt %d, int %u", prompt, interruptible);

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_TONE_PROMPT_PLAY);
    message->tone = NULL;
    message->prompt = prompt;
    message->prompt_format = format;
    message->rate = rate;
    message->interruptible = interruptible;
    message->client_lock = client_lock;
    message->client_lock_mask = client_lock_mask;

    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_TONE_PROMPT_PLAY, message, &theKymera->lock);
    theKymera->tone_count++;
}

void appKymeraTonePlay(const ringtone_note *tone, bool interruptible,
                       uint16 *client_lock, uint16 client_lock_mask)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    DEBUG_LOGF("appKymeraTonePlay, queue tone %p, int %u", tone, interruptible);

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_TONE_PROMPT_PLAY);
    message->tone = tone;
    message->prompt = FILE_NONE;
    message->rate = KYMERA_TONE_GEN_RATE;
    message->interruptible = interruptible;
    message->client_lock = client_lock;
    message->client_lock_mask = client_lock_mask;

    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_TONE_PROMPT_PLAY, message, &theKymera->lock);
    theKymera->tone_count++;
}

void appKymeraA2dpStart(uint16 *client_lock, uint16 client_lock_mask,
                        const a2dp_codec_settings *codec_settings,
                        int16 volume_in_db, uint8 master_pre_start_delay)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    DEBUG_LOGF("appKymeraA2dpStart, seid %u, lock %u, busy_lock %u", codec_settings->seid, theKymera->lock, theKymera->busy_lock);

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_A2DP_START);
    message->lock = client_lock;
    message->lock_mask = client_lock_mask;
    message->codec_settings = *codec_settings;
    message->volume_in_db = volume_in_db;
    message->master_pre_start_delay = master_pre_start_delay;
    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_A2DP_START,
                             message,
                             &theKymera->lock);
}

void appKymeraA2dpStop(uint8 seid, Source source)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    MessageId mid = appA2dpIsSeidSource(seid) ? KYMERA_INTERNAL_A2DP_STOP_FORWARDING :
                                                KYMERA_INTERNAL_A2DP_STOP;
    DEBUG_LOGF("appKymeraA2dpStop, seid %u", seid);

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_A2DP_STOP);
    message->seid = seid;
    message->source = source;
    MessageSendConditionally(&theKymera->task, mid, message, &theKymera->lock);
}

void appKymeraA2dpSetVolume(int16 volume_in_db)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    DEBUG_LOGF("appKymeraA2dpSetVolume, volume %u", volume_in_db);

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_A2DP_SET_VOL);
    message->volume_in_db = volume_in_db;
    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_A2DP_SET_VOL, message, &theKymera->lock);
}

void appKymeraScoStartForwarding(Sink forwarding_sink, bool enable_mic_fwd)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    DEBUG_LOGF("appKymeraScoStartForwarding, queue sink %p, state %u", forwarding_sink, appKymeraGetState());
    PanicNull(forwarding_sink);

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_SCO_START_FORWARDING_TX);
    message->forwarding_sink = forwarding_sink;
    message->enable_mic_fwd = enable_mic_fwd;
    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_SCO_START_FORWARDING_TX, message, &theKymera->lock);
}

void appKymeraScoStopForwarding(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    DEBUG_LOGF("appKymeraScoStopForwarding, state %u", appKymeraGetState());

    if (!appKymeraHandleInternalScoForwardingStopTx())
        MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_SCO_STOP_FORWARDING_TX, NULL, &theKymera->lock);
}


kymera_chain_handle_t appKymeraScoCreateChain(const appKymeraScoChainInfo *info)
{    
    kymeraTaskData *theKymera = KymeraGetTaskData();
    DEBUG_LOGF("appKymeraCreateScoChain, mode %u, sco_fwd %u, mic_fwd %u, cvc_2_mic %u, rate %u", 
               info->mode, info->sco_fwd, info->mic_fwd, info->cvc_2_mic, info->rate);

    theKymera->sco_info = info;

    /* Ensure audio is turned on */
    OperatorFrameworkEnable(1);
    
    /* Configure DSP power mode appropriately for SCO chain */
    appKymeraConfigureDspPowerMode(FALSE);

    /* Create chain and return handle */
    theKymera->chainu.sco_handle = ChainCreate(info->chain);

    /* Now chain is created, we can decrement reference count */
    OperatorFrameworkEnable(0);
    
    return theKymera->chainu.sco_handle;
}

static void appKymeraScoStartHelper(Sink audio_sink, const appKymeraScoChainInfo *info, uint8 wesco,
                                    int16 volume_in_db, uint8 pre_start_delay, bool conditionally)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_SCO_START);
    PanicNull(audio_sink);

    message->audio_sink = audio_sink;
    message->wesco      = wesco;
    message->volume_in_db     = volume_in_db;
    message->pre_start_delay = pre_start_delay;
    message->sco_info   = info;
    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_SCO_START, message, conditionally ? &theKymera->busy_lock : NULL);
}

bool appKymeraScoStart(Sink audio_sink, appKymeraScoMode mode, bool *allow_scofwd, bool *allow_micfwd,
                       uint8 wesco, int16 volume_in_db, uint8 pre_start_delay)
{     
    const bool cvc_2_mic = appConfigScoMic2() != microphone_none;
    const appKymeraScoChainInfo *info = appKymeraScoFindChain(appKymeraScoChainTable,
                                                              mode, *allow_scofwd, *allow_micfwd,
                                                              cvc_2_mic);
    if (!info)
        info = appKymeraScoFindChain(appKymeraScoChainTable,
                                     mode, FALSE, FALSE, cvc_2_mic);
    
    if (info)
    {
        DEBUG_LOGF("appKymeraScoStart, queue sink 0x%x", audio_sink);
        *allow_scofwd = info->sco_fwd;
        *allow_micfwd = info->mic_fwd;
        appKymeraScoStartHelper(audio_sink, info, wesco, volume_in_db, pre_start_delay, TRUE);
        return TRUE;
    }
    else
    {
        DEBUG_LOGF("appKymeraScoStart, failed to find suitable SCO chain");
        return FALSE;
    }
}

void appKymeraScoStop(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    DEBUG_LOG("appKymeraScoStop");

    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_SCO_STOP, NULL, &theKymera->lock);
}

void appKymeraScoSlaveStartHelper(Source link_source, int16 volume_in_db, const appKymeraScoChainInfo *info, uint16 delay)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    DEBUG_LOGF("appKymeraScoSlaveStartHelper, delay %u", delay);

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_SCO_SLAVE_START);
    message->link_source = link_source;
    message->volume_in_db = volume_in_db;
    message->sco_info = info;
    message->pre_start_delay = delay;
    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_SCO_SLAVE_START, message, &theKymera->lock);
}

void appKymeraScoSlaveStart(Source link_source, int16 volume_in_db, bool allow_micfwd, uint16 pre_start_delay)
{
    DEBUG_LOGF("appKymeraScoSlaveStart, source 0x%x", link_source);
    const bool cvc_2_mic = appConfigScoMic2() != microphone_none;
    
    const appKymeraScoChainInfo *info = appKymeraScoFindChain(appKymeraScoSlaveChainTable,
                                                              SCO_WB, FALSE, allow_micfwd,
                                                              cvc_2_mic);

    
    PanicNull(link_source);
    appKymeraScoSlaveStartHelper(link_source, volume_in_db, info, pre_start_delay);
}

void appKymeraScoSlaveStop(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    DEBUG_LOG("appKymeraScoSlaveStop");

    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_SCOFWD_RX_STOP, NULL, &theKymera->lock);
}

void appKymeraScoSetVolume(int16 volume_in_db)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    DEBUG_LOGF("appKymeraScoSetVolume msg, vol %u", volume_in_db);

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_SCO_SET_VOL);
    message->volume_in_db = volume_in_db;

    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_SCO_SET_VOL, message, &theKymera->lock);
}

void appKymeraScoMicMute(bool mute)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    DEBUG_LOGF("appKymeraScoMicMute msg, mute %u", mute);

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_SCO_MIC_MUTE);
    message->mute = mute;
    MessageSend(&theKymera->task, KYMERA_INTERNAL_SCO_MIC_MUTE, message);
}

void appKymeraScoUseLocalMic(void)
{
    /* Only do something if both EBs support MIC forwarding */
    if (appConfigMicForwardingEnabled())
    {
        kymeraTaskData *theKymera = KymeraGetTaskData();

        DEBUG_LOG("appKymeraScoUseLocalMic");

        MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_MICFWD_LOCAL_MIC, NULL, &theKymera->lock);
    }
}

void appKymeraScoUseRemoteMic(void)
{
    /* Only do something if both EBs support MIC forwarding */
    if (appConfigMicForwardingEnabled())
    {
        kymeraTaskData *theKymera = KymeraGetTaskData();

        DEBUG_LOG("appKymeraScoUseRemoteMic");

        MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_MICFWD_REMOTE_MIC, NULL, &theKymera->lock);
    }
}

static void kymera_dsp_msg_handler(MessageFromOperator *op_msg)
{
    PanicFalse(op_msg->len == KYMERA_OP_MSG_LEN);

    switch (op_msg->message[KYMERA_OP_MSG_WORD_MSG_ID])
    {
        case KYMERA_OP_MSG_ID_TONE_END:
            DEBUG_LOG("KYMERA_OP_MSG_ID_TONE_END");
            appKymeraTonePromptStop();
        break;

        default:
        break;
    }
}

static void kymera_msg_handler(Task task, MessageId id, Message msg)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    UNUSED(task);
    switch (id)
    {
        case MESSAGE_FROM_OPERATOR:
            kymera_dsp_msg_handler((MessageFromOperator *)msg);
        break;

        case MESSAGE_SOURCE_EMPTY:
        break;

        case MESSAGE_STREAM_DISCONNECT:
            DEBUG_LOG("appKymera MESSAGE_STREAM_DISCONNECT");
            appKymeraTonePromptStop();
        break;

        case MESSAGE_USB_ENUMERATED:
        {
            const MESSAGE_USB_ENUMERATED_T *m = (const MESSAGE_USB_ENUMERATED_T *)msg;
            DEBUG_LOG("appkymera MESSAGE_USB_ENUMERATED.");
            KymeraAnc_TuningCreateChain(m->sample_rate);
            break;
        }


        case MESSAGE_USB_SUSPENDED:
            DEBUG_LOG("appkymera MESSAGE_USB_SUSPENDED");
            KymeraAnc_TuningDestroyChain();
        break;

        case KYMERA_INTERNAL_A2DP_START:
        {
            const KYMERA_INTERNAL_A2DP_START_T *m = (const KYMERA_INTERNAL_A2DP_START_T *)msg;
            uint8 seid = m->codec_settings.seid;

            /* Check if we are busy (due to other chain in use) */
            if (!appA2dpIsSeidSource(seid) && theKymera->busy_lock)
            {
               /* Re-send message blocked on busy_lock */
                MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_A2DP_START);
                *message = *m;
                MessageSendConditionally(&theKymera->task, id, message, &theKymera->busy_lock);
                break;
            }

            /* If there is no pre-start delay, or during the pre-start delay, the
            start can be cancelled if there is a stop on the message queue */
            MessageId mid = appA2dpIsSeidSource(seid) ? KYMERA_INTERNAL_A2DP_STOP_FORWARDING :
                                                        KYMERA_INTERNAL_A2DP_STOP;
            if (MessageCancelFirst(&theKymera->task, mid))
            {
                /* A stop on the queue was cancelled, clear the starter's lock
                and stop starting */
                DEBUG_LOGF("appKymera not starting due to queued stop, seid=%u", seid);
                if (m->lock)
                {
                    *m->lock &= ~m->lock_mask;
                }
                /* Also clear kymera's lock, since no longer starting */
                appKymeraClearStartingLock(theKymera);
                break;
            }
            if (m->master_pre_start_delay)
            {
                /* Send another message before starting kymera. */
                MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_A2DP_START);
                *message = *m;
                --message->master_pre_start_delay;
                MessageSend(&theKymera->task, id, message);
                appKymeraSetStartingLock(theKymera);
                break;
            }
        }
        // fallthrough (no message cancelled, zero master_pre_start_delay)
        case KYMERA_INTERNAL_A2DP_STARTING:
        {
            const KYMERA_INTERNAL_A2DP_START_T *m = (const KYMERA_INTERNAL_A2DP_START_T *)msg;
            if (appKymeraHandleInternalA2dpStart(m))
            {
                /* Start complete, clear locks. */
                appKymeraClearStartingLock(theKymera);
                if (m->lock)
                {
                    *m->lock &= ~m->lock_mask;
                }
            }
            else
            {
                /* Start incomplete, send another message. */
                MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_A2DP_START);
                *message = *m;
                MessageSend(&theKymera->task, KYMERA_INTERNAL_A2DP_STARTING, message);
            }
        }
        break;

        case KYMERA_INTERNAL_A2DP_STOP:
        case KYMERA_INTERNAL_A2DP_STOP_FORWARDING:
            appKymeraHandleInternalA2dpStop(msg);
        break;

        case KYMERA_INTERNAL_A2DP_SET_VOL:
        {
            KYMERA_INTERNAL_A2DP_SET_VOL_T *m = (KYMERA_INTERNAL_A2DP_SET_VOL_T *)msg;
            appKymeraHandleInternalA2dpSetVolume(m->volume_in_db);
        }
        break;

        case KYMERA_INTERNAL_SCO_START:
        {
            const KYMERA_INTERNAL_SCO_START_T *m = (const KYMERA_INTERNAL_SCO_START_T *)msg;
            if (m->pre_start_delay)
            {
                /* Resends are sent unconditonally, but the lock is set blocking
                   other new messages */
                appKymeraSetStartingLock(KymeraGetTaskData());
                appKymeraScoStartHelper(m->audio_sink, m->sco_info, m->wesco, m->volume_in_db,
                                        m->pre_start_delay - 1, FALSE);
            }
            else
            {
                appKymeraHandleInternalScoStart(m->audio_sink, m->sco_info, m->wesco, m->volume_in_db);
                appKymeraClearStartingLock(KymeraGetTaskData());
            }
        }
        break;

        case KYMERA_INTERNAL_SCO_START_FORWARDING_TX:
        {
            const KYMERA_INTERNAL_SCO_START_FORWARDING_TX_T *m =
                    (const KYMERA_INTERNAL_SCO_START_FORWARDING_TX_T*)msg;
            appKymeraHandleInternalScoForwardingStartTx(m->forwarding_sink);
        }
        break;

        case KYMERA_INTERNAL_SCO_STOP_FORWARDING_TX:
        {
            appKymeraHandleInternalScoForwardingStopTx();
        }
        break;

        case KYMERA_INTERNAL_SCO_SET_VOL:
        {
            KYMERA_INTERNAL_SCO_SET_VOL_T *m = (KYMERA_INTERNAL_SCO_SET_VOL_T *)msg;
            appKymeraHandleInternalScoSetVolume(m->volume_in_db);
        }
        break;

        case KYMERA_INTERNAL_SCO_MIC_MUTE:
        {
            KYMERA_INTERNAL_SCO_MIC_MUTE_T *m = (KYMERA_INTERNAL_SCO_MIC_MUTE_T *)msg;
            appKymeraHandleInternalScoMicMute(m->mute);
        }
        break;


        case KYMERA_INTERNAL_SCO_STOP:
        {
            appKymeraHandleInternalScoStop();
        }
        break;

        case KYMERA_INTERNAL_SCO_SLAVE_START:
        {
            const KYMERA_INTERNAL_SCO_SLAVE_START_T *m = (const KYMERA_INTERNAL_SCO_SLAVE_START_T *)msg;
            if (theKymera->busy_lock)
            {
               /* Re-send message blocked on busy_lock */
                MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_SCO_SLAVE_START);
                *message = *m;
                MessageSendConditionally(&theKymera->task, id, message, &theKymera->busy_lock);
            }

#if 0
            /* If we are not idle (a pre-requisite) and this message can be delayed,
               then re-send it. The normal situation is message delays when stopping
               A2DP/AV. That is calls were issued in the right order to stop A2DP then
               start SCO receive but the number of messages required for each were
               different, leading the 2nd action to complete 1st. */
            if (   start_req->pre_start_delay
                && appKymeraGetState() != KYMERA_STATE_IDLE)
            {
                DEBUG_LOG("appKymeraHandleInternalScoForwardingStartRx, re-queueing.");
                appKymeraScoFwdStartReceiveHelper(start_req->link_source, start_req->volume,
                                                  start_req->sco_info,
                                                  start_req->pre_start_delay - 1);
                return;
            }
#endif

            else
            {
                appKymeraHandleInternalScoSlaveStart(m->link_source, m->sco_info, m->volume_in_db);
            }
        }
        break;

        case KYMERA_INTERNAL_SCOFWD_RX_STOP:
        {
            appKymeraHandleInternalScoSlaveStop();
        }
        break;

        case KYMERA_INTERNAL_TONE_PROMPT_PLAY:
            appKymeraHandleInternalTonePromptPlay(msg);
        break;

        case KYMERA_INTERNAL_MICFWD_LOCAL_MIC:
            appKymeraSwitchSelectMic(MIC_SELECTION_LOCAL);
            break;

        case KYMERA_INTERNAL_MICFWD_REMOTE_MIC:
            appKymeraSwitchSelectMic(MIC_SELECTION_REMOTE);
            break;

        case KYMERA_INTERNAL_ANC_TUNING_START:
        {
            const KYMERA_INTERNAL_ANC_TUNING_START_T *m = (const KYMERA_INTERNAL_ANC_TUNING_START_T *)msg;
            KymeraAnc_TuningCreateChain(m->usb_rate);
        }
        break;

        case KYMERA_INTERNAL_ANC_TUNING_STOP:
            KymeraAnc_TuningDestroyChain();
            break;

        default:
            break;
    }
}

bool appKymeraInit(Task init_task)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    memset(theKymera, 0, sizeof(*theKymera));
    theKymera->task.handler = kymera_msg_handler;
    theKymera->state = KYMERA_STATE_IDLE;
    theKymera->output_rate = 0;
    theKymera->lock = theKymera->busy_lock = 0;
    theKymera->a2dp_seid = AV_SEID_INVALID;
    theKymera->tone_count = 0;
    appKymeraExternalAmpSetup();
    if (bundle_config.number_of_capability_bundles > 0)
        ChainSetDownloadableCapabilityBundleConfig(&bundle_config);
    theKymera->mic = MIC_SELECTION_LOCAL;
    Microphones_Init();

    UNUSED(init_task);
    return TRUE;
}
