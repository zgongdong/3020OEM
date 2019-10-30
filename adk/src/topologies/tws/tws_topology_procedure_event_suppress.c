/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Procedure to suppress or expose event injections into topology rules engines. 
*/

#include "tws_topology_procedures.h"
#include "tws_topology_private.h"
#include "tws_topology_procedure_event_suppress.h"
#include "tws_topology_rule_events.h"

/* proc specific includes here */

#include <logging.h>
#include <phy_state.h>

#include <message.h>

const EVENT_SUPPRESS_TYPE_T proc_event_suppress = {TRUE, FALSE};
const EVENT_SUPPRESS_TYPE_T proc_event_expose_in_case = {FALSE, TRUE};
const EVENT_SUPPRESS_TYPE_T proc_event_expose_out_case = {FALSE, FALSE};

void TwsTopology_ProcedureEventSuppressStart(Task result_task,
                                             twstop_proc_start_cfm_func_t proc_start_cfm_fn,
                                             twstop_proc_complete_func_t proc_complete_fn,
                                             Message goal_data);
void TwsTopology_ProcedureEventSuppressCancel(twstop_proc_cancel_cfm_func_t proc_cancel_cfm_fn);

tws_topology_procedure_fns_t proc_event_suppress_fns = {
    TwsTopology_ProcedureEventSuppressStart,
    TwsTopology_ProcedureEventSuppressCancel,
    NULL,
};

void TwsTopology_ProcedureEventSuppressStart(Task result_task,
                                             twstop_proc_start_cfm_func_t proc_start_cfm_fn,
                                             twstop_proc_complete_func_t proc_complete_fn,
                                             Message goal_data)
{
    EVENT_SUPPRESS_TYPE_T* suppress_type = (EVENT_SUPPRESS_TYPE_T*)goal_data;

    UNUSED(result_task);

    DEBUG_LOG("TwsTopology_ProcedureEventSuppressStart suppress %u expected_in_case %u", suppress_type->suppress, suppress_type->expected_in_case);

    /* procedure started synchronously so indicate success */
    proc_start_cfm_fn(tws_topology_procedure_event_suppress, proc_result_success);

    if (suppress_type->suppress)
    {
        TwsTopologyGetTaskData()->suppress_events = TRUE;
    }
    else
    {
        TwsTopologyGetTaskData()->suppress_events = FALSE;

        /* generate latest phy state event into rules engine now that suppression has been lifted. */
        phyState current_state = appPhyStateGetState();
        if (suppress_type->expected_in_case && (current_state != PHY_STATE_IN_CASE))
        {
            twsTopology_RulesSetEvent(TWSTOP_RULE_EVENT_OUT_CASE);
        }
        else if (!suppress_type->expected_in_case && (current_state == PHY_STATE_IN_CASE))
        {
            twsTopology_RulesSetEvent(TWSTOP_RULE_EVENT_IN_CASE);
        }
    }

    /* procedure completed synchronously so indicate completed already */
    TwsTopology_DelayedCompleteCfmCallback(proc_complete_fn, tws_topology_procedure_event_suppress, proc_result_success);
}

void TwsTopology_ProcedureEventSuppressCancel(twstop_proc_cancel_cfm_func_t proc_cancel_cfm_fn)
{
    DEBUG_LOG("TwsTopology_ProcedureEventSuppressCancel");
    /* nothing to cancel, just return success to keep goal engine happy */
    TwsTopology_DelayedCancelCfmCallback(proc_cancel_cfm_fn, tws_topology_procedure_event_suppress, proc_result_success);
}
