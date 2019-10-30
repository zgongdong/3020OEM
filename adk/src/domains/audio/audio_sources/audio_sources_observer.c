/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of the audio_sources_observer composite.
*/

#include "audio_sources.h"

#include <panic.h>

#include "audio_sources_interface_registry.h"



void AudioSources_RegisterObserver(audio_source_t source, const audio_source_observer_interface_t * observer)
{
    AudioInterface_Register(source, audio_interface_type_observer_registry, observer);
}

void AudioSources_OnVolumeChange(audio_source_t source, event_origin_t origin, volume_t volume)
{
    audio_source_observer_interface_t * interface = AudioInterface_Get(source, audio_interface_type_observer_registry);

    if((interface != NULL) && interface->OnVolumeChange)
    {
        interface->OnVolumeChange(source, origin, volume);
    }
}


