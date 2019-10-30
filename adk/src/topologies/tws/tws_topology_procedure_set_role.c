/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      
*/

#include "tws_topology_procedure_set_role.h"
#include "tws_topology_procedures.h"
#include "tws_topology_private.h"
#include "tws_topology_client_msgs.h"

/* proc specific includes here */

#include <logging.h>

#include <message.h>

const SET_ROLE_TYPE_T proc_set_role_primary_role = {tws_topology_role_primary, FALSE};
const SET_ROLE_TYPE_T proc_set_role_acting_primary_role = {tws_topology_role_primary, TRUE};
const SET_ROLE_TYPE_T proc_set_role_secondary_role = {tws_topology_role_secondary, FALSE};
const SET_ROLE_TYPE_T proc_set_role_dfu_role = {tws_topology_role_dfu, FALSE};
const SET_ROLE_TYPE_T proc_set_role_none = {tws_topology_role_none, FALSE};

void TwsTopology_ProcedureSetRoleStart(Task result_task,
                                        twstop_proc_start_cfm_func_t proc_start_cfm_fn,
                                        twstop_proc_complete_func_t proc_complete_fn,
                                        Message goal_data);
void TwsTopology_ProcedureSetRoleCancel(twstop_proc_cancel_cfm_func_t proc_cancel_cfm_fn);

tws_topology_procedure_fns_t proc_set_role_fns = {
    TwsTopology_ProcedureSetRoleStart,
    TwsTopology_ProcedureSetRoleCancel,
    NULL,
};

void TwsTopology_ProcedureSetRoleStart(Task result_task,
                                        twstop_proc_start_cfm_func_t proc_start_cfm_fn,
                                        twstop_proc_complete_func_t proc_complete_fn,
                                        Message goal_data)
{
    SET_ROLE_TYPE_T* role_type = (SET_ROLE_TYPE_T*)goal_data;

    UNUSED(result_task);

    DEBUG_LOG("TwsTopology_ProcedureSetRoleStart %u", role_type);

    /* procedure started synchronously so indicate success */
    proc_start_cfm_fn(tws_topology_procedure_set_role, proc_result_success);

    /* start the procedure 
     *  - update the role single point of truth here in topology
     */
    twsTopology_SetRole(role_type->role);
    twsTopology_SetActingInRole(role_type->acting_in_role);

    /* procedure completed synchronously so indicate completed already */
    TwsTopology_DelayedCompleteCfmCallback(proc_complete_fn, tws_topology_procedure_set_role, proc_result_success);
}

void TwsTopology_ProcedureSetRoleCancel(twstop_proc_cancel_cfm_func_t proc_cancel_cfm_fn)
{
    DEBUG_LOG("TwsTopology_ProcedureSetRoleCancel");
    /* nothing to cancel, just return success to keep goal engine happy */
    TwsTopology_DelayedCancelCfmCallback(proc_cancel_cfm_fn, tws_topology_procedure_set_role, proc_result_success);
}
