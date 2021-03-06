/*!
\copyright  Copyright (c) 2008 - 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       pairing.c
\brief      Pairing task
*/

#include "pairing.h"
#include "pairing_plugin.h"
#include "pairing_config.h"

#include "earbud.h"
#include "sdp.h"
#include "peer_signalling.h"
#include "earbud_config.h"
#include "earbud_init.h"
#include "scofwd_profile_config.h"
#include "earbud_log.h"
#include "gaia_handler.h"
#include "gatt_handler.h"
#include "device_properties.h"
#include "peer_link_keys.h"

#include <device_db_serialiser.h>
#include <bredr_scan_manager.h>
#include <connection_manager.h>
#include <profile_manager.h>
#include <gatt.h>

#include <panic.h>
#include <connection.h>
#include <device.h>
#include <device_list.h>
#include <ps.h>
#include <string.h>
#include <stdio.h>

/*! Macro for simplifying creating messages */
#define MAKE_PAIRING_MESSAGE(TYPE) \
    TYPE##_T *message = PanicUnlessNew(TYPE##_T);

/*! Macro to make message with variable length for array fields. */
#define MAKE_PAIRING_MESSAGE_WITH_LEN(TYPE, LEN) \
    TYPE##_T *message = (TYPE##_T *) PanicUnlessMalloc(sizeof(TYPE##_T) + LEN);

/*
 * Function Prototypes
 */
static void pairing_SetState(pairingTaskData *thePairing, pairingState state);

/*!< pairing task */
pairingTaskData pairing_task_data;

static void pairing_ResetPendingBleAddress(void)
{
    pairingTaskData *thePairing = PairingGetTaskData();
    
    BdaddrTypedSetEmpty(&thePairing->pending_ble_address);
}

/*! \brief Notify clients of pairing activity. */
static void pairing_MsgActivity(pairingStatus status, bdaddr* device_addr, uint16 tws_version)
{
    MAKE_PAIRING_MESSAGE(PAIRING_ACTIVITY);
    message->status = status;

    if (device_addr)
    {
        message->device_addr = *device_addr;
    }
    else
    {
        BdaddrSetZero(&message->device_addr);
    }

    message->tws_version = tws_version;
    TaskList_MessageSend(TaskList_GetFlexibleBaseTaskList(PairingGetPairingActivity()), PAIRING_ACTIVITY, message);
}

static void pairing_EirSetupCompleteCallback(bool success)
{
    DEBUG_LOGF("pairing_EirSetupCompleteCallback success=%d", success);

    if(success)
    {
        MessageSend(appInitGetInitTask(), PAIRING_INIT_CFM, NULL);
        pairing_SetState(PairingGetTaskData(), PAIRING_STATE_IDLE);
    }
    else
    {
        Panic();
    }
}

static void pairing_EnterInitialising(pairingTaskData *thePairing)
{
    UNUSED(thePairing);

    DEBUG_LOG("pairing_EnterInitialising");

    ScanManager_ConfigureEirData(pairing_EirSetupCompleteCallback);

}

/*! @brief Actions to take when leaving #PAIRING_STATE_INITIALISING. */
static void pairing_ExitInitialising(pairingTaskData *thePairing)
{
    UNUSED(thePairing);

    DEBUG_LOG("pairing_ExitInitialising");
}

static void pairing_EnterIdle(pairingTaskData *thePairing)
{
    DEBUG_LOG("pairing_EnterIdle");

    /* Send message to disable page and inquiry scan after being idle for a period of time */
    MessageSendLater(&thePairing->task, PAIRING_INTERNAL_DISABLE_SCAN, NULL, D_SEC(appConfigPairingScanDisableDelay()));

    if(thePairing->stop_task)
    {
        MessageSend(thePairing->stop_task, PAIRING_STOP_CFM, NULL);
        thePairing->stop_task = NULL;
    }

    /* Unlock pairing and permit a pairing operation */
    thePairing->pairing_lock = 0;
}

static void pairing_ExitIdle(pairingTaskData *thePairing)
{
    DEBUG_LOG("pairing_ExitIdle");

    /* Lock pairing now that a pairing operation is underway */
    thePairing->pairing_lock = 1;
}

static void pairing_EnterDiscoverable(pairingTaskData *thePairing)
{
    static const uint32 iac_array[] = { 0x9E8B33 };

    DEBUG_LOG("pairing_EnterHandsetDiscoverable");

    /* Cancel any delayed message to disable page and inquiry scan */
    MessageCancelAll(&thePairing->task, PAIRING_INTERNAL_DISABLE_SCAN);

    /* No longer idle, starting a pairing operation so need to be connectable */
    BredrScanManager_PageScanRequest(&thePairing->task, SCAN_MAN_PARAMS_TYPE_FAST);

    /* Inquiry Scan on GIAC only */
    ConnectionWriteInquiryAccessCode(&thePairing->task, iac_array, 1);

    /* Enable inquiry scan so handset can find us */
    BredrScanManager_InquiryScanRequest(&thePairing->task, SCAN_MAN_PARAMS_TYPE_FAST);

    /* Start pairing timeout */
    MessageCancelFirst(&thePairing->task, PAIRING_INTERNAL_TIMEOUT_IND);
    if (thePairing->is_user_initiated && appConfigHandsetPairingTimeout())
        MessageSendLater(&thePairing->task, PAIRING_INTERNAL_TIMEOUT_IND, 0, D_SEC(appConfigHandsetPairingTimeout()));
    else if (!thePairing->is_user_initiated && appConfigAutoHandsetPairingTimeout())
        MessageSendLater(&thePairing->task, PAIRING_INTERNAL_TIMEOUT_IND, 0, D_SEC(appConfigAutoHandsetPairingTimeout()));

    /* Not initially pairing with BLE Handset */
    thePairing->pair_le_task = NULL;

    /* Indicate pairing active */
    if (thePairing->is_user_initiated)
    {
        TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(PairingGetClientList()), PAIRING_ACTIVE_USER_INITIATED);
    }
    else
    {
        TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(PairingGetClientList()), PAIRING_ACTIVE);
    }

    /* notify clients that pairing is in progress */
    pairing_MsgActivity(pairingInProgress, NULL, DEVICE_TWS_UNKNOWN);
}


