/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief
*/

#include "tws_topology_procedure_disconnect_handset.h"
#include "tws_topology_procedures.h"

#include <bt_device.h>
#include <device_list.h>
#include <device_properties.h>
#include <handset_service.h>
#include <connection_manager.h>

#include <logging.h>

#include <message.h>



static void twsTopology_ProcDisconnectHandsetHandleMessage(Task task, MessageId id, Message message);
void TwsTopology_ProcedureDisconnectHandsetStart(Task result_task,
                                        twstop_proc_start_cfm_func_t proc_start_cfm_fn,
                                        twstop_proc_complete_func_t proc_complete_fn,
                                        Message goal_data);
void TwsTopology_ProcedureDisconnectHandsetCancel(twstop_proc_cancel_cfm_func_t proc_cancel_cfm_fn);

tws_topology_procedure_fns_t proc_disconnect_handset_fns = {
    TwsTopology_ProcedureDisconnectHandsetStart,
    TwsTopology_ProcedureDisconnectHandsetCancel,
    NULL,
};

typedef struct
{
    TaskData task;
    twstop_proc_complete_func_t complete_fn;
    uint8 profiles_status;
    bool active;
} twsTopProcDisconnectHandsetTaskData;

twsTopProcDisconnectHandsetTaskData twstop_proc_disconnect_handset = {twsTopology_ProcDisconnectHandsetHandleMessage};

#define TwsTopProcDisconnectHandsetGetTaskData()     (&twstop_proc_disconnect_handset)
#define TwsTopProcDisconnectHandsetGetTask()         (&twstop_proc_disconnect_handset.task)

static void twsTopology_ProcDisconnectHandsetResetProc(void)
{
    twsTopProcDisconnectHandsetTaskData* td = TwsTopProcDisconnectHandsetGetTaskData();

    td->active = FALSE;
    td->profiles_status = 0;
}

void TwsTopology_ProcedureDisconnectHandsetStart(Task result_task,
                                        twstop_proc_start_cfm_func_t proc_start_cfm_fn,
                                        twstop_proc_complete_func_t proc_complete_fn,
                                        Message goal_data)
{
    twsTopProcDisconnectHandsetTaskData* td = TwsTopProcDisconnectHandsetGetTaskData();
    bdaddr handset_addr;
    bdaddr le_handset_addr;
    bool bredr_handset = FALSE;
    bool le_handset = HandsetService_GetConnectedLeHandsetAddress(&le_handset_addr);

    UNUSED(result_task);
    UNUSED(goal_data);

    DEBUG_LOG("TwsTopology_ProcedureDisconnectHandsetStart");

    TwsTopProcDisconnectHandsetGetTaskData()->complete_fn = proc_complete_fn;

    appDeviceGetHandsetBdAddr(&handset_addr);
    bredr_handset = ConManagerIsConnected(&handset_addr);
    if (!bredr_handset && !le_handset)
    {
        proc_start_cfm_fn(tws_topology_procedure_disconnect_handset, proc_result_success);
        TwsTopology_DelayedCompleteCfmCallback(proc_complete_fn,
                                               tws_topology_procedure_disconnect_handset,
                                               proc_result_success);
    }
    else
    {
        if (bredr_handset)
        {
            /* Record the currently connected profiles in case the disconnect is cancelled. */
            device_t handset_device = DeviceList_GetFirstDeviceWithPropertyValue(device_property_bdaddr, &handset_addr, sizeof(bdaddr));
            td->profiles_status = BtDevice_GetConnectedProfiles(handset_device);

            HandsetService_DisconnectRequest(TwsTopProcDisconnectHandsetGetTask(), &handset_addr);
        }
        else
        {
            HandsetService_DisconnectRequest(TwsTopProcDisconnectHandsetGetTask(), &le_handset_addr);
        }

        /* start the procedure */
        td->complete_fn = proc_complete_fn;
        td->active = TRUE;

        proc_start_cfm_fn(tws_topology_procedure_disconnect_handset, proc_result_success);
    }
}

void TwsTopology_ProcedureDisconnectHandsetCancel(twstop_proc_cancel_cfm_func_t proc_cancel_cfm_fn)
{
    DEBUG_LOG("TwsTopology_ProcedureDisconnectHandsetCancel");

    twsTopology_ProcDisconnectHandsetResetProc();
    TwsTopology_DelayedCancelCfmCallback(proc_cancel_cfm_fn,
                                         tws_topology_procedure_disconnect_handset,
                                         proc_result_success);
}

static void twsTopology_ProcDisconnectHandsetHandleHandsetConnectCfm(const HANDSET_SERVICE_CONNECT_CFM_T *cfm)
{
    DEBUG_LOG("twsTopology_ProcDisconnectHandsetHandleHandsetConnectCfm status %d", cfm->status);

    UNUSED(cfm);
}

static void twsTopology_ProcDisconnectHandsetHandleHandsetDisconnectCfm(const HANDSET_SERVICE_DISCONNECT_CFM_T *cfm)
{
    twsTopProcDisconnectHandsetTaskData* td = TwsTopProcDisconnectHandsetGetTaskData();
    bdaddr le_handset_addr;
    bool le_handset = HandsetService_GetConnectedLeHandsetAddress(&le_handset_addr);

    DEBUG_LOG("twsTopology_ProcDisconnectHandsetHandleHandsetDisconnectCfm status %d", cfm->status);

    if (le_handset)
    {
        HandsetService_DisconnectRequest(TwsTopProcDisconnectHandsetGetTask(), &le_handset_addr);
    }
    else
    {
        twsTopology_ProcDisconnectHandsetResetProc();
        td->complete_fn(tws_topology_procedure_disconnect_handset, proc_result_success);
    }
}

static void twsTopology_ProcDisconnectHandsetHandleMessage(Task task, MessageId id, Message message)
{
    twsTopProcDisconnectHandsetTaskData* td = TwsTopProcDisconnectHandsetGetTaskData();

    UNUSED(task);

    if (!td->active)
    {
        return;
    }

    switch (id)
    {
    case HANDSET_SERVICE_CONNECT_CFM:
        twsTopology_ProcDisconnectHandsetHandleHandsetConnectCfm((const HANDSET_SERVICE_CONNECT_CFM_T *)message);
        break;

    case HANDSET_SERVICE_DISCONNECT_CFM:
        twsTopology_ProcDisconnectHandsetHandleHandsetDisconnectCfm((const HANDSET_SERVICE_DISCONNECT_CFM_T *)message);
        break;

    default:
        break;
    }
}
