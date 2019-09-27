/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Configuration related definitions for the shadow_profile state machine.
*/

#ifndef SHADOW_PROFILE_CONFIG_H_
#define SHADOW_PROFILE_CONFIG_H_

#include <message.h>


/*! Delay to use on the Primary between a failed shadow ACL connect req and
    trying the shadow ACL connect req again.

    Most reasons for a shadow ACL connect req to fail will require indefinite
    retries. e.g. connection timeout (link-loss).

    It is recommended to keep this delay large enough to avoid flooding the
    firmware with shadow ACL requests.
*/
#define shadowProfileConfig_ShadowAclRetryDelay()  D_SEC(1)

#endif /* SHADOW_PROFILE_CONFIG_H_ */