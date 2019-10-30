/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      The audio source observer interface implementation provided by Shadow Profile
*/

#ifndef SHADOW_PROFILE_VOLUME_OBSERVER_H_
#define SHADOW_PROFILE_VOLUME_OBSERVER_H_

#include "audio_sources_observer_interface.h"

/*! \brief Gets the Shadow Profile observer interface.

    \return The audio source observer interface provided by Shadow Profile
 */
const audio_source_observer_interface_t * ShadowProfile_GetObserverInterface(void);

#endif /* SHADOW_PROFILE_VOLUME_OBSERVER_H_ */
