/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Configuration related definitions for the mirror_profile state machine.
*/

#ifndef MIRROR_PROFILE_CONFIG_H_
#define MIRROR_PROFILE_CONFIG_H_

#include <message.h>


/*! Delay to use on the Primary between a failed mirror ACL connect req and
    trying the mirror ACL connect req again.

    Most reasons for a mirror ACL connect req to fail will require indefinite
    retries. e.g. connection timeout (link-loss).

    It is recommended to keep this delay large enough to avoid flooding the
    firmware with mirror ACL requests.
*/
#define mirrorProfileConfig_MirrorAclRetryDelay()  D_SEC(1)

/*! Set to TRUE to enable A2DP mirroring.
    A2DP mirroring is currently not supported.
*/
#define mirrorProfileConfig_EnableA2DPMirroring() (TRUE)

/*! This is the length of time after eSCO/A2DP mirroring stops before the link
    policy is set to a lower power (and higher latency) setting. */
#define mirrorProfileConfig_LinkPolicyIdleTimeout() D_SEC(5)

#endif /* MIRROR_PROFILE_CONFIG_H_ */