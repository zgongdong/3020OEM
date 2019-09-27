/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Procedure for closing all remaining connections. This should normally be 
            called after requesting normal shutdowns of connections.
*/

#include "tws_topology_procedure_clean_connections.h"

#include <connection_manager.h>

#include <logging.h>

#include <message.h>
#include <panic.h>


typedef struct
{
    TaskData task;
    twstop_proc_complete_func_t complete_fn;
    bool active;
} twsTopProcCleanConnectionsTaskData;

static void twsTopology_ProcCleanConnectionsHandleMessage(Task task, MessageId id, Message message);

twsTopProcCleanConnectionsTaskData twstop_proc_clean_connections = {.task = twsTopology_ProcCleanConnectionsHandleMessage};

#define TwsTopProcCleanConnectionsGetTaskData()     (&twstop_proc_clean_connections)
#define TwsTopProcCleanConnectionsGetTask()         (&twstop_proc_clean_connections.task)



void TwsTopology_ProcedureCleanConnectionsStart(Task result_task,
                                                   twstop_proc_start_cfm_func_t proc_start_cfm_fn,
                                                   twstop_proc_complete_func_t proc_complete_fn,
                                                   Message goal_data);
void TwsTopology_ProcedureCleanConnectionsCancel(twstop_proc_cancel_cfm_func_t proc_cancel_cfm_fn);


tws_topology_procedure_fns_t proc_clean_connections_fns = {
    TwsTopology_ProcedureCleanConnectionsStart,
    TwsTopology_ProcedureCleanConnectionsCancel,
    NULL,
};


void TwsTopology_ProcedureCleanConnectionsStart(Task result_task,
                                                twstop_proc_start_cfm_func_t proc_start_cfm_fn,
                                                twstop_proc_complete_func_t proc_complete_fn,
                                                Message goal_data)
{
    twsTopProcCleanConnectionsTaskData* td = TwsTopProcCleanConnectionsGetTaskData();

    UNUSED(result_task);
    UNUSED(goal_data);

    DEBUG_LOG("TwsTopology_ProcedureCleanConnectionsStart");

    td->complete_fn = proc_complete_fn;
    td->active = TRUE;

    ConManagerTerminateAllAcls(TwsTopProcCleanConnectionsGetTask());

    proc_start_cfm_fn(tws_topology_procedure_clean_connections, proc_result_success);
}


void TwsTopology_ProcedureCleanConnectionsCancel(twstop_proc_cancel_cfm_func_t proc_cancel_cfm_fn)
{
    DEBUG_LOG("TwsTopology_ProcedureCleanConnectionsCancel");

    proc_cancel_cfm_fn(tws_topology_procedure_clean_connections, proc_result_success);
    TwsTopProcCleanConnectionsGetTaskData()->active = FALSE;
}


static void twsTopology_ProcCleanConnectionsHandleCloseAllCfm(void)
{
    DEBUG_LOG("twsTopology_ProcCleanConnectionsHandleCloseAllCfm");

    if (TwsTopProcCleanConnectionsGetTaskData()->active)
    {
        TwsTopProcCleanConnectionsGetTaskData()->complete_fn(tws_topology_procedure_clean_connections, 
                                                             proc_result_success);
    }
}


static void twsTopology_ProcCleanConnectionsHandleMessage(Task task, MessageId id, Message message)
{
    twsTopProcCleanConnectionsTaskData* td = TwsTopProcCleanConnectionsGetTaskData();

    UNUSED(task);
    UNUSED(message);

    if (!td->active)
    {
        return;
    }

    switch (id)
    {
    case CON_MANAGER_CLOSE_ALL_CFM:
        twsTopology_ProcCleanConnectionsHandleCloseAllCfm();
        break;

    default:
        break;
    }
}

