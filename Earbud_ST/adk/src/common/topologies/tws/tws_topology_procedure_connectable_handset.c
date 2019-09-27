/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief
*/

#include "tws_topology_procedure_connectable_handset.h"
#include "tws_topology_procedures.h"

#include <handset_service.h>

#include <logging.h>

#include <message.h>


/*! Parameter definition for connectable enable */
const CONNECTABLE_HANDSET_PARAMS_T proc_connectable_handset_enable = { .enable = TRUE };
/*! Parameter definition for connectable disable */
const CONNECTABLE_HANDSET_PARAMS_T proc_connectable_handset_disable = { .enable = FALSE };

static void twsTopology_ProcConnectableHandsetHandleMessage(Task task, MessageId id, Message message);
void TwsTopology_ProcedureConnectableHandsetStart(Task result_task,
                                        twstop_proc_start_cfm_func_t proc_start_cfm_fn,
                                        twstop_proc_complete_func_t proc_complete_fn,
                                        Message goal_data);
void TwsTopology_ProcedureConnectableHandsetCancel(twstop_proc_cancel_cfm_func_t proc_cancel_cfm_fn);

tws_topology_procedure_fns_t proc_connectable_handset_fns = {
    TwsTopology_ProcedureConnectableHandsetStart,
    TwsTopology_ProcedureConnectableHandsetCancel,
    NULL,
};

typedef struct
{
    TaskData task;
    CONNECTABLE_HANDSET_PARAMS_T params;
} twsTopProcConnectableHandsetTaskData;

twsTopProcConnectableHandsetTaskData twstop_proc_connectable_handset = {twsTopology_ProcConnectableHandsetHandleMessage};

#define TwsTopProcConnectableHandsetGetTaskData()     (&twstop_proc_connectable_handset)
#define TwsTopProcConnectableHandsetGetTask()         (&twstop_proc_connectable_handset.task)

void TwsTopology_ProcedureConnectableHandsetStart(Task result_task,
                                        twstop_proc_start_cfm_func_t proc_start_cfm_fn,
                                        twstop_proc_complete_func_t proc_complete_fn,
                                        Message goal_data)
{
    twsTopProcConnectableHandsetTaskData* td = TwsTopProcConnectableHandsetGetTaskData();
    CONNECTABLE_HANDSET_PARAMS_T* params = (CONNECTABLE_HANDSET_PARAMS_T*)goal_data;

    UNUSED(result_task);

    td->params = *params;

    /* start the procedure */
    if (params->enable)
    {
        DEBUG_LOG("TwsTopology_ProcedureConnectableHandsetStart ENABLE");

        HandsetService_ConnectableRequest(TwsTopProcConnectableHandsetGetTask());
    }
    else
    {
        DEBUG_LOG("TwsTopology_ProcedureConnectableHandsetStart DISABLE");

        HandsetService_CancelConnectableRequest(TwsTopProcConnectableHandsetGetTask());
    }

    proc_start_cfm_fn(tws_topology_procedure_connectable_handset, proc_result_success);

    /* must use delayed cfm callback to prevent script engine recursion */
    TwsTopology_DelayedCompleteCfmCallback(proc_complete_fn,
                                           tws_topology_procedure_connectable_handset,
                                           proc_result_success);
}

void TwsTopology_ProcedureConnectableHandsetCancel(twstop_proc_cancel_cfm_func_t proc_cancel_cfm_fn)
{
    twsTopProcConnectableHandsetTaskData* td = TwsTopProcConnectableHandsetGetTaskData();

    if (td->params.enable)
    {
        DEBUG_LOG("TwsTopology_ProcedureConnectableHandsetCancel cancel enable");

        HandsetService_CancelConnectableRequest(TwsTopProcConnectableHandsetGetTask());
    }
    else
    {
        DEBUG_LOG("TwsTopology_ProcedureConnectableHandsetCancel cancel disable");

        HandsetService_ConnectableRequest(TwsTopProcConnectableHandsetGetTask());
    }

    /* must use delayed cfm callback to prevent script engine recursion */
    TwsTopology_DelayedCancelCfmCallback(proc_cancel_cfm_fn,
                                         tws_topology_procedure_connectable_handset,
                                         proc_result_success);
}

static void twsTopology_ProcConnectableHandsetHandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);
    UNUSED(id);
}