static void pairing_ExitDiscoverable(pairingTaskData *thePairing)
{
    DEBUG_LOG("pairing_ExitDiscoverable");

    /* No longer discoverable, disable inquiry scan */
    BredrScanManager_InquiryScanRelease(&thePairing->task);

    /* Cancel pairing timeout */
    MessageCancelFirst(&thePairing->task, PAIRING_INTERNAL_TIMEOUT_IND);

    /* Stop pairing indication */
    if (thePairing->is_user_initiated)
    {
        TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(PairingGetClientList()), PAIRING_INACTIVE_USER_INITIATED);
    }
    else
    {
        TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(PairingGetClientList()), PAIRING_INACTIVE);
    }
}


static void pairing_EnterPendingAuthentication(pairingTaskData *thePairing)
{
    DEBUG_LOG("pairing_EnterPendingAuthentication");

    /* Cancel pairing timeout */
    MessageCancelAll(&thePairing->task, PAIRING_INTERNAL_TIMEOUT_IND);
    MessageSendLater(&thePairing->task, PAIRING_INTERNAL_TIMEOUT_IND, 0, D_SEC(appConfigAuthenticationTimeout()));

}

static void pairing_ExitPendingAuthentication(pairingTaskData *thePairing)
{
    DEBUG_LOG("pairing_ExitPendingAuthentication");

    /* Cancel pairing timeout */
    MessageCancelAll(&thePairing->task, PAIRING_INTERNAL_TIMEOUT_IND);
}


/*! \brief Set Pairing FSM state

    Called to change state.  Handles calling the state entry and exit
    functions for the new and old states.
*/
static void pairing_SetState(pairingTaskData *thePairing, pairingState state)
{
    DEBUG_LOGF("pairing_SetState, Current State = %d, New State = %d", thePairing->state, state);

    if(thePairing->state != state)
    {

        switch (thePairing->state)
        {
            case PAIRING_STATE_INITIALISING:
                pairing_ExitInitialising(thePairing);
                break;

            case PAIRING_STATE_IDLE:
                pairing_ExitIdle(thePairing);
                break;

            case PAIRING_STATE_DISCOVERABLE:
                pairing_ExitDiscoverable(thePairing);
                break;

            case PAIRING_STATE_PENDING_AUTHENTICATION:
                pairing_ExitPendingAuthentication(thePairing);
                break;

            default:
                break;
        }

        /* Set new state */
        thePairing->state = state;

        /* Handle state entry functions */
        switch (state)
        {
            case PAIRING_STATE_INITIALISING:
                pairing_EnterInitialising(thePairing);
                break;

            case PAIRING_STATE_IDLE:
                pairing_EnterIdle(thePairing);
                break;

            case PAIRING_STATE_DISCOVERABLE:
                pairing_EnterDiscoverable(thePairing);
                break;

            case PAIRING_STATE_PENDING_AUTHENTICATION:
                pairing_EnterPendingAuthentication(thePairing);
                break;

            default:
                break;
        }
    }
    else /* thePairing->state == state */
    {
        Panic();
    }
}


/*! \brief Get Pairing FSM state

    Returns current state of the Pairing FSM.
*/
static pairingState pairing_GetState(pairingTaskData *thePairing)
{
    return thePairing->state;
}

static void pairing_SendNotReady(Task task)
{
    MAKE_PAIRING_MESSAGE(PAIRING_PAIR_CFM);
    message->status = pairingNotReady;
    MessageSend(task, PAIRING_PAIR_CFM, message);
}

static void pairing_Complete(pairingTaskData *thePairing, pairingStatus status, const bdaddr *bd_addr)
{
    DEBUG_LOG("pairing_Complete: status=%d", status);

    if (thePairing->client_task)
    {
        MAKE_PAIRING_MESSAGE(PAIRING_PAIR_CFM);
        if (bd_addr)
        {
            message->device_bd_addr = *bd_addr;
        }
        else
        {
            DeviceDbSerialiser_Serialise();
        }
        message->status = status;
        MessageSend(thePairing->client_task, PAIRING_PAIR_CFM, message);
    }
    if (pairingSuccess == status)
    {
        TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(PairingGetClientList()), PAIRING_COMPLETE);
        /* notify clients that pairing succeeded */
        pairing_MsgActivity(pairingSuccess, (bdaddr*)bd_addr, DEVICE_TWS_UNKNOWN);
    }
    else
    {
        if (status != pairingStopped)
        {
            TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(PairingGetClientList()), PAIRING_FAILED);
        }
    }

    /* notify clients that handset pairing is not in progress */
    pairing_MsgActivity(pairingNotInProgress, NULL, DEVICE_TWS_UNKNOWN);

    /* Move back to 'idle' state */
    pairing_SetState(thePairing, PAIRING_STATE_IDLE);
}

uint8 profile_list[] =
{
    profile_manager_hfp_profile,
    profile_manager_a2dp_profile,
    profile_manager_avrcp_profile,
    profile_manager_max_number_of_profiles
};

/*! @brief Create attributes for a handset device. */
static void pairing_DeviceUpdate(const bdaddr* device_addr, uint16 tws_version, uint16 flags)
{
    device_t device = BtDevice_GetDeviceCreateIfNew(device_addr, DEVICE_TYPE_HANDSET);

    DEBUG_LOG("pairing_DeviceUpdate: add handset");
    
    Device_SetPropertyU16(device, device_property_tws_version, tws_version);

    /* Set flag indicating just paired with device, this
       will temporarily prevent us from initiating connections to the device */
    Device_SetPropertyU16(device, device_property_flags, flags | DEVICE_FLAGS_JUST_PAIRED);
    Device_SetPropertyU8(device, device_property_a2dp_volume, A2dpProfile_GetDefaultVolume());
    Device_SetPropertyU8(device, device_property_hfp_profile, hfp_handsfree_107_profile);
    Device_SetProperty(device, device_property_profiles_connect_order, profile_list, sizeof(profile_list));
}

