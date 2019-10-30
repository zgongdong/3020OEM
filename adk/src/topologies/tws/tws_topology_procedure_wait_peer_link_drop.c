/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Procedure for a Secondary to wait for peer link to drop.
*/

#include "tws_topology_procedure_wait_peer_link_drop.h"
#include "tws_topology_procedures.h"

#include <connection_manager.h>
#include <bt_device.h>

#include <logging.h>

#include <message.h>

static void twsTopology_ProcWaitPeerLinkDropHandleMessage(Task task, MessageId id, Message message);
void TwsTopology_ProcedureWaitPeerLinkDropStart(Task result_task,
                                                twstop_proc_start_cfm_func_t proc_start_cfm_fn,
                                                twstop_proc_complete_func_t proc_complete_fn,
                                                Message goal_data);
void TwsTopology_ProcedureWaitPeerLinkDropCancel(twstop_proc_cancel_cfm_func_t proc_cancel_fn);

tws_topology_procedure_fns_t proc_wait_peer_link_drop_fns = {
    TwsTopology_ProcedureWaitPeerLinkDropStart,
    TwsTopology_ProcedureWaitPeerLinkDropCancel,
    NULL,
};

typedef struct
{
    TaskData task;
    twstop_proc_complete_func_t complete_fn;
} twsTopProcWaitPeerLinkDropTaskData;

twsTopProcWaitPeerLinkDropTaskData twstop_proc_wait_peer_link_drop = {twsTopology_ProcWaitPeerLinkDropHandleMessage};

#define TwsTopProcWaitPeerLinkDropGetTaskData()     (&twstop_proc_wait_peer_link_drop)
#define TwsTopProcWaitPeerLinkDropGetTask()         (&twstop_proc_wait_peer_link_drop.task)

void TwsTopology_ProcedureWaitPeerLinkDropStart(Task result_task,
                                                twstop_proc_start_cfm_func_t proc_start_cfm_fn,
                                                twstop_proc_complete_func_t proc_complete_fn,
                                                Message goal_data)
{
    twsTopProcWaitPeerLinkDropTaskData* td = TwsTopProcWaitPeerLinkDropGetTaskData();
    bdaddr peer_addr;

    UNUSED(result_task);
    UNUSED(goal_data);

    DEBUG_LOG("TwsTopology_ProcedureWaitPeerLinkDropStart");

    /* start is synchronous, use the callback to confirm now */
    proc_start_cfm_fn(tws_topology_procedure_wait_peer_link_drop, proc_result_success);

    /* register to get notification of peer disconnect, do it now before checking synchrnously
     * to avoid message race */
    ConManagerRegisterTpConnectionsObserver(cm_transport_bredr, TwsTopProcWaitPeerLinkDropGetTask());

    if (!appDeviceGetPeerBdAddr(&peer_addr))
    {
        /* no peer address, shouldn't happen, but would be a hard fail */
        TwsTopology_DelayedCompleteCfmCallback(proc_complete_fn,
                                               tws_topology_procedure_wait_peer_link_drop,
                                               proc_result_failed);
        ConManagerUnregisterTpConnectionsObserver(cm_transport_bredr, TwsTopProcWaitPeerLinkDropGetTask());
    }
    else
    {
        if (!ConManagerIsConnected(&peer_addr))
        {
            /* not connected already is success already */
            proc_start_cfm_fn(tws_topology_procedure_wait_peer_link_drop, proc_result_success);
            TwsTopology_DelayedCompleteCfmCallback(proc_complete_fn,
                                                   tws_topology_procedure_wait_peer_link_drop,
                                                   proc_result_success);
            ConManagerUnregisterTpConnectionsObserver(cm_transport_bredr, TwsTopProcWaitPeerLinkDropGetTask());
        }
        else
        {
            /* remember the completion callback and we wait for indication from connection
             * manager */
            td->complete_fn = proc_complete_fn;
            DEBUG_LOG("TwsTopology_ProcedureWaitPeerLinkDropStart waiting for peer link to drop");
            
            /*! \todo needs timeout to execute secondary no role find role */ 
        }
    }

}

void TwsTopology_ProcedureWaitPeerLinkDropCancel(twstop_proc_cancel_cfm_func_t proc_cancel_fn)
{
    DEBUG_LOG("TwsTopology_ProcedureWaitPeerLinkDropCancel");

    ConManagerUnregisterTpConnectionsObserver(cm_transport_bredr, TwsTopProcWaitPeerLinkDropGetTask());
    TwsTopology_DelayedCancelCfmCallback(proc_cancel_fn, tws_topology_procedure_wait_peer_link_drop, proc_result_success);
}

static void twsTopology_ProcWaitPeerLinkDropHandleConManTpDisconnectInd(const CON_MANAGER_TP_DISCONNECT_IND_T* ind)
{
    twsTopProcWaitPeerLinkDropTaskData* td = TwsTopProcWaitPeerLinkDropGetTaskData();

    DEBUG_LOG("twsTopology_ProcWaitPeerLinkDropHandleConManTpDisconnectInd %04x,%02x,%06lx",
                ind->tpaddr.taddr.addr.nap, ind->tpaddr.taddr.addr.uap, ind->tpaddr.taddr.addr.lap);

    if (appDeviceIsPeer(&ind->tpaddr.taddr.addr))
    {
        ConManagerUnregisterTpConnectionsObserver(cm_transport_bredr, TwsTopProcWaitPeerLinkDropGetTask());
        TwsTopology_DelayedCompleteCfmCallback(td->complete_fn,
                                               tws_topology_procedure_wait_peer_link_drop,
                                               proc_result_success);
    }
}

static void twsTopology_ProcWaitPeerLinkDropHandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch (id)
    {
        case CON_MANAGER_TP_DISCONNECT_IND:
            twsTopology_ProcWaitPeerLinkDropHandleConManTpDisconnectInd((CON_MANAGER_TP_DISCONNECT_IND_T*)message);
            break;
        default:
            DEBUG_LOG("twsTopology_ProcWaitPeerLinkDropHandleMessage unhandled id 0x%x", id);
            break;
    }
}

