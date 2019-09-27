/*!
\copyright  Copyright (c) 2008 - 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
*/

#include "connection_manager.h"
#include "connection_manager_data.h"
#include "connection_manager_notify.h"

#include <logging.h>

#include <message.h>
#include <panic.h>

/*! @{ Macros to make connection manager messages. */
#define MAKE_CONMAN_MESSAGE(TYPE) TYPE##_T *message = PanicUnlessNew(TYPE##_T);
/*! @} */

/*! List of client tasks which have registered to receive CON_MANAGER_CONNECTION_IND
    messages */
static task_list_t connection_client_tasks;
static task_list_t bredr_observer_tasks;
static task_list_t ble_observer_tasks;

/******************************************************************************/
static void conManagerMsgConnectionInd(const tp_bdaddr *tpaddr, bool connected, hci_status reason)
{
    MAKE_CONMAN_MESSAGE(CON_MANAGER_CONNECTION_IND);
    message->bd_addr = tpaddr->taddr.addr;
    message->connected = connected;
    message->ble = (tpaddr->transport == TRANSPORT_BLE_ACL);
    message->reason = reason;
    TaskList_MessageSend(&connection_client_tasks, CON_MANAGER_CONNECTION_IND, message);
}

/******************************************************************************/
static task_list_t* conManagerGetTaskListForTransport(TRANSPORT_T transport)
{
    if(transport == TRANSPORT_BLE_ACL)
        return &ble_observer_tasks;
    
    return &bredr_observer_tasks;
}

/******************************************************************************/
static void conManagerMsgTpConnectionInd(const tp_bdaddr *tpaddr)
{
    task_list_t* list = conManagerGetTaskListForTransport(tpaddr->transport);
    
    MAKE_CONMAN_MESSAGE(CON_MANAGER_TP_CONNECT_IND);
    message->tpaddr = *tpaddr;
    TaskList_MessageSend(list, CON_MANAGER_TP_CONNECT_IND, message);
}

/******************************************************************************/
static void conManagerMsgTpDisconnectionInd(const tp_bdaddr *tpaddr, hci_status reason)
{
    task_list_t* list = conManagerGetTaskListForTransport(tpaddr->transport);
    
    MAKE_CONMAN_MESSAGE(CON_MANAGER_TP_DISCONNECT_IND);
    message->tpaddr = *tpaddr;
    message->reason = reason;
    TaskList_MessageSend(list, CON_MANAGER_TP_DISCONNECT_IND, message);
}

/******************************************************************************/
void conManagerNotifyObservers(const tp_bdaddr *tpaddr, bool connected, hci_status reason)
{
    conManagerMsgConnectionInd(tpaddr, connected, reason);
    if(connected)
    {
        conManagerMsgTpConnectionInd(tpaddr);
    }
    else
    {
        conManagerMsgTpDisconnectionInd(tpaddr, reason);
    }
}

/******************************************************************************/
void conManagerNotifyInit(void)
{
    /* create a task list to track tasks interested in connection
     * event indications */
    TaskList_Initialise(&connection_client_tasks);
    TaskList_Initialise(&bredr_observer_tasks);
    TaskList_Initialise(&ble_observer_tasks);
}

/******************************************************************************/
void ConManagerRegisterConnectionsClient(Task client_task)
{
    TaskList_AddTask(&connection_client_tasks, client_task);
}

/******************************************************************************/
void ConManagerRegisterTpConnectionsObserver(cm_transport_t transport_mask, Task client_task)
{
    if((transport_mask & cm_transport_bredr) == cm_transport_bredr)
        TaskList_AddTask(&bredr_observer_tasks, client_task);
    
    if((transport_mask & cm_transport_ble) == cm_transport_ble)
        TaskList_AddTask(&ble_observer_tasks, client_task);
}
