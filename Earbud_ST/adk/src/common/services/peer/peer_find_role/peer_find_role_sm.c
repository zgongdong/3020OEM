/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Implementation of state machine transitions for the service finding the peer 
            and selecting role using LE
*/

#include <panic.h>
#include <logging.h>

#include <app/bluestack/dm_prim.h>

#include <bt_device.h>
#include <gatt_role_selection_server_uuids.h>

#include "peer_find_role_sm.h"
#include "peer_find_role_private.h"
#include "peer_find_role_config.h"
#include "peer_find_role_init.h"

#define MAKE_PRIM_C(TYPE) MESSAGE_MAKE(prim,TYPE##_T); prim->common.op_code = TYPE; prim->common.length = sizeof(TYPE##_T);

/*! Internal function for entering the state #PEER_FIND_ROLE_STATE_INITIALISED

    This state can be entered in response to errors elsewhere, so if 
    there is an active GATT connection a disconnection is requested.

    There is no further action until PeerFindRole_FindRole() is called.
 */
static void peer_find_role_enter_initialised(void)
{
    peerFindRoleTaskData *pfr = PeerFindRoleGetTaskData();

    uint16 cid = pfr->gatt_cid;
    pfr->gatt_cid = 0;
    pfr->gatt_encrypted = FALSE;

    if (cid)
    {
        DEBUG_LOG("peer_find_role_enter_initialised. Disconnecting cid:%d", cid);

        /*! \todo We don't get an explicit disconnect ind. Goes to gatt handler
                Connection Manager changes may now give us something better */
        GattManagerDisconnectRequest(cid);
    }
    else if (pfr->timeout_means_timeout)
    {
        peer_find_role_notify_timeout();
    }
    else
    {
        peer_find_role_notify_clients_if_pending();
    }
}


/*! Internal function for entering the state #PEER_FIND_ROLE_STATE_CHECKING_PEER

    This function checks whether we know of a peer device. If not
    we notify clients that PeerFindRole_FindRole() has completed 
    with the message #PEER_FIND_ROLE_NO_PEER.

    If we know of a peer we jump to the state #PEER_FIND_ROLE_STATE_DISCOVER

    Other cases are errors
 */
static void peer_find_role_enter_checking_peer(void)
{
    peerFindRoleTaskData *pfr = PeerFindRoleGetTaskData();

    DEBUG_LOG("peer_find_role_enter_checking_peer.");

    if (!appDeviceGetPrimaryBdAddr(&pfr->primary_addr))
    {
        peer_find_role_completed(PEER_FIND_ROLE_NO_PEER);
    }
    else
    {
        if (appDeviceGetMyBdAddr(&pfr->my_addr))
        {
#ifdef INCLUDE_SM_PRIVACY_1P2
                /* See BT spec documentation of the LE set privacy mode command.
                Setting device privacy mode means the local controller will accept
                both RPA and identity addresses from the remote controller. This
                is opposed to network privacy mode, where only RPA will be accepted. */
                MAKE_PRIM_C(DM_HCI_ULP_SET_PRIVACY_MODE_REQ);
                prim->peer_identity_address_type = TYPED_BDADDR_PUBLIC;
                BdaddrConvertVmToBluestack(&prim->peer_identity_address, &pfr->primary_addr);
                prim->privacy_mode = 1; /* Device privacy mode */
                VmSendDmPrim(prim);
#endif
            peer_find_role_set_state(PEER_FIND_ROLE_STATE_DISCOVER);
            return;
        }

        /*! \todo A Panic here is dangerous, Factory reset / remove peer ? */
        DEBUG_LOG("peer_find_role_enter_checking_peer. No attributes !");
        Panic();
    }
}


/*! Calculate time to delay advertising.

    Advertising is started immediately on the first connection attempt,
    but is delayed on retries to reduce the risk of the two devices
    initiating connections to each other.

    There is no delay if we have active media as there will be only 
    advertising.

    In priority order
    \li busy (active) - do not delay, we won't be scanning
    \li connected to a handset - delay by a medium amount
    \li this device has the Peripheral role - delay by a large amount
    \li this device has the Cental role - delay by a small amount

    \param reconnecting FALSE if this is the first connection attempt of a 
        find role process
 */
