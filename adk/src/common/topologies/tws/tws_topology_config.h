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

/*! Initial time for a peer find role command before notifying that a role
    has not yet been found. */
#define TwsTopologyConfig_InitialPeerFindRoleTimeoutS()     (3)

/*! Time for Secondary to wait for BR/EDR ACL connection to Primary following
    role selection, before falling back to retry role selection and potentially
    becoming an acting primary. */
#define TwsTopologyConfig_SecondaryPeerConnectTimeoutMs()   (5000)

/*! Time for Primary to wait for BR/EDR ACL connection to handset. */
#define TwsTopologyConfig_PrimaryHandsetConnectTimeoutMs()   (5000)

/*! Time for Primary to wait for BR/EDR ACL connection to be made by the Secondary
    following role selection, before falling back to retry role selection. */
#define TwsTopologyConfig_PrimaryPeerConnectTimeoutMs()     (10240)

/*! Time for Handover to be retried following a previous handover attempt. */
#define TwsTopologyConfig_HandoverRetryTimeoutMs()     (500)

/*! maximum number of retry attempts for Handover upon a handover imeout */
#define TwsTopologyConfig_HandoverMaxRetryAttempts()     (10)

/*! Keep dynamic handover enabled by default */
#define ENABLE_DYNAMIC_HANDOVER 

#define TwsTopologyConfig_PeerProfiles() (DEVICE_PROFILE_A2DP | DEVICE_PROFILE_SCOFWD | DEVICE_PROFILE_PEERSIG)

#define TwsTopologyConfig_OptimisedRoleSwitchSupported()   (TRUE)

/*! State proxy events to register, feature specific */
#define TwsTopologyConfig_StateProxyRegisterEvents() 0

