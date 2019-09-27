/****************************************************************************
Copyright (c) 2018 Qualcomm Technologies International, Ltd.

FILE NAME
    sink_audio_routing_data.h

DESCRIPTION
    sink_audio_routing module data access
*/

#ifndef SINK_AUDIO_ROUTING_DATA_H_
#define SINK_AUDIO_ROUTING_DATA_H_

#include <audio_plugin_if.h>
#include <audio_instance.h>
#include "src/domains/audio/src/legacy/sink_audio_routing_types.h"
#include "audio_sources_list.h"
#include "sink_audio_routing_config_def.h"

void sinkAudioRoutingDataInit(void);
audio_source_t sinkAudioGetRequestedAudioSource(void);
void sinkAudioSetRequestedAudioSource(audio_source_t source);
voice_mic_params_t * sinkAudioGetVoiceMicParams(void);
void sinkAudioSetVoiceMicParams(voice_mic_params_t * voice_mic_params);
void sinkAudioSetA2dpSourcePriority(a2dp_source_priority_t priority);
a2dp_source_priority_t sinkAudioGetA2dpSourcePriority(void);
bool sinkAudioGetManualSourceSelectionEnabled(void);
void sinkAudioSetManualSourceSelectionEnabled(bool enable);
audio_gating sinkAudioGetGatedAudio(void);
void sinkAudioSetGatedAudio(audio_gating gated_audio);

#endif /* SINK_AUDIO_ROUTING_DATA_H_ */