static void peer_find_role_calculate_backoff(bool reconnecting)
{
    peerFindRoleTaskData *pfr = PeerFindRoleGetTaskData();

    DEBUG_LOG("peer_find_role_calculate_backoff. reconnecting:%d a2dp/sco:%d",
                reconnecting , peer_find_role_media_active());

    if (/*reconnecting &&*/ !peer_find_role_media_active())
    {
        if (appDeviceIsHandsetConnected())
        {
            pfr->advertising_backoff_ms = 200;

            DEBUG_LOG("peer_find_role_calculate_backoff as %dms. Connected to a handset.",
                        pfr->advertising_backoff_ms);
        }
        else if (peer_find_role_is_central())
        {
            pfr->advertising_backoff_ms = 100;

            DEBUG_LOG("peer_find_role_calculate_backoff as %dms. We have 'Central' role",
                        pfr->advertising_backoff_ms);
        }
        else
        {
            pfr->advertising_backoff_ms = 300;

            DEBUG_LOG("peer_find_role_calculate_backoff as %dms.",
                        pfr->advertising_backoff_ms);
        }
    }
}


/*! Helper function to start scanning

    This function checks whether we are already scanning and whether
    scanning is allowed.

    If needed scanning is then enabled using \ref le_scan_manager
 */
static void peer_find_role_start_scanning_if_inactive(void)
{
    peerFindRoleTaskData *pfr = PeerFindRoleGetTaskData();
    le_advertising_report_filter_t filter;
    uint16 uuid = UUID16_ROLE_SELECTION_SERVICE;

    if (!pfr->scan && !peer_find_role_media_active())
    {
        filter.ad_type = ble_ad_type_complete_uuid16;
        filter.interval = filter.size_pattern = sizeof(uuid);
        filter.pattern = (uint8*)&uuid;

        peer_find_role_activity_set(PEER_FIND_ROLE_ACTIVE_SCANNING);

        /*! \todo Try filtering */
        pfr->scan = LeScanManager_Start(le_scan_interval_fast, &filter);
        if (NULL == pfr->scan)
        {
            DEBUG_LOG("peer_find_role_enter_discover. Unable to acquire scan");
            /*! \todo Panic during development only */
            Panic();
        }
    }
}


/*! Internal function for entering the state #PEER_FIND_ROLE_STATE_DISCOVER

    On entering the state we start scanning if it is permitted, and not 
    already running.

    If we are entering this state to retry a connection a delay before
    starting advertising is calculated.

    Finally we send ourselves a delayed message to start advertising
    #PEER_FIND_ROLE_INTERNAL_TIMEOUT_ADVERT_BACKOFF. The delay may be zero.

    \param old_state The state we have left to enter this state
 */
static void peer_find_role_enter_discover(PEER_FIND_ROLE_STATE old_state)
{
    peerFindRoleTaskData *pfr = PeerFindRoleGetTaskData();

    DEBUG_LOG("peer_find_role_enter_discover");

    peer_find_role_calculate_backoff(PEER_FIND_ROLE_STATE_CHECKING_PEER != old_state);

    peer_find_role_start_scanning_if_inactive();

    MessageSendLater(PeerFindRoleGetTask(),
                     PEER_FIND_ROLE_INTERNAL_TIMEOUT_ADVERT_BACKOFF, NULL,
                     pfr->advertising_backoff_ms);

    pfr->advertising_backoff_ms = 0;
}


/*! Internal function for entering the state #PEER_FIND_ROLE_STATE_DISCOVER_CONNECTABLE

    On entering the state we start to set up advertising. The response to this
    command will initiate connectable advertising.

    There is also a check to see if scanning should be started. In normal
    use, scanning will have started by now - but it is possible that
    other activities that block scanning have ended since entry to the state
    #PEER_FIND_ROLE_STATE_DISCOVER
 */
static void peer_find_role_enter_discover_connectable(void)
{
    peerFindRoleTaskData *pfr = PeerFindRoleGetTaskData();
    uint16 uuid = UUID_ROLE_SELECTION_SERVICE;
    ble_adv_params_t    adv_params;

    DEBUG_LOG("peer_find_role_enter_discover_connectable");

    /* This is necessary as media may have stopped between entering
        discover state and arriving here */
    peer_find_role_start_scanning_if_inactive();

    pfr->advert = AdvertisingManager_NewAdvert();
    if (NULL == pfr->advert)
    {
        DEBUG_LOG("peer_find_role_enter_discover_connectable. Unable to acquire advert");
        /*! \todo Panic during development only */
        Panic();
    }

    peer_find_role_activity_set(PEER_FIND_ROLE_ACTIVE_ADVERTISING);

    AdvertisingManager_SetService(pfr->advert, uuid);
    AdvertisingManager_SetAdvertisingType(pfr->advert, ble_adv_ind);
    
    adv_params.undirect_adv.filter_policy = ble_filter_none;
    adv_params.undirect_adv.adv_interval_min = PeerFindRoleConfigMinimumAdvertisingInterval();
    adv_params.undirect_adv.adv_interval_max = PeerFindRoleConfigMaximumAdvertisingInterval();

    AdvertisingManager_SetAdvertParams(pfr->advert, &adv_params);

        /* Want to use connectable advertising, so just set data 
           And we get a message back */
    AdvertisingManager_SetAdvertData(pfr->advert, PeerFindRoleGetTask());
}


