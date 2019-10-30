/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of shadow A2DP audio source interface.
*/

#ifndef SHADOW_PROFILE_AUDIO_SOURCE_H_
#define SHADOW_PROFILE_AUDIO_SOURCE_H_

#include "audio_sources.h"
#include "audio_sources_audio_interface.h"
#include "source_param_types.h"

/*! \brief Gets the shadow A2DP audio interface.

    \return The audio source audio interface for a shadow A2DP source
 */
const audio_source_audio_interface_t *ShadowProfile_GetAudioInterface(void);


/*! \brief Read the connect parameters from the source and store them in the
        shadow profile a2dp state.

    \param source The audio source.

    \return TRUE if connect parameters were valid, else FALSE.
 */
bool ShadowProfile_StoreAudioSourceParameters(audio_source_t source);

/*! \brief Clear stored connect parameters for the source from the shadow profile
        a2dp state.

    \param source The audio source.
 */
void ShadowProfile_ClearAudioSourceParameters(audio_source_t source);

/*! \brief Start audio synchronisation with the other earbud. */
void ShadowProfile_StartAudioSynchronisation(void);

/*! \brief Stop audio synchronisation with the other earbud. */
void ShadowProfile_StopAudioSynchronisation(void);


#endif
