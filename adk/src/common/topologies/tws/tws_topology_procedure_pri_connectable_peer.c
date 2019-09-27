/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Procedure to enable/disable BREDR connectable (pagescan) for peer connectivity.
*/

#include "tws_topology_procedure_pri_connectable_peer.h"
#include "tws_topology_procedures.h"
#include "tws_topology_config.h"

#include <bredr_scan_manager.h>
#include <connection_manager.h>
#include <bt_device.h>
#include <peer_find_role.h>

#include <logging.h>

#include <message.h>

/*! Parameter definition for connectable enable */
const PRI_CONNECTABLE_PEER_PARAMS_T proc_connectable_peer_enable = { .enable = TRUE };
/*! Parameter definition for connectable disable */
const PRI_CONNECTABLE_PEER_PARAMS_T proc_pri_connectable_peer_disable = {FALSE};

static void twsTopology_ProcPriConnectablePeerHandleMessage(Task task, MessageId id, Message message);
void TwsTopology_ProcedurePriConnectablePeerStart(Task result_task,
                                        twstop_proc_start_cfm_func_t proc_start_cfm_fn,
                                        twstop_proc_complete_func_t proc_complete_fn,
                                        Message goal_data);
void TwsTopology_ProcedurePriConnectablePeerCancel(twstop_proc_cancel_cfm_func_t proc_cancel_cfm_fn);

tws_topology_procedure_fns_t proc_pri_connectable_peer_fns = {
    TwsTopology_ProcedurePriConnectablePeerStart,
    TwsTopology_ProcedurePriConnectablePeerCancel,
    NULL,
};

typedef struct
{
    TaskData task;
    PRI_CONNECTABLE_PEER_PARAMS_T params;
    twstop_proc_complete_func_t complete_fn;
    twstop_proc_cancel_cfm_func_t cancel_fn;
    bool active;
    proc_result_t complete_status;
} twsTopProcPriConnectablePeerTaskData;

twsTopProcPriConnectablePeerTaskData twstop_proc_pri_connectable_peer = {twsTopology_ProcPriConnectablePeerHandleMessage};

#define TwsTopProcPriConnectablePeerGetTaskData()     (&twstop_proc_pri_connectable_peer)
#define TwsTopProcPriConnectablePeerGetTask()         (&twstop_proc_pri_connectable_peer.task)

typedef enum
{
    /*! Timeout when the Secondary Earbud failed to connect ACL. */
    TWS_TOP_PROC_PRI_CONNECTABLE_PEER_INTERNAL_CONNECT_TIMEOUT,
} tws_top_proc_pri_connectable_peer_internal_message_t;

static void twsTopology_ProcPriConnectablePeerReset(void)
{
    twsTopProcPriConnectablePeerTaskData* td = TwsTopProcPriConnectablePeerGetTaskData();
    MessageCancelFirst(TwsTopProcPriConnectablePeerGetTask(), TWS_TOP_PROC_PRI_CONNECTABLE_PEER_INTERNAL_CONNECT_TIMEOUT);
    ConManagerUnregisterTpConnectionsObserver(cm_transport_bredr, TwsTopProcPriConnectablePeerGetTask());
    PeerFindRole_UnregisterTask(TwsTopProcPriConnectablePeerGetTask());
    td->active = FALSE;
}

void TwsTopology_ProcedurePriConnectablePeerStart(Task result_task,
                                        twstop_proc_start_cfm_func_t proc_start_cfm_fn,
                                        twstop_proc_complete_func_t proc_complete_fn,
                                        Message goal_data)
{
    twsTopProcPriConnectablePeerTaskData* td = TwsTopProcPriConnectablePeerGetTaskData();
    PRI_CONNECTABLE_PEER_PARAMS_T* params = (PRI_CONNECTABLE_PEER_PARAMS_T*)goal_data;

    UNUSED(result_task);

    /* start the procedure */
    td->params = *params;
    td->complete_fn = proc_complete_fn;
    td->cancel_fn = 0;
    td->complete_status = proc_result_success;
    td->active = TRUE;

    /* Register to be able to receive PEER_FIND_ROLE_CANCELLED */
    PeerFindRole_RegisterTask(TwsTopProcPriConnectablePeerGetTask());

    /* procedure starts synchronously so return TRUE */
    proc_start_cfm_fn(tws_topology_procedure_pri_connectable_peer, proc_result_success);

    if (td->params.enable)
    {
        DEBUG_LOG("TwsTopology_ProcedurePriConnectablePeerStart ENABLE");

        BredrScanManager_PageScanRequest(TwsTopProcPriConnectablePeerGetTask(), SCAN_MAN_PARAMS_TYPE_SLOW);

        /* register to get notified of connection to peer and
         * start timeout for the secondary to establish the ACL */
        ConManagerRegisterTpConnectionsObserver(cm_transport_bredr, TwsTopProcPriConnectablePeerGetTask());
        MessageSendLater(TwsTopProcPriConnectablePeerGetTask(), TWS_TOP_PROC_PRI_CONNECTABLE_PEER_INTERNAL_CONNECT_TIMEOUT, NULL, TwsTopologyConfig_PrimaryPeerConnectTimeoutMs()); 
    }
    else
    {
        DEBUG_LOG("TwsTopology_ProcedurePriConnectablePeerStart DISABLE");

        BredrScanManager_PageScanRelease(TwsTopProcPriConnectablePeerGetTask());

        /* Nothing more to do so set the completion status now. */
        td->complete_status = proc_result_success;

        /* Cancel any ongoing find role; wait for it to be confirmed */
        PeerFindRole_FindRoleCancel();
    }
}

