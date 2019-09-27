/****************************************************************************
Copyright (c) 2018 Qualcomm Technologies International, Ltd.

FILE NAME
    sink_audio_routing_data.c

DESCRIPTION
    sink_audio_routing module data access
*/

#include <string.h>
#include "src/apps/common/audio_routing/sink_audio_routing_data.h"
#include "sink_audio_routing_config_def.h"
#include "src/domains/audio/src/legacy/sink_audio_routing_types.h"

typedef struct  __sink_audio_global_data_t
{
    unsigned short requested_audio_source;
    voice_mic_params_t      voice_mic_params;
    a2dp_source_priority_t  a2dp_source_priority;
    unsigned                manual_source_selection:1;
    unsigned                :5;
    unsigned                gated_audio:8;              /* Bitmask indicating which audio sources are prevented from being routed */
}sink_audio_global_data_t;

static sink_audio_global_data_t gAudioData;

void sinkAudioRoutingDataInit(void)
{
    memset(&gAudioData, 0, sizeof(gAudioData));
}

audio_source_t sinkAudioGetRequestedAudioSource(void)
{
    return gAudioData.requested_audio_source;
}

void sinkAudioSetRequestedAudioSource(audio_source_t source)
{
    gAudioData.requested_audio_source = source;
}

voice_mic_params_t * sinkAudioGetVoiceMicParams(void)
{
    return &gAudioData.voice_mic_params;
}

void sinkAudioSetVoiceMicParams(voice_mic_params_t * voice_mic_params)
{
    gAudioData.voice_mic_params = *voice_mic_params;
}

void sinkAudioSetA2dpSourcePriority(a2dp_source_priority_t priority)
{
    gAudioData.a2dp_source_priority = priority;
}

a2dp_source_priority_t sinkAudioGetA2dpSourcePriority(void)
{
    return gAudioData.a2dp_source_priority;
}

bool sinkAudioGetManualSourceSelectionEnabled(void)
{
    return gAudioData.manual_source_selection;
}

void sinkAudioSetManualSourceSelectionEnabled(bool enable)
{
    gAudioData.manual_source_selection = enable;
}

audio_gating sinkAudioGetGatedAudio(void)
{
    return gAudioData.gated_audio;
}

void sinkAudioSetGatedAudio(audio_gating gated_audio)
{
    gAudioData.gated_audio = gated_audio;
}
