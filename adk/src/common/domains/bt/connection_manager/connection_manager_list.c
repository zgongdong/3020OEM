/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
*/

#include "connection_manager_list.h"
#include "connection_manager_qos.h"
#include "connection_manager_msg.h"

#include <bdaddr.h>
#include <logging.h>
#include <panic.h>

/*! Structure used to hold information about a single device, managed by
    the connection manager.
    This structure should not be accessed directly.
 */
struct __cm_connection_t
{
    /*! Bluetooth address of the device */
    TaskData task_data;
    /*! Bluetooth address of the device */
    tp_bdaddr tpaddr;
    /*! A non-zero value indicates that a procedure is operational on
        the device. It will clear to 0 when complete */
    uint16 lock;
    /*! State of the device */
    cm_connection_state_t state:4;
    /*! Number of functions interested in this device currently.
        \note The count relies on calls to \ref ConManagerCreateAcl
        and \ref ConManagerReleaseAcl being paired */
    unsigned users:3;
    /*! Flag that indicates if this is an incoming ACL */
    bool local:1;
    /*! The current link policy for this connection */
    lpPerConnectionState lp_state;
    /*! The most recently requested QoS for this connection */
    uint8 qos_list[cm_qos_max]; 
};

#define CON_MANAGER_MAX_CONNECTIONS 4

cm_connection_t connections[CON_MANAGER_MAX_CONNECTIONS];

#define for_all_connections(connection) for(connection = &connections[0]; connection < &connections[CON_MANAGER_MAX_CONNECTIONS]; connection++)

/******************************************************************************/
Task ConManagerGetTask(cm_connection_t* connection)
{
    if(connection)
    {
        if(connection->task_data.handler)
        {
            return &connection->task_data;
        }
    }
    return NULL;
}

/******************************************************************************/
void ConManagerDebugAddress(const tp_bdaddr *tpaddr)
{
    UNUSED(tpaddr);
    DEBUG_LOGF("ConManagerDebugAddress, transport %02x, type %02x, addr %04x,%02x,%06lx",
               tpaddr->transport, 
               tpaddr->taddr.type,
               tpaddr->taddr.addr.nap, 
               tpaddr->taddr.addr.uap, 
               tpaddr->taddr.addr.lap);
}

/******************************************************************************/
void ConManagerDebugConnection(const cm_connection_t *connection)
{
    DEBUG_LOGF("ConManagerDebugConnection, state %u, lock %u, users %u",
               connection->state, connection->lock, connection->users);
    ConManagerDebugAddress(&connection->tpaddr);
}

/******************************************************************************/
void ConManagerSetConnectionState(cm_connection_t *connection, cm_connection_state_t state)
{
    connection->state = state;
    connection->lock = (state & ACL_STATE_LOCK);

    DEBUG_LOGF("ConManagerSetConnectionState");
    ConManagerDebugConnection(connection);
}

/******************************************************************************/
cm_connection_state_t conManagerGetConnectionState(const cm_connection_t *connection)
{
    if(connection)
        return connection->state;
    
    return ACL_DISCONNECTED;
}

/******************************************************************************/
cm_connection_t *ConManagerFindConnectionFromBdAddr(const tp_bdaddr *tpaddr)
{
    cm_connection_t *connection;
    
    for_all_connections(connection)
    {
        if (BdaddrTpIsSame(&connection->tpaddr, tpaddr))
            return connection;
    }
    return NULL;
}

/******************************************************************************/
static cm_connection_t *conManagerFindConnectionFromState(cm_transport_t transport_mask, cm_connection_state_t state)
{
    cm_connection_t *connection;
    
    for_all_connections(connection)
    {
        cm_transport_t connection_transport = (cm_transport_t)(1 << connection->tpaddr.transport);
        
        if(connection_transport & transport_mask)
        {
            if (connection->state == state)
                return connection;
        }
    }
    return NULL;
}

cm_connection_t *ConManagerFindFirstActiveLink(void)
{
    cm_connection_t *connection;

    for_all_connections(connection)
    {
        if (   connection->state != ACL_DISCONNECTED
            && connection->state != ACL_DISCONNECTED_LINK_LOSS)
        {
            DEBUG_LOG("ConManagerFindFirstActiveLink. Found 0x%x. State: %x Lock:%x Users:%x",
                        connection->tpaddr.taddr.addr.lap, 
                        connection->state, connection->lock,
                        connection->users);
            return connection;
        }
    }

    DEBUG_LOG("ConManagerFindFirstActiveLink. None found.");
    return NULL;
}


/******************************************************************************/
static cm_connection_t *conManagerFindUnusedConnection(void)
{
    tp_bdaddr tpaddr;
    BdaddrTpSetEmpty(&tpaddr);
    
    DEBUG_LOGF("ConManagerGetUnusedConnection");
    return ConManagerFindConnectionFromBdAddr(&tpaddr);
}

