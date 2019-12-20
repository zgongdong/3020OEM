/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Procedure for allowing or disallowing handset connections

            \note this allows the connection if one arrives, but does not 
            make any changes to allow a connection
*/

#include "tws_topology_procedure_allow_handset_connect.h"

#include <connection_manager.h>

#include <logging.h>

#include <message.h>
#include <panic.h>


const ALLOW_HANDSET_CONNECT_PARAMS_T proc_allow_handset_connect_allow =  { .allow = TRUE };
const ALLOW_HANDSET_CONNECT_PARAMS_T proc_allow_handset_connect_disallow = { .allow = FALSE };


void TwsTopology_ProcedureAllowHandsetConnectStart(Task result_task,
                                                   twstop_proc_start_cfm_func_t proc_start_cfm_fn,
                                                   twstop_proc_complete_func_t proc_complete_fn,
                                                   Message goal_data);
void TwsTopology_ProcedureAllowHandsetConnectCancel(twstop_proc_cancel_cfm_func_t proc_cancel_cfm_fn);

const tws_topology_procedure_fns_t proc_allow_handset_connect_fns = {
    TwsTopology_ProcedureAllowHandsetConnectStart,
    TwsTopology_ProcedureAllowHandsetConnectCancel,
    NULL,
};


void TwsTopology_ProcedureAllowHandsetConnectStart(Task result_task,
                                                   twstop_proc_start_cfm_func_t proc_start_cfm_fn,
                                                   twstop_proc_complete_func_t proc_complete_fn,
                                                   Message goal_data)
{
    const ALLOW_HANDSET_CONNECT_PARAMS_T *param = (const ALLOW_HANDSET_CONNECT_PARAMS_T *)goal_data;

    UNUSED(result_task);

    DEBUG_LOG("TwsTopology_ProcedureAllowHandsetConnectStart allow %d", param->allow);

    ConManagerAllowHandsetConnect(param->allow);

    proc_start_cfm_fn(tws_topology_procedure_allow_handset_connection, proc_result_success);
    TwsTopology_DelayedCompleteCfmCallback(proc_complete_fn, tws_topology_procedure_allow_handset_connection, proc_result_success);
}

void TwsTopology_ProcedureAllowHandsetConnectCancel(twstop_proc_cancel_cfm_func_t proc_cancel_cfm_fn)
{
    DEBUG_LOG("TwsTopology_ProcedureAllowHandsetConnectCancel");

    proc_cancel_cfm_fn(tws_topology_procedure_allow_handset_connection, proc_result_success);
}

