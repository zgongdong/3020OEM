/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       connection_manager_list.h
\brief      Header file for Connection Manager List
*/

#ifndef __CON_MANAGER_LIST_H
#define __CON_MANAGER_LIST_H

#include <connection_manager.h>

/*! Flag used on ACL states to indicate if the state represents an activity
    that will finish. */
#define ACL_STATE_LOCK (0x8)

/*! Internal state for an individual ACK link */
typedef enum
{
    /*! The ACL is connecting. Lock variables will clear once connected, or failed */
    ACL_CONNECTING               = 0 + ACL_STATE_LOCK,
    /*! The ACL is connected. */
    ACL_CONNECTED                = 1,
    /*! The ACL is searching for a matching SDP record. Lock variables will clear
        once completed. */
    ACL_CONNECTED_SDP_SEARCH     = 2 + ACL_STATE_LOCK,
    /*! The ACL is disconnected. */
    ACL_DISCONNECTED             = 3,
    /*! The ACL is disconected due to link-loss. */
    ACL_DISCONNECTED_LINK_LOSS   = 4,
    /*! The ACL is waiting on LE scan to pause */
    ACL_CONNECTING_PENDING_PAUSE = 5 + ACL_STATE_LOCK
    
} cm_connection_state_t;

struct __cm_connection_t;

typedef struct __cm_connection_t cm_connection_t;

typedef struct
{
    cm_connection_t* connection;
} cm_list_iterator_t;

Task ConManagerGetTask(cm_connection_t* connection);

/*! \brief Debug device address 
    \param tpaddr Device address 
*/
void ConManagerDebugAddress(const tp_bdaddr *tpaddr);

/*! \brief Debug connection address 
    \param connection Pointer to connection 
*/
void ConManagerDebugConnection(const cm_connection_t *connection);

/*! \brief Set the state of a connection 
    \param connection Pointer to connection 
    \param state The connection state to set 
*/
void ConManagerSetConnectionState(cm_connection_t *connection, cm_connection_state_t state);

/*! \brief Get the state of a connection 
    \param connection Pointer to connection
    \return The state of the connection or ACL_DISCONNECTED if connection is NULL
*/
cm_connection_state_t conManagerGetConnectionState(const cm_connection_t *connection);

/*! \brief Find a connection from its bdaddr 
    \param tpaddr Device address 
    \return Pointer to the connection or NULL if none found matching tpaddr
*/
cm_connection_t *ConManagerFindConnectionFromBdAddr(const tp_bdaddr *tpaddr);

/*! \brief Check if there is any connection in a given state 
    \param transport_mask Set to cm_transport_all to check all connections, or 
           cm_transport_bredr/ble to only look for connections on specific transport
    \param state The connection state to check for
    \return TRUE if any connections on the specified transport(s) are in this state,
            otherwise FALSE.
*/
bool conManagerAnyLinkInState(cm_transport_t transport_mask, cm_connection_state_t state);

/*! \brief Set or clear locally initiated ACL flag for a connection. 
    \param connection Pointer to connection
    \param local TRUE if the connection is locally initiated, otherwise FALSE
*/
void ConManagerSetConnectionLocal(cm_connection_t *connection, bool local);

/*! \brief Increment number of users for a connection.
    \param connection Pointer to connection 
*/
void conManagerAddConnectionUser(cm_connection_t *connection);

/*! \brief Decrement number of users for a connection.
    \param connection Pointer to connection 
*/
void conManagerRemoveConnectionUser(cm_connection_t *connection);

/*! \brief Reset number of users for a connection.
    \param connection Pointer to connection 
*/
void conManagerResetConnectionUsers(cm_connection_t *connection);

/*! \brief Check if there are any users for a connection.
    \param connection Pointer to connection 
    \return TRUE if connection has any users, otherwise FALSE
*/
bool conManagerConnectionIsInUse(cm_connection_t *connection);

/*! \brief Get the connection lock pointer. 
    \return Pointer to the connection lock
*/
uint16* conManagerGetConnectionLock(cm_connection_t *connection);

/*! \brief Get the connection low power state.
    \param connection Pointer to connection 
    \param lp_state Pointer to lp_state which will be populated by this function 
*/
void conManagerGetLpState(cm_connection_t *connection, lpPerConnectionState* lp_state);

/*! \brief Set the connection low power state.
    \param connection Pointer to connection 
    \param lp_state The lp_state to set
*/
void conManagerSetLpState(cm_connection_t *connection, lpPerConnectionState lp_state);

/*! \brief Get the connection QoS list
    \param connection Pointer to connection 
    \return An array of size cm_qos_max to store counts
*/
uint8* conManagerGetQosList(cm_connection_t* connection);

/*! \brief Check if connection was initiated locally.
    \param connection Pointer to connection 
    \return TRUE if connection was locally initiated, otherwise FALSE
*/
bool conManagerConnectionIsLocallyInitiated(const cm_connection_t *connection);

/*! \brief Add (or update) connection to connections list. 
    \param tpaddr The Device address of the remote device
    \param state The initial state of the connection
    \param is_local TRUE if connection was initiated locally, otherwise FALSE.
    \return Pointer to the connection if successfully added/updated, otherwise NULL
*/
cm_connection_t *ConManagerAddConnection(const tp_bdaddr *tpaddr, cm_connection_state_t state, bool is_local);

/*! \brief Remove connection from the connections list.
    \param connection Pointer to connection 
*/
void conManagerRemoveConnection(cm_connection_t *connection);

/*! \brief Initialise connections list */
void ConManagerConnectionInit(void);

/*! \brief Get the head connection from the list
    \param iterator Optional iterator to use with ConManagerListNextConnection
    \return The connection at the head of the list
*/
cm_connection_t* ConManagerListHeadConnection(cm_list_iterator_t* iterator);

/*! \brief Get the next connection from an iterator
    \param iterator The iterator
    \return The next connection or NULL if the end of the list has been reached
*/
cm_connection_t* ConManagerListNextConnection(cm_list_iterator_t* iterator);

/*! \brief Get the Transport Bluetooth Address of a connection
    \param connection The connection
    \return The address on success, otherwise NULL
*/
const tp_bdaddr* ConManagerGetConnectionTpAddr(cm_connection_t* connection);

/*! \brief Find an active connection

    \return A connection that is not in a disconnected state or NULL
            if not found */
cm_connection_t *ConManagerFindFirstActiveLink(void);

#endif
