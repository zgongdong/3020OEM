/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   a2dp_profile_types A2DP profile
\ingroup    a2dp_profile
\brief      A2DP profile types
*/

#ifndef A2DP_PROFILE_TYPES_H_
#define A2DP_PROFILE_TYPES_H_

#include <a2dp.h>

#include "audio_sync.h"

/*! \brief Application A2DP state machine states */
typedef enum
{
    A2DP_STATE_NULL                                             = 0x00, /*!< Initial state */
    A2DP_STATE_DISCONNECTED                                     = 0x01, /*!< No A2DP connection */
    A2DP_STATE_CONNECTING_LOCAL	                                = 0x02, /*!< Locally initiated connection in progress */
    A2DP_STATE_CONNECTING_REMOTE                                = 0x03, /*!< Remotely initiated connection is progress */
    A2DP_STATE_CONNECTING_CROSSOVER                             = 0x04, /*!< Locally and remotely initiated connection is progress */
        A2DP_STATE_CONNECTED_SIGNALLING                         = 0x10, /*!< Signalling channel connected */
        A2DP_STATE_CONNECTING_MEDIA_LOCAL                       = 0x11, /*!< Locally initiated media channel connection in progress */
        A2DP_STATE_CONNECTING_MEDIA_REMOTE_SYNC                 = 0x12, /*!< Remotely initiated media channel connection in progress */
            A2DP_STATE_CONNECTED_MEDIA	                        = 0x30, /*!< Media channel connected (parent-pseudo state) */
                A2DP_STATE_CONNECTED_MEDIA_STREAMING            = 0x31, /*!< Media channel streaming */
                A2DP_STATE_CONNECTED_MEDIA_STREAMING_MUTED      = 0x32, /*!< Media channel streaming but muted (suspend failed) */
                A2DP_STATE_CONNECTED_MEDIA_SUSPENDING_LOCAL     = 0x33, /*!< Locally initiated media channel suspend in progress */
                A2DP_STATE_CONNECTED_MEDIA_SUSPENDED	        = 0x34, /*!< Media channel suspended */
                A2DP_STATE_CONNECTED_MEDIA_RECONFIGURING        = 0x35, /*!< Media channel suspended, reconfiguring the codec */
                A2DP_STATE_CONNECTED_MEDIA_STARTING_LOCAL_SYNC  = 0x70, /*!< Locally initiated media channel start in progress, syncing slave */
                A2DP_STATE_CONNECTED_MEDIA_STARTING_REMOTE_SYNC = 0x71, /*!< Remotely initiated media channel start in progress, syncing slave */
            A2DP_STATE_DISCONNECTING_MEDIA                      = 0x13, /*!< Locally initiated media channel disconnection in progress */
    A2DP_STATE_DISCONNECTING                                    = 0x0A  /*!< Disconnecting signalling and media channels */
} avA2dpState;

/*! \brief AV suspend reasons

    The suspend reasons define the cause for suspending the AV streaming,
    a reason must be specified when calling appAvStreamingSuspend() and
    appAvStreamingResume().
*/
typedef enum
{
    AV_SUSPEND_REASON_SCO    = (1 << 0), /*!< Suspend AV due to active SCO link */
    AV_SUSPEND_REASON_HFP    = (1 << 1), /*!< Suspend AV due to HFP activity */
    AV_SUSPEND_REASON_AV     = (1 << 2), /*!< Suspend AV due to AV activity */
    AV_SUSPEND_REASON_RELAY  = (1 << 3), /*!< Suspend AV due to master suspend request */
    AV_SUSPEND_REASON_REMOTE = (1 << 4), /*!< Suspend AV due to remote request */
    AV_SUSPEND_REASON_SCOFWD = (1 << 5)  /*!< Suspend AV due to SCO forwarding */
} avSuspendReason;

/*! \brief A2DP module state */
typedef struct
{
    avA2dpState     state;                 /*!< Current state of A2DP state machine */
    uint16          lock;                  /*!< A2DP operation lock, used to serialise A2DP operations */
    uint8           device_id;             /*!< A2DP device identifier from A2DP library */
    uint8           stream_id;             /*!< A2DP stream identifier from A2DP library */
    uint8           current_seid;          /*!< Currently active SEID */
    uint8           last_known_seid;       /*!< Last known SEID, unlike current_seid this doesn't get cleared on disconnect  */
    uint8           sync_counter;          /*!< Used to counter syncs and provide sync ids */
    avSuspendReason suspend_state;         /*!< Bitmap of active suspend reasons */
    unsigned        flags:6;               /*!< General connection flags */
    unsigned        connect_retries:3;     /*!< Number of connection retries */
    unsigned        local_initiated:1;     /*!< Flag to indicate if connection was locally initiated */
    unsigned        disconnect_reason:4;   /*!< Reason for disconnect */

    audio_sync_t    *sync_inst;            /*!< AV instance to sync with, e.g. a peer earbud. */
    audio_sync_t    sync_if;               /*!< The interface to register with an AV instance that wants to sync with this one. */
} a2dpTaskData;

#endif /* A2DP_PROFILE_TYPES_H_ */
