/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Procedure for a Primary to command the Secondary becomes Primary.
*/

#include "tws_topology_procedure_command_role_switch.h"
#include "tws_topology_procedures.h"
#include "tws_topology_peer_sig.h"

#include <logging.h>

#include <message.h>

void TwsTopology_ProcedureCommandRoleSwitchStart(Task result_task,
                                                 twstop_proc_start_cfm_func_t proc_start_cfm_fn,
                                                 twstop_proc_complete_func_t proc_complete_fn,
                                                 Message goal_data);
void TwsTopology_ProcedureCommandRoleSwitchCancel(twstop_proc_cancel_cfm_func_t proc_cancel_fn);

tws_topology_procedure_fns_t proc_command_role_switch_fns = {
    TwsTopology_ProcedureCommandRoleSwitchStart,
    TwsTopology_ProcedureCommandRoleSwitchCancel,
    NULL,
};

void TwsTopology_ProcedureCommandRoleSwitchStart(Task result_task,
                                                 twstop_proc_start_cfm_func_t proc_start_cfm_fn,
                                                 twstop_proc_complete_func_t proc_complete_fn,
                                                 Message goal_data)
{
    UNUSED(result_task);
    UNUSED(goal_data);

    DEBUG_LOG("TwsTopology_ProcedureCommandRoleSwitchStart");

    TwsTopology_SecondaryRoleSwitchCommand();

    /* start and complete immediately */
    proc_start_cfm_fn(tws_topology_procedure_command_role_switch, proc_result_success);
    TwsTopology_DelayedCompleteCfmCallback(proc_complete_fn, tws_topology_procedure_command_role_switch, proc_result_success);
}

void TwsTopology_ProcedureCommandRoleSwitchCancel(twstop_proc_cancel_cfm_func_t proc_cancel_fn)
{
    DEBUG_LOG("TwsTopology_ProcedureCommandRoleSwitchCancel");
    TwsTopology_DelayedCancelCfmCallback(proc_cancel_fn, tws_topology_procedure_command_role_switch, proc_result_success);
}