/*! \brief Set SCO forwarding supported features

    \param bd_addr Pointer to read-only device BT address to set featured for.
    \param supported_features Support featured for device.

static void pairing_SetScoForwardFeatures(const bdaddr *bd_addr, uint16 supported_features)
{
    device_t device = BtDevice_GetDeviceForBdAddr(bd_addr);
    if (device)
    {
        DEBUG_LOGF("appDeviceSetScoForwardFeatures, features %04x", supported_features);
        Device_SetPropertyU16(device, device_property_sco_fwd_features, supported_features);
    }
}
*/
static void pairing_UpdateLinkMode(const bdaddr *bd_addr, cl_sm_link_key_type key_type)
{
    /* Check if the key_type generated is p256. If yes then set the
    * attribute.mode to sink_mode_unknown. Once the encryption type is known in
    * CL_SM_ENCRYPTION_CHANGE_IND or  CL_SM_ENCRYPT_CFM message,device
    * attributes will be updated accordingly with proper mode.
    * Update the device attributes with this information to be reused later.
    */
    if((key_type == cl_sm_link_key_unauthenticated_p256) ||
        (key_type == cl_sm_link_key_authenticated_p256))
    {
        DEBUG_LOG("pairing_UpdateLinkMode: link_mode UNKNOWN");
        appDeviceSetLinkMode(bd_addr, DEVICE_LINK_MODE_UNKNOWN);
    }
    else
    {
        DEBUG_LOG("pairing_UpdateLinkMode: link_mode NO_SECURE_CONNECTION");
        appDeviceSetLinkMode(bd_addr, DEVICE_LINK_MODE_NO_SECURE_CONNECTION);
    }
}

/*! \brief Use link key for attached device to derive key for peer earbud.
 */
static void pairing_HandleClSmGetAuthDeviceConfirm(pairingTaskData* thePairing,
                                                     CL_SM_GET_AUTH_DEVICE_CFM_T *cfm)
{
    DEBUG_LOGF("pairing_HandleClSmGetAuthDeviceConfirm %d", cfm->status);
    UNUSED(thePairing);

    if ((cfm->status == success) && (cfm->size_link_key == 8))
    {
        PeerLinkKeys_SendKeyToPeer(&cfm->bd_addr, cfm->size_link_key, cfm->link_key);
    }
    else
    {
        /* Failed to find link key data for device, we just added it
         * this is bad. */
        Panic();
    }
}

/*! \brief Handle authentication confirmation

    The firmware has indicated that authentication with the remote device
    has completed, we should only receive this message in the pairing states.

    If we're pairing with a peer earbud, delete any previous pairing and then
    update the TWS+ service record with the address of the peer earbud.

    If we're pairing with a handset start SDP search to see if phone supports
    TWS+.
*/
static void pairing_HandleClSmAuthenticateConfirm(const CL_SM_AUTHENTICATE_CFM_T *cfm)
{
    pairingTaskData *thePairing = PairingGetTaskData();

    DEBUG_LOG("pairing_HandleClSmAuthenticateConfirm, state %d, status %d, bonded %d", pairing_GetState(thePairing), cfm->status, cfm->bonded);

    Pairing_PluginPairingComplete();

    switch (pairing_GetState(thePairing))
    {
        case PAIRING_STATE_PENDING_AUTHENTICATION:
            if (cfm->status == auth_status_success)
            {
                bool expected_ble = BtDevice_ResolvedBdaddrIsSame(&cfm->bd_addr, &thePairing->pending_ble_address);

                /* Add attributes, set pre paired flag if this address is known */
                if (BdaddrIsSame(&cfm->bd_addr, &thePairing->device_to_pair_with_bdaddr))
                {
                    pairing_DeviceUpdate(&cfm->bd_addr, DEVICE_TWS_UNKNOWN, DEVICE_FLAGS_PRE_PAIRED_HANDSET);
                }
                else if(expected_ble)
                {
                    DEBUG_LOG("pairing_HandleClSmAuthenticateConfirm, BLE Device");
                }
                else
                {
                    pairing_DeviceUpdate(&cfm->bd_addr, DEVICE_TWS_UNKNOWN, 0);
                }

                BtDevice_PrintAllDevices();

                if (cfm->bonded)
                {
                    /* Update the device link mode based on the key type */
                    pairing_UpdateLinkMode(&cfm->bd_addr, cfm->key_type);

                    /* Wait for TWS version, store BT address of authenticated device */
                    thePairing->device_to_pair_with_bdaddr = cfm->bd_addr;

                    if(!expected_ble) /* Don't consider ble complete until CL_SM_BLE_SIMPLE_PAIRING_COMPLETE_IND */
                    {
                        /* Send confirmation to main task */
                        pairing_Complete(thePairing, pairingSuccess, &cfm->bd_addr);
                    }
                }
            }
            else
            {
                /* Send confirmation with error to main task */
                pairing_Complete(thePairing, pairingAuthenticationFailed, &cfm->bd_addr);
            }
            break;

        default:
            if (cfm->status == auth_status_success)
            {
                DEBUG_LOG("pairing_HandleClSmAuthenticateConfirm, unexpected authentication");
            }
            break;
    }
}


/*! \brief Handle authorisation request for unknown device

    This function is called to handle an authorisation request for a unknown
    device.

    TODO: Update "In all cases we authorise the device, and if we're in pairing
    mode we mark the device as trusted."
*/
static bool pairing_HandleClSmAuthoriseIndication(const CL_SM_AUTHORISE_IND_T *ind)
{
    pairingTaskData *thePairing = PairingGetTaskData();

    DEBUG_LOGF("pairing_HandleClSmAuthoriseIndication, state %d, protocol %d, channel %d, incoming %d",
                   pairing_GetState(thePairing), ind->protocol_id, ind->channel, ind->incoming);

    /*! \todo BLE: The pairing module doesn't handle LE connections at present */
    if (ind->protocol_id == protocol_le_l2cap)
    {
        DEBUG_LOG("pairing_HandleClSmAuthoriseIndication 1");
        return FALSE;
    }
    if ((ind->protocol_id == protocol_l2cap) && (ind->channel == 1))
    {
        /* SDP L2CAP, always allow */
        ConnectionSmAuthoriseResponse(&ind->bd_addr, ind->protocol_id, ind->channel, ind->incoming, TRUE);

        DEBUG_LOG("pairing_HandleClSmAuthoriseIndication 2");
        /* Return indicating CL_SM_AUTHORISE_IND will be handled by us */
        return TRUE;
    }
    else if (appDeviceIsPeer(&ind->bd_addr))
    {
        DEBUG_LOG("pairing_HandleClSmAuthoriseIndication 3");
        /* Connection is to peer, so nothing to do with handset pairing. Let
           connection mananger deal with authorisation by returning FALSE
           indicating CL_SM_AUTHORISE_IND hasn't been handled, caller has to
           deal with it */
        return FALSE;
    }
    else if (pairing_GetState(thePairing) == PAIRING_STATE_DISCOVERABLE)
    {
        DEBUG_LOG("pairing_HandleClSmAuthoriseIndication 4");
        /* Pairing in progress and it is early enough to cancel it
         * cancel it and accept the connection */
        Pairing_PairStop(NULL);
        ConnectionSmAuthoriseResponse(&ind->bd_addr, ind->protocol_id, ind->channel, ind->incoming, TRUE);

        /* Return indicating CL_SM_AUTHORISE_IND will be handled by us */
        return TRUE;
    }
    else if (pairing_GetState(thePairing) == PAIRING_STATE_PENDING_AUTHENTICATION)
    {
        DEBUG_LOG("pairing_HandleClSmAuthoriseIndication 5");
        /* Pairing in progress, but too late to cancel the pairing,
         * refuse the connection */
        ConnectionSmAuthoriseResponse(&ind->bd_addr, ind->protocol_id, ind->channel, ind->incoming, FALSE);

        /* Return indicating CL_SM_AUTHORISE_IND will be handled by us */
        return TRUE;
    }
    else
    {
        DEBUG_LOG("pairing_HandleClSmAuthoriseIndication 6");
        /* Return indicating CL_SM_AUTHORISE_IND hasn't been handled, caller has to deal with it */
        return FALSE;
    }
}


