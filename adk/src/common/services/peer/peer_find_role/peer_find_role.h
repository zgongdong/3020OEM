/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   peer_find_role Peer Find Role Service (LE)
\ingroup    peer_service    
\brief      Peer service for finding peer using LE and selecting the role.
*/

#ifndef PEER_FIND_ROLE_H_
#define PEER_FIND_ROLE_H_

#include <message.h>
#include "domain_message.h"

/*\{*/

/*! Messages that may be sent externally by the peer find role service */
typedef enum
{
        /* The peer find role service is initialised */
    PEER_FIND_ROLE_INIT_CFM,

        /*! There is no known peer */
    PEER_FIND_ROLE_NO_PEER = PEER_FIND_ROLE_MESSAGE_BASE,

        /*! The process of finding peer/deciding role has been cancelled*/
    PEER_FIND_ROLE_CANCELLED,

        /*! Did not determine a role within the initial timeout from PeerFindRole_FindRole.
            May continue trying to find a role, see the API definition for
            PeerFindRole_FindRole(). Recommended that device acts as a primary. */
    PEER_FIND_ROLE_ACTING_PRIMARY,
        /*! Negotiation with peer has selected the primary role for this device */
    PEER_FIND_ROLE_PRIMARY,
        /*! Negotiation with peer has selected the secondary role for this device */
    PEER_FIND_ROLE_SECONDARY,
} peer_find_role_message_t;


/*! Type representing the score for one of the two peers.

    The scores are compared to determine the device best suited to be primary. */
typedef uint16 peer_find_role_score_t;


/*! \brief Initialise the PEER FIND ROLE component

    \param init_task    Task to send init completion message (if any) to

    \returns TRUE
*/
bool PeerFindRole_Init(Task init_task);


/*! Start the process of finding the required role from a peer

    Assumed pre-requisite is that the peer is missing and has no connection
    over BREDR or LE.

    Messages are sent based on the outcome
    * PEER_FIND_ROLE_NO_PEER
    * PEER_FIND_ROLE_CANCELLED
    * PEER_FIND_ROLE_ACTING_PRIMARY
    * PEER_FIND_ROLE_PRIMARY
    * PEER_FIND_ROLE_SECONDARY

    \param high_speed_time_ms Specify the time to attempt to find a peer at 
                higher duty cycles. After this time the service will reduce
                the rate a which a peer is looked for, but will continue to
                look.  After this initial time the message 
                PEER_FIND_ROLE_ACTING_PRIMARY will be sent. If a time of 0ms
                is supplied, then the service will never use higher duty cycle
                or send a PEER_FIND_ROLE_ACTING_PRIMARY message.

    \note Supplying a negative time to high_speed_time_ms will finish
          the find role attempt on the timeout. The message 
          PEER_FIND_ROLE_ACTING_PRIMARY will not be sent until advertising
          and scanning has terminated.

    \note When function is called again before the outcome message is sent,
          then it will return without any effect.
          That means no new message should be expected.
 */
void PeerFindRole_FindRole(int32 high_speed_time_ms);


/*! Cancel a previous find role

    The current FindRole will complete. If it was successfully cancelled then 
    a #PEER_FIND_ROLE_CANCELLED message will be sent.
    
    The FindRole may still complete with a role or timeout message.
 */
void PeerFindRole_FindRoleCancel(void);


/*! Register a task to receive messages related to the role 

    \param t The task being registered
*/
void PeerFindRole_RegisterTask(Task t);


/*! Handler for connection library messages

    This function needs to be called by the task that is registered for
    connection library messages, as the majority of these are sent to
    a single registered task.

    \param      id      The identifier of the connection library message
    \param[in]  message Pointer to the message content. Can be NULL.
    \param      already_handled Flag indicating if another task has already
                        dealt with this message

    \return TRUE if the handler has dealt with the message, FALSE otherwise
*/
bool PeerFindRole_HandleConnectionLibraryMessages(MessageId id, Message message,
                                                  bool already_handled);


/*! Generate and return our current score 

    \return The current score that would be used as part of the role 
        calculations.
*/
peer_find_role_score_t PeerFindRole_CurrentScore(void);

/*\}*/


/*! \defgroup   peer_find_role Peer Find Role Service (LE)

    The peer find role service is responsible for finding a peer device, 
    and determining the roles each device should take  based on the state 
    of the devices.

    The service is started by a call to PeerFindRole_FindRole(). 
    The service then continues until a peer device is detected and a role 
    selected. A timeout can be supplied in the initial call to 
    PeerFindRole_FindRole(). When this timeout expires a 
    #PEER_FIND_ROLE_ACTING_PRIMARY message is sent to the 
    registered tasks, but the service continues looking for a role.

    The service operates by scanning for devices and also advertising its
    presence, normally simultaneously. Exceptions to this are
    \li After a connection failure the start of advertising is delayed.
        If the failure was caused by the two devices connecting to each
        other at the same time, this should ensure that only one is
        advertising initially.
    \li If a device is busy with media, either music or a telephone call
        then the device will not scan. This means that if both devices 
        are busy, perhaps connected to different devices, they will not 
        be able to connect to each other until one stops.

    Once devices are discovered and a connection made, the role is
    determined by comparison of a figure of merit provided by the GATT 
    role selection service. This allows a decision to be made and one
    device is told to assume the primary role with the message
    #PEER_FIND_ROLE_PRIMARY, while the other will be told
    #PEER_FIND_ROLE_SECONDARY.

    For more comprehensive details of the implemetation look at the state 
    machine documentation below (state transitions).

    As part of the implementation internal messages are sometimes sent
    delayed, offering the chance for them to be cancelled, or using condition
    flags so that messages are sent when all activities blocking them
    have finished. See the media_busy element of peerFindRoleTaskData as
    an example.

*/


#endif /* PEER_FIND_ROLE_H_ */