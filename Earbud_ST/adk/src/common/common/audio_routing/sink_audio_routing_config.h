/****************************************************************************
Copyright (c) 2018 Qualcomm Technologies International, Ltd.

FILE NAME
    sink_audio_routing_config.h

DESCRIPTION
    sink_audio_routing module config data access
*/

#ifndef SINK_AUDIO_ROUTING_CONFIG_H_
#define SINK_AUDIO_ROUTING_CONFIG_H_

#include "audio_config.h"
#include <audio_plugin_if.h>
#include "src/domains/audio/src/legacy/sink_microphones.h"
#include "audio_sources_list.h"

bool sinkAudioGetAutoSuspendOnCallEnabled(void);
uint16 sinkAudioGetSilenceThreshold(void);
uint16 sinkAudioGetSilenceTriggerTime(void);
uint16 sinkAudioGetAmpPowerUpTime(void);
bool sinkAudioSetAmpPowerUpTime(uint16 time);
uint16 sinkAudioGetSourceDisconnectDelay(void);
uint16 sinkAudioGetAmpPowerDownTime(void);
bool sinkAudioSetAmpPowerDownTime(uint16 time);
uint8 sinkAudioGetAudioActivePio(void);
uint8 sinkAudioGetAuxOutDetectPio(void);
uint8 sinkAudioGetPowerOnPio(void);
uint8 sinkAudioGetDefaultAudioSource(void);
void sinkAudioRoutingGetSessionData(void);
void sinkAudioRoutingSetSessionData(void);
bool sinkAudioGetLowPowerCodecEnabled(void);
void sinkAudioGetSourcePriorities(audio_source_t * source_priority_list);
bool sinkAudioGetMixingOfVoiceAndAudioEnabled(void);
uint8 sinkAudioGetAudioMutePio(void);
bool sinkAudioGetManualSourceSelectionFromConfig(void);
audio_quality_t sinkAudioGetVoiceQuality(void);
audio_quality_t sinkAudioGetMusicQuality(void);
rendering_mode_t sinkAudioGetAudioRenderingMode(void);

/****************************************************************************
NAME
    SinkWiredPlayUsbAndWiredInLimbo()

    DESCRIPTION
    Gets the Sink Wired Play Usb And Wired In Limbo

RETURNS
    bool

*/
bool SinkUsbPlayUsbAndWiredInLimbo(void);

#endif /* SINK_AUDIO_ROUTING_CONFIG_H_ */