/******************************************************************************/
bool conManagerAnyLinkInState(cm_transport_t transport_mask, cm_connection_state_t state)
{
    cm_connection_t *connection = conManagerFindConnectionFromState(transport_mask, state);
    
    if(connection)
    {
        DEBUG_LOGF("ConManagerAnyLinkInState %d", state);
        ConManagerDebugConnection(connection);
        return TRUE;
    }
    return FALSE;
}

/******************************************************************************/
void ConManagerSetConnectionLocal(cm_connection_t *connection, bool local)
{
    if (connection)
        connection->local = local;
}

/******************************************************************************/
void conManagerAddConnectionUser(cm_connection_t *connection)
{
    if(connection)
        connection->users += 1;
}

/******************************************************************************/
void conManagerRemoveConnectionUser(cm_connection_t *connection)
{
    if(connection && connection->users)
        connection->users -= 1;
}

/******************************************************************************/
void conManagerResetConnectionUsers(cm_connection_t *connection)
{
    if(connection)
        connection->users = 0;
}

/******************************************************************************/
bool conManagerConnectionIsInUse(cm_connection_t *connection)
{
    if(connection)
        return (connection->users > 0);
    
    return FALSE;
}

/******************************************************************************/
uint16* conManagerGetConnectionLock(cm_connection_t *connection)
{
    if(connection)
        return &connection->lock;
    
    return NULL;
}

/******************************************************************************/
void conManagerGetLpState(cm_connection_t *connection, lpPerConnectionState* lp_state)
{
    if (connection)
        *lp_state = connection->lp_state;
}

/******************************************************************************/
void conManagerSetLpState(cm_connection_t *connection, lpPerConnectionState lp_state)
{
    if (connection)
        connection->lp_state = lp_state;
}

/******************************************************************************/
uint8* conManagerGetQosList(cm_connection_t* connection)
{
    if(connection)
    {
        return connection->qos_list;
    }
    
    return NULL;
}

/******************************************************************************/
bool conManagerConnectionIsLocallyInitiated(const cm_connection_t *connection)
{
    return connection ? connection->local : FALSE;
}

/******************************************************************************/
static void conManagerResetConnection(cm_connection_t* connection)
{
    if (connection)
    {
        MessageFlushTask(&connection->task_data);
        memset(connection, 0, sizeof(cm_connection_t));
        ConManagerSetConnectionState(connection, ACL_DISCONNECTED);
        BdaddrTpSetEmpty(&connection->tpaddr);
        conManagerResetConnectionUsers(connection);
        connection->lp_state.pt_index = POWERTABLE_UNASSIGNED;
    }
}

/******************************************************************************/
static cm_connection_t* conManagerReuseLinkLossConnection(void)
{
    cm_connection_t *connection = conManagerFindConnectionFromState(cm_transport_all, ACL_DISCONNECTED_LINK_LOSS);
    
    conManagerResetConnection(connection);
    
    return connection;
}

/******************************************************************************/
cm_connection_t* ConManagerAddConnection(const tp_bdaddr *tpaddr, cm_connection_state_t state, bool is_local)
{
    cm_connection_t *connection = ConManagerFindConnectionFromBdAddr(tpaddr);
    if (connection)
    {
        DEBUG_LOG("ConManagerAddConnection - existing found");
        ConManagerSetConnectionState(connection, state);
        return connection;
    }
    else
    {
        connection = conManagerFindUnusedConnection();
        if (connection == NULL)
        {
            DEBUG_LOG("ConManagerAddConnection - re-use link loss");
            connection = conManagerReuseLinkLossConnection();
        }

        if (connection)
        {
            DEBUG_LOG("ConManagerAddConnection - new");
            conManagerResetConnection(connection);
            
            connection->tpaddr = *tpaddr;
            ConManagerSetConnectionState(connection, state);
            ConManagerSetConnectionLocal(connection, is_local);
            connection->task_data.handler = ConManagerConnectionHandleMessage;
            return connection;
        }

        DEBUG_LOGF("ConManagerAddConnection - No space!");
        Panic();
    }

    return NULL;
}

/******************************************************************************/
void conManagerRemoveConnection(cm_connection_t *connection)
{
    if (connection)
    {
        conManagerResetConnection(connection);
    }
}

/******************************************************************************/
void ConManagerConnectionInit(void)
{
    cm_connection_t* connection;
    
    for_all_connections(connection)
    {
        conManagerResetConnection(connection);
    }
}

/******************************************************************************/
cm_connection_t* ConManagerListHeadConnection(cm_list_iterator_t* iterator)
{
    if(iterator)
    {
        iterator->connection = &connections[0];
    }
    
    return &connections[0];
}

/******************************************************************************/
cm_connection_t* ConManagerListNextConnection(cm_list_iterator_t* iterator)
{
    cm_connection_t* connection = NULL;
    
    PanicNull(iterator);
    
    iterator->connection++;
    
    if(iterator->connection < &connections[CON_MANAGER_MAX_CONNECTIONS])
    {
        connection = iterator->connection;
    }
    
    return connection;
}

/******************************************************************************/
const tp_bdaddr* ConManagerGetConnectionTpAddr(cm_connection_t* connection)
{
    if(connection)
    {
        return &connection->tpaddr;
    }
    return NULL;
}