/*! Internal function for entering the state #PEER_FIND_ROLE_STATE_CONNECTING_TO_DISCOVERED

    Having detected a device to connect to, the parameters for the
    next connection are updated. The response to this command will
    initiate a connection.
 */
static void peer_find_role_enter_connecting_to_discovered(void)
{
    typed_bdaddr tb;
    peerFindRoleTaskData *pfr = PeerFindRoleGetTaskData();

    DEBUG_LOG("peer_find_role_enter_connecting_to_discovered");

    tb.type = TYPED_BDADDR_PUBLIC;
    tb.addr = pfr->primary_addr;

    GattManagerConnectAsCentral(PeerFindRoleGetTask(),
                                &tb,
                                gatt_connection_ble_master_directed,
                                TRUE);

    peer_find_role_activity_set(PEER_FIND_ROLE_ACTIVE_CONNECTING);
}


void peer_find_role_stop_scan_if_active(void)
{
    peerFindRoleTaskData *pfr = PeerFindRoleGetTaskData();

    if (pfr->scan)
    {
        LeScanManager_Stop(pfr->scan);
        pfr->scan = NULL;
        peer_find_role_activity_clear(PEER_FIND_ROLE_ACTIVE_SCANNING);
    }
}


/*! Internal function for exiting the state #PEER_FIND_ROLE_STATE_DISCOVER

    We may have been scanning in this state and will need to cancel 
    scanning if we have seen a device, or are leaving the state for 
    anything other than starting advertising (while scanning).

    \param new_state The next state. Used to determine the action to take.
 */
static void peer_find_role_exit_discover(PEER_FIND_ROLE_STATE new_state)
{
    bool cancel_scan = TRUE;

    DEBUG_LOG("peer_find_role_exit_discover - next state:%d",new_state);

    /* Although we might be exiting the state because of this message...
       ...cancel it anyway */
    MessageCancelAll(PeerFindRoleGetTask(), PEER_FIND_ROLE_INTERNAL_TIMEOUT_ADVERT_BACKOFF);

    switch (new_state)
    {
        case PEER_FIND_ROLE_STATE_DISCOVER_CONNECTABLE:
            /* We are simply adding advertising to the mix */
            DEBUG_LOG("peer_find_role_exit_discover - next state:PEER_FIND_ROLE_STATE_DISCOVER_CONNECTABLE");
            cancel_scan = FALSE;
            break;

        case PEER_FIND_ROLE_STATE_DISCOVERED_DEVICE:
            DEBUG_LOG("peer_find_role_exit_discover - next state:PEER_FIND_ROLE_STATE_DISCOVERED_DEVICE");
            break;

        default:
            break;
    }

    if (cancel_scan)
    {
        peer_find_role_stop_scan_if_active();
    }
}


/*! Internal function for exiting the state #PEER_FIND_ROLE_STATE_DISCOVER_CONNECTABLE

    In this state we may have been scanning and will have been advertising.
    Scanning and advertising will be stopped if still active.

    \param new_state This is the state we are entering, used solely for debug
 */
static void peer_find_role_exit_discover_connectable(PEER_FIND_ROLE_STATE new_state)
{
    peerFindRoleTaskData *pfr = PeerFindRoleGetTaskData();

    DEBUG_LOG("peer_find_role_exit_discover_connectable - next state:%d",new_state);

    peer_find_role_stop_scan_if_active();

    switch (new_state)
    {
        case PEER_FIND_ROLE_STATE_DISCOVERED_DEVICE:
            DEBUG_LOG("peer_find_role_exit_discover_connectable - next state:PEER_FIND_ROLE_STATE_DISCOVERED_DEVICE");
            break;

        case PEER_FIND_ROLE_STATE_CLIENT:
            break;

        default:
            break;
    }

    GattManagerCancelWaitForRemoteClient();
    if (pfr->advert)
    {
        AdvertisingManager_DeleteAdvert(pfr->advert);
        pfr->advert = FALSE;
    }
}