/*! \brief Handle IO capabilities request

    This function is called when the remote device wants to know the devices IO
    capabilities.  If we are pairing we respond indicating the headset has no input
    or ouput capabilities, if we're not pairing then just reject the request.
*/
static void pairing_HandleClSmIoCapabilityReqIndication(const CL_SM_IO_CAPABILITY_REQ_IND_T *ind)
{
    pairingTaskData *thePairing = PairingGetTaskData();

    pairing_io_capability_rsp_t response =
    {
        .io_capability = cl_sm_reject_request,
        .mitm = mitm_not_required,
        .bonding = TRUE,
        .key_distribution = KEY_DIST_NONE,
        .oob_data = oob_data_none,
        .oob_hash_c = NULL,
        .oob_rand_r = NULL
    };

    DEBUG_LOG("pairing_HandleClSmIoCapabilityReqIndication state=%d type=%d trans=%d sm_over_BREDR=%d addr=%04x %02x %06x",
                    pairing_GetState(thePairing), ind->tpaddr.taddr.type,
                    ind->tpaddr.transport,ind->sm_over_bredr,
                    ind->tpaddr.taddr.addr.nap,
                    ind->tpaddr.taddr.addr.uap,
                    ind->tpaddr.taddr.addr.lap);

    if(Pairing_PluginHandleIoCapabilityRequest(ind, &response))
    {
        pairing_SetState(thePairing, PAIRING_STATE_PENDING_AUTHENTICATION);
    }
    else
    {
        switch (pairing_GetState(thePairing))
        {
            case PAIRING_STATE_PENDING_AUTHENTICATION:
                response.io_capability = cl_sm_io_cap_no_input_no_output;
                if (ind->tpaddr.transport == TRANSPORT_BLE_ACL)
                {
                    response.key_distribution = KEY_DIST_RESPONDER_ENC_CENTRAL |
                                                KEY_DIST_RESPONDER_ID |
                                                KEY_DIST_INITIATOR_ENC_CENTRAL |
                                                KEY_DIST_INITIATOR_ID;
                }
                break;

            case PAIRING_STATE_DISCOVERABLE:
                if (ind->tpaddr.transport == TRANSPORT_BLE_ACL)
                {
                    response.io_capability = cl_sm_io_cap_no_input_no_output;

                    response.key_distribution = KEY_DIST_RESPONDER_ENC_CENTRAL |
                                                KEY_DIST_RESPONDER_ID |
                                                KEY_DIST_INITIATOR_ENC_CENTRAL |
                                                KEY_DIST_INITIATOR_ID;
                }
                else
                {
                    response.io_capability = cl_sm_io_cap_no_input_no_output;
                    pairing_SetState(thePairing, PAIRING_STATE_PENDING_AUTHENTICATION);
                }
                break;

            case PAIRING_STATE_IDLE:
                if (ind->tpaddr.transport == TRANSPORT_BLE_ACL)
                {
                    bool random_address = (ind->tpaddr.taddr.type == TYPED_BDADDR_RANDOM);
                    bool existing =  BtDevice_isKnownBdAddr(&ind->tpaddr.taddr.addr);

                    /* Clear expected address for BLE pairing. We should not have a crossover
                       but in any case, safest to remove any pending */
                    pairing_ResetPendingBleAddress();

                    DEBUG_LOG("pairing_HandleClSmIoCapabilityReqIndication Handling BLE pairing. Addr %06x",ind->tpaddr.taddr.addr.lap);
                    DEBUG_LOG("pairing_HandleClSmIoCapabilityReqIndication. random:%d ble_permission:%d sm_over_bredr:%d",
                        random_address, thePairing->ble_permission, ind->sm_over_bredr );

                        /* BLE pairing is not allowed */
                    if (thePairing->ble_permission == pairingBleDisallowed)
                    {
                        break;
                    }
                        /* Eliminate this being a valid Cross Transport Key derivation(CTKD) */
                    if (ind->sm_over_bredr && !existing)
                    {
                        break;
                    }
                        /* Static address only allowed in one permission mode, anything else is OK now */
                    if (   !random_address && !existing
                        && thePairing->ble_permission != pairingBleAllowAll)
                    {
                        DEBUG_LOG("pairing_HandleClSmIoCapabilityReqIndication  Would have bomed based on permission");
    //                    break;
                    }

                    response.io_capability = cl_sm_io_cap_no_input_no_output;

                    response.key_distribution = (KEY_DIST_RESPONDER_ENC_CENTRAL |
                                                 KEY_DIST_RESPONDER_ID |
                                                 KEY_DIST_INITIATOR_ENC_CENTRAL |
                                                 KEY_DIST_INITIATOR_ID);

                    thePairing->pending_ble_address = ind->tpaddr.taddr;
                    pairing_SetState(thePairing, PAIRING_STATE_PENDING_AUTHENTICATION);
                }
                break;

            default:
                break;
        }
    }
    DEBUG_LOGF("pairing_HandleClSmIoCapabilityReqIndication, accept %d", (response.io_capability != cl_sm_reject_request));

    ConnectionSmIoCapabilityResponse(&ind->tpaddr,
                                     response.io_capability,
                                     response.mitm,
                                     response.bonding,
                                     response.key_distribution,
                                     response.oob_data,
                                     response.oob_hash_c,
                                     response.oob_rand_r);
}

