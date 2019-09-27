/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Implementation of state machine transitions for the PEER PAIRING OVER LE service
*/

#include <gatt.h>
#include <panic.h>
#include <util.h>

#include <logging.h>

#include <gatt_root_key_server.h>
#include <gatt_root_key_server_uuids.h>
#include <gatt_handler.h>

#include "peer_pair_le.h"
#include "peer_pair_le_sm.h"
#include "peer_pair_le_init.h"
#include "peer_pair_le_private.h"
#include "peer_pair_le_key.h"

#include <uuid.h>


static bool peer_pair_le_is_discovery_state(PEER_PAIR_LE_STATE state)
{
    return    PEER_PAIR_LE_STATE_DISCOVERY == state
           || PEER_PAIR_LE_STATE_SELECTING == state;
}


bool peer_pair_le_in_pairing_state(void)
{
    switch (peer_pair_le_get_state())
    {
        case PEER_PAIR_LE_STATE_PAIRING_AS_CLIENT:
        case PEER_PAIR_LE_STATE_PAIRING_AS_SERVER:
            return TRUE;

        default:
            return FALSE;
    }
}


static void peer_pair_le_start_advertising(void)
{
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();
    uint8 uuid[] = UUID_128_FORMAT_uint8(UUID128_ROOT_KEY_SERVICE);
    ble_adv_params_t    adv_params;

    DEBUG_LOG("peer_pair_le_start_advertising");

    ppl->advert = AdvertisingManager_NewAdvert();
    if (NULL == ppl->advert)
    {
        DEBUG_LOG("peer_pair_le_start_advertising. Unable to acquire advert");
        /*! \todo Panic during development only */
        Panic();
    }
    AdvertisingManager_SetServiceUuid128(ppl->advert, uuid);
    AdvertisingManager_SetAdvertisingType(ppl->advert, ble_adv_ind);
    
    adv_params.undirect_adv.filter_policy = ble_filter_none;
    adv_params.undirect_adv.adv_interval_min = PeerPairLeConfigMinimumAdvertisingInterval();
    adv_params.undirect_adv.adv_interval_max = PeerPairLeConfigMaximumAdvertisingInterval();
    AdvertisingManager_SetAdvertParams(ppl->advert, &adv_params);

        /* Want to use connectable advertising, so just set data 
           And we get a message back */
    AdvertisingManager_SetAdvertData(ppl->advert, PeerPairLeGetTask());

        /*! \todo This is out of sync completely with the gatt handler */
}


/* This is used when we get a server connect */
static void peer_pair_le_cancel_advertising(void)
{
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();

    DEBUG_LOG("peer_pair_le_cancel_advertising");

    if (NULL == ppl->advert)
    {
        DEBUG_LOG("peer_pair_le_cancel_advertising. Unable to acquire advert");
        /*! \todo Panic during development only */
        Panic();
    }
    AdvertisingManager_DeleteAdvert(ppl->advert);
}


static void peer_pair_le_stop_advertising(void)
{
    DEBUG_LOG("peer_pair_le_stop_advertising");

    GattManagerCancelWaitForRemoteClient();

    peer_pair_le_cancel_advertising();
}


static void peer_pair_le_start_scanning(void)
{
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();
    uint8 uuid[] = UUID_128_FORMAT_uint8(UUID128_ROOT_KEY_SERVICE);
    le_advertising_report_filter_t filter;

    DEBUG_LOG("peer_pair_le_start_scanning");

    memset(ppl->scanned_devices, 0, sizeof(ppl->scanned_devices));

    filter.ad_type = ble_ad_type_complete_uuid128;
    filter.interval = filter.size_pattern = sizeof(uuid);
    filter.pattern = uuid;

    ppl->scan = LeScanManager_Start(le_scan_interval_fast, &filter);
    if (NULL == ppl->scan)
    {
        DEBUG_LOG("peer_pair_le_start_scanning. Unable to acquire scan");
        /*! \todo Panic during development only */
        Panic();
    }
}


static void peer_pair_le_stop_scanning(void)
{
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();

    DEBUG_LOG("peer_pair_le_stop_scanning");

    if (ppl->scan)
    {
        MessageCancelAll(PeerPairLeGetTask(), PEER_PAIR_LE_TIMEOUT_FROM_FIRST_SCAN);
        LeScanManager_Stop(ppl->scan);
    }
}


static void peer_pair_le_enter_discovery(PEER_PAIR_LE_STATE old_state)
{
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();

    DEBUG_LOG("peer_pair_le_enter_discovery");

    ppl->gatt_cid = 0;

    peer_pair_le_start_advertising();

    if (!peer_pair_le_is_discovery_state(old_state))
    {
        peer_pair_le_start_scanning();
    }
}


