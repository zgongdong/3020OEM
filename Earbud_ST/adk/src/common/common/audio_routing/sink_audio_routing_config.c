/****************************************************************************
Copyright (c) 2018 Qualcomm Technologies International, Ltd.
 
FILE NAME
    sink_audio_routing_config.c
 
DESCRIPTION
    sink_audio_routing module config data access
*/

#include "src/apps/common/audio_routing/sink_audio_routing_config.h"
#include "sink_audio_routing_config_def.h"
#include "src/apps/common/audio_routing/sink_audio_routing_data.h"
#include "src/domains/config/src/legacy/sink_config_store.h"

#include <string.h>


bool sinkAudioGetAutoSuspendOnCallEnabled(void)
{
    sink_audio_readonly_config_def_t *read_data = NULL;
    bool auto_suspend_enabled = FALSE;

    sinkConfigStoreGetReadOnlyConfig(SINK_AUDIO_READONLY_CONFIG_BLK_ID, (const void **)&read_data);

    auto_suspend_enabled = read_data->AssumeAutoSuspendOnCall;
    sinkConfigStoreReleaseConfig(SINK_AUDIO_READONLY_CONFIG_BLK_ID);

    return auto_suspend_enabled;
}


uint16 sinkAudioGetSilenceThreshold(void)
{
    sink_audio_readonly_config_def_t *read_data = NULL;
    uint16 silence_threshold = 0x0; /* By default silence detection is set to disabled */

    sinkConfigStoreGetReadOnlyConfig(SINK_AUDIO_READONLY_CONFIG_BLK_ID, (const void **)&read_data);

    silence_threshold = read_data->SilenceDetSettings.threshold;
    sinkConfigStoreReleaseConfig(SINK_AUDIO_READONLY_CONFIG_BLK_ID);

    return silence_threshold;
}


uint16 sinkAudioGetSilenceTriggerTime(void)
{
    sink_audio_readonly_config_def_t *read_data = NULL;
    uint16 silence_trigger_time = 0x0;

    sinkConfigStoreGetReadOnlyConfig(SINK_AUDIO_READONLY_CONFIG_BLK_ID, (const void **)&read_data);

    silence_trigger_time = read_data->SilenceDetSettings.trigger_time;
    sinkConfigStoreReleaseConfig(SINK_AUDIO_READONLY_CONFIG_BLK_ID);

    return silence_trigger_time;
}


uint16 sinkAudioGetAmpPowerUpTime(void)
{
    sink_audio_readonly_config_def_t *read_data = NULL;
    uint16 amp_unmute_time = 0x0;

    sinkConfigStoreGetReadOnlyConfig(SINK_AUDIO_READONLY_CONFIG_BLK_ID, (const void **)&read_data);

    amp_unmute_time = read_data->AudioAmpUnmuteTime_ms;
    sinkConfigStoreReleaseConfig(SINK_AUDIO_READONLY_CONFIG_BLK_ID);

    return amp_unmute_time;
}

bool sinkAudioSetAmpPowerUpTime(uint16 time)
{
    sink_audio_readonly_config_def_t *read_data = NULL;

    if (sinkConfigStoreGetWriteableConfig(SINK_AUDIO_READONLY_CONFIG_BLK_ID, (void **)&read_data, 0))
    {
        read_data->AudioAmpUnmuteTime_ms = time;
        sinkConfigStoreUpdateWriteableConfig(SINK_AUDIO_READONLY_CONFIG_BLK_ID);
        return TRUE;
    }

    return FALSE;
}


uint16 sinkAudioGetSourceDisconnectDelay(void)
{
    sink_audio_readonly_config_def_t *read_data = NULL;
    uint16 delay = 0x0;

    sinkConfigStoreGetReadOnlyConfig(SINK_AUDIO_READONLY_CONFIG_BLK_ID, (const void **)&read_data);

    delay = read_data->audio_switch_delay_after_disconn;
    sinkConfigStoreReleaseConfig(SINK_AUDIO_READONLY_CONFIG_BLK_ID);

    return delay;
}

uint16 sinkAudioGetAmpPowerDownTime(void)
{
    sink_audio_readonly_config_def_t *read_data = NULL;
    uint16 amp_mute_time = 0x0;

    sinkConfigStoreGetReadOnlyConfig(SINK_AUDIO_READONLY_CONFIG_BLK_ID, (const void **)&read_data);

    amp_mute_time = read_data->AudioAmpMuteTime_ms;
    sinkConfigStoreReleaseConfig(SINK_AUDIO_READONLY_CONFIG_BLK_ID);

    return amp_mute_time;
}

