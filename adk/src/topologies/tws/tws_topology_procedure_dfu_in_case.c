/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Procedure for disconnecting connection not required during DFU in case.

            This is a simple procedure with the aim of leaving all 
            behaviours active.
*/

#include "tws_topology_procedure_dfu_in_case.h"
#include <hfp_profile.h>
#include "tws_topology_procedures.h"

#include <logging.h>

void TwsTopology_ProcedureDfuInCaseStart(Task result_task,
                                        twstop_proc_start_cfm_func_t proc_start_cfm_fn,
                                        twstop_proc_complete_func_t proc_complete_fn,
                                        Message goal_data);
void TwsTopology_ProcedureDfuInCaseCancel(twstop_proc_cancel_cfm_func_t proc_cancel_cfm_fn);

tws_topology_procedure_fns_t proc_dfu_in_case_fns = {
    TwsTopology_ProcedureDfuInCaseStart,
    TwsTopology_ProcedureDfuInCaseCancel,
    NULL,
};

void TwsTopology_ProcedureDfuInCaseStart(Task result_task,
                                        twstop_proc_start_cfm_func_t proc_start_cfm_fn,
                                        twstop_proc_complete_func_t proc_complete_fn,
                                        Message goal_data)
{
    UNUSED(goal_data);
    UNUSED(result_task);

    DEBUG_LOG("TwsTopology_ProcedureDfuInCaseStart");

    /* As of now disconnect HFP connection during DFU in case */
    appHfpDisconnectInternal();

    /* procedure started synchronously, confirm start now */
    proc_start_cfm_fn(tws_topology_procedure_dfu_in_case, proc_result_success);

    /* procedure completed synchronously so indicate completed already */
    TwsTopology_DelayedCompleteCfmCallback(proc_complete_fn, tws_topology_procedure_dfu_in_case, proc_result_success);
}

void TwsTopology_ProcedureDfuInCaseCancel(twstop_proc_cancel_cfm_func_t proc_cancel_cfm_fn)
{
    DEBUG_LOG("TwsTopology_ProcedureDfuInCaseCancel");

    /* nothing to cancel, just return success to keep goal engine happy */
    TwsTopology_DelayedCancelCfmCallback(proc_cancel_cfm_fn, tws_topology_procedure_dfu_in_case, proc_result_success);
}

