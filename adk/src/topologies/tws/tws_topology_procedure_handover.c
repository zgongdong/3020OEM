#include "tws_topology_private.h"
#include "tws_topology_config.h"
#include "tws_topology.h"
#include "tws_topology_procedure_handover.h"
#include "tws_topology_procedures.h"
#include "handover_profile.h"
#include <bt_device.h>
#include <logging.h>
#include <message.h>

/*! handover delay parameter for script */
const HANDOVER_PARAMS_T handover_delay = {TRUE};
const HANDOVER_PARAMS_T handover_no_delay = {FALSE};

/*! handover procedure task data */
typedef struct
{
    TaskData task;
    bool handover_in_progress;
    twstop_proc_complete_func_t complete_fn;
} twsTopProcHandoverTaskData;

/*! handover procedure message handler */
static void twsTopology_ProcHandoverHandleMessage(Task task, MessageId id, Message message);

twsTopProcHandoverTaskData twstop_proc_handover = {.task = twsTopology_ProcHandoverHandleMessage};

#define TwsTopProcHandoverGetTaskData()     (&twstop_proc_handover)
#define TwsTopProcHandoverGetTask()         (&twstop_proc_handover.task)

typedef enum
{
    /*! Internal message to start the handover */
    TWS_TOP_PROC_HANDOVER_INTERNAL_START,
} tws_top_proc_handover_internal_message_t;

void TwsTopology_ProcedureHandoverStart(Task result_task,
                                        twstop_proc_start_cfm_func_t proc_start_cfm_fn,
                                        twstop_proc_complete_func_t proc_complete_fn,
                                        Message goal_data);

void TwsTopology_ProcedureHandoverCancel(twstop_proc_cancel_cfm_func_t proc_cancel_cfm_fn);

tws_topology_procedure_fns_t proc_handover_fns = {
    TwsTopology_ProcedureHandoverStart,
    TwsTopology_ProcedureHandoverCancel,
    NULL,
    };

static void twsTopology_ProcHandoverReset(void)
{
    MessageCancelFirst(TwsTopProcHandoverGetTask(), TWS_TOP_PROC_HANDOVER_INTERNAL_START);
    HandoverProfile_ClientUnregister(TwsTopProcHandoverGetTask());
}

static proc_result_t twsTopology_ProcGetStatus(handover_profile_status_t status)
{
    proc_result_t result = proc_result_success;

    DEBUG_LOG("twsTopology_ProcGetStatus() Status: %d", status);

    /* Handle return status from procedure */
    if(status != HANDOVER_PROFILE_STATUS_SUCCESS)
    {
        /* Handle failure or non-success cases */
        switch (status)
        {
            case HANDOVER_PROFILE_STATUS_PEER_CONNECT_FAILED:
            case HANDOVER_PROFILE_STATUS_PEER_CONNECT_CANCELLED:
            case HANDOVER_PROFILE_STATUS_PEER_DISCONNECTED:
            case HANDOVER_PROFILE_STATUS_PEER_LINKLOSS:
            case HANDOVER_PROFILE_STATUS_HANDOVER_FAILURE:
                result = proc_result_failed;
            break;

            case HANDOVER_PROFILE_STATUS_HANDOVER_TIMEOUT:
            case HANDOVER_PROFILE_STATUS_HANDOVER_VETOED:
            {
                TwsTopologyGetTaskData()->handover_info.handover_retry_count++;
                if(TwsTopologyGetTaskData()->handover_info.handover_retry_count >= TwsTopologyConfig_HandoverMaxRetryAttempts())
                {
                    TwsTopologyGetTaskData()->handover_info.handover_retry_count = 0;
                    result = proc_result_failed;
                }
                else
                {
                    /* we have exhausted maximum number of handover retry attempts, flag failed event */
                    result = proc_result_timeout;
                }
            }
            break;
            default:
            break;
        }
    }

    return result;
}

