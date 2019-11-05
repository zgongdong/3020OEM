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
#include <timestamp_event.h>

#include <logging.h>

#include <message.h>
#include <panic.h>

#define TWSTOP_PROC_CLOSE_ALL_CFM_TIMEOUT_MS    (150)

typedef enum
{
    TWSTOP_PROC_INTERNAL_TIMEOUT,
};

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

static void TwsTopology_ProcedureCleanConnectionsResetProc(void)
{
    DEBUG_LOG("TwsTopology_ProcedureCleanConnectionsResetProc");

    TimestampEvent(TIMESTAMP_EVENT_CLEAN_CONNECTIONS_COMPLETED);

    MessageCancelFirst(TwsTopProcCleanConnectionsGetTask(), TWSTOP_PROC_INTERNAL_TIMEOUT);
    TwsTopProcCleanConnectionsGetTaskData()->active = FALSE;
}

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

    TimestampEvent(TIMESTAMP_EVENT_CLEAN_CONNECTIONS_STARTED);

    ConManagerTerminateAllAcls(TwsTopProcCleanConnectionsGetTask());
    MessageSendLater(TwsTopProcCleanConnectionsGetTask(), TWSTOP_PROC_INTERNAL_TIMEOUT, NULL, TWSTOP_PROC_CLOSE_ALL_CFM_TIMEOUT_MS);

    proc_start_cfm_fn(tws_topology_procedure_clean_connections, proc_result_success);
}


void TwsTopology_ProcedureCleanConnectionsCancel(twstop_proc_cancel_cfm_func_t proc_cancel_cfm_fn)
{
    DEBUG_LOG("TwsTopology_ProcedureCleanConnectionsCancel");

    TwsTopology_ProcedureCleanConnectionsResetProc();
    proc_cancel_cfm_fn(tws_topology_procedure_clean_connections, proc_result_success);
}


static void twsTopology_ProcCleanConnectionsHandleCloseAllCfm(void)
{
    DEBUG_LOG("twsTopology_ProcCleanConnectionsHandleCloseAllCfm");

    if (TwsTopProcCleanConnectionsGetTaskData()->active)
    {
        TwsTopology_ProcedureCleanConnectionsResetProc();
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
    case TWSTOP_PROC_INTERNAL_TIMEOUT:
        DEBUG_LOG("twsTopology_ProcCleanConnectionsHandleMessage ******************************** TIMEOUT waiting on CON_MANAGER_CLOSE_ALL_CFM");
        // fall-thru
    case CON_MANAGER_CLOSE_ALL_CFM:
        twsTopology_ProcCleanConnectionsHandleCloseAllCfm();
        break;

    default:
        break;
    }
}

