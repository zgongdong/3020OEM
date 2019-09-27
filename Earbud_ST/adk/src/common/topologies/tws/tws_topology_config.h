/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Configuration / parameters used by the topology.
*/

#include "bredr_scan_manager.h"

/*! Inquiry scan parameter set */
extern const bredr_scan_manager_parameters_t inquiry_scan_params;

/*! Page scan parameter set */
extern const bredr_scan_manager_parameters_t page_scan_params;

/*! Time for Secondary to wait for BR/EDR ACL connection to Primary following
    role selection, before falling back to retry role selection and potentially
    becoming an acting primary. */
#define TwsTopologyConfig_SecondaryPeerConnectTimeoutMs()   (5000)

/*! Time for Primary to wait for BR/EDR ACL connection to handset. */
#define TwsTopologyConfig_PrimaryHandsetConnectTimeoutMs()   (5000)

#ifdef INCLUDE_SHADOWING
#define TwsTopologyConfig_PeerProfiles() (DEVICE_PROFILE_PEERSIG)
#else
#define TwsTopologyConfig_PeerProfiles() (DEVICE_PROFILE_A2DP | DEVICE_PROFILE_SCOFWD | DEVICE_PROFILE_PEERSIG)
#endif