/*! Internal function called when entering the state #PEER_FIND_ROLE_STATE_DISCOVERED_DEVICE

    This function creates a message #PEER_FIND_ROLE_INTERNAL_CONNECT_TO_PEER
    that will be sent if we manage to stop any ongoing advertising. The message 
    will be cancelled if we leave the state. 

    We may be about to accept a connection from another device, which is why we 
    send a conditional message.
 */
static void peer_find_role_enter_discovered_device(void)
{
    DEBUG_LOG("peer_find_role_enter_discovered_device");

    /* cancel the timeout, now that peer has been seen */
    peer_find_role_cancel_initial_timeout();

    peer_find_role_message_send_when_inactive(PEER_FIND_ROLE_INTERNAL_CONNECT_TO_PEER, NULL);
}


/*! Internal function for exiting the state #PEER_FIND_ROLE_STATE_DISCOVERED_DEVICE

    We make sure that the message #PEER_FIND_ROLE_INTERNAL_CONNECT_TO_PEER is
    cancelled at this point. 

    \note The message might have already expired. Cancelling a non-existent message
    is not harmful.
 */
static void peer_find_role_exit_discovered_device(void)
{
    DEBUG_LOG("peer_find_role_exit_discovered_device");

    peer_find_role_message_cancel_inactive(PEER_FIND_ROLE_INTERNAL_CONNECT_TO_PEER);
}


/*! Internal function for exiting the state #PEER_FIND_ROLE_STATE_CONNECTING_TO_DISCOVERED

    This function makes sure that we cancel an attempt to connect if 
    the next state represents anything other than having made the connection.

    \param new_state The state we are entering next
 */
static void peer_find_role_exit_connecting_to_discovered(PEER_FIND_ROLE_STATE new_state)
{
    if (new_state != PEER_FIND_ROLE_STATE_SERVER)
    {
        DEBUG_LOG("peer_find_role_exit_connecting_to_discovered. Cancelling server connection");

        GattManagerCancelConnectToRemoteServer(PeerFindRoleGetTask());
    }
}


/*! Internal function for entering the state #PEER_FIND_ROLE_STATE_AWAITING_DISCONNECT

    On entering the state we start a timer #PEER_FIND_ROLE_INTERNAL_TIMEOUT_NOT_DISCONNECTED
    is started. Expiry of this would force a disconnection.
 */
static void peer_find_role_enter_awaiting_disconnect(void)
{
    DEBUG_LOG("peer_find_role_enter_awaiting_disconnect");

    MessageSendLater(PeerFindRoleGetTask(), 
                     PEER_FIND_ROLE_INTERNAL_TIMEOUT_NOT_DISCONNECTED, NULL,
                     PeerFindRoleConfigGattDisconnectTimeout());
}


/*! Internal function for exiting the state #PEER_FIND_ROLE_STATE_AWAITING_DISCONNECT

    As we have now left the state we cancel the message that 
    forces a disconnect if the connection has not already been 
    dropped 
 */
static void peer_find_role_exit_awaiting_disconnect(void)
{
    DEBUG_LOG("peer_find_role_exit_awaiting_disconnect");

    MessageCancelAll(PeerFindRoleGetTask(), PEER_FIND_ROLE_INTERNAL_TIMEOUT_NOT_DISCONNECTED);
}


/*! Internal function for entering the state #PEER_FIND_ROLE_STATE_CLIENT

    On entering the state we request details of the specific role
    selection service from the connected device.

    The device must provide the GATT service UUID_ROLE_SELECTION_SERVICE
 */
static void peer_find_role_enter_client(void)
{
    peerFindRoleTaskData *pfr = PeerFindRoleGetTaskData();
    gatt_uuid_t uuid = UUID_ROLE_SELECTION_SERVICE;

    DEBUG_LOG("peer_find_role_enter_client");

    /* cancel the timeout, now that peer has been seen */
    peer_find_role_cancel_initial_timeout();

    /*! \todo Ask the client to initialise itself, so we don't have to know about UUID */
    GattDiscoverPrimaryServiceRequest(PeerFindRoleGetTask(), pfr->gatt_cid, 
                                      gatt_uuid16, &uuid);
}


/*! Internal function for entering the state #PEER_FIND_ROLE_STATE_SERVER

    On entering the state we request security.
 */
