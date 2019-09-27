/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Procedure for allowing Bluetooth activities. 

            \note this allows the activities... but does not create them

            The procedured allows traditional Bluetooth (BREDR) and also Bluetooth
            Low Energy activities.

            BREDR is allowing by enabling scanning, used for creating connections.
            LE is allowed by enabling scanning and advertising. 

            Internally the requested enables are all run asynchronously. The 
            confirmation messages are handled separately and a conditional message
            send is used to confirm the overall permission.
*/

#include "tws_topology_procedure_permit_bt.h"
#include "tws_topology_procedures.h"

#include <le_advertising_manager.h>
#include <le_scan_manager_protected.h>
#include <bredr_scan_manager_protected.h>

#include <logging.h>

#include <message.h>
#include <panic.h>

static void twsTopology_ProcPermitBtHandleMessage(Task task, MessageId id, Message message);
void TwsTopology_ProcedurePermitBtStart(Task result_task,
                                        twstop_proc_start_cfm_func_t proc_start_cfm_fn,
                                        twstop_proc_complete_func_t proc_complete_fn,
                                        Message goal_data);
void TwsTopology_ProcedurePermitBtCancel(twstop_proc_cancel_cfm_func_t proc_cancel_cfm_fn);

tws_topology_procedure_fns_t proc_permit_bt_fns = {
    TwsTopology_ProcedurePermitBtStart,
    TwsTopology_ProcedurePermitBtCancel,
    NULL,
};

typedef struct
{
    TaskData task;
    twstop_proc_complete_func_t complete_fn;
    uint16 pending_permissions;
    bool active;
} twsTopProcPermitBtTaskData;

twsTopProcPermitBtTaskData twstop_proc_permit_bt = {twsTopology_ProcPermitBtHandleMessage};

/*! Enables run in parallel and we don't need confirmation of their success
    Bitmasks are used to record the status for debug purposes only */
#define TWS_TOP_PROC_PERMIT_LE_SCAN       (1 << 0)
#define TWS_TOP_PROC_PERMIT_ADVERTISING   (1 << 1)

#define TwsTopProcPermitBtGetTaskData()     (&twstop_proc_permit_bt)
#define TwsTopProcPermitBtGetTask()         (&twstop_proc_permit_bt.task)

typedef enum
{
        /*! All permit functions have completed */
    TWS_TOP_PROC_PERMIT_BT_INTERNAL_COMPLETED,
} tws_top_proc_permit_bt_internal_message_t;


void TwsTopology_ProcedurePermitBtStart(Task result_task,
                                        twstop_proc_start_cfm_func_t proc_start_cfm_fn,
                                        twstop_proc_complete_func_t proc_complete_fn,
                                        Message goal_data)
{
    twsTopProcPermitBtTaskData *permitData = TwsTopProcPermitBtGetTaskData();

    UNUSED(result_task);
    UNUSED(goal_data);

    DEBUG_LOG("TwsTopology_ProcedurePermitBtStart");

    permitData->complete_fn = proc_complete_fn;
    permitData->active = TRUE;

    BredrScanManager_ScanEnable();
    LeAdvertisingManager_AllowAdvertising(TwsTopProcPermitBtGetTask(), TRUE);
    LeScanManager_Enable(TwsTopProcPermitBtGetTask());

    permitData->pending_permissions =   TWS_TOP_PROC_PERMIT_LE_SCAN
                                      | TWS_TOP_PROC_PERMIT_ADVERTISING;
    MessageSendConditionally(TwsTopProcPermitBtGetTask(),
                             TWS_TOP_PROC_PERMIT_BT_INTERNAL_COMPLETED, NULL, 
                             &TwsTopProcPermitBtGetTaskData()->pending_permissions);

    /* procedure started synchronously so return TRUE */
    proc_start_cfm_fn(tws_topology_procedure_permit_bt, proc_result_success);
}

void TwsTopology_ProcedurePermitBtCancel(twstop_proc_cancel_cfm_func_t proc_cancel_cfm_fn)
{
    twsTopProcPermitBtTaskData *permitData = TwsTopProcPermitBtGetTaskData();

    DEBUG_LOG("TwsTopology_ProcedurePermitBtCancel");

    MessageCancelFirst(TwsTopProcPermitBtGetTask(), TWS_TOP_PROC_PERMIT_BT_INTERNAL_COMPLETED);
    permitData->pending_permissions = 0;
    permitData->active = FALSE;

    TwsTopology_DelayedCancelCfmCallback(proc_cancel_cfm_fn, tws_topology_procedure_permit_bt, proc_result_success);
}

static void twsTopology_ProcPermitBtHandleLeAdvManagerAllowCfm(const LE_ADV_MGR_ALLOW_ADVERTISING_CFM_T *cfm)
{
    DEBUG_LOG("twsTopology_ProcPermitBtHandleLeAdvManagerAllowCfm %u", cfm->status);

    if (le_adv_mgr_status_success == cfm->status)
    {
        TwsTopProcPermitBtGetTaskData()->pending_permissions &= ~TWS_TOP_PROC_PERMIT_ADVERTISING;
    }
    else
    {
        Panic();
    }
}

static void twsTopology_ProcPermitBtHandleLeScanManagerEnableCfm(const LE_SCAN_MANAGER_ENABLE_CFM_T *cfm)
{
    DEBUG_LOG("twsTopology_ProcPermitBtHandleLeScanManagerEnableCfm %u", cfm->status);

    if (LE_SCAN_MANAGER_RESULT_SUCCESS == cfm->status)
    {
        TwsTopProcPermitBtGetTaskData()->pending_permissions &= ~TWS_TOP_PROC_PERMIT_LE_SCAN;
    }
    else
    {
        Panic();
    }
}

static void twsTopology_ProcPermitBtHandleMessage(Task task, MessageId id, Message message)
{
    twsTopProcPermitBtTaskData *permitData = TwsTopProcPermitBtGetTaskData();

    UNUSED(task);

    if (!permitData->active)
    {
        return;
    }

    switch (id)
    {
        case TWS_TOP_PROC_PERMIT_BT_INTERNAL_COMPLETED:
            /*! \todo This is naive. Cancel needs to be dealt with */
            TwsTopProcPermitBtGetTaskData()->complete_fn(tws_topology_procedure_permit_bt, proc_result_success);
            permitData->active = FALSE;
            break;

        case LE_ADV_MGR_ALLOW_ADVERTISING_CFM:
            twsTopology_ProcPermitBtHandleLeAdvManagerAllowCfm((const LE_ADV_MGR_ALLOW_ADVERTISING_CFM_T *)message);
            break;

        case LE_SCAN_MANAGER_ENABLE_CFM:
            twsTopology_ProcPermitBtHandleLeScanManagerEnableCfm((const LE_SCAN_MANAGER_ENABLE_CFM_T *)message);
            break;

        default:
            break;
    }
}