bool sinkAudioSetAmpPowerDownTime(uint16 time)
{
    sink_audio_readonly_config_def_t *read_data = NULL;

    if (sinkConfigStoreGetWriteableConfig(SINK_AUDIO_READONLY_CONFIG_BLK_ID, (void **)&read_data, 0))
    {
        read_data->AudioAmpMuteTime_ms = time;
        sinkConfigStoreUpdateWriteableConfig(SINK_AUDIO_READONLY_CONFIG_BLK_ID);
        return TRUE;
    }

    return FALSE;
}


uint8 sinkAudioGetAudioActivePio(void)
{
    sink_audio_readonly_config_def_t *read_data = NULL;
    uint8 audio_active_pio = 0x0;

    sinkConfigStoreGetReadOnlyConfig(SINK_AUDIO_READONLY_CONFIG_BLK_ID, (const void **)&read_data);

    audio_active_pio = read_data->AudioActivePIO;
    sinkConfigStoreReleaseConfig(SINK_AUDIO_READONLY_CONFIG_BLK_ID);

    return audio_active_pio;
}


uint8 sinkAudioGetAuxOutDetectPio(void)
{
    sink_audio_readonly_config_def_t *read_data = NULL;
    uint8 aux_out_detect_pio = 0x0;

    sinkConfigStoreGetReadOnlyConfig(SINK_AUDIO_READONLY_CONFIG_BLK_ID, (const void **)&read_data);

    aux_out_detect_pio = read_data->aux_out_detect;
    sinkConfigStoreReleaseConfig(SINK_AUDIO_READONLY_CONFIG_BLK_ID);

    return aux_out_detect_pio;
}


uint8 sinkAudioGetPowerOnPio(void)
{
    sink_audio_readonly_config_def_t *read_data = NULL;
    uint8 power_on_pio = 0x0;

    sinkConfigStoreGetReadOnlyConfig(SINK_AUDIO_READONLY_CONFIG_BLK_ID, (const void **)&read_data);

    power_on_pio = read_data->PowerOnPIO;
    sinkConfigStoreReleaseConfig(SINK_AUDIO_READONLY_CONFIG_BLK_ID);

    return power_on_pio;
}


uint8 sinkAudioGetAudioMutePio(void)
{
    sink_audio_readonly_config_def_t *read_data = NULL;
    uint8 audio_mute_pio = 0x0;

    sinkConfigStoreGetReadOnlyConfig(SINK_AUDIO_READONLY_CONFIG_BLK_ID, (const void **)&read_data);

    audio_mute_pio = read_data->AudioMutePIO;
    sinkConfigStoreReleaseConfig(SINK_AUDIO_READONLY_CONFIG_BLK_ID);

    return audio_mute_pio;
}

uint8 sinkAudioGetDefaultAudioSource(void)
{
    sink_audio_readonly_config_def_t *read_data = NULL;
    uint8 default_source = 0x0;

    sinkConfigStoreGetReadOnlyConfig(SINK_AUDIO_READONLY_CONFIG_BLK_ID, (const void **)&read_data);

    default_source = read_data->defaultSource;
    sinkConfigStoreReleaseConfig(SINK_AUDIO_READONLY_CONFIG_BLK_ID);

    return default_source;
}


void sinkAudioRoutingGetSessionData(void)
{
    sink_audio_writeable_config_def_t *session_data = NULL;

    if(sinkConfigStoreGetReadOnlyConfig(SINK_AUDIO_WRITEABLE_CONFIG_BLK_ID, (const void **)&session_data))
    {
        sinkAudioSetRequestedAudioSource(session_data->requested_audio_source);
        sinkConfigStoreReleaseConfig(SINK_AUDIO_WRITEABLE_CONFIG_BLK_ID);
    }
}

void sinkAudioRoutingSetSessionData(void)
{
    sink_audio_writeable_config_def_t *write_data = NULL;

    if (sinkConfigStoreGetWriteableConfig(SINK_AUDIO_WRITEABLE_CONFIG_BLK_ID, (void **)&write_data, 0))
    {
        write_data->requested_audio_source = sinkAudioGetRequestedAudioSource();
        sinkConfigStoreUpdateWriteableConfig(SINK_AUDIO_WRITEABLE_CONFIG_BLK_ID);
    }
}


bool sinkAudioGetLowPowerCodecEnabled(void)
{
    sink_audio_readonly_config_def_t *read_data = NULL;
    bool low_power_codec_enabled = FALSE;

    sinkConfigStoreGetReadOnlyConfig(SINK_AUDIO_READONLY_CONFIG_BLK_ID, (const void **)&read_data);

    low_power_codec_enabled = read_data->UseLowPowerAudioCodecs;
    sinkConfigStoreReleaseConfig(SINK_AUDIO_READONLY_CONFIG_BLK_ID);

    return low_power_codec_enabled;
}