static void pairing_HandleClSmRemoteIoCapabilityIndication(const CL_SM_REMOTE_IO_CAPABILITY_IND_T *ind)
{
    DEBUG_LOGF("pairing_HandleClSmRemoteIoCapabilityIndication %lx auth:%x io:%x oib:%x", ind->tpaddr.taddr.addr.lap,
                                                                       ind->authentication_requirements,
                                                                       ind->io_capability,
                                                                       ind->oob_data_present);

    if(Pairing_PluginHandleRemoteIoCapability(ind))
    {
        DEBUG_LOG("pairing_HandleClSmRemoteIoCapabilityIndication handled by plugin");
    }
}

/*! \brief Handle request to start pairing with a device. */
static void pairing_HandleInternalPairRequest(pairingTaskData *thePairing, PAIR_REQ_T *req)
{
    DEBUG_LOGF("pairing_HandleInternalPairRequest, state %d", pairing_GetState(thePairing));

    switch (pairing_GetState(thePairing))
    {
        case PAIRING_STATE_IDLE:
        {
            /* Store client task */
            thePairing->client_task = req->client_task;

            /* Store address of device to pair with, 0 we should go discoverable */
            thePairing->device_to_pair_with_bdaddr = req->addr;

            /* no address, go discoverable for inquiry process */
            if (BdaddrIsZero(&req->addr))
            {
                /* Move to 'discoverable' state to start inquiry & page scan */
                pairing_SetState(thePairing, PAIRING_STATE_DISCOVERABLE);
            }
            else
            {
                /* Address of device known, just start authentication using configured timeout */
                ConnectionSmAuthenticate(appGetAppTask(), &req->addr, appConfigAuthenticationTimeout());
                pairing_SetState(thePairing, PAIRING_STATE_PENDING_AUTHENTICATION);
            }
        }
        break;

        default:
        {
            pairing_SendNotReady(req->client_task);
        }
        break;
    }
}

static void pairing_HandleInternalPairLePeerRequest(pairingTaskData *thePairing, PAIR_LE_PEER_REQ_T *req)
{
    DEBUG_LOGF("pairing_HandleInternalPairLePeerRequest, state %d server %d", pairing_GetState(thePairing), req->le_peer_server);

    if(pairing_GetState(thePairing) == PAIRING_STATE_IDLE)
    {
        /* Store client task */
        thePairing->client_task = req->client_task;

        thePairing->pending_ble_address = req->typed_addr;

        thePairing->pairing_lock = TRUE;

        if(req->le_peer_server)
        {
            ConnectionDmBleSecurityReq(&thePairing->task,
                                        &req->typed_addr,
                                        ble_security_authenticated_bonded,
                                        ble_connection_slave_directed);
        }
        pairing_SetState(thePairing, PAIRING_STATE_PENDING_AUTHENTICATION);
    }
    else
    {
        pairing_SendNotReady(req->client_task);
    }
}

static void pairing_HandleInternalTimeoutIndications(pairingTaskData *thePairing)
{
    DEBUG_LOG("pairing_HandleInternalTimeoutIndications");

    switch (pairing_GetState(thePairing))
    {
        case PAIRING_STATE_DISCOVERABLE:
        case PAIRING_STATE_PENDING_AUTHENTICATION:
            /* Send confirmation with error to main task */
            pairing_Complete(thePairing, pairingTimeout, NULL);
            break;

        default:
            break;
    }
}


/*! \brief Handle request to stop pairing. */
static void pairing_HandleInternalPairStopReq(pairingTaskData* thePairing, PAIRING_INTERNAL_PAIR_STOP_REQ_T* req)
{
    DEBUG_LOG("pairing_HandleInternalPairStopReq state=%d", pairing_GetState(thePairing));

    if(pairing_GetState(thePairing) == PAIRING_STATE_IDLE)
    {
        if(req->client_task)
        {
            MessageSend(req->client_task, PAIRING_STOP_CFM, NULL);
        }
    }
    else
    {
        if(thePairing->stop_task)
        {
            Panic();
        }

        thePairing->stop_task = req->client_task;
    }

    switch (pairing_GetState(thePairing))
    {
        case PAIRING_STATE_DISCOVERABLE:
            /* just send complete message with stopped status, there is an auto
             * transition back to idle after sending the message */
            pairing_Complete(thePairing, pairingStopped, NULL);
            break;

        default:
            break;
    }
}

/*! \brief Handle receipt of message to disable scanning.
 */
static void pairing_HandleInternalDisableScan(void)
{
    pairingTaskData *thePairing = PairingGetTaskData();

    DEBUG_LOG("pairing_HandleInternalDisableScan");

    BredrScanManager_InquiryScanRelease(&thePairing->task);
    BredrScanManager_PageScanRelease(&thePairing->task);
}


/*! \brief Handle confirmation of adding link key for handset to PDL
 */
static void pairing_HandleClSmAddAuthDeviceConfirm(CL_SM_ADD_AUTH_DEVICE_CFM_T *cfm)
{
    DEBUG_LOGF("pairing_HandlePeerSigPairHandsetConfirm %d", cfm->status);

    /* Complete setup by adding device attributes for the device
     * default to TWS+ */
    pairing_DeviceUpdate(&cfm->bd_addr, DEVICE_TWS_VERSION, DEVICE_FLAGS_PRE_PAIRED_HANDSET);

    /* Set event indicating we've received a handset link-key.
       Can be used by client to decide to connect to the handset. */
    pairing_MsgActivity(pairingLinkKeyReceived, NULL, DEVICE_TWS_UNKNOWN);

    /* Tell the peer that the link key has been set ok */
    PeerLinkKeys_SendKeyResponseToPeer(&cfm->bd_addr, (cfm->status == success));
}


/*! \brief Handler for CL_SM_SEC_MODE_CONFIG_CFM.

    Handle Security mode confirmation */
static void pairing_HandleClSmSecurityModeConfigConfirm(const CL_SM_SEC_MODE_CONFIG_CFM_T *cfm)
{
    DEBUG_LOG("pairing_HandleClSmSecurityModeConfigConfirm");

    /* Check if setting security mode was successful */
    if (!cfm->success)
        Panic();
}


