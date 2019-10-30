/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief
*/

#ifndef A2DP_PROFILE_CAPS_H_
#define A2DP_PROFILE_CAPS_H_

void appAvUpdateSbcMonoTwsCapabilities(uint8 *caps, uint32 sample_rate);
void appAvUpdateAptxMonoTwsCapabilities(uint8 *caps, uint32 sample_rate);
void appAvUpdateSbcCapabilities(uint8 *caps, uint32 sample_rate);

#endif /* A2DP_PROFILE_CAPS_H_ */
