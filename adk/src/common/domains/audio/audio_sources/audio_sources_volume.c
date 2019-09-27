/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of the audio_sources_volume composite.
*/

#include "audio_sources.h"

#include <panic.h>

#include "audio_sources_interface_registry.h"


void AudioSources_RegisterVolume(audio_source_t source, const audio_source_volume_interface_t * volume)
{
    AudioInterface_Register(source, audio_interface_type_volume_registry, volume);
}

volume_t AudioSources_GetVolume(audio_source_t source)
{
    volume_t volume = FULL_SCALE_VOLUME;
    audio_source_volume_interface_t * interface = AudioInterface_Get(source, audio_interface_type_volume_registry);

    if((interface != NULL) && interface->GetVolume)
    {
        volume = interface->GetVolume(source);
    }

    return volume;
}

void AudioSources_SetVolume(audio_source_t source, volume_t volume)
{
    audio_source_volume_interface_t * interface = AudioInterface_Get(source, audio_interface_type_volume_registry);

    if((interface != NULL) && interface->SetVolume)
    {
        interface->SetVolume(source, volume);
    }
}
