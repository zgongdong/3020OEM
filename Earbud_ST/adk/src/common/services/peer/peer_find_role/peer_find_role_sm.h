/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Header file for state machine transitions in the service finding the peer 
            and selecting role using LE
*/

#ifndef PEER_FIND_ROLE_SM_H_
#define PEER_FIND_ROLE_SM_H_

#include "peer_find_role.h"


/*! \addtogroup peer_find_role

    <H2>State transitions</H2>

    The LE peer find role service uses a state variable to record its current 
    status see #PEER_FIND_ROLE_STATE for state names. The use of the state 
    allows the processing of messages and function calls to check what 
    action is required, or error to generate.

    The state is set using the function peer_find_role_set_state(). This 
    function can trigger actions based on the state that has been entered 
    (or exited).

    The basics of the state are documented in the diagram below.

    \startuml

    note "For clarity not all state transitions shown" as N1

    [*] -down-> UNINITIALISED : Start
    note left of UNINITIALISED : State names shortened, removing prefix of PEER_FIND_ROLE_STATE_

    UNINITIALISED : Initial state on power up
    UNINITIALISED --> INITIALISED : peer_find_role_init()

    INITIALISED: Awaiting a request to find role
    INITIALISED --> CHECKING_PEER : PeerFindRole_FindRole()

    CHECKING_PEER: Verifying that we have previously paired
    CHECKING_PEER --> INITIALISED : Not yet paired
    CHECKING_PEER --> DISCOVER : Paired

    DISCOVER: Looking for a peer device.\nWill \b not enable scanning if streaming/in call.
    DISCOVER: Start a timeout to enable advertising
    DISCOVER --> DISCOVER_CONNECTABLE : Internal timeout to start advertising
    DISCOVER --> DISCOVERED_DEVICE : Received an advert for matching device

    DISCOVER_CONNECTABLE : Looking for peer
    DISCOVER_CONNECTABLE : Also advertising
    DISCOVER_CONNECTABLE --> DISCOVERED_DEVICE : Received an advert for matching device
    DISCOVER_CONNECTABLE --> CLIENT : GATT_MANAGER_REMOTE_CLIENT_CONNECT_CFM (success).\nRemote device connected to us.
    DISCOVER_CONNECTABLE --> DISCOVER : GATT_MANAGER_REMOTE_CLIENT_CONNECT_CFM (failure).
    DISCOVER_CONNECTABLE --> DISCOVER : No longer streaming/in call.\n(re)start scanning.
    DISCOVER_CONNECTABLE --> DISCOVER_CONNECTABLE : streaming/in call.\nstop scanning.

    DISCOVERED_DEVICE: Found a peer device. 
    DISCOVERED_DEVICE: Waiting to ensure scaning and advertising completed and we don't connect
    DISCOVERED_DEVICE --> CONNECTING_TO_DISCOVERED : Scanning/Advertising ended
    DISCOVERED_DEVICE --> CLIENT : GATT_MANAGER_REMOTE_CLIENT_CONNECT_CFM (success).\nRemote device connected to us.
    DISCOVERED_DEVICE --> DISCOVER : GATT_MANAGER_REMOTE_CLIENT_CONNECT_CFM (failure)

    CONNECTING_TO_DISCOVERED: Connecting to the device we found
    CONNECTING_TO_DISCOVERED --> SERVER : GATT_MANAGER_REMOTE_SERVER_CONNECT_CFM (success)
    CONNECTING_TO_DISCOVERED --> DISCOVER : GATT_MANAGER_REMOTE_SERVER_CONNECT_CFM (failure)

    CLIENT: Connected as a GATT client
    CLIENT --> AWAITING_ENCRYPTION : Connected to the peers server
    CLIENT --> DISCOVER : Link disconnected\nConnection manager

    SERVER : Connected as a GATT server
    SERVER : Encrypt the link on entry
    SERVER : Wait for client to finish
    SERVER --> AWAITING_DISCONNECT : Commanded to change state.
    SERVER --> DISCOVER : Link disconnected\nConnection manager
    SERVER --> INITIALISED : Error encrypting the link

    AWAITING_ENCRYPTION : Connected as a GATT client, link not yet encrypted
    AWAITING_ENCRYPTION --> DECIDING : Link encrypted successfully
    AWAITING_ENCRYPTION --> DISCOVER : Link disconnected\nConnection manager

    DECIDING : Deciding which role we should assume
    DECIDING : Read score from server
    DECIDING --> AWAITING_CONFIRM : Have score, informed peer of requested state
    DECIDING --> DISCOVER : Link disconnected\nConnection manager

    AWAITING_CONFIRM : Awaiting confirmation of role
    AWAITING_CONFIRM --> INITIALISED : Server confirmed change. Change our own state.
    AWAITING_CONFIRM --> DISCOVER : Link disconnected\nConnection manager

    AWAITING_DISCONNECT : We have informed client of new state (we were server)
    AWAITING_DISCONNECT : Waiting for a disconnect or timeout
    AWAITING_DISCONNECT --> INITIALISED : Link disconnected (by client)
    AWAITING_DISCONNECT --> INITIALISED : Time out expired\nDisconnected ourselves

    \enduml

*/

/*! Possible states of the find role service */
typedef enum
{
        /*! Peer find role module is not yet initialised */
    PEER_FIND_ROLE_STATE_UNINITIALISED,
        /*! Initialised. No action in progress. */
    PEER_FIND_ROLE_STATE_INITIALISED,
        /*! Waiting to find out if we are paired with a peer */
    PEER_FIND_ROLE_STATE_CHECKING_PEER,
        /*! Scanning to find a compatible device */
    PEER_FIND_ROLE_STATE_DISCOVER,
        /*! Scanning - but also using connectable advertising for 
            other devices to connect */
    PEER_FIND_ROLE_STATE_DISCOVER_CONNECTABLE,
        /*! We have discovered a device while scanning. Waiting while we
            cancel scanning and advertising */
    PEER_FIND_ROLE_STATE_DISCOVERED_DEVICE,
        /*! Trying to connect to the device we discovered */
    PEER_FIND_ROLE_STATE_CONNECTING_TO_DISCOVERED,
        /*! Connected as a GATT client (someone connected to us) */
    PEER_FIND_ROLE_STATE_CLIENT,
        /*! Connected as a GATT server */
    PEER_FIND_ROLE_STATE_SERVER,
        /*! Waiting for confirmation the link is encrypted */
    PEER_FIND_ROLE_STATE_AWAITING_ENCRYPTION,
        /*! Deciding which role to use */
    PEER_FIND_ROLE_STATE_DECIDING,
        /*! Waiting for the role to be confirmed */
    PEER_FIND_ROLE_STATE_AWAITING_CONFIRM,
        /*! Completed, but waiting for client to disconnect */
    PEER_FIND_ROLE_STATE_AWAITING_DISCONNECT,
} PEER_FIND_ROLE_STATE;


/*! Change the state of the peer find role service

    This function changes the state. It can perform actions as a result of
    \li leaving the previous state
    \li entering the new state

    \param state The new state to change to
*/
void peer_find_role_set_state(PEER_FIND_ROLE_STATE state);


/*! Retrieve the current state of the peer find role service.

    See #PEER_FIND_ROLE_STATE */
#define peer_find_role_get_state() (PeerFindRoleGetTaskData()->state)



#endif /* PEER_FIND_ROLE_SM_H_ */
