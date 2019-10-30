/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Procedure for prohibiting Bluetooth activities.

            The procedured blocks traditional Bluetooth (BREDR) and also Bluetooth
            Low Energy activities.

            BREDR is blocked by stopping scanning, used for creating connections.
            LE is blocked by suspending scanning and advertising. This blocks 
            connections without the need to specifically block activity in the
            connection manager.

            Internally the requested suspends are all run asynchronously. The 
            confirmation messages are handled separately and a conditional message
            send is used to confirm the overall suspension.
*/

#include "tws_topology_procedure_prohibit_bt.h"
#include "tws_topology_procedures.h"

#include <le_advertising_manager.h>
#include <le_scan_manager_protected.h>
#include <bredr_scan_manager_protected.h>

#include <logging.h>

#include <message.h>
#include <panic.h>

static void twsTopology_ProcProhibitBtHandleMessage(Task task, MessageId id, Message message);
void TwsTopology_ProcedureProhibitBtStart(Task result_task,
                                        twstop_proc_start_cfm_func_t proc_start_cfm_fn,
                                        twstop_proc_complete_func_t proc_complete_fn,
                                        Message goal_data);
void TwsTopology_ProcedureProhibitBtCancel(twstop_proc_cancel_cfm_func_t proc_cancel_cfm_fn);

tws_topology_procedure_fns_t proc_prohibit_bt_fns = {
    TwsTopology_ProcedureProhibitBtStart,
    TwsTopology_ProcedureProhibitBtCancel,
    NULL,
};

typedef struct
{
    TaskData task;
    twstop_proc_complete_func_t complete_fn;
    uint16 pending_prohibitions;
    bool active;
} twsTopProcProhibitBtTaskData;

twsTopProcProhibitBtTaskData twstop_proc_prohibit_bt = {twsTopology_ProcProhibitBtHandleMessage};

#define TWS_TOP_PROC_PROHIBIT_BREDR_SCAN    (1 << 0)
#define TWS_TOP_PROC_PROHIBIT_LE_SCAN       (1 << 1)
#define TWS_TOP_PROC_PROHIBIT_ADVERTISING   (1 << 2)

#define TwsTopProcProhibitBtGetTaskData()     (&twstop_proc_prohibit_bt)
#define TwsTopProcProhibitBtGetTask()         (&twstop_proc_prohibit_bt.task)

typedef enum
{
        /*! All prohibit functions have completed */
    TWS_TOP_PROC_PROHIBIT_BT_INTERNAL_COMPLETED,
} tws_top_proc_prohibit_bt_internal_message_t;


void TwsTopology_ProcedureProhibitBtStart(Task result_task,
                                        twstop_proc_start_cfm_func_t proc_start_cfm_fn,
                                        twstop_proc_complete_func_t proc_complete_fn,
                                        Message goal_data)
{
    twsTopProcProhibitBtTaskData *td = TwsTopProcProhibitBtGetTaskData();
    UNUSED(result_task);
    UNUSED(goal_data);

    DEBUG_LOG("TwsTopology_ProcedureProhibitBtStart");

    td->complete_fn = proc_complete_fn;
    td->active = TRUE;

    BredrScanManager_ScanDisable(TwsTopProcProhibitBtGetTask());
    LeAdvertisingManager_AllowAdvertising(TwsTopProcProhibitBtGetTask(), FALSE);
    LeScanManager_Disable(TwsTopProcProhibitBtGetTask());

    td->pending_prohibitions =   TWS_TOP_PROC_PROHIBIT_BREDR_SCAN
                               | TWS_TOP_PROC_PROHIBIT_LE_SCAN
                               | TWS_TOP_PROC_PROHIBIT_ADVERTISING;
    MessageSendConditionally(TwsTopProcProhibitBtGetTask(),
                             TWS_TOP_PROC_PROHIBIT_BT_INTERNAL_COMPLETED, NULL, 
                             &TwsTopProcProhibitBtGetTaskData()->pending_prohibitions);

    /* procedure started synchronously so return TRUE */
    proc_start_cfm_fn(tws_topology_procedure_prohibit_bt, proc_result_success);
}

