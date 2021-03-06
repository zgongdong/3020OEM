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
#include <connection_manager.h>
#include <peer_find_role.h>

#include <logging.h>

#include <message.h>
#include <panic.h>



void TwsTopology_ProcedureConnectHandsetStart(Task result_task,
                                        twstop_proc_start_cfm_func_t proc_start_cfm_fn,
                                        twstop_proc_complete_func_t proc_complete_fn,
                                        Message goal_data);
void TwsTopology_ProcedureConnectHandsetCancel(twstop_proc_cancel_cfm_func_t proc_cancel_cfm_fn);

const tws_topology_procedure_fns_t proc_connect_handset_fns = {
    TwsTopology_ProcedureConnectHandsetStart,
    TwsTopology_ProcedureConnectHandsetCancel,
    NULL,
};

typedef struct
{
    TaskData task;
    twstop_proc_complete_func_t complete_fn;
    uint8 profiles_status;
    bool active;
    bool prepare_requested;
    bdaddr handset_addr;
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

/*! \brief Send a response to a PEER_FIND_ROLE_PREPARE_FOR_ROLE_SELECTION.

    This will only send the response if we have received a
    PEER_FIND_ROLE_PREPARE_FOR_ROLE_SELECTION, otherwise it will do
    nothing.

    Note: There should only ever be one response per
          PEER_FIND_ROLE_PREPARE_FOR_ROLE_SELECTION received, hence why
          this is guarded on the prepare_requested flag.
*/
static void twsTopology_ProcConnectHandsetPeerFindRolePrepareRespond(void)
{
    twsTopProcConnectHandsetTaskData* td = TwsTopProcConnectHandsetGetTaskData();

    if (td->prepare_requested)
    {
        PeerFindRole_PrepareResponse();
        td->prepare_requested = FALSE;
    }
}

static void twsTopology_ProcConnectHandsetResetProc(void)
{
    twsTopProcConnectHandsetTaskData* td = TwsTopProcConnectHandsetGetTaskData();

    twsTopology_ProcConnectHandsetPeerFindRolePrepareRespond();

    td->profiles_status = 0;
    td->active = FALSE;
    td->prepare_requested = FALSE;
    BdaddrSetZero(&td->handset_addr);

    PeerFindRole_DisableScanning(FALSE);
    PeerFindRole_UnregisterPrepareClient(TwsTopProcConnectHandsetGetTask());
    ConManagerUnregisterConnectionsClient(TwsTopProcConnectHandsetGetTask());
}

void TwsTopology_ProcedureConnectHandsetStart(Task result_task,
                                        twstop_proc_start_cfm_func_t proc_start_cfm_fn,
                                        twstop_proc_complete_func_t proc_complete_fn,
                                        Message goal_data)
{
    twsTopProcConnectHandsetTaskData* td = TwsTopProcConnectHandsetGetTaskData();
    TWSTOP_PRIMARY_GOAL_CONNECT_HANDSET_T* chp = (TWSTOP_PRIMARY_GOAL_CONNECT_HANDSET_T*)goal_data;

    UNUSED(result_task);

    DEBUG_LOG("TwsTopology_ProcedureConnectHandsetStart profiles 0x%x", chp->profiles);

    /* Block scanning temporarily while we are connecting */
    PeerFindRole_DisableScanning(TRUE);

    /* save state to perform the procedure */
    td->complete_fn = proc_complete_fn;
    td->active = TRUE;
    td->profiles_status = chp->profiles;
    BdaddrSetZero(&td->handset_addr);

    /* start the procedure */
    if (appDeviceGetHandsetBdAddr(&td->handset_addr))
    {
        PeerFindRole_RegisterPrepareClient(TwsTopProcConnectHandsetGetTask());
        HandsetService_ConnectAddressRequest(TwsTopProcConnectHandsetGetTask(), &td->handset_addr, chp->profiles);
        ConManagerRegisterConnectionsClient(TwsTopProcConnectHandsetGetTask());
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

    /* \todo  Call the new HandsetService_StopConnect function when it exists? */

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
    else
    {
        Panic();
    }
    
}

static void twsTopology_ProcConnectHandsetHandleHandsetConnectStopCfm(const HANDSET_SERVICE_CONNECT_STOP_CFM_T* cfm)
{
    twsTopProcConnectHandsetTaskData* td = TwsTopProcConnectHandsetGetTaskData();

    DEBUG_LOG("twsTopology_ProcConnectHandsetHandleHandsetConnectStopCfm status 0x%x", cfm->status);

    if (BdaddrIsSame(&td->handset_addr, &cfm->addr))
    {
        twsTopology_ProcConnectHandsetPeerFindRolePrepareRespond();
    }
}

static void twsTopology_ProcConnectHandsetHandlePeerFindRolePrepareForRoleSelection(void)
{
    twsTopProcConnectHandsetTaskData* td = TwsTopProcConnectHandsetGetTaskData();

    DEBUG_LOG("twsTopology_ProcConnectHandsetHandlePeerFindRolePrepareForRoleSelection");

    HandsetService_StopConnect(TwsTopProcConnectHandsetGetTask(), &td->handset_addr);
    td->prepare_requested = TRUE;
}


/*! Use connection manager indication to re-enable scanning once we connect to handset

    We will do this anyway once we are fully connected to the handset (all selected
    profiles), but that can take some time.

    \param conn_ind The Connection manager indication
 */
static void twsTopology_ProcConnectHandsetHandleConMgrConnInd(const CON_MANAGER_CONNECTION_IND_T *conn_ind)
{
    DEBUG_LOG("twsTopology_ProcConnectHandsetHandleConMgrConnInd LAP:x%06x ble:%d conn:%d", 
                    conn_ind->bd_addr.lap, conn_ind->ble, conn_ind->connected);

    if (!conn_ind->ble
        && conn_ind->connected
        && appDeviceIsHandset(&conn_ind->bd_addr))
    {
        /* Additional call here as we only care about the handset connection,
            not the profiles */
        PeerFindRole_DisableScanning(FALSE);
        ConManagerUnregisterConnectionsClient(TwsTopProcConnectHandsetGetTask());
    }
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

    case HANDSET_SERVICE_CONNECT_STOP_CFM:
        twsTopology_ProcConnectHandsetHandleHandsetConnectStopCfm((const HANDSET_SERVICE_CONNECT_STOP_CFM_T *)message);
        break;

    case CON_MANAGER_CONNECTION_IND:
        twsTopology_ProcConnectHandsetHandleConMgrConnInd((const CON_MANAGER_CONNECTION_IND_T *)message);
        break;

    case PEER_FIND_ROLE_PREPARE_FOR_ROLE_SELECTION:
        twsTopology_ProcConnectHandsetHandlePeerFindRolePrepareForRoleSelection();
        break;

    default:
        DEBUG_LOG("twsTopology_ProcConnectHandsetHandleMessage unhandled id 0x%x(%d)", id, id);
        break;
    }
}
