/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Shadow profile channel for sending messages between Primary & Secondary.
*/

#ifdef INCLUDE_SHADOWING

#include <stdlib.h>

#include <bdaddr.h>
#include <sink.h>

#include "audio_sources.h"
#include "kymera_adaptation_audio_protected.h"

#include "shadow_profile_signalling.h"
#include "shadow_profile_typedef.h"
#include "shadow_profile_marshal_typedef.h"
#include "shadow_profile_private.h"

/*! The stream context rate is represented as Hz/25 */
#define STREAM_CONTEXT_RATE_MULTIPLIER 25

#define peerSigTx(message, type) appPeerSigMarshalledMsgChannelTx(\
    ShadowProfile_GetTask(), \
    PEER_SIG_MSG_CHANNEL_SHADOW_PROFILE, \
    (message), MARSHAL_TYPE(type))


/*
    Functions sending a shadow_profile channel message
*/

void ShadowProfile_SendHfpVolumeToSecondary(uint8 volume)
{
    shadow_profile_hfp_volume_ind_t* msg = PanicUnlessMalloc(sizeof(*msg));
    msg->volume = volume;
    peerSigTx(msg, shadow_profile_hfp_volume_ind_t);
}

void ShadowProfile_SendHfpCodecToSecondary(hfp_codec_mode_t codec_mode)
{
    shadow_profile_hfp_codec_ind_t* msg = PanicUnlessMalloc(sizeof(*msg));
    msg->codec_mode = codec_mode;
    peerSigTx(msg, shadow_profile_hfp_codec_ind_t);
}

void ShadowProfile_SendA2dpStreamContextToSecondary(void)
{
    shadow_profile_a2dp_t *a2dp_state = ShadowProfile_GetA2dpState();

    if (appPeerSigIsConnected())
    {
        shadow_profile_stream_context_t *context = PanicUnlessMalloc(sizeof(*context));
        memset(context, 0, sizeof(*context));

        context->cid = a2dp_state->cid;
        context->mtu = a2dp_state->mtu;
        context->seid = a2dp_state->seid;
        context->sample_rate = (uint16)((a2dp_state->sample_rate) / STREAM_CONTEXT_RATE_MULTIPLIER);
        context->content_protection_type = a2dp_state->content_protection ?
                    UINT16_BUILD(AVDTP_CP_TYPE_SCMS_MSB, AVDTP_CP_TYPE_SCMS_LSB) : 0;

        peerSigTx(context, shadow_profile_stream_context_t);
    }
}

static void shadowProfile_HandleA2dpStreamContext(const shadow_profile_stream_context_t *context)
{
    shadow_profile_a2dp_t *a2dp_state = ShadowProfile_GetA2dpState();

    a2dp_state->cid = context->cid;
    a2dp_state->mtu = context->mtu;
    a2dp_state->seid = context->seid;
    a2dp_state->sample_rate = context->sample_rate * STREAM_CONTEXT_RATE_MULTIPLIER;
    a2dp_state->content_protection = (context->content_protection_type != 0);
}

/*
    Handlers for receiving shadow_profile channel messages.
*/

/* \brief Handle PEER_SIG_MSG_CHANNEL_RX_IND */
void ShadowProfile_HandlePeerSignallingMessage(const PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND_T *ind)
{
    SHADOW_LOGF("ShadowProfile_HandlePeerSignallingMessage. Channel 0x%x, type %d", ind->channel, ind->type);

    switch (ind->type)
    {
    case MARSHAL_TYPE(shadow_profile_hfp_volume_ind_t):
        {
            const shadow_profile_hfp_volume_ind_t* vol_ind = (const shadow_profile_hfp_volume_ind_t*)ind->msg;

            ShadowProfile_SetScoVolume(vol_ind->volume);
        }
        break;

    case MARSHAL_TYPE(shadow_profile_hfp_codec_ind_t):
        {
            const shadow_profile_hfp_codec_ind_t* codec_ind = (const shadow_profile_hfp_codec_ind_t*)ind->msg;

            ShadowProfile_SetScoCodec(codec_ind->codec_mode);
        }
        break;

    case MARSHAL_TYPE(shadow_profile_stream_context_t):
        shadowProfile_HandleA2dpStreamContext((const shadow_profile_stream_context_t*)ind->msg);
        break;

    default:
        SHADOW_LOGF("ShadowProfile_HandlePeerSignallingMessage unhandled type 0x%x", ind->type);
        break;
    }

    /* free unmarshalled msg */
    free(ind->msg);
}

/* \brief Handle PEER_SIG_MSG_CHANNEL_TX_CFM */
void ShadowProfile_HandlePeerSignallingMessageTxConfirm(const PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM_T *cfm)
{
    /*SHADOW_LOGF("ShadowProfile_HandlePeerSignallingMessageTxConfirm. channel %u status %u", cfm->channel, cfm->status);*/

    /* free the memory for the marshalled message now confirmed sent */
    free(cfm->msg_ptr);
}

#endif /* INCLUDE_SCOFWD */
