/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Header file for the peer service finding peer using LE and selecting
            the role.
*/

#ifndef PEER_FIND_ROLE_PRIVATE_H_
#define PEER_FIND_ROLE_PRIVATE_H_

#include <message.h>

#include <task_list.h>
#include <gatt_role_selection_client.h>
#include <gatt_role_selection_server.h>

#include <le_scan_manager.h>
#include <le_advertising_manager.h>

#include "peer_find_role_scoring.h"
#include "peer_find_role_sm.h"


/*! Enumerated type (bit mask) to record operations that may operate 
    simultaneously or take time to cancel */
typedef enum
{
        /*! We are still scanning */
    PEER_FIND_ROLE_ACTIVE_SCANNING        = (1 << 0),
        /*! We are (connectable) advertising */
    PEER_FIND_ROLE_ACTIVE_ADVERTISING     = (1 << 1),
        /*! We are connecting */
    PEER_FIND_ROLE_ACTIVE_CONNECTING      = (1 << 2),
} peerFindRoleActiveStatus_t;


/*! Enumerated type (bit mask) to record when media is busy, that
    may affect our operation. */
typedef enum
{
        /*! Audio streaming is active */
    PEER_FIND_ROLE_AUDIO_STREAMING      = (1 << 0),
        /*! Call is inactive */
    PEER_FIND_ROLE_CALL_ACTIVE          = (1 << 1),
} peerFindRoleMediaBusyStatus_t;

/*! Structure to hold information used by the peer find role service */
typedef struct 
{
    /*! Task for handling messages */
    TaskData                    _task;

    /*! List of tasks to send messages to */
    task_list_t                *registered_tasks;

    /*! address of this device 
        \todo Will be removed once RRA working */
    bdaddr                      my_addr;
    /*! address of peer device 
        \todo Will be removed once RRA working */
    bdaddr                      primary_addr;

    /*! Timeout, if any, to be used when attempting to find role */
    uint32                      role_timeout_ms;

    /*! Delay before starting adverts when entering a discovery phase */
    uint32                      advertising_backoff_ms;

    /*! The connection identifier of the gatt connection being used */
    uint16                      gatt_cid;

    /*! Role that has been selected for this device. Stashed temporarily
        while waiting for links to be cleared */
    peer_find_role_message_t    selected_role;

    /*! The GATT connection has been encrypted */
    bool                        gatt_encrypted;

    /*! When find role times-out, stop all activity 
        \todo This is a temporary feature */
    bool                        timeout_means_timeout;

    /*! The advertisement settings */
    adv_mgr_advert_t           *advert;

    /*! The scanner... */
    le_scan_handle_t            scan;

    /*! What operations are busy? Bitfield using peerFindRoleActiveStatus_t 

        This can be used in MessageSendConditional() */
    uint16                      active;

    /*! What media operations are active? Bitfield using
        peerFindRoleMediaBusyStatus_t

        This can be used in MessageSendConditional() */ 
    uint16                      media_busy;

    /*! The current internal state */
    PEER_FIND_ROLE_STATE        state;

    /*! Role selection server data. Always ready as a server */
    GATT_ROLE_SELECTION_SERVER  role_selection_server;

    /*! Role selection client data (if acting as a client) */
    GATT_ROLE_SELECTION_CLIENT  role_selection_client;

    /*! The selected role for the other peer */
    GattRoleSelectionServiceControlOpCode remote_role;

    /*! Information used by the scoring algorithms when calculating score */
    peer_find_role_scoring_t    scoring_info;

    /*! Test override of score. */
    grss_figure_of_merit_t      score_override;
} peerFindRoleTaskData;


/*! Global data for the find role service.

    Although global scope, the structure definition and variable are not visible
    outside of the peer find role service.
*/
extern peerFindRoleTaskData peer_find_role;

/*! Access the task data for the peer find role service */
#define PeerFindRoleGetTaskData()   (&peer_find_role)

/*! Access the task for the peer find role service */
#define PeerFindRoleGetTask()       (&peer_find_role._task)