void sinkAudioGetSourcePriorities(audio_source_t * source_priority_list)
{
    sink_audio_readonly_config_def_t *read_data = NULL;

    sinkConfigStoreGetReadOnlyConfig(SINK_AUDIO_READONLY_CONFIG_BLK_ID, (const void **)&read_data);

    source_priority_list[0] = read_data->seqSourcePriority1;
    source_priority_list[1] = read_data->seqSourcePriority2;
    source_priority_list[2] = read_data->seqSourcePriority3;
    source_priority_list[3] = read_data->seqSourcePriority4;
    source_priority_list[4] = read_data->seqSourcePriority5;
    source_priority_list[5] = read_data->seqSourcePriority6;

    sinkConfigStoreReleaseConfig(SINK_AUDIO_READONLY_CONFIG_BLK_ID);
}

bool sinkAudioGetMixingOfVoiceAndAudioEnabled(void)
{
    sink_audio_readonly_config_def_t *read_data = NULL;
    bool enabled = FALSE;

    sinkConfigStoreGetReadOnlyConfig(SINK_AUDIO_READONLY_CONFIG_BLK_ID, (const void **)&read_data);

    enabled = read_data->PluginFeatures.enableMixingOfVoiceAndAudio;
    sinkConfigStoreReleaseConfig(SINK_AUDIO_READONLY_CONFIG_BLK_ID);

    return enabled;
}

bool sinkAudioGetManualSourceSelectionFromConfig(void)
{
    sink_audio_readonly_config_def_t *read_data = NULL;
    bool manual_source_selection = FALSE;

    sinkConfigStoreGetReadOnlyConfig(SINK_AUDIO_READONLY_CONFIG_BLK_ID, (const void **)&read_data);

    manual_source_selection = read_data->PluginFeatures.manual_source_selection;
    sinkConfigStoreReleaseConfig(SINK_AUDIO_READONLY_CONFIG_BLK_ID);

    return manual_source_selection;
}

audio_quality_t sinkAudioGetVoiceQuality(void)
{
    sink_audio_readonly_config_def_t *read_data = NULL;
    audio_quality_t voice_quality = audio_quality_high;

    sinkConfigStoreGetReadOnlyConfig(SINK_AUDIO_READONLY_CONFIG_BLK_ID, (const void **)&read_data);

    voice_quality = read_data->audio_quality.voice_quality;
    sinkConfigStoreReleaseConfig(SINK_AUDIO_READONLY_CONFIG_BLK_ID);

    return voice_quality;
}

audio_quality_t sinkAudioGetMusicQuality(void)
{
    sink_audio_readonly_config_def_t *read_data = NULL;
    audio_quality_t music_quality = audio_quality_high;

    sinkConfigStoreGetReadOnlyConfig(SINK_AUDIO_READONLY_CONFIG_BLK_ID, (const void **)&read_data);

    music_quality = read_data->audio_quality.music_quality;
    sinkConfigStoreReleaseConfig(SINK_AUDIO_READONLY_CONFIG_BLK_ID);

    return music_quality;
}

rendering_mode_t sinkAudioGetAudioRenderingMode(void)
{
    sink_audio_readonly_config_def_t *read_data = NULL;
    rendering_mode_t rendering_mode = multi_channel_rendering;

    sinkConfigStoreGetReadOnlyConfig(SINK_AUDIO_READONLY_CONFIG_BLK_ID, (const void **)&read_data);

    rendering_mode = read_data->AudioRenderingMode;
    sinkConfigStoreReleaseConfig(SINK_AUDIO_READONLY_CONFIG_BLK_ID);

    return rendering_mode;
}

/****************************************************************************
NAME
    SinkWiredPlayUsbAndWiredInLimbo()

    DESCRIPTION
    Gets the Sink Wired Play Usb And Wired In Limbo

RETURNS
    bool

*/
bool SinkUsbPlayUsbAndWiredInLimbo(void)
{
    bool result = FALSE;
    sink_audio_readonly_config_def_t *ro_data;

    sinkConfigStoreGetReadOnlyConfig(SINK_AUDIO_READONLY_CONFIG_BLK_ID, (const void **)&ro_data);
    result = ro_data->PlayUsbWiredInLimbo;
    sinkConfigStoreReleaseConfig(SINK_AUDIO_READONLY_CONFIG_BLK_ID);

    return result;
}