void TwsTopology_ProcedurePriConnectablePeerCancel(twstop_proc_cancel_cfm_func_t proc_cancel_cfm_fn)
{
    twsTopProcPriConnectablePeerTaskData* td = TwsTopProcPriConnectablePeerGetTaskData();

    DEBUG_LOG("TwsTopology_ProcedurePriConnectablePeerCancel");

    if (td->params.enable)
    {
        twsTopology_ProcPriConnectablePeerReset();
        TwsTopology_DelayedCancelCfmCallback(proc_cancel_cfm_fn, tws_topology_procedure_pri_connectable_peer, proc_result_success);
    }
    else
    {
        /* Need to wait for the PeerFindRoleFindRoleCancel() to complete */
        td->cancel_fn = proc_cancel_cfm_fn;
    }
}

static void twsTopology_ProcPriConnectablePeerHandleConManagerTpConnectInd(CON_MANAGER_TP_CONNECT_IND_T* ind)
{
    twsTopProcPriConnectablePeerTaskData* td = TwsTopProcPriConnectablePeerGetTaskData();
    bdaddr peer_addr;

    appDeviceGetPeerBdAddr(&peer_addr);
    if (BdaddrIsSame(&ind->tpaddr.taddr.addr, &peer_addr))
    {
        DEBUG_LOG("twsTopology_ProcPriConnectablePeerHandleConManagerTpConnectInd");

        /* Nothing more to do so set the completion status now. */
        td->complete_status = proc_result_success;

        /* Cancel an ongoing find role - we have connected */
        PeerFindRole_FindRoleCancel();
    }
}

static void twsTopology_ProcPriConnectablePeerHandleConnectTimeout(void)
{
    twsTopProcPriConnectablePeerTaskData* td = TwsTopProcPriConnectablePeerGetTaskData();
    DEBUG_LOG("twsTopology_ProcPriConnectablePeerHandleConnectTimeout");

    /* Nothing more to do so set the completion status now. */
    td->complete_status = proc_result_timeout;

    /* Cancel an ongoing find role - new one should be requested */
    PeerFindRole_FindRoleCancel();
}

static void twsTopology_ProcPriConnectablePeerHandlePeerFindRoleCancelled(void)
{
    twsTopProcPriConnectablePeerTaskData* td = TwsTopProcPriConnectablePeerGetTaskData();

    DEBUG_LOG("twsTopology_ProcPriConnectablePeerHandlePeerFindRoleCancelled");

    if (td->cancel_fn)
    {
        TwsTopology_DelayedCancelCfmCallback(td->cancel_fn, tws_topology_procedure_pri_connectable_peer, proc_result_success);
    }
    else
    {
        /* The td->complete_status must have been set by the code that called
           PeerFindRole_FindRoleCancel */
        TwsTopology_DelayedCompleteCfmCallback(td->complete_fn, tws_topology_procedure_pri_connectable_peer, td->complete_status);
    }

    twsTopology_ProcPriConnectablePeerReset();
}

static void twsTopology_ProcPriConnectablePeerHandleMessage(Task task, MessageId id, Message message)
{
    twsTopProcPriConnectablePeerTaskData* td = TwsTopProcPriConnectablePeerGetTaskData();

    UNUSED(task);

    if (!td->active)
    {
        return;
    }

    switch (id)
    {
        case CON_MANAGER_TP_CONNECT_IND:
            twsTopology_ProcPriConnectablePeerHandleConManagerTpConnectInd((CON_MANAGER_TP_CONNECT_IND_T*)message);
            break;

        case TWS_TOP_PROC_PRI_CONNECTABLE_PEER_INTERNAL_CONNECT_TIMEOUT:
            twsTopology_ProcPriConnectablePeerHandleConnectTimeout();
            break;

        case PEER_FIND_ROLE_CANCELLED:
            twsTopology_ProcPriConnectablePeerHandlePeerFindRoleCancelled();
            break;

        default:
            DEBUG_LOG("twsTopology_ProcPriConnectablePeerHandleMessage id %x", id);
    }
}