static void peer_pair_le_exit_discovery(PEER_PAIR_LE_STATE new_state)
{
    DEBUG_LOG("peer_pair_le_exit_discovery");

    switch (new_state)
    {
        case PEER_PAIR_LE_STATE_SELECTING:
            /* We have seen at least one advert we like, so nothing changes */
            DEBUG_LOG("peer_pair_le_exit_discovery: Expected, nothing to see here");
            break;

        case PEER_PAIR_LE_STATE_DISCOVERY:
            /* Going from SELECTING to DISCOVERY state requires stopping advertising,
               so that DISCOVERY state can start it again. */
            peer_pair_le_stop_advertising();
            break;

        case PEER_PAIR_LE_STATE_CONNECTING:
            /* We have identified a device. Need to stop our adverts */
            peer_pair_le_stop_advertising();
            peer_pair_le_stop_scanning();
            break;

        case PEER_PAIR_LE_STATE_PAIRING_AS_SERVER:
            peer_pair_le_stop_scanning();
                /*! \todo This is a problem with GATT being outside advert manager */
            peer_pair_le_cancel_advertising();
            break;
            
        default:
            DEBUG_LOG("peer_pair_le_exit_discovery: Need to do something, state:%d", new_state);
            Panic();
            break;
    }
}


/* Compare bdaddr. Treat LAP as most important distinguisher.
    Rationale: Expect devices to be from same MFR (if public) and if a RRA address
               checks the most bits first.

    Do not just use memcmp as the bdaddr structure is not packed.
 */
static bool peer_pair_le_bdaddr_greater(const bdaddr *first, const bdaddr *second)
{
    if (first->lap != second->lap)
    {
        return first->lap > second->lap;
    }

    if (first->nap != second->nap)
    {
        return first->nap > second->nap;
    }

    return first->uap > second->uap;
}


static void peer_pair_le_enter_selecting(void)
{
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();
    uint32 delay = appConfigPeerPairLeTimeoutPeerSelect();

    /* If we can pair, then devices will see each other well within the long timeout
       Use double the delay on one of them */
    if (peer_pair_le_bdaddr_greater(&ppl->scanned_devices[0].addr,&ppl->local_addr))
    {
        /*! \todo Need an approach if RRA is the same as ours. 
            Best not detected here */
        delay += appConfigPeerPairLeTimeoutPeerSelect();
    }

    DEBUG_LOG("peer_pair_le_enter_selecting. Delay %d ms", delay);

    /*! \todo Want to randomise settings somewhat. Otherwise crossovers will be VERY likely.
        That will include the timeout from first scan detected, as well as intervals used. */
    MessageSendLater(PeerPairLeGetTask(), PEER_PAIR_LE_TIMEOUT_FROM_FIRST_SCAN, NULL, 
                     delay);
}


static void peer_pair_le_enter_pending_local_addr(void)
{
    DEBUG_LOG("peer_pair_le_enter_pending_local_addr");

    ConnectionReadLocalAddr(PeerPairLeGetTask());
}


static void peer_pair_le_enter_idle(void)
{
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();

    DEBUG_LOG("peer_pair_le_enter_idle");

    /* Probably only ever enter this state with a find pending */
    if (ppl->find_pending)
    {
        MessageSend(PeerPairLeGetTask(), PEER_PAIR_LE_INTERNAL_FIND_PEER, NULL);
    }
}


static void peer_pair_le_enter_connecting(void)
{
    typed_bdaddr tb;
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();

    DEBUG_LOG("peer_pair_le_enter_connecting");
    
    tb.type = TYPED_BDADDR_PUBLIC;
    tb.addr = ppl->scanned_devices[0].addr;

    GattManagerConnectAsCentral(PeerPairLeGetTask(),
                                &tb,
                                gatt_connection_ble_master_directed,
                                TRUE);
}


static void peer_pair_le_enter_negotiate_p_role(void)
{
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();
    gatt_uuid_t uuid[] = UUID_128_FORMAT_gatt_uuid_t(UUID128_ROOT_KEY_SERVICE);

    DEBUG_LOG("peer_pair_le_enter_negotiate_p_role");

    /*! \todo Ask the client to initialise itself, so we don't have to know about UUID */
    GattDiscoverPrimaryServiceRequest(PeerPairLeGetTask(), ppl->gatt_cid, 
                                      gatt_uuid128, uuid);
}


static void peer_pair_le_enter_negotiate_c_role(void)
{
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();
    const GRKS_KEY_T *secret = &peer_pair_le_key;

    DEBUG_LOG("peer_pair_le_enter_negotiate_c_role");

    GattRootKeyServerReadyForChallenge(&PeerPairLeGetRootKeyServer(),
                                        secret,
                                        &ppl->local_addr, &ppl->peer);
}


