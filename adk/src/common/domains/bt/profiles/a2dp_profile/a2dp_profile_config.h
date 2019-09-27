/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       a2dp_profile_config.h
\brief      Configuration related definitions for A2DP state machine.
*/

#ifndef A2DP_PROFILE_CONFIG_H_
#define A2DP_PROFILE_CONFIG_H_


/*! Time to wait to connect A2DP media channel after a locally initiated A2DP connection */
#define appConfigA2dpMediaConnectDelayAfterLocalA2dpConnectMs() D_SEC(3)

/*! This config relates to the behavior of the TWS standard master when AAC sink.
    If TRUE, it will forward the received stereo AAC data to the slave earbud.
    If FALSE, it will transcode one channel to SBC mono and forward. */
#define appConfigAacStereoForwarding() TRUE

/*! Default volume gain in dB */
#define appConfigDefaultVolumedB() (-10)

#endif /* A2DP_PROFILE_CONFIG_H_ */