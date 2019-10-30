/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Procedure for prohibiting Bluetooth Low Energy connections

            \note this prohibits the activities... but does not create them

            The procedure informs connection manager that such connections are
            prohibited. 
*/

#include "tws_topology_procedure_prohibit_connection_le.h"
#include "tws_topology_procedures.h"

#include <connection_manager.h>

#include <logging.h>

#include <message.h>
#include <panic.h>

void TwsTopology_ProcedureProhibitConnectionLeStart(Task result_task,
                                        twstop_proc_start_cfm_func_t proc_start_cfm_fn,
                                        twstop_proc_complete_func_t proc_complete_fn,
                                        Message goal_data);
void TwsTopology_ProcedureProhibitConnectionLeCancel(twstop_proc_cancel_cfm_func_t proc_cancel_cfm_fn);

tws_topology_procedure_fns_t proc_prohibit_connection_le_fns = {
    TwsTopology_ProcedureProhibitConnectionLeStart,
    TwsTopology_ProcedureProhibitConnectionLeCancel,
    NULL,
};

void TwsTopology_ProcedureProhibitConnectionLeStart(Task result_task,
                                                 twstop_proc_start_cfm_func_t proc_start_cfm_fn,
                                                 twstop_proc_complete_func_t proc_complete_fn,
                                                 Message goal_data)
{
    UNUSED(result_task);
    UNUSED(goal_data);

    DEBUG_LOG("TwsTopology_ProcedureProhibitConnectionLeStart");

    ConManagerAllowConnection(cm_transport_ble, FALSE);

    proc_start_cfm_fn(tws_topology_procedure_prohibit_connection_le, proc_result_success);
    TwsTopology_DelayedCompleteCfmCallback(proc_complete_fn, tws_topology_procedure_prohibit_connection_le, proc_result_success);
}

void TwsTopology_ProcedureProhibitConnectionLeCancel(twstop_proc_cancel_cfm_func_t proc_cancel_cfm_fn)
{
    DEBUG_LOG("TwsTopology_ProcedureProhibitConnectionLeCancel");

    proc_cancel_cfm_fn(tws_topology_procedure_prohibit_connection_le, proc_result_success);
}