static void peer_find_role_enter_server(void)
{
    peerFindRoleTaskData *pfr = PeerFindRoleGetTaskData();
    tp_bdaddr tb;

    PanicFalse(VmGetBdAddrtFromCid(pfr->gatt_cid, &tb));

    DEBUG_LOG("peer_find_role_enter_server 0x%06x Transp:LE%d Type:Public:%d", tb.taddr.addr.lap,
                                        tb.transport == TRANSPORT_BLE_ACL,
                                        tb.taddr.type == TYPED_BDADDR_PUBLIC);

    ConnectionDmBleSecurityReq(PeerFindRoleGetTask(), &tb.taddr, 
                               ble_security_encrypted_bonded, 
                               ble_connection_master_directed);
}


/*! Internal function for entering the state #PEER_FIND_ROLE_STATE_AWAITING_ENCRYPTION

    On entering the state we check if the link is already encrypted 
    and move to deciding state if so */
static void peer_find_role_enter_awaiting_encryption(void)
{
    peerFindRoleTaskData *pfr = PeerFindRoleGetTaskData();

    DEBUG_LOG("peer_find_role_enter_awaiting_encryption");

    if (pfr->gatt_encrypted)
    {
        peer_find_role_set_state(PEER_FIND_ROLE_STATE_DECIDING);
    }
}


/*! Internal function for entering the state #PEER_FIND_ROLE_STATE_DECIDING

    On entering the state we read the value used to determine
    roles from our peer
 */
static void peer_find_role_enter_deciding(void)
{
    peerFindRoleTaskData *pfr = PeerFindRoleGetTaskData();

    DEBUG_LOG("peer_find_role_enter_deciding");

    GattRoleSelectionClientReadFigureOfMerit(&pfr->role_selection_client);
}


void peer_find_role_set_state(PEER_FIND_ROLE_STATE new_state)
{
    peerFindRoleTaskData *pfr = PeerFindRoleGetTaskData();

    PEER_FIND_ROLE_STATE old_state = peer_find_role_get_state();

    if (old_state == new_state)
    {
        DEBUG_LOG("peer_find_role_set_state. Attempt to transition to same state:%d",
                   old_state);
        Panic();    /*! \todo Remove panic once topology implementation stable */
        return;
    }

    DEBUG_LOG("peer_find_role_set_state. Transition %d->%d. Busy flags were:0x%x",
                old_state, new_state,
                PeerFindRoleGetBusy());

    /* Pattern is to run functions for exiting state first */
    switch (old_state)
    {
        case PEER_FIND_ROLE_STATE_DISCOVER:
            peer_find_role_exit_discover(new_state);
            break;

        case PEER_FIND_ROLE_STATE_DISCOVER_CONNECTABLE:
            peer_find_role_exit_discover_connectable(new_state);
            break;

        case PEER_FIND_ROLE_STATE_DISCOVERED_DEVICE:
            peer_find_role_exit_discovered_device();
            break;

        case PEER_FIND_ROLE_STATE_CONNECTING_TO_DISCOVERED:
            peer_find_role_exit_connecting_to_discovered(new_state);
            break;

        case PEER_FIND_ROLE_STATE_AWAITING_DISCONNECT:
            peer_find_role_exit_awaiting_disconnect();
            break;

        default:
            break;
    }

    pfr->state = new_state;

    switch (new_state)
    {
        case PEER_FIND_ROLE_STATE_INITIALISED:
            peer_find_role_enter_initialised();
            break;

        case PEER_FIND_ROLE_STATE_CHECKING_PEER:
            peer_find_role_enter_checking_peer();
            break;

        case PEER_FIND_ROLE_STATE_DISCOVER:
            peer_find_role_enter_discover(old_state);
            break;

        case PEER_FIND_ROLE_STATE_DISCOVER_CONNECTABLE:
            peer_find_role_enter_discover_connectable();
            break;

        case PEER_FIND_ROLE_STATE_DISCOVERED_DEVICE:
            peer_find_role_enter_discovered_device();
            break;

        case PEER_FIND_ROLE_STATE_CONNECTING_TO_DISCOVERED:
            peer_find_role_enter_connecting_to_discovered();
            break;

        case PEER_FIND_ROLE_STATE_CLIENT:
            peer_find_role_enter_client();
            break;

        case PEER_FIND_ROLE_STATE_SERVER:
            peer_find_role_enter_server();
            break;

        case PEER_FIND_ROLE_STATE_AWAITING_ENCRYPTION:
            peer_find_role_enter_awaiting_encryption();
            break;

        case PEER_FIND_ROLE_STATE_DECIDING:
            peer_find_role_enter_deciding();
            break;

        case PEER_FIND_ROLE_STATE_AWAITING_DISCONNECT:
            peer_find_role_enter_awaiting_disconnect();
            break;

        default:
            break;
    }
}

