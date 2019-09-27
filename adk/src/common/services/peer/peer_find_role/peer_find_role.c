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


/*! \todo Stub implementation 
 */
void PeerFindRole_FindRole(int32 high_speed_time_ms)
{
    if(peer_find_role_get_state() == PEER_FIND_ROLE_STATE_INITIALISED)
    {
        peerFindRoleTaskData *pfr = PeerFindRoleGetTaskData();

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
    peer_find_role_cancel_initial_timeout();

    MessageSend(PeerFindRoleGetTask(), PEER_FIND_ROLE_INTERNAL_CANCEL_FIND_ROLE, NULL);
}


void PeerFindRole_RegisterTask(Task t)
{
    if (PEER_FIND_ROLE_STATE_UNINITIALISED == peer_find_role_get_state())
    {
        DEBUG_LOG("PeerFindRole_RegisterTask. Attempt to register task before initialised.");
        Panic();
        return;
    }

    TaskList_AddTask(PeerFindRoleGetTaskList(), t);
}


peer_find_role_score_t PeerFindRole_CurrentScore(void)
{
    peer_find_role_score_t score = 0;

    return score;
}


/*! Internal handler for adverts

    Check if the advert we have received is expected (matches address), 
    changing state if so.

    \param[in] advert The advertising indication from the connection library
*/
static void peer_find_role_handle_advertising_report_ind(const CL_DM_BLE_ADVERTISING_REPORT_IND_T *advert)
{
    peerFindRoleTaskData *pfr = PeerFindRoleGetTaskData();

    DEBUG_LOG("peer_find_role_handle_advertising_report_ind ADDR:%06x PERM:%06x",
                    advert->current_taddr.addr.lap, advert->permanent_taddr.addr.lap);

    if (BdaddrIsSame(&advert->permanent_taddr.addr, &pfr->primary_addr))
    {
        peer_find_role_set_state(PEER_FIND_ROLE_STATE_DISCOVERED_DEVICE);
    }
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
        DEBUG_LOG("PeerFindRole_HandleConnectionLibraryMessages. CL_SM_ENCRYPTION_CHANGE_IND %d",
                    ind->encrypted);
    
        pfr->gatt_encrypted = ind->encrypted;

        if (ind->encrypted)
        {
            if (PEER_FIND_ROLE_STATE_AWAITING_ENCRYPTION == peer_find_role_get_state())
            {
                peer_find_role_set_state(PEER_FIND_ROLE_STATE_DECIDING);
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

        case PEER_FIND_ROLE_STATE_DISCOVER:
        case PEER_FIND_ROLE_STATE_DISCOVER_CONNECTABLE:
            if (CL_DM_BLE_ADVERTISING_REPORT_IND == id)
            {
                peer_find_role_handle_advertising_report_ind((const CL_DM_BLE_ADVERTISING_REPORT_IND_T *)message);
                return TRUE;
            }
            break;

        default:
            break;
    }

    return FALSE;
}