/*! Access the list of tasks registered with the peer find role service */
#define PeerFindRoleGetTaskList()   (peer_find_role.registered_tasks)

/*! Access the scoring information for the peer find role service */
#define PeerFindRoleGetScoring()    (&peer_find_role.scoring_info)

/*! Access the busy status for the peer find role service */
#define PeerFindRoleGetBusy()       (peer_find_role.active)

/*! Set the busy status for the peer find role service 
    \param new_value    Value to set
*/
#define PeerFindRoleSetBusy(new_value)  do { \
                                            peer_find_role.active = (new_value); \
                                        } while(0)


/*! Message identifiers used for internal messages */
typedef enum
{
        /*! Update the score and notify GATT server (if any) */
    PEER_FIND_ROLE_INTERNAL_UPDATE_SCORE,

        /*! Cancel a find role (if any) */
    PEER_FIND_ROLE_INTERNAL_CANCEL_FIND_ROLE,

        /*! Scanning and advertising stopped */
    PEER_FIND_ROLE_INTERNAL_CONNECT_TO_PEER,

        /*! Telephony is now busy */
    PEER_FIND_ROLE_INTERNAL_TELEPHONY_ACTIVE,

        /*! Telephony is no longer busy */
    PEER_FIND_ROLE_INTERNAL_TELEPHONY_IDLE,

        /*! Audio streaming is now busy */
    PEER_FIND_ROLE_INTERNAL_STREAMING_ACTIVE,

        /*! Audio streaming is no longer busy */
    PEER_FIND_ROLE_INTERNAL_STREAMING_IDLE,

        /*! Now able to enable scanning */
    PEER_FIND_ROLE_INTERNAL_ENABLE_SCANNING,

        /*! Failed to find peer within requested time */
    PEER_FIND_ROLE_INTERNAL_TIMEOUT_CONNECTION = 0x80,

        /*! Timer for starting advertising has finished */
    PEER_FIND_ROLE_INTERNAL_TIMEOUT_ADVERT_BACKOFF,

        /*! Timer for disconnecting link from server side, if not done */
    PEER_FIND_ROLE_INTERNAL_TIMEOUT_NOT_DISCONNECTED,
} peer_find_role_internal_message_t;


/*! Is this device in the central role ?

    Function that checks whether our device is marked as the central one.

    \todo Since this doesn't change once set may be useful to calculate 
        as part of init. Not the boot init as in that case we may not know C/P.

    \return TRUE if central, FALSE if peripheral or error detected
 */
bool peer_find_role_is_central(void);


/*! Set activity flag(s) for the find role task 

    \param flags_to_set Bitmask of activities that will block actions
*/
void peer_find_role_activity_set(peerFindRoleActiveStatus_t flags_to_set);


/*! Clear activity flag(s) for the find role task 

    Clearing flags may cause a message to be sent

    \param flags_to_clear Bitmask of activities that no longer block actions
*/
void peer_find_role_activity_clear(peerFindRoleActiveStatus_t flags_to_clear);


/*! Send the peer_find_role service a message when there are no longer 
    any active activities.

    \param id The message type identifier. 
    \param message The message data (if any).
*/
void peer_find_role_message_send_when_inactive(MessageId id, void *message);


/*! Cancel messages queued for the peer_find_role service if 
    there are still activities active

    \param id The message type identifier. 
*/
void peer_find_role_message_cancel_inactive(MessageId id);


/*! Stop scanning if in progress */
void peer_find_role_stop_scan_if_active(void);

/*! Override the score used in role selection.
 
    Setting a non-zero value will use that value rather than the
    calculated one for future role selection decisions.

    Setting a value of 0, will cancel the override and revert to
    using the calculated score.

    Note that use of this function will not initiate role selection,
    only influence *future* decisions.

    \param score Score to use for role selection decisions.
*/
void PeerFindRole_OverrideScore(grss_figure_of_merit_t score);

/*! Check if there are any blocking media actions active.

    \return FALSE if no media blocking, TRUE otherwise
*/
#define peer_find_role_media_active() (peer_find_role.media_busy != 0)


#endif /* PEER_FIND_ROLE_PRIVATE_H_ */
