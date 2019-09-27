/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Procedure for Primary to connect BR/EDR ACL to Handset.
*/

#include "tws_topology_procedure_connect_handset.h"
#include "tws_topology_procedures.h"
#include "tws_topology_config.h"
#include "tws_topology_primary_rules.h"

#include <handset_service.h>

#include <logging.h>

#include <message.h>
#include <panic.h>



void TwsTopology_ProcedureConnectHandsetStart(Task result_task,
                                        twstop_proc_start_cfm_func_t proc_start_cfm_fn,
                                        twstop_proc_complete_func_t proc_complete_fn,
                                        Message goal_data);
void TwsTopology_ProcedureConnectHandsetCancel(twstop_proc_cancel_cfm_func_t proc_cancel_cfm_fn);

tws_topology_procedure_fns_t proc_connect_handset_fns = {
    TwsTopology_ProcedureConnectHandsetStart,
    TwsTopology_ProcedureConnectHandsetCancel,
    NULL,
};

typedef struct
{
    TaskData task;
    twstop_proc_complete_func_t complete_fn;
    uint16* connect_wait_lock;
    uint8 profiles_status;
    bool active;
} twsTopProcConnectHandsetTaskData;

twsTopProcConnectHandsetTaskData twstop_proc_connect_handset;

#define TwsTopProcConnectHandsetGetTaskData()     (&twstop_proc_connect_handset)
#define TwsTopProcConnectHandsetGetTask()         (&twstop_proc_connect_handset.task)

/*! Internal messages use by this ConnectHandset procedure. */
typedef enum
{
    PROC_CONNECT_HANDSET_INTERNAL_ACL_CONNECT,
    PROC_CONNECT_HANDSET_INTERNAL_ACL_CONNECT_TIMEOUT,
} procConnetHandsetInternalMessages;

static void twsTopology_ProcConnectHandsetHandleMessage(Task task, MessageId id, Message message);

twsTopProcConnectHandsetTaskData twstop_proc_connect_handset = {twsTopology_ProcConnectHandsetHandleMessage};

static void twsTopology_ProcConnectHandsetResetProc(void)
{
    twsTopProcConnectHandsetTaskData* td = TwsTopProcConnectHandsetGetTaskData();
    td->profiles_status = 0;
    td->active = FALSE;
}

void TwsTopology_ProcedureConnectHandsetStart(Task result_task,
                                        twstop_proc_start_cfm_func_t proc_start_cfm_fn,
                                        twstop_proc_complete_func_t proc_complete_fn,
                                        Message goal_data)
{
    twsTopProcConnectHandsetTaskData* td = TwsTopProcConnectHandsetGetTaskData();
    TWSTOP_PRIMARY_GOAL_CONNECT_HANDSET_T* chp = (TWSTOP_PRIMARY_GOAL_CONNECT_HANDSET_T*)goal_data;
    bdaddr handset_addr;

    UNUSED(result_task);

    DEBUG_LOG("TwsTopology_ProcedureConnectHandsetStart profiles 0x%x", chp->profiles);

    /* save state to perform the procedure */
    td->complete_fn = proc_complete_fn;
    td->active = TRUE;
    td->profiles_status = chp->profiles;

    /* start the procedure */
    if (appDeviceGetHandsetBdAddr(&handset_addr))
    {
        HandsetService_ConnectAddressRequest(TwsTopProcConnectHandsetGetTask(), &handset_addr, chp->profiles);
        proc_start_cfm_fn(tws_topology_procedure_connect_handset, proc_result_success);
    }
    else
    {
        DEBUG_LOG("TwsTopology_ProcedureConnectHandsetStart shouldn't be called with no paired handset");
        Panic();
    }
}

void TwsTopology_ProcedureConnectHandsetCancel(twstop_proc_cancel_cfm_func_t proc_cancel_cfm_fn)
{
    DEBUG_LOG("TwsTopology_ProcedureConnectHandsetCancel");
    twsTopology_ProcConnectHandsetResetProc();
    TwsTopology_DelayedCancelCfmCallback(proc_cancel_cfm_fn, tws_topology_procedure_connect_handset, proc_result_success);
}

static void twsTopology_ProcConnectHandsetProfilesStatus(uint8 profile)
{
    twsTopProcConnectHandsetTaskData* td = TwsTopProcConnectHandsetGetTaskData();

    /* clear the connected profile */
    td->profiles_status &= ~profile;

    /* report start complete if all done */
    if (!td->profiles_status)
    {
        twsTopology_ProcConnectHandsetResetProc();
        td->complete_fn(tws_topology_procedure_connect_handset, proc_result_success);
    }
}

static void twsTopology_ProcConnectHandsetHandleHandsetConnectCfm(const HANDSET_SERVICE_CONNECT_CFM_T *cfm)
{
    twsTopProcConnectHandsetTaskData* td = TwsTopProcConnectHandsetGetTaskData();

    DEBUG_LOG("twsTopology_ProcConnectHandsetHandleHandsetConnectCfm status %d", cfm->status);

    if (cfm->status == handset_service_status_success)
    {
        device_t dev = BtDevice_GetDeviceForBdAddr(&cfm->addr);
        uint8 profiles_connected = BtDevice_GetConnectedProfiles(dev);
        twsTopology_ProcConnectHandsetProfilesStatus(profiles_connected);
    }
    else if (cfm->status != handset_service_status_cancelled)
    {
        twsTopology_ProcConnectHandsetResetProc();
        td->complete_fn(tws_topology_procedure_connect_handset, proc_result_failed);
    }
}

static void twsTopology_ProcConnectHandsetHandleHandsetDisconnectCfm(const HANDSET_SERVICE_DISCONNECT_CFM_T *cfm)
{
    DEBUG_LOG("twsTopology_ProcConnectHandsetHandleHandsetDisconnectCfm status %d", cfm->status);

    UNUSED(cfm);
}

static void twsTopology_ProcConnectHandsetHandleMessage(Task task, MessageId id, Message message)
{
    twsTopProcConnectHandsetTaskData* td = TwsTopProcConnectHandsetGetTaskData();

    UNUSED(task);
    UNUSED(message);

    /* ignore any delivered messages if no longer active */
    if (!td->active)
    {
        return;
    }

    switch (id)
    {
    case HANDSET_SERVICE_CONNECT_CFM:
        twsTopology_ProcConnectHandsetHandleHandsetConnectCfm((const HANDSET_SERVICE_CONNECT_CFM_T *)message);
        break;

    case HANDSET_SERVICE_DISCONNECT_CFM:
        twsTopology_ProcConnectHandsetHandleHandsetDisconnectCfm((const HANDSET_SERVICE_DISCONNECT_CFM_T *)message);
        break;

    default:
        break;
    }
}