void TwsTopology_ProcedureProhibitBtCancel(twstop_proc_cancel_cfm_func_t proc_cancel_cfm_fn)
{
    twsTopProcProhibitBtTaskData* td = TwsTopProcProhibitBtGetTaskData();

    DEBUG_LOG("TwsTopology_ProcedureProhibitBtCancel");

    MessageCancelFirst(TwsTopProcProhibitBtGetTask(), TWS_TOP_PROC_PROHIBIT_BT_INTERNAL_COMPLETED);
    td->pending_prohibitions = 0;
    td->active = FALSE;

    TwsTopology_DelayedCancelCfmCallback(proc_cancel_cfm_fn, tws_topology_procedure_prohibit_bt, proc_result_success);
}

static void twsTopology_ProcProhibitBtHandleBredrScanManagerDisableCfm(const BREDR_SCAN_MANAGER_DISABLE_CFM_T *cfm)
{
    DEBUG_LOG("twsTopology_ProcProhibitBtHandleBredrScanManagerDisableCfm %u", cfm->disabled);

    if (cfm->disabled)
    {
        TwsTopProcProhibitBtGetTaskData()->pending_prohibitions &= ~TWS_TOP_PROC_PROHIBIT_BREDR_SCAN;
    }
    else
    {
        Panic();
    }
}


static void twsTopology_ProcProhibitBtHandleLeAdvManagerAllowCfm(const LE_ADV_MGR_ALLOW_ADVERTISING_CFM_T *cfm)
{
    DEBUG_LOG("twsTopology_ProcProhibitBtHandleLeAdvManagerAllowCfm %u", cfm->status);

    if (le_adv_mgr_status_success == cfm->status)
    {
        TwsTopProcProhibitBtGetTaskData()->pending_prohibitions &= ~TWS_TOP_PROC_PROHIBIT_ADVERTISING;
    }
    else
    {
        Panic();
    }
}


static void twsTopology_ProcProhibitBtHandleLeScanManagerDisableCfm(const LE_SCAN_MANAGER_DISABLE_CFM_T *cfm)
{
    DEBUG_LOG("twsTopology_ProcProhibitBtHandleLeScanManagerDisableCfm %u", cfm->status);

    switch (cfm->status)
    {
    case LE_SCAN_MANAGER_RESULT_SUCCESS:
        TwsTopProcProhibitBtGetTaskData()->pending_prohibitions &= ~TWS_TOP_PROC_PROHIBIT_LE_SCAN;
        break;

    case LE_SCAN_MANAGER_RESULT_BUSY:
        /* Retry the disable until it works. */
        LeScanManager_Disable(TwsTopProcProhibitBtGetTask());
        break;

    default:
        Panic();
    }
}


static void twsTopology_ProcProhibitBtHandleMessage(Task task, MessageId id, Message message)
{
    twsTopProcProhibitBtTaskData* td = TwsTopProcProhibitBtGetTaskData();

    UNUSED(task);

    if (!td->active)
    {
        return;
    }

    switch (id)
    {
        case TWS_TOP_PROC_PROHIBIT_BT_INTERNAL_COMPLETED:
            /*! \todo This is naive. Cancel needs to be dealt with */
            td->complete_fn(tws_topology_procedure_prohibit_bt, proc_result_success);
            td->active = FALSE;
            break;

        case BREDR_SCAN_MANAGER_DISABLE_CFM:
            twsTopology_ProcProhibitBtHandleBredrScanManagerDisableCfm((const BREDR_SCAN_MANAGER_DISABLE_CFM_T*)message);
            break;

        case LE_ADV_MGR_ALLOW_ADVERTISING_CFM:
            twsTopology_ProcProhibitBtHandleLeAdvManagerAllowCfm((const LE_ADV_MGR_ALLOW_ADVERTISING_CFM_T*)message);
            break;

        case LE_SCAN_MANAGER_DISABLE_CFM:
            twsTopology_ProcProhibitBtHandleLeScanManagerDisableCfm((const LE_SCAN_MANAGER_DISABLE_CFM_T *)message);
            break;

        default:
            break;
    }
}
