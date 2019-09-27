/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Initialisation functions for the Peer find role service (using LE)
*/

#ifndef PEER_FIND_ROLE_INIT_H_
#define PEER_FIND_ROLE_INIT_H_

#include "peer_find_role.h"
#include "peer_find_role_private.h"

/*! Finish the find role procedure sending a message to registered tasks 

    If required any links will be disconnected.

    \param  role    The identifier of the message to send
*/
void peer_find_role_completed(peer_find_role_message_t role);


/*! Cancel the timeout for finding a peer

    If there is a timer running after a call to PeerFindRole_FindRole(), then
    the timeout is cancelled.
 */
void peer_find_role_cancel_initial_timeout(void);


/*! inform clients that we did not find our role in the requested time

    Send #PEER_FIND_ROLE_ACTING_PRIMARY message to clients.
    This is a response in the PeerFindRole_FindRole() procedure.
 */
void peer_find_role_notify_timeout(void);


/*! if a specific response is pending inform clients that we have finished

    When a primary role is selected the clients are notified immediately.
    Secondary roles delay reporting until all activity is known to have 
    stopped */
void peer_find_role_notify_clients_if_pending(void);



#endif /* PEER_FIND_ROLE_INIT_H_ */
