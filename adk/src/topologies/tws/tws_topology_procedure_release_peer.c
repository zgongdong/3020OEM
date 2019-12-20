/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Procedure to release the lock on the peer link ACL.
            This can lead to the peer link being destroyed.
*/

#include "tws_topology_procedure_release_peer.h"

#include <bt_device.h>
#include <connection_manager.h>

#include <logging.h>




void TwsTopology_ProcedureReleasePeerStart(Task result_task,
                                        twstop_proc_start_cfm_func_t proc_start_cfm_fn,
                                        twstop_proc_complete_func_t proc_complete_fn,
                                        Message goal_data);
void TwsTopology_ProcedureReleasePeerCancel(twstop_proc_cancel_cfm_func_t proc_cancel_cfm_fn);

const tws_topology_procedure_fns_t proc_release_peer_fns = {
    TwsTopology_ProcedureReleasePeerStart,
    TwsTopology_ProcedureReleasePeerCancel,
    NULL,
};

void TwsTopology_ProcedureReleasePeerStart(Task result_task,
                                           twstop_proc_start_cfm_func_t proc_start_cfm_fn,
                                           twstop_proc_complete_func_t proc_complete_fn,
                                           Message goal_data)
{
    bdaddr peer_addr;

    UNUSED(result_task);
    UNUSED(goal_data);

    DEBUG_LOG("TwsTopology_ProcedureReleasePeerStart");

    if (appDeviceGetPeerBdAddr(&peer_addr))
    {
        ConManagerReleaseAcl(&peer_addr);
    }

    proc_start_cfm_fn(tws_topology_procedure_release_peer, proc_result_success);
    TwsTopology_DelayedCompleteCfmCallback(proc_complete_fn,
                                               tws_topology_procedure_release_peer,
                                               proc_result_success);
}

void TwsTopology_ProcedureReleasePeerCancel(twstop_proc_cancel_cfm_func_t proc_cancel_cfm_fn)
{
    DEBUG_LOG("TwsTopology_ProcedureReleasePeerCancel");

    TwsTopology_DelayedCancelCfmCallback(proc_cancel_cfm_fn,
                                         tws_topology_procedure_release_peer,
                                         proc_result_success);
}

