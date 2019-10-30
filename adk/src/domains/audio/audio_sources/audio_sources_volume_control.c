/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of the audio_sources_volume_control composite.
*/

#include "audio_sources.h"

#include <panic.h>

#include "audio_sources_interface_registry.h"

void AudioSources_RegisterVolumeControl(audio_source_t source, const audio_source_volume_control_interface_t * interface)
{
    AudioInterface_Register(source, audio_interface_type_volume_control, interface);
}

bool AudioSources_IsVolumeControlRegistered(audio_source_t source)
{
    return ((AudioInterface_Get(source, audio_interface_type_volume_control) == NULL) ? FALSE : TRUE);
}

void AudioSources_VolumeUp(audio_source_t source)
{
    audio_source_volume_control_interface_t * interface = AudioInterface_Get(source, audio_interface_type_volume_control);

    if ((interface != NULL) && (interface->VolumeUp))
    {
        interface->VolumeUp(source);
    }
}

void AudioSources_VolumeDown(audio_source_t source)
{
    audio_source_volume_control_interface_t * interface = AudioInterface_Get(source, audio_interface_type_volume_control);

    if ((interface != NULL) && (interface->VolumeDown))
    {
        interface->VolumeDown(source);
    }
}

void AudioSources_VolumeSetAbsolute(audio_source_t source, volume_t volume)
{
    audio_source_volume_control_interface_t * interface = AudioInterface_Get(source, audio_interface_type_volume_control);

    if ((interface != NULL) && (interface->VolumeSetAbsolute))
    {
        interface->VolumeSetAbsolute(source, volume);
    }
}

void AudioSources_Mute(audio_source_t source, mute_state_t state)
{
    audio_source_volume_control_interface_t * interface = AudioInterface_Get(source, audio_interface_type_volume_control);

    if ((interface != NULL) && (interface->Mute))
    {
        interface->Mute(source, state);
    }
}