static void twsTopology_ProcHandoverStart(void)
{
    twsTopProcHandoverTaskData* td = TwsTopProcHandoverGetTaskData();
    handover_profile_status_t status = HANDOVER_PROFILE_STATUS_HANDOVER_FAILURE;
    bdaddr address;
    tp_bdaddr vm_addr;

    if(appDeviceIsHandsetConnected() && appDeviceGetHandsetBdAddr(&address))
    {
        DEBUG_LOG("twsTopology_ProcedureHandoverStart() Started");
        BdaddrTpFromBredrBdaddr(&vm_addr, &address);
        status = HandoverProfile_Handover(&vm_addr);
    }

    /* Wait for the asynchronous response from handover profile only if status is HANDOVER_PROFILE_STATUS_SUCCESS */
    if(status != HANDOVER_PROFILE_STATUS_SUCCESS)
    {
        twsTopology_ProcHandoverReset();
        TwsTopology_DelayedCompleteCfmCallback(td->complete_fn, tws_topology_procedure_handover,twsTopology_ProcGetStatus(status));
    }
    else
    {
        DEBUG_LOG("twsTopology_ProcedureHandoverStart() Success, handover is in progress ");
        td->handover_in_progress = TRUE;
    }
}

static void twsTopology_ProcHandoverHandleMessage(Task task, MessageId id, Message message)
{
    twsTopProcHandoverTaskData* td = TwsTopProcHandoverGetTaskData();
    UNUSED(task);
    UNUSED(message);

    DEBUG_LOG("twsTopology_ProcHandoverHandleMessage() Recieved id: 0x%x(%d)", id, id);
    switch (id)
    {
        case TWS_TOP_PROC_HANDOVER_INTERNAL_START:
        {
            twsTopology_ProcHandoverStart();
        }
        break;

        case HANDOVER_PROFILE_HANDOVER_COMPLETE_IND:
        {
            td->handover_in_progress = FALSE;
            twsTopology_ProcHandoverReset();
            TwsTopology_DelayedCompleteCfmCallback(td->complete_fn, tws_topology_procedure_handover,proc_result_success);
        }
        break;
        
        default:
        break;
    }
}


void TwsTopology_ProcedureHandoverStart(Task result_task,
                                                  twstop_proc_start_cfm_func_t proc_start_cfm_fn,
                                                  twstop_proc_complete_func_t proc_complete_fn,
                                                  Message goal_data)
{
    DEBUG_LOG("TwsTopology_ProcedureHandOverStart");
    UNUSED(result_task);

    HANDOVER_PARAMS_T* params = (HANDOVER_PARAMS_T*)goal_data;
    twsTopProcHandoverTaskData* td = TwsTopProcHandoverGetTaskData();
    td->complete_fn = proc_complete_fn;
    td->handover_in_progress = FALSE;

    /* Register to be able to receive HANDOVER_PROFILE_HANDOVER_STATUS_IND */
    HandoverProfile_ClientRegister(TwsTopProcHandoverGetTask());

    /* procedure started synchronously so indicate success */
    proc_start_cfm_fn(tws_topology_procedure_handover, proc_result_success);

     /* Trigger handover procedure internal start message */
    if(params->delay)
    {
        MessageSendLater(TwsTopProcHandoverGetTask(), TWS_TOP_PROC_HANDOVER_INTERNAL_START, NULL, TwsTopologyConfig_HandoverRetryTimeoutMs());
    }
    else
    {
        twsTopology_ProcHandoverStart();
    }
}

void TwsTopology_ProcedureHandoverCancel(twstop_proc_cancel_cfm_func_t proc_cancel_cfm_fn)
{
    twsTopProcHandoverTaskData* td = TwsTopProcHandoverGetTaskData();
    DEBUG_LOG("TwsTopology_ProcedureHandOverCancel");

    if(td->handover_in_progress)
    {
        /* handover has started and we are waiting for asynchronous complete message from handover profile, 
            hence flag cancel failure.*/
        TwsTopology_DelayedCancelCfmCallback(proc_cancel_cfm_fn, tws_topology_procedure_handover, proc_result_failed);
    }
    else
    {
        twsTopology_ProcHandoverReset();
        TwsTopology_DelayedCancelCfmCallback(proc_cancel_cfm_fn, tws_topology_procedure_handover, proc_result_success);
    }
}
