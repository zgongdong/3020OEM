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

#include <bredr_scan_manager.h>

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
} twsTopProcPriConnectablePeerTaskData;

twsTopProcPriConnectablePeerTaskData twstop_proc_pri_connectable_peer = {twsTopology_ProcPriConnectablePeerHandleMessage};

#define TwsTopProcPriConnectablePeerGetTaskData()     (&twstop_proc_pri_connectable_peer)
#define TwsTopProcPriConnectablePeerGetTask()         (&twstop_proc_pri_connectable_peer.task)

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

    if (params->enable)
    {
        DEBUG_LOG("TwsTopology_ProcedurePriConnectablePeerStart ENABLE");

        BredrScanManager_PageScanRequest(TwsTopProcPriConnectablePeerGetTask(), SCAN_MAN_PARAMS_TYPE_SLOW);
    }
    else
    {
        DEBUG_LOG("TwsTopology_ProcedurePriConnectablePeerStart DISABLE");

        BredrScanManager_PageScanRelease(TwsTopProcPriConnectablePeerGetTask());
    }

    /* procedure started synchronously so return TRUE */
    proc_start_cfm_fn(tws_topology_procedure_pri_connectable_peer, proc_result_success);
    /* nothing to wait for, so indicate complete as well. */
    TwsTopology_DelayedCompleteCfmCallback(proc_complete_fn, tws_topology_procedure_pri_connectable_peer, proc_result_success);
}

void TwsTopology_ProcedurePriConnectablePeerCancel(twstop_proc_cancel_cfm_func_t proc_cancel_cfm_fn)
{
    twsTopProcPriConnectablePeerTaskData* td = TwsTopProcPriConnectablePeerGetTaskData();

    DEBUG_LOG("TwsTopology_ProcedurePriConnectablePeerCancel");

    if (td->params.enable)
    {
        DEBUG_LOG("TwsTopology_ProcedurePriConnectablePeerCancel cancel enable");
 
        BredrScanManager_PageScanRelease(TwsTopProcPriConnectablePeerGetTask());
    }
    else
    {
        DEBUG_LOG("TwsTopology_ProcedurePriConnectablePeerCancel cancel disable");

        BredrScanManager_PageScanRequest(TwsTopProcPriConnectablePeerGetTask(), SCAN_MAN_PARAMS_TYPE_SLOW);
    }

    TwsTopology_DelayedCancelCfmCallback(proc_cancel_cfm_fn, tws_topology_procedure_pri_connectable_peer, proc_result_success);
}

static void twsTopology_ProcPriConnectablePeerHandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);

    switch (id)
    {
        default:
            DEBUG_LOG("twsTopology_ProcPriConnectablePeerHandleMessage id %x", id);
    }
}
