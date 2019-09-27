/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of audio source interface for shadow A2DP.
*/

#ifdef INCLUDE_SHADOWING

#include "shadow_profile_private.h"
#include "shadow_profile_audio_source.h"

#include "audio_sources.h"
#include "kymera_adaptation_audio_protected.h"
#include "volume_system.h"
#include "logging.h"

#include <panic.h>
#include <stream.h>
#include <stdlib.h>
#include <sink.h>

static bool shadowProfile_GetConnectParameters(audio_source_t source, source_defined_params_t *source_params);
static void shadowProfile_FreeParameters(audio_source_t source, source_defined_params_t *source_params);
static bool shadowProfile_GetDisconnectParameters(audio_source_t source, source_defined_params_t *source_params);
static bool shadowProfile_IsAudioAvailable(audio_source_t source);

static const audio_source_audio_interface_t shadow_audio_interface =
{
    .GetConnectParameters = shadowProfile_GetConnectParameters,
    .ReleaseConnectParameters = shadowProfile_FreeParameters,
    .GetDisconnectParameters = shadowProfile_GetDisconnectParameters,
    .ReleaseDisconnectParameters = shadowProfile_FreeParameters,
    .IsAudioAvailable = shadowProfile_IsAudioAvailable
};

static bool shadowProfile_GetConnectParameters(audio_source_t source, source_defined_params_t *source_params)
{
    audio_connect_parameters_t *params = PanicUnlessNew(audio_connect_parameters_t);
    shadow_profile_a2dp_t *a2dp_state = ShadowProfile_GetA2dpState();

    memset(params, 0, sizeof(*params));
    params->client_lock = ShadowProfile_GetA2dpStartLockAddr();
    params->client_lock_mask = SHADOW_PROFILE_AUDIO_START_LOCK;
    params->volume = Volume_CalculateOutputVolume(AudioSources_GetVolume(source));
    params->rate = a2dp_state->sample_rate;
    //params->channel_mode = 0;
    params->seid = a2dp_state->seid;
    params->sink = StreamL2capSink(a2dp_state->cid);
    params->content_protection = a2dp_state->content_protection;
    //params->bitpool = 0;
    //params->format = 0;
    params->packet_size = a2dp_state->mtu;

    source_params->data = (void *)params;
    source_params->data_length = sizeof(*params);

    UNUSED(source);

    return TRUE;
}

static bool shadowProfile_GetDisconnectParameters(audio_source_t source, source_defined_params_t *source_params)
{
    shadow_profile_a2dp_t *a2dp_state = ShadowProfile_GetA2dpState();
    audio_disconnect_parameters_t *params = PanicUnlessNew(audio_disconnect_parameters_t);

    memset(params, 0, sizeof(*params));
    params->source = StreamL2capSource(a2dp_state->cid);
    params->seid = a2dp_state->seid;

    source_params->data = (void *)params;
    source_params->data_length = sizeof(*params);

    UNUSED(source);

    return TRUE;
}

static void shadowProfile_FreeParameters(audio_source_t source, source_defined_params_t *source_params)
{
    UNUSED(source);

    free(source_params->data);
    source_params->data = NULL;
    source_params->data_length = 0;
}

static bool shadowProfile_IsAudioAvailable(audio_source_t source)
{
    bool is_available = FALSE;

    if(source == audio_source_a2dp_1 && ShadowProfile_IsSecondary())
    {
        switch (ShadowProfile_GetState())
        {
            case SHADOW_PROFILE_STATE_A2DP_CONNECTING:
            case SHADOW_PROFILE_STATE_A2DP_CONNECTED:
                is_available = TRUE;
            break;
            default:
            break;
        }
    }
    return is_available;
}

static void shadowProfile_StoreAudioConnectParameters(const audio_connect_parameters_t *params)
{
    shadow_profile_a2dp_t *a2dp = ShadowProfile_GetA2dpState();

    a2dp->cid = SinkGetL2capCid(params->sink);
    a2dp->mtu = params->packet_size;
    a2dp->seid = params->seid;
    a2dp->sample_rate = params->rate;
    a2dp->content_protection = params->content_protection;

    DEBUG_LOG("shadowProfile_StoreAudioConnectParameters sink:0x%x cid:0x%x mtu:%d seid:%d rate:%d cp:%d",
            params->sink, a2dp->cid, a2dp->mtu, a2dp->seid, a2dp->sample_rate, a2dp->content_protection);
}

static void shadowProfile_ClearAudioConnectParameters(void)
{
    shadow_profile_a2dp_t *a2dp_state = ShadowProfile_GetA2dpState();
    memset(a2dp_state, 0, sizeof(*a2dp_state));
    a2dp_state->cid = L2CA_CID_INVALID;
}

void ShadowProfile_StoreAudioSourceParameters(audio_source_t source)
{
    source_defined_params_t source_params = {0, NULL};
    audio_connect_parameters_t *audio_params;

    PanicFalse(AudioSources_GetConnectParameters(source, &source_params));

    audio_params = source_params.data;

    shadowProfile_StoreAudioConnectParameters(audio_params);

    AudioSources_ReleaseConnectParameters(source, &source_params);
}

void ShadowProfile_ClearAudioSourceParameters(audio_source_t source)
{
    UNUSED(source);
    shadowProfile_ClearAudioConnectParameters();
}

const audio_source_audio_interface_t * ShadowProfile_GetAudioInterface(void)
{
    return &shadow_audio_interface;
}

#endif /* INCLUDE_SHADOWING */