static void peer_pair_le_enter_completed(void)
{
    DEBUG_LOG("peer_pair_le_enter_completed");

    MessageSend(PeerPairLeGetTask(), PEER_PAIR_LE_INTERNAL_COMPLETED, NULL);
}


static void peer_pair_le_exit_completed(void)
{
    DEBUG_LOG("peer_pair_le_exit_completed");

    peer_pair_le_disconnect();
}

static void peer_pair_le_enter_completed_wait_for_disconnect(void)
{
    DEBUG_LOG("peer_pair_le_enter_completed_wait_for_disconnect");

    MessageSendLater(PeerPairLeGetTask(), PEER_PAIR_LE_INTERNAL_COMPLETED, NULL,
                     appConfigPeerPairLeTimeoutServerCompleteDisconnect());
}

static void peer_pair_le_exit_completed_wait_for_disconnect(void)
{
    DEBUG_LOG("peer_pair_le_exit_completed_wait_for_disconnect");

    MessageCancelAll(PeerPairLeGetTask(), PEER_PAIR_LE_INTERNAL_COMPLETED);
}


static void peer_pair_le_enter_initialsed(void)
{
    DEBUG_LOG("peer_pair_le_enter_initialsed");

    peer_pair_le_stop_service();
}


static void peer_pair_le_enter_pairing_as_server(void)
{
    DEBUG_LOG("peer_pair_le_enter_pairing_as_server");
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();
    tp_bdaddr tb;

    PanicFalse(VmGetBdAddrtFromCid(ppl->gatt_cid, &tb));

    ConnectionDmBleSecurityReq(PeerPairLeGetTask(), &tb.taddr, ble_security_authenticated_bonded, ble_connection_slave_directed);
}


static void peer_pair_le_enter_pairing_as_client(void)
{
    DEBUG_LOG("peer_pair_le_enter_pairing_as_client");
}

void peer_pair_le_set_state(PEER_PAIR_LE_STATE new_state)
{
    PEER_PAIR_LE_STATE old_state = peer_pair_le_get_state();

    if (old_state == new_state)
    {
        DEBUG_LOG("peer_pair_le_set_state. Attempt to transition to same state:%d",
                   old_state);
        Panic();    /*! \todo Remove panic once implementation stable */
        return;
    }

    DEBUG_LOG("peer_pair_le_set_state. Transition %d->%d",
                old_state, new_state);

    /* Pattern is to run functions for exiting state first */
    switch (old_state)
    {
        case PEER_PAIR_LE_STATE_DISCOVERY:
        case PEER_PAIR_LE_STATE_SELECTING:
            peer_pair_le_exit_discovery(new_state);
            break;

        case PEER_PAIR_LE_STATE_COMPLETED_WAIT_FOR_DISCONNECT:
            peer_pair_le_exit_completed_wait_for_disconnect();
            break;

        case PEER_PAIR_LE_STATE_COMPLETED:
            peer_pair_le_exit_completed();
            break;

        default:
            break;
    }

    peer_pair_le.state = new_state;

    switch (new_state)
    {
        case PEER_PAIR_LE_STATE_INITIALISED:
            peer_pair_le_enter_initialsed();
            break;

        case PEER_PAIR_LE_STATE_PENDING_LOCAL_ADDR:
            peer_pair_le_enter_pending_local_addr();
            break;

        case PEER_PAIR_LE_STATE_IDLE:
            peer_pair_le_enter_idle();
            break;

        case PEER_PAIR_LE_STATE_DISCOVERY:
            peer_pair_le_enter_discovery(old_state);
            break;

        case PEER_PAIR_LE_STATE_SELECTING:
            peer_pair_le_enter_selecting();
            break;

        case PEER_PAIR_LE_STATE_CONNECTING:
            peer_pair_le_enter_connecting();
            break;

        case PEER_PAIR_LE_STATE_PAIRING_AS_SERVER:
            peer_pair_le_enter_pairing_as_server();
            break;

        case PEER_PAIR_LE_STATE_PAIRING_AS_CLIENT:
            peer_pair_le_enter_pairing_as_client();
            break;

        case PEER_PAIR_LE_STATE_NEGOTIATE_P_ROLE:
            peer_pair_le_enter_negotiate_p_role();
            break;

        case PEER_PAIR_LE_STATE_NEGOTIATE_C_ROLE:
            peer_pair_le_enter_negotiate_c_role();
            break;

        case PEER_PAIR_LE_STATE_COMPLETED_WAIT_FOR_DISCONNECT:
            peer_pair_le_enter_completed_wait_for_disconnect();
            break;

        case PEER_PAIR_LE_STATE_COMPLETED:
            peer_pair_le_enter_completed();
            break;

        default:
            break;
    }
}
