/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      The voice source interface implementation for shadow profile
*/

#ifndef SHADOW_PROFILE_VOICE_SOURCE_H_
#define SHADOW_PROFILE_VOICE_SOURCE_H_

#include "voice_sources_audio_interface.h"

/*! \brief Gets the shadow profile voice interface.

    \return The voice source interface for shadow profile
 */
const voice_source_audio_interface_t * ShadowProfile_GetVoiceInterface(void);

#endif /* SHADOW_PROFILE_VOICE_SOURCE_H_ */
