/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Miscellaneous functions for the PEER service to find peer using LE
            and select role
*/

#include <panic.h>

#include <logging.h>

#include <bt_device.h>

#include "earbud_init.h"

#include "peer_find_role.h"
#include "peer_find_role_init.h"
#include "peer_find_role_config.h"
#include "peer_find_role_private.h"

#include "timestamp_event.h"


bool peer_find_role_is_central(void)
{
    peerFindRoleTaskData *pfr = PeerFindRoleGetTaskData();
    uint16 flags;

    if (appDeviceGetFlags(&pfr->my_addr, &flags))
    {
        if (flags & DEVICE_FLAGS_SHADOWING_C_ROLE)
        {
            return TRUE;
        }
    }
    return FALSE;
}


void PeerFindRole_FindRole(int32 high_speed_time_ms)
{
    DEBUG_LOG("PeerFindRole_FindRole. Current state:%d. Timeout:%d", peer_find_role_get_state(), high_speed_time_ms);

    if(peer_find_role_get_state() == PEER_FIND_ROLE_STATE_INITIALISED)
    {
        peerFindRoleTaskData *pfr = PeerFindRoleGetTaskData();

        TimestampEvent(TIMESTAMP_EVENT_PEER_FIND_ROLE_STARTED);

        if (high_speed_time_ms < 0)
        {
            high_speed_time_ms = -high_speed_time_ms;
            pfr->timeout_means_timeout = TRUE;
        }

        pfr->role_timeout_ms = high_speed_time_ms;
        pfr->advertising_backoff_ms = PeerFindRoleConfigInitialAdvertisingBackoffMs();

        if (high_speed_time_ms != 0)
        {
            /* Start the timeout before setting the state as the state change can cancel it */
            MessageSendLater(PeerFindRoleGetTask(), PEER_FIND_ROLE_INTERNAL_TIMEOUT_CONNECTION,
                             NULL, pfr->role_timeout_ms);
        }

        peer_find_role_set_state(PEER_FIND_ROLE_STATE_CHECKING_PEER);
    }
}


void PeerFindRole_FindRoleCancel(void)
{
    DEBUG_LOG("PeerFindRole_FindRoleCancel. Current state:%d", peer_find_role_get_state());

    peer_find_role_cancel_initial_timeout();

    MessageSend(PeerFindRoleGetTask(), PEER_FIND_ROLE_INTERNAL_CANCEL_FIND_ROLE, NULL);
}


void PeerFindRole_DisableScanning(bool disable)
{
    DEBUG_LOG("PeerFindRole_DisableScanning disable:%d",disable);

    peer_find_role_update_media_flag(disable, PEER_FIND_ROLE_SCANNING_DISABLED);
}


void PeerFindRole_RegisterTask(Task t)
{
    if (PEER_FIND_ROLE_STATE_UNINITIALISED == peer_find_role_get_state())
    {
        DEBUG_LOG("PeerFindRole_RegisterTask. Attempt to register task before initialised.");
        Panic();
        return;
    }

    TaskList_AddTask(TaskList_GetFlexibleBaseTaskList(PeerFindRoleGetTaskList()), t);
}


void PeerFindRole_UnregisterTask(Task t)
{
    TaskList_RemoveTask(TaskList_GetFlexibleBaseTaskList(PeerFindRoleGetTaskList()), t);
}


peer_find_role_score_t PeerFindRole_CurrentScore(void)
{
    peer_find_role_calculate_score();
    return peer_find_role_score();
}


void PeerFindRole_RegisterPrepareClient(Task task)
{
    peerFindRoleTaskData *pfr = PeerFindRoleGetTaskData();
    PanicNotZero(TaskList_Size(&pfr->prepare_tasks));
    TaskList_AddTask(&pfr->prepare_tasks, task);
}


void PeerFindRole_UnregisterPrepareClient(Task task)
{
    peerFindRoleTaskData *pfr = PeerFindRoleGetTaskData();
    TaskList_RemoveTask(&pfr->prepare_tasks, task);
}


void PeerFindRole_PrepareResponse(void)
{
    MessageSend(PeerFindRoleGetTask(), PEER_FIND_ROLE_INTERNAL_PREPARED, NULL);
}



/*! Check if link to our peer is now encrypted and send ourselves a message if so 

    \param[in]  ind The encryption indication from connection library

    \return TRUE if the indication is for the address we expected, FALSE otherwise
*/
static bool peer_find_role_handle_encryption_change(const CL_SM_ENCRYPTION_CHANGE_IND_T *ind)
{
    peerFindRoleTaskData *pfr = PeerFindRoleGetTaskData();

    if (BdaddrIsSame(&ind->tpaddr.taddr.addr, &pfr->primary_addr))
    {
        DEBUG_LOG("peer_find_role_handle_encryption_change. CL_SM_ENCRYPTION_CHANGE_IND %d state %d",
                    ind->encrypted, peer_find_role_get_state());

        pfr->gatt_encrypted = ind->encrypted;

        if (ind->encrypted)
        {
            if (PEER_FIND_ROLE_STATE_CLIENT_AWAITING_ENCRYPTION == peer_find_role_get_state())
            {
                peer_find_role_set_state(PEER_FIND_ROLE_STATE_CLIENT_PREPARING);
            }
        }
        return TRUE;
    }
    return FALSE;
}


bool PeerFindRole_HandleConnectionLibraryMessages(MessageId id, Message message,
                                                  bool already_handled)
{
    /* Encryption is an indication. We don't care if already handled */
    if (CL_SM_ENCRYPTION_CHANGE_IND == id)
    {
        return peer_find_role_handle_encryption_change((const CL_SM_ENCRYPTION_CHANGE_IND_T *)message);
    }

    if (already_handled)
    {
        return FALSE;
    }

    switch(peer_find_role_get_state())
    {
        case PEER_FIND_ROLE_STATE_UNINITIALISED:
            /* Waiting for confirmation that resolvable addresses in use */
            if (CL_DM_BLE_CONFIGURE_LOCAL_ADDRESS_CFM == id)
            {
                const CL_DM_BLE_CONFIGURE_LOCAL_ADDRESS_CFM_T *address_cfm = (const CL_DM_BLE_CONFIGURE_LOCAL_ADDRESS_CFM_T *)message;

                PanicFalse(success == address_cfm->status);

                MessageSend(appInitGetInitTask(), PEER_FIND_ROLE_INIT_CFM, NULL);
                return TRUE;
            }
            break;

        default:
            break;
    }

    return FALSE;
}