/*  Handle Pin code indication

    The firmware has indicated that it requires the pin code, we should
    only send the pin code response if the application is in one of the
    pairing mode, otherwise we send a NULL pin code to reject the request.
*/
static void pairing_HandleClPinCodeIndication(const CL_SM_PIN_CODE_IND_T *ind)
{
    uint16 pin_length = 0;
    uint8 pin[16];

    DEBUG_LOG("pairing_HandleClPinCodeIndication");

    /* Only respond to pin indication request when pairing or inquiry */
    if (appSmIsPairing())
    {
        /* Attempt to retrieve fixed pin in PS, if not reject pairing */
        pin_length = PsFullRetrieve(PSKEY_FIXED_PIN, pin, sizeof(pin));
        if (pin_length > 16)
            pin_length = 0;

        /* Respond to the PIN code request */
        ConnectionSmPinCodeResponse(&ind->taddr, (uint8)pin_length, pin);
    }
    else
    {
        /* Respond to the PIN code request with empty pin code */
        ConnectionSmPinCodeResponse(&ind->taddr, 0, (uint8 *)"");
    }
}


/*  Handle the user passkey confirmation.

    This function is called to handle confirmation for the user that the passkey
    matches, since the headset doesn't have a display we just send a reject
    immediately.
*/
static void pairing_HandleClSmUserConfirmationReqIndication(const CL_SM_USER_CONFIRMATION_REQ_IND_T *ind)
{
    pairing_user_confirmation_rsp_t response;

    DEBUG_LOG("pairing_HandleClSmUserConfirmationReqIndication");

    if(!Pairing_PluginHandleUserConfirmationRequest(ind, &response))
    {
        response = pairing_user_confirmation_reject;
    }

    if(response != pairing_user_confirmation_wait)
    {
        bool accept = response != pairing_user_confirmation_reject;
        ConnectionSmUserConfirmationResponse(&ind->tpaddr, accept);
    }
}


/*! \brief Handler for CL_SM_USER_PASSKEY_REQ_IND.
*/
static void pairing_HandleClSmUserPasskeyReqIndication(const CL_SM_USER_PASSKEY_REQ_IND_T *ind)
{
    DEBUG_LOG("pairing_HandleClSmUserPasskeyReqIndication");

    ConnectionSmUserPasskeyResponse(&ind->tpaddr, TRUE, 0);
}

/*! \brief Handler for CL_SM_BLE_SIMPLE_PAIRING_COMPLETE_IND_T . */
static void pairing_HandleClSmBleSimplePairingCompleteInd(const CL_SM_BLE_SIMPLE_PAIRING_COMPLETE_IND_T *ind)
{
    pairingTaskData *thePairing = PairingGetTaskData();
    bool current_request = BdaddrTypedIsSame(&thePairing->pending_ble_address,
                                             &ind->tpaddr.taddr);
    bool any_pending = !BdaddrTypedIsEmpty(&thePairing->pending_ble_address);
    pairingBlePermission permission = thePairing->ble_permission;

    pairingStatus pairing_status = pairingFailed;

    DEBUG_LOG("pairing_HandleClSmBleSimplePairingCompleteInd Any:%d Matches:%d Permission %d",
                    any_pending, current_request, permission);

    Pairing_PluginPairingComplete();

    if (current_request && thePairing->pair_le_task != NULL)
    {
    /*  Indicate pairing status to client and remain in Discoverable state  */
        MAKE_PAIRING_MESSAGE(PAIRING_PAIR_CFM);

        DEBUG_LOG("pairing_HandleClSmBleSimplePairingCompleteInd: LE handset status %d", ind->status);

        if (ind->status == success)
        {
            message->status = pairingSuccess;
        }
        else
        {
            message->status = pairingFailed;
        }

        message->device_bd_addr = ind->tpaddr.taddr.addr;
        MessageSend(thePairing->pair_le_task, PAIRING_PAIR_CFM, message);

        thePairing->pair_le_task = NULL;
    }
    else
    {
        if (any_pending && current_request && permission > pairingBleDisallowed)
        {
            bool hset = appDeviceIsHandset(&ind->permanent_taddr.addr);
            bool permanent = !BdaddrTypedIsEmpty(&ind->permanent_taddr);

            DEBUG_LOG("pairing_HandleClSmBleSimplePairingCompleteInd Handset:%d Permanent:%d",
                        hset, permanent);

            if (!hset)
            {
                hset = appDeviceTypeIsHandset(&ind->permanent_taddr.addr);
                if(hset)
                {
                    DEBUG_LOG("pairing_HandleClSmBleSimplePairingCompleteInd. Note device is a handset, but no TWS information");
                }
                else
                {
                    DEBUG_LOG("pairing_HandleClSmBleSimplePairingCompleteInd. Note device is NOT a handset");
                }
            }

            if (   (permission == pairingBleOnlyPairedHandsets && hset)
                || (permission == pairingBleAllowOnlyResolvable && permanent)
                || (permission == pairingBleAllowAll))
            {
                DEBUG_LOG("pairing_HandleClSmBleSimplePairingCompleteInd Link is expected");
            }
            else
            {
                uint16 cid = GattGetCidForBdaddr(&ind->tpaddr.taddr);

                if (cid && cid != INVALID_CID)
                {
                    DEBUG_LOG("pairing_HandleClSmBleSimplePairingCompleteInd disconnect GATT cid %d",cid);
                    GattDisconnectRequest(cid);
                }

                if (permanent)
                {
                    ConnectionSmDeleteAuthDeviceReq(ind->permanent_taddr.type, &ind->permanent_taddr.addr);
                }
                else
                {
                    ConnectionSmDeleteAuthDeviceReq(ind->tpaddr.taddr.type, &ind->tpaddr.taddr.addr);
                }
            }
            pairing_status = pairingSuccess;

            pairing_ResetPendingBleAddress();
        }

        pairing_Complete(thePairing, pairing_status, &ind->tpaddr.taddr.addr);
    }
}

static void pairing_handleClSmEncryptionChangeInd(const CL_SM_ENCRYPTION_CHANGE_IND_T *ind)
{
    DEBUG_LOG("pairing_handleSmEncryptionChangeInd - Address %x Encrypted:%d",
                    ind->tpaddr.taddr.addr.lap,
                    ind->encrypted);
}

static void pairing_handleClSmBleLinkSecurityInd(const CL_SM_BLE_LINK_SECURITY_IND_T *ind)
{
    DEBUG_LOG("pairing_handleSmBleLinkSecurityInd. Link security(sc?):%d x%04x",
                        ind->le_link_sc, ind->bd_addr.lap);
}

