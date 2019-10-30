/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Protected interface to shadow profile. These functions should not
            be used by customer code.
*/

#ifndef SHADOW_PROFILE_PROTECTED_H_
#define SHADOW_PROFILE_PROTECTED_H_

#include "shadow_profile.h"

/*! \brief Set peer link policy to the requested mode.

    \param mode The new mode to set.
 */
void ShadowProfile_UpdatePeerLinkPolicy(lp_power_mode mode);

/*! \brief Set peer link policy to the requested mode and block waiting for
           the mode to change.

    \param mode The new mode to set.
    \param timeout_ms  The time to wait for the mode to change in milli-seconds.

    \return TRUE if the mode was changed before the timeout expired. FALSE otherwise.
 */
bool ShadowProfile_UpdatePeerLinkPolicyBlocking(lp_power_mode mode, uint32 timeout_ms);

#endif