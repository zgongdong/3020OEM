#include "tws_topology_private.h"
#include "tws_topology_config.h"
#include "tws_topology.h"
#include "tws_topology_procedure_le_connectable_handset.h"
#include "tws_topology_procedures.h"
#include "handset_service.h"
#include <bt_device.h>
#include <logging.h>
#include <message.h>

/*! LE connectable procedure task data */
typedef struct
{
    TaskData task;
    twstop_proc_complete_func_t complete_fn;
} twsTopProcLeConnectableTaskData;

/*! LE connectable procedure message handler */
static void twsTopology_ProcLeConnectableHandleMessage(Task task, MessageId id, Message message);

twsTopProcLeConnectableTaskData twstop_proc_le_connectable = {.task = twsTopology_ProcLeConnectableHandleMessage};

#define TwsTopProcLeConnectableGetTaskData()     (&twstop_proc_le_connectable)
#define TwsTopProcLeConnectableGetTask()         (&twstop_proc_le_connectable.task)

const TWSTOP_PRIMARY_GOAL_LE_CONNECTABLE_HANDSET_T le_disable_connectable={FALSE};
const TWSTOP_PRIMARY_GOAL_LE_CONNECTABLE_HANDSET_T le_enable_connectable={TRUE};

void TwsTopology_ProcedureLeConnectableStart(Task result_task,
                                                   twstop_proc_start_cfm_func_t proc_start_cfm_fn,
                                                   twstop_proc_complete_func_t proc_complete_fn,
                                                   Message goal_data);

void TwsTopology_ProcedureLeConnectableCancel(twstop_proc_cancel_cfm_func_t proc_cancel_cfm_fn);

tws_topology_procedure_fns_t proc_le_connectable_fns = {
    TwsTopology_ProcedureLeConnectableStart,
    TwsTopology_ProcedureLeConnectableCancel,
    NULL,
    };

static void twsTopology_ProcLeConnectableReset(void)
{
    HandsetService_ClientUnregister(TwsTopProcLeConnectableGetTask());
}

static proc_result_t twsTopology_ProcGetStatus(handset_service_status_t status)
{
    proc_result_t result = proc_result_success;

    DEBUG_LOG("twsTopology_ProcGetStatus() Status: %d", status);

    /* Handle return status from procedure */
    if(status != handset_service_status_success)
       result = proc_result_failed;

    return result;
}

static void twsTopology_ProcLeConnectableHandleMessage(Task task, MessageId id, Message message)
{
    twsTopProcLeConnectableTaskData* td = TwsTopProcLeConnectableGetTaskData();
    UNUSED(task);
    UNUSED(message);

    DEBUG_LOG("twsTopology_ProcLeConnectableHandleMessage() Recieved id: 0x%x(%d)", id, id);
    switch (id)
    {
        case HANDSET_SERVICE_LE_CONNECTABLE_IND:
        {
            HANDSET_SERVICE_LE_CONNECTABLE_IND_T* msg = (HANDSET_SERVICE_LE_CONNECTABLE_IND_T*)message;
            DEBUG_LOG("twsTopology_ProcLeConnectableHandleMessage() LE Connectable: %d, Status: %d", msg->le_connectable,msg->status);
            td->complete_fn(tws_topology_procedure_le_connectable,twsTopology_ProcGetStatus(msg->status));
        }
        break;
        
        default:
        break;
    }
}

void TwsTopology_ProcedureLeConnectableStart(Task result_task,
                                                  twstop_proc_start_cfm_func_t proc_start_cfm_fn,
                                                  twstop_proc_complete_func_t proc_complete_fn,
                                                  Message goal_data)
{
    DEBUG_LOG("TwsTopology_ProcedureLeConnectableStart");
    UNUSED(result_task);

    TWSTOP_PRIMARY_GOAL_LE_CONNECTABLE_HANDSET_T* params = (TWSTOP_PRIMARY_GOAL_LE_CONNECTABLE_HANDSET_T*)goal_data;
    twsTopProcLeConnectableTaskData* td = TwsTopProcLeConnectableGetTaskData();
    td->complete_fn = proc_complete_fn;

    /* Register to be able to receive HANDSET_SERVICE_BLE_CONNECTABLE_CFM */
    HandsetService_ClientRegister(TwsTopProcLeConnectableGetTask());

    /* procedure started synchronously so indicate success */
    proc_start_cfm_fn(tws_topology_procedure_le_connectable, proc_result_success);

    DEBUG_LOG("TwsTopology_ProcedureLeConnectableStart Enable %d ", params->enable);
    /* Start/Stop advertising */
    HandsetService_SetBleConnectable(params->enable);
} 

void TwsTopology_ProcedureLeConnectableCancel(twstop_proc_cancel_cfm_func_t proc_cancel_cfm_fn)
{
    DEBUG_LOG("TwsTopology_ProcedureLeConnectableCancel");

    twsTopology_ProcLeConnectableReset();
    TwsTopology_DelayedCancelCfmCallback(proc_cancel_cfm_fn, tws_topology_procedure_le_connectable, proc_result_success);
}