static void pairing_handleDmBleSecurityCfm(const CL_DM_BLE_SECURITY_CFM_T *ble_security_cfm)
{
    DEBUG_LOG("peerPairLe_handleDmBleSecurityCfm: state=%d sts=%d lap=%06x",
              PairingGetTaskData()->state,
              ble_security_cfm->status, ble_security_cfm->taddr.addr.lap);

    switch (ble_security_cfm->status)
    {
        case ble_security_success:
            DEBUG_LOG("**** ble_security_success");
            break;

        case ble_security_pairing_in_progress:
            DEBUG_LOG("**** ble_security_pairing_in_progress");
            break;

        case ble_security_link_key_missing:
            DEBUG_LOG("**** ble_security_link_key_missing");
            break;

        case ble_security_fail:
            DEBUG_LOG("**** ble_security_fail");
            break;

        default:
            DEBUG_LOG("**** Unknown sts");
            break;
    }
}

bool Pairing_HandleConnectionLibraryMessages(MessageId id,Message message, bool already_handled)
{
    bool handled = TRUE;

    switch (id)
    {
        case CL_SM_SEC_MODE_CONFIG_CFM:
            pairing_HandleClSmSecurityModeConfigConfirm((CL_SM_SEC_MODE_CONFIG_CFM_T *)message);
            break;

        case CL_SM_PIN_CODE_IND:
            pairing_HandleClPinCodeIndication((CL_SM_PIN_CODE_IND_T *)message);
            break;

        case CL_SM_AUTHENTICATE_CFM:
            if (!already_handled)
            {
                pairing_HandleClSmAuthenticateConfirm((CL_SM_AUTHENTICATE_CFM_T *)message);
            }
            else
            {
                handled = FALSE;
            }
            break;

        case CL_SM_AUTHORISE_IND:
            return pairing_HandleClSmAuthoriseIndication((CL_SM_AUTHORISE_IND_T *)message);

        case CL_SM_IO_CAPABILITY_REQ_IND:
            if (!already_handled)
            {
                pairing_HandleClSmIoCapabilityReqIndication((CL_SM_IO_CAPABILITY_REQ_IND_T *)message);
            }
            else
            {
                handled = FALSE;
            }
            break;

        case CL_SM_USER_CONFIRMATION_REQ_IND:
            pairing_HandleClSmUserConfirmationReqIndication((CL_SM_USER_CONFIRMATION_REQ_IND_T *)message);
            break;

        case CL_SM_REMOTE_IO_CAPABILITY_IND:
            if (!already_handled)
            {
                pairing_HandleClSmRemoteIoCapabilityIndication((CL_SM_REMOTE_IO_CAPABILITY_IND_T *)message);
            }
            else
            {
                handled = FALSE;
            }
            break;

        case CL_SM_USER_PASSKEY_REQ_IND:
            pairing_HandleClSmUserPasskeyReqIndication((CL_SM_USER_PASSKEY_REQ_IND_T *)message);
            break;

        case CL_SM_ENCRYPTION_CHANGE_IND:
            pairing_handleClSmEncryptionChangeInd((CL_SM_ENCRYPTION_CHANGE_IND_T*)message);
            break;

        case CL_SM_BLE_LINK_SECURITY_IND:
            pairing_handleClSmBleLinkSecurityInd((CL_SM_BLE_LINK_SECURITY_IND_T*)message);
            break;

        case CL_SM_USER_PASSKEY_NOTIFICATION_IND:
        case CL_SM_KEYPRESS_NOTIFICATION_IND:
        case CL_DM_WRITE_APT_CFM:
            /* These messages are associated with pairing, although as
               indications they required no handling */
            break;

        case CL_DM_LINK_SUPERVISION_TIMEOUT_IND:
            break;

        case CL_SM_BLE_SIMPLE_PAIRING_COMPLETE_IND:
            pairing_HandleClSmBleSimplePairingCompleteInd((const CL_SM_BLE_SIMPLE_PAIRING_COMPLETE_IND_T *)message);
            break;

        case CL_DM_BLE_SECURITY_CFM:
            pairing_handleDmBleSecurityCfm((CL_DM_BLE_SECURITY_CFM_T*)message);
            break;

        default:
            handled = FALSE;
            break;

    }
    return handled;
}

/*! \brief Message Handler

    This function is the main message handler for the pairing module.
*/
static void pairing_HandleMessage(Task task, MessageId id, Message message)
{
    pairingTaskData *thePairing = (pairingTaskData *)task;

    switch (id)
    {
        case CL_DM_WRITE_INQUIRY_MODE_CFM:
        case CL_DM_WRITE_INQUIRY_ACCESS_CODE_CFM:
            break;

        case CL_SM_GET_AUTH_DEVICE_CFM:
            pairing_HandleClSmGetAuthDeviceConfirm(thePairing, (CL_SM_GET_AUTH_DEVICE_CFM_T*)message);
            break;

        case PAIRING_INTERNAL_PAIR_REQ:
            pairing_HandleInternalPairRequest(thePairing, (PAIR_REQ_T *)message);
            break;

        case PAIRING_INTERNAL_LE_PEER_PAIR_REQ:
            pairing_HandleInternalPairLePeerRequest(thePairing, (PAIR_LE_PEER_REQ_T *)message);
            break;

        case PAIRING_INTERNAL_TIMEOUT_IND:
            pairing_HandleInternalTimeoutIndications(thePairing);
            break;

        case PAIRING_INTERNAL_PAIR_STOP_REQ:
            pairing_HandleInternalPairStopReq(thePairing, (PAIRING_INTERNAL_PAIR_STOP_REQ_T *)message);
            break;

        case PAIRING_INTERNAL_DISABLE_SCAN:
            pairing_HandleInternalDisableScan();
            break;

        case CL_SM_ADD_AUTH_DEVICE_CFM:
            pairing_HandleClSmAddAuthDeviceConfirm((CL_SM_ADD_AUTH_DEVICE_CFM_T*)message);
            break;

        case CL_DM_BLE_SECURITY_CFM:
            pairing_handleDmBleSecurityCfm((CL_DM_BLE_SECURITY_CFM_T*)message);
            break;

        default:
            appHandleUnexpected(id);
            break;
    }
}


