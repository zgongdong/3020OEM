/*!
\copyright  Copyright (c) 2018 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       avrcp_profile_types.h
\brief      Header file for AVRCP types
*/

#ifndef AVRCP_PROFILE_TYPES_H_
#define AVRCP_PROFILE_TYPES_H_

#include <avrcp.h>
#include <csrtypes.h>
#include <rtime.h>
#include <task_list.h>

/*! Flag used on AVRCP states to indicate if the state represents an activity
    that will finish. */
#define AVRCP_STATE_LOCK (0x10)

/*! Application A2DP state machine states */
typedef enum
{
    AVRCP_STATE_NULL = 0,                                               /*!< Initial state */
    AVRCP_STATE_DISCONNECTED = 1,                                       /*!< No AVRCP connection */
    AVRCP_STATE_CONNECTING_LOCAL = 2 + AVRCP_STATE_LOCK,                /*!< Locally initiated connection in progress */
    AVRCP_STATE_CONNECTING_LOCAL_WAIT_RESPONSE = 3 + AVRCP_STATE_LOCK,  /*!< Locally initiated connection in progress, waiting got client task to respond */
    AVRCP_STATE_CONNECTING_REMOTE = 4 + AVRCP_STATE_LOCK,               /*!< Remotely initiated connection is progress */
    AVRCP_STATE_CONNECTING_REMOTE_WAIT_RESPONSE = 5 + AVRCP_STATE_LOCK, /*!< Remotely initiated connection in progress, waiting got client task to respond */
    AVRCP_STATE_CONNECTED = 6,                                          /*!< Control channel connected */
    AVRCP_STATE_DISCONNECTING = 7 + AVRCP_STATE_LOCK                    /*!< Disconnecting control channel */
} avAvrcpState;

typedef struct
{
    AVRCP          *avrcp;                /*!< AVRCP profile library instance */
    avAvrcpState    state;                /*!< Current state of AVRCP state machine */
    uint16          lock;                 /*!< AVRCP operation lock, used to serialise AVRCP operations */
    uint16          notification_lock;    /*!< Register notification lock, used to serialise notifications */
    uint16          playback_lock;        /*!< Playback status lock, set when AVRCP Play or Pause sent.  Cleared when playback status received, or after timeout. */
    unsigned        op_id:8;              /*!< Last sent AVRCP operation ID, used to determine which confirmation or error tone to play */
    unsigned        op_state:1;           /*!< Last sent AVRCP operation state, used to determine if operation was button press or release */
    unsigned        op_repeat:1;          /*!< Last send AVRCP operation was a repeat */
    unsigned        volume_time_valid:1;  /*!< Does volume_time field contain valid time */
    unsigned        supported_events:13;  /*!< Bitmask of events supported. See the avrcp_supported_events enum.*/
    unsigned        changed_events:13;    /*!< Bitmask of events that have changed but notifications not sent. See the avrcp_supported_events enum.*/
    unsigned        registered_events:13; /*!< Bitmask of events that we have registered successfully. See the avrcp_supported_events enum.*/
    task_list_t    *client_list;          /*!< List of clients for AVRCP messages for this link */
    uint8           client_lock;          /*!< Count of the number of clients registered for this AVRCP link */
    uint8           client_responses;     /*!< Count of outstanding response to requests sent to registered clients */
    Task            vendor_task;          /*!< Task to receive vendor commands */
    uint8          *vendor_data;          /*!< Data for current vendor command */
    uint16          vendor_opid;          /*!< Operation identifier of the current vendor command */
    uint8           volume;               /*!< Current avrcp instance volume */
    rtime_t         volume_time;          /*!< Time of last volume change, only valid if volume_time_valid is set */
    avrcp_play_status play_status;        /*! Current play status of the AVRCP connection.
                                              This is not always known. See \ref avrcp_play_hint */
    avrcp_play_status play_hint;          /*!< Our local guess at the play status. Not always accurate. */
} avrcpTaskData;

#endif /* AVRCP_PROFILE_TYPES_H_ */