bool Pairing_Init(Task init_task)
{
    pairingTaskData *thePairing = PairingGetTaskData();

    DEBUG_LOG("Pairing_Init");

    /* Set up task handler */
    thePairing->task.handler = pairing_HandleMessage;

    TaskList_InitialiseWithCapacity(PairingGetClientList(), PAIRING_CLIENT_TASK_LIST_INIT_CAPACITY);

    /* Initialise state */
    thePairing->state = PAIRING_STATE_NULL;
    thePairing->pairing_lock = 1;
    pairing_SetState(thePairing, PAIRING_STATE_INITIALISING);
    thePairing->ble_permission = pairingBleAllowAll;
    thePairing->stop_task = NULL;
    TaskList_InitialiseWithCapacity(PairingGetPairingActivity(), PAIRING_ACTIVITY_LIST_INIT_CAPACITY);

    Pairing_PluginInit();

    /* register pairing as a client of the peer signalling task, it means
     * we will may get PEER_SIG_CONNECTION_IND messages if the signalling
     * channel becomes available again to retry sending the link key */
    appPeerSigClientRegister(&thePairing->task);

    appInitSetInitTask(init_task);
    return TRUE;
}

/*! \brief Pair with a device, where inquiry is required. */
void Pairing_Pair(Task client_task, bool is_user_initiated)
{
    MAKE_PAIRING_MESSAGE(PAIR_REQ);
    pairingTaskData *thePairing = PairingGetTaskData();

    DEBUG_LOG("Pairing_Pair");

    message->client_task = client_task;
    message->is_user_initiated = is_user_initiated;

    BdaddrSetZero(&message->addr);
    MessageSendConditionally(&thePairing->task, PAIRING_INTERNAL_PAIR_REQ,
                             message, &thePairing->pairing_lock);
}

/*! \brief Pair with a device where the address is already known.
 */
void Pairing_PairAddress(Task client_task, bdaddr* device_addr)
{
    MAKE_PAIRING_MESSAGE(PAIR_REQ);
    pairingTaskData *thePairing = PairingGetTaskData();

    DEBUG_LOG("Pairing_PairAddress");

    message->client_task = client_task;
    message->addr = *device_addr;
    message->is_user_initiated = FALSE;

    MessageSendConditionally(&thePairing->task, PAIRING_INTERNAL_PAIR_REQ,
                             message, &thePairing->pairing_lock);
}

/*! \brief Pair with le peer device. This is a temporary function until
           discovery is removed from the pairing module.
 */
void Pairing_PairLePeer(Task client_task, typed_bdaddr* device_addr, bool server)
{
    MAKE_PAIRING_MESSAGE(PAIR_LE_PEER_REQ);
    pairingTaskData *thePairing = PairingGetTaskData();

    DEBUG_LOG("Pairing_PairLePeer state %d", pairing_GetState(thePairing));

    message->client_task = client_task;
    message->le_peer_server = server;

    if(device_addr)
    {
        message->typed_addr = *device_addr;
    }
    else
    {
        BdaddrTypedSetEmpty(&message->typed_addr);
    }

    MessageSendConditionally(&thePairing->task, PAIRING_INTERNAL_LE_PEER_PAIR_REQ,
                             message, &thePairing->pairing_lock);
}

void Pairing_PairLeAddress(Task client_task, const typed_bdaddr* device_addr)
{
    pairingTaskData *thePairing = PairingGetTaskData();

    DEBUG_LOG("Pairing_PairLeAddress state %d addr %04x %02x %06x",
              pairing_GetState(thePairing),
              device_addr->addr.nap,
              device_addr->addr.uap,
              device_addr->addr.lap);

    /*  Only supported in Discoverable state
     *  Pairing plugin handles handset pairing so don't interfere if one is set up */
    if ((pairing_GetState(thePairing) == PAIRING_STATE_DISCOVERABLE) && !Pairing_PluginIsRegistered())
    {
        thePairing->pair_le_task = client_task;
        thePairing->pending_ble_address = *device_addr;

        ConnectionDmBleSecurityReq(&thePairing->task,
                        device_addr,
                        ble_security_authenticated_bonded,
                        ble_connection_slave_directed);
    }
    else
    {
        pairing_SendNotReady(client_task);
    }
}

/*! @brief Stop a pairing.
 */
void Pairing_PairStop(Task client_task)
{
    pairingTaskData *thePairing = PairingGetTaskData();

    MAKE_PAIRING_MESSAGE(PAIRING_INTERNAL_PAIR_STOP_REQ);

    DEBUG_LOG("Pairing_PairStop");

    message->client_task = client_task;

    MessageSend(&thePairing->task, PAIRING_INTERNAL_PAIR_STOP_REQ, message);
}

/*! brief Register to receive #PAIRING_ACTIVITY messages. */
void Pairing_ActivityClientRegister(Task task)
{
    DEBUG_LOG("Pairing_ActivityClientRegister");

    TaskList_AddTask(TaskList_GetFlexibleBaseTaskList(PairingGetPairingActivity()), task);
}

/*! brief Add a device to the PDL. */
void Pairing_AddAuthDevice(const bdaddr* address, const uint16 key_length, const uint16* link_key)
{
    pairingTaskData *thePairing = PairingGetTaskData();

    DEBUG_LOG("Pairing_AddAuthDevice");

    /* Store the link key in the PDL */
    ConnectionSmAddAuthDevice(&thePairing->task,
                              address,
                              FALSE,
                              TRUE,
                              cl_sm_link_key_unauthenticated_p192, /* TODO is this right? */
                              key_length,
                              link_key);
}

static void pairing_RegisterMessageGroup(Task task, message_group_t group)
{
    PanicFalse(group == PAIRING_MESSAGE_GROUP);
    TaskList_AddTask(TaskList_GetFlexibleBaseTaskList(PairingGetClientList()), task);
}

/*
 * TEST FUNCTIONS
 */
void Pairing_SetLinkTxReqd(void)
{
    bdaddr handset_addr;

    DEBUG_LOG("Setting handset link key TX is required");
    appDeviceGetHandsetBdAddr(&handset_addr);
    BtDevice_SetHandsetLinkKeyTxReqd(&handset_addr, TRUE);
}

void Pairing_ClearLinkTxReqd(void)
{
    bdaddr handset_addr;

    DEBUG_LOG("Pairing_ClearLinkTxReqd");
    appDeviceGetHandsetBdAddr(&handset_addr);
    BtDevice_SetHandsetLinkKeyTxReqd(&handset_addr, FALSE);
}

MESSAGE_BROKER_GROUP_REGISTRATION_MAKE(PAIRING, pairing_RegisterMessageGroup, NULL);

