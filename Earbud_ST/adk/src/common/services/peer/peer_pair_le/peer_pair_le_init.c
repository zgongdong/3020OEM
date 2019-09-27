/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Initialisation functions for the Peer Pairing over LE service
*/

#include <bdaddr.h>
#include <connection.h>
#include <logging.h>
#include <panic.h>

#include <gatt.h>
#include <gatt_manager.h>
#include <gatt_handler.h>
#include <gatt_handler_db.h>

#include <le_advertising_manager.h>
#include <connection_manager.h>
#include <gatt_root_key_client.h>
#include <gatt_root_key_server.h>
#include <bt_device.h>

#include "device_properties.h"
#include <device_db_serialiser.h>

#include <device_list.h>

#include <earbud_config.h>
#include <earbud_init.h>

#include "peer_pair_le.h"
#include "peer_pair_le_private.h"
#include "peer_pair_le_sm.h"
#include "peer_pair_le_init.h"
#include "peer_pair_le_key.h"


static void peer_pair_le_send_pair_confirm(peer_pair_le_status_t status)
{
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();
    MAKE_PEER_PAIR_LE_MESSAGE(PEER_PAIR_LE_PAIR_CFM);

    message->status = status;

    /*! \todo Not a task, but a pointer */
    /* Delay the message until we uninitialise the module
       Cast the data pointer to a Task... as there isn't another conditional
       API based on a pointer. */
    MessageSendConditionallyOnTask(ppl->client, PEER_PAIR_LE_PAIR_CFM, message,
                                   (Task*)&PeerPairLeGetData());
}


static void peer_pair_le_handle_local_bdaddr(const CL_DM_LOCAL_BD_ADDR_CFM_T *cfm)
{
    if (cfm->status != hci_success)
    {
        DEBUG_LOG("Failed to read local BDADDR");
        Panic();
    }
    PeerPairLeGetData()->local_addr = cfm->bd_addr;

    peer_pair_le_set_state(PEER_PAIR_LE_STATE_IDLE);
}


static void peer_pair_le_handle_find_peer(void)
{
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();

    switch (peer_pair_le_get_state())
    {
        case PEER_PAIR_LE_STATE_PENDING_LOCAL_ADDR:
            ppl->find_pending = TRUE;
            return;

        case PEER_PAIR_LE_STATE_IDLE:
            break;

        default:
            DEBUG_LOG("peer_pair_le_handle_find_peer called in bad state: %d",
                        peer_pair_le_get_state());
            /*! \todo Remove panic and send message to client once code better */
            Panic();
            break;
    }

    ppl->find_pending = FALSE;
    peer_pair_le_set_state(PEER_PAIR_LE_STATE_DISCOVERY);
}


static void peer_pair_le_handle_completed(void)
{
    peer_pair_le_set_state(PEER_PAIR_LE_STATE_DISCONNECTING);
}


static void peer_pair_le_handle_scan_timeout(void)
{
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();

    if (PEER_PAIR_LE_STATE_SELECTING != peer_pair_le_get_state())
    {
        return;
    }
    if (BdaddrIsZero(&ppl->scanned_devices[0].addr))
    {
        return;
    }

    /* Eliminate the scan result if RSSI too close */
    if (!BdaddrIsZero(&ppl->scanned_devices[1].addr))
    {
        int difference = ppl->scanned_devices[0].rssi - ppl->scanned_devices[1].rssi;
        if (difference < appConfigPeerPairLeMinRssiDelta())
        {
            peer_pair_le_set_state(PEER_PAIR_LE_STATE_DISCOVERY);
        }
    }
    peer_pair_le_set_state(PEER_PAIR_LE_STATE_CONNECTING);
}


static void peer_pair_le_handle_advert_set_data_cfm(const APP_ADVMGR_ADVERT_SET_DATA_CFM_T *message)
{
    UNUSED(message);

    DEBUG_LOG("peer_pair_le_handle_advert_set_data_cfm");
    GattManagerWaitForRemoteClient(PeerPairLeGetTask(), NULL, gatt_connection_ble_slave_undirected);
}


static void peer_pair_le_handle_con_manager_connection(const CON_MANAGER_CONNECTION_IND_T *conn)
{
    PEER_PAIR_LE_STATE state;
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();

    if (!conn->ble || conn->connected || !BdaddrIsSame(&conn->bd_addr, &ppl->peer))
        return;

    state = peer_pair_le_get_state();

    switch (state)
    {
        case PEER_PAIR_LE_STATE_COMPLETED_WAIT_FOR_DISCONNECT:
        case PEER_PAIR_LE_STATE_DISCONNECTING:
            DEBUG_LOG("peer_pair_le_handle_con_manager_connection. Peer BLE Link disconnected");
            peer_pair_le_set_state(PEER_PAIR_LE_STATE_INITIALISED);
        break;

        default:
            DEBUG_LOG("peer_pair_le_handle_con_manager_connection. IGNORED. State:%d Connected:%d",
                       state, conn->connected);
        break;
    }
}


static void peer_pair_le_handle_remote_client_connect_cfm(const GATT_MANAGER_REMOTE_CLIENT_CONNECT_CFM_T *cfm)
{
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();

    DEBUG_LOG("peer_pair_le_handle_remote_client_connect_cfm. sts:%d", cfm->status);

    if (gatt_status_success == cfm->status)
    {
        ppl->peer = cfm->taddr.addr;
        ppl->gatt_cid = cfm->cid;
        peer_pair_le_set_state(PEER_PAIR_LE_STATE_PAIRING_AS_SERVER);
    }
    else
    {
        /*! \todo restart advertising */
    }

}


static void peer_pair_le_handle_remote_server_connect_cfm(const GATT_MANAGER_REMOTE_SERVER_CONNECT_CFM_T *cfm)
{
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();

    DEBUG_LOG("peer_pair_le_handle_remote_server_connect_cfm. sts:%d", cfm->status);

    if (gatt_status_success == cfm->status)
    {
        ppl->peer = cfm->taddr.addr;
        ppl->gatt_cid = cfm->cid;
        peer_pair_le_set_state(PEER_PAIR_LE_STATE_PAIRING_AS_CLIENT);
    }
    else if (gatt_status_failure == cfm->status)
    {
        /* Get a generic failure code if failed to connect */
        peer_pair_le_set_state(PEER_PAIR_LE_STATE_DISCOVERY);
    }
    else 
    {
        DEBUG_LOG("peer_pair_le_handle_remote_server_connect_cfm. Unknown sts:%d", cfm->status);
        peer_pair_le_set_state(PEER_PAIR_LE_STATE_DISCOVERY);
    }
}


static void peer_pair_le_handle_primary_service_discovery(const GATT_DISCOVER_PRIMARY_SERVICE_CFM_T* discovery)
{
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();

    DEBUG_LOG("peer_pair_le_handle_primary_service_discovery. sts:%d more:%d",
                discovery->status, discovery->more_to_come);

    if (gatt_status_success == discovery->status)
    {
        GattRootKeyClientInit(&ppl->root_key_client, 
                              PeerPairLeGetTask(), 
                              discovery->cid, discovery->handle, discovery->end);
    }
}


static void peer_pair_le_handle_client_init(const GATT_ROOT_KEY_CLIENT_INIT_CFM_T *init)
{
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();
    const GRKS_KEY_T *secret = &peer_pair_le_key;

    DEBUG_LOG("peer_pair_le_handle_client_init. sts:%d", init->status);

    if (gatt_root_key_client_status_success == init->status)
    {
        GattRootKeyClientChallengePeer(&ppl->root_key_client, 
                        secret, &ppl->local_addr, 
                        &ppl->peer);
    }
    else
    {
        /*! \todo Handle challenge failure */
    }

}


static void peer_pair_le_convert_key_to_grks_key(GRKS_KEY_T *dest_key, const uint16 *src)
{
    uint8 *dest = dest_key->key;
    int octets = GRKS_KEY_SIZE_128BIT_OCTETS;

    while (octets > 1)
    {
        *dest++ = *src & 0xFF;
        *dest++ = ((*src++)>>8) & 0xFF;
        octets -= 2;
    }
}


static void peer_pair_le_convert_grks_key_to_key(uint16 *dest, const GRKS_KEY_T *src_key)
{
    const uint8 *src = src_key->key;
    int words = GRKS_KEY_SIZE_128BIT_WORDS;
    uint16 word;

    while (words--)
    {
        word = *src++;
        word += (*src++) << 8;
        *dest++ = word;
    }
}


static void peer_pair_le_get_root_keys(GRKS_KEY_T *ir, GRKS_KEY_T *er)
{
    cl_root_keys    root_keys;
    PanicFalse(ConnectionReadRootKeys(&root_keys));

    peer_pair_le_convert_key_to_grks_key(ir, root_keys.ir);
    peer_pair_le_convert_key_to_grks_key(er, root_keys.er);
}


static void peer_pair_le_handle_client_challenge_ind(const GATT_ROOT_KEY_CLIENT_CHALLENGE_PEER_IND_T *challenge)
{
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();

    DEBUG_LOG("peer_pair_le_handle_client_challenge_ind. sts:%d", challenge->status);

    if (gatt_root_key_challenge_status_success == challenge->status)
    {
        GRKS_IR_KEY_T ir;
        GRKS_IR_KEY_T er;

        /* Service API defined as IR & ER so convert internally */
        peer_pair_le_get_root_keys(&ir, &er);

        GattRootKeyClientWriteKeyPeer(&ppl->root_key_client, &ir, &er);
    }

    /*! \todo Handle challenge failure */
}


static void peer_pair_le_add_device_entry(const bdaddr *address, deviceType type, unsigned flags)
{
    device_t device = BtDevice_GetDeviceCreateIfNew(address, type);

    Device_SetPropertyU16(device, device_property_tws_version, DEVICE_TWS_STANDARD); /*! \todo Set correct TWS version */
    Device_SetPropertyU16(device, device_property_flags, flags);
    Device_SetPropertyU16(device, device_property_sco_fwd_features, 0x0);
}


static void peer_pair_le_add_paired_device_entries(unsigned local_flag, unsigned remote_flag)
{
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();

    DEBUG_LOG("peer_pair_le_add_paired_device_entries. Me: %04x Primary:%d C_Role:%d",
                    ppl->local_addr.lap, 
                    !!(local_flag & DEVICE_FLAGS_SHADOWING_C_ROLE),
                    !!(local_flag & DEVICE_FLAGS_PRIMARY_ADDR));

    peer_pair_le_add_device_entry(&ppl->local_addr, DEVICE_TYPE_SELF, 
                                  DEVICE_FLAGS_SHADOWING_ME | local_flag);

    DEBUG_LOG("peer_pair_le_add_paired_device_entries. Peer: %04x Primary:%d C_Role:%d",
                    ppl->peer.lap, 
                    !!(remote_flag & DEVICE_FLAGS_SHADOWING_C_ROLE),
                    !!(remote_flag & DEVICE_FLAGS_PRIMARY_ADDR));

    peer_pair_le_add_device_entry(&ppl->peer, DEVICE_TYPE_EARBUD, 
                                  DEVICE_FLAGS_JUST_PAIRED | remote_flag);

    /* Now the peer has paired, update the PDL with its persistent device data. This is in order to ensure
       we don't lose peer attributes in the case of unexpected power loss. N.b. Normally serialisation occurs
       during a controlled shutdown of the App. */
    DeviceDbSerialiser_Serialise();
    BtDevice_PrintAllDevices();

}

static void peer_pair_le_clone_peer_keys(void)
{
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();

    DEBUG_LOG("peer_pair_le_clone_peer_keys");

    /*! \todo Is Panic too severe. Bear in mind this is early days peer pairing */
    PanicFalse(ConnectionTrustedDeviceListDuplicate(&ppl->peer, &ppl->local_addr));
}


static void peer_pair_le_handle_client_keys_written(const GATT_ROOT_KEY_CLIENT_WRITE_KEY_IND_T *written)
{
    peer_pair_le_status_t status = peer_pair_le_status_success;

    DEBUG_LOG("peer_pair_le_handle_client_keys_written. sts:%d", written->status);

    if (gatt_root_key_client_write_keys_success != written->status)
    {
        status = peer_pair_le_status_failed;
        peer_pair_le_send_pair_confirm(status);
        peer_pair_le_set_state(PEER_PAIR_LE_STATE_COMPLETED);
    }
    else
    {
        cl_irk irk;

        /* We have updated the keys on our peer, so the IRK we have for them is wrong */
        ConnectionSmGetLocalIrk(&irk);
        ConnectionReplaceIrk(PeerPairLeGetTask(), &PeerPairLeGetData()->peer, &irk);

        peer_pair_le_clone_peer_keys();

        peer_pair_le_add_paired_device_entries(DEVICE_FLAGS_SECONDARY_ADDR, DEVICE_FLAGS_SHADOWING_C_ROLE |
                                                                            DEVICE_FLAGS_PRIMARY_ADDR);
        peer_pair_le_send_pair_confirm(status);
        peer_pair_le_set_state(PEER_PAIR_LE_STATE_COMPLETED);
    }

}


static void peer_pair_le_handle_server_challenge_ind(const GATT_ROOT_KEY_SERVER_CHALLENGE_IND_T *challenge)
{
    DEBUG_LOG("peer_pair_le_handle_server_challenge_ind. sts:%d", challenge->status);

    if (gatt_root_key_challenge_status_success != challenge->status)
    {
        /*! \todo Handle challenge failure */
    }
}


static void peer_pair_le_handle_server_keys_written(const GATT_ROOT_KEY_SERVER_KEY_UPDATE_IND_T *written)
{
    cl_root_keys root_keys;
    cl_irk irk;

    DEBUG_LOG("peer_pair_le_handle_server_keys_written");

    DEBUG_LOG("IR: %02x %02x %02x %02x %02x",written->ir.key[0],written->ir.key[1],written->ir.key[2],
                                   written->ir.key[3],written->ir.key[4]);
    DEBUG_LOG("ER: %02x %02x %02x %02x %02x",written->er.key[0],written->er.key[1],written->er.key[2],
                                   written->er.key[3],written->er.key[4]);

    peer_pair_le_convert_grks_key_to_key(root_keys.ir, &written->ir);
    peer_pair_le_convert_grks_key_to_key(root_keys.er, &written->er);
    PanicFalse(ConnectionSetRootKeys(&root_keys));

    ConnectionSmGetLocalIrk(&irk);
    DEBUG_LOG("Local IRK :%04x %04x %04x %04x",irk.irk[0],irk.irk[1],irk.irk[2],irk.irk[3]);

    peer_pair_le_clone_peer_keys();

    peer_pair_le_send_pair_confirm(peer_pair_le_status_success);
    peer_pair_le_add_paired_device_entries(DEVICE_FLAGS_SHADOWING_C_ROLE | DEVICE_FLAGS_PRIMARY_ADDR,
                                           DEVICE_FLAGS_SECONDARY_ADDR);

    peer_pair_le_set_state(PEER_PAIR_LE_STATE_COMPLETED_WAIT_FOR_DISCONNECT);
}


static void peer_pair_le_handler(Task task, MessageId id, Message message)
{
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();
    UNUSED(task);

    if (!ppl)
    {
        /* We are not initialised. Discard message */
        return;
    }

    switch (id)
    {
        /* ---- Internal messages ---- */
        case PEER_PAIR_LE_INTERNAL_FIND_PEER:
            peer_pair_le_handle_find_peer();
            break;

        case PEER_PAIR_LE_INTERNAL_COMPLETED:
            peer_pair_le_handle_completed();
            break;

        /* ---- Timeouts ---- */
        case PEER_PAIR_LE_TIMEOUT_FROM_FIRST_SCAN :
            peer_pair_le_handle_scan_timeout();
            break;

        /* ---- connection library responses (directed to this task) ---- */
        case CL_DM_LOCAL_BD_ADDR_CFM:
            peer_pair_le_handle_local_bdaddr((const CL_DM_LOCAL_BD_ADDR_CFM_T *)message);
            break;

        /* ---- Advertising Manager messages ---- */
        case APP_ADVMGR_ADVERT_SET_DATA_CFM:
            peer_pair_le_handle_advert_set_data_cfm((const APP_ADVMGR_ADVERT_SET_DATA_CFM_T *)message);
            break;

        /* ---- Connection Manager messages ---- */
        case CON_MANAGER_CONNECTION_IND:
            peer_pair_le_handle_con_manager_connection((const CON_MANAGER_CONNECTION_IND_T *)message);
            break;

        /* ---- GATT Manager messages ---- */
        case GATT_MANAGER_REMOTE_CLIENT_CONNECT_CFM:
            peer_pair_le_handle_remote_client_connect_cfm((const GATT_MANAGER_REMOTE_CLIENT_CONNECT_CFM_T *)message);
            break;

        case GATT_MANAGER_REMOTE_SERVER_CONNECT_CFM:
            peer_pair_le_handle_remote_server_connect_cfm((const GATT_MANAGER_REMOTE_SERVER_CONNECT_CFM_T *)message);
            break;

        case GATT_MANAGER_CANCEL_REMOTE_CLIENT_CONNECT_CFM:
            /* Don't really care about status, but we requested this - so report */
            DEBUG_LOG("peer_pair_le_handler. GATT_MANAGER_CANCEL_REMOTE_CLIENT_CONNECT_CFM sts:%d",
                        ((GATT_MANAGER_CANCEL_REMOTE_CLIENT_CONNECT_CFM_T*)message)->status);
            break;

        /* ---- GATT messages ---- */
        case GATT_DISCOVER_PRIMARY_SERVICE_CFM:
            peer_pair_le_handle_primary_service_discovery((const GATT_DISCOVER_PRIMARY_SERVICE_CFM_T* )message);
            break;

        case GATT_ROOT_KEY_CLIENT_INIT_CFM:
            peer_pair_le_handle_client_init((const GATT_ROOT_KEY_CLIENT_INIT_CFM_T *)message);
            break;

        case GATT_ROOT_KEY_CLIENT_CHALLENGE_PEER_IND:
            peer_pair_le_handle_client_challenge_ind((const GATT_ROOT_KEY_CLIENT_CHALLENGE_PEER_IND_T *)message);
            break;

        case GATT_ROOT_KEY_CLIENT_WRITE_KEY_IND:
            peer_pair_le_handle_client_keys_written((const GATT_ROOT_KEY_CLIENT_WRITE_KEY_IND_T *)message);
            break;

        case GATT_ROOT_KEY_SERVER_CHALLENGE_IND:
            peer_pair_le_handle_server_challenge_ind((const GATT_ROOT_KEY_SERVER_CHALLENGE_IND_T *)message);
            break;

        case GATT_ROOT_KEY_SERVER_KEY_UPDATE_IND:
            peer_pair_le_handle_server_keys_written((GATT_ROOT_KEY_SERVER_KEY_UPDATE_IND_T *)message);
            break;

        default:
            DEBUG_LOG("peer_pair_le_handler. Unhandled message %d(0x%x)", id, id);
            break;
    }
} 


void peer_pair_le_init(void)
{
    if (PEER_PAIR_LE_STATE_UNINITIALISED == peer_pair_le_get_state())
    {
        gatt_root_key_server_init_params_t prms = {TRUE,GATT_ROOT_KEY_SERVICE_FEATURES_DEFAULT};
        
        PeerPairLeGetTask()->handler = peer_pair_le_handler;
        
        if (!GattRootKeyServerInit(&PeerPairLeGetRootKeyServer(),
                        PeerPairLeGetTask(),&prms,
                        HANDLE_ROOT_TRANSFER_SERVICE, HANDLE_ROOT_TRANSFER_SERVICE_END))
        {
            DEBUG_LOG("peer_pair_le_init. Server init failed");
            Panic();
        }
    
        ConManagerRequestDefaultQos(cm_transport_ble, cm_qos_low_latency);
    
        peer_pair_le_set_state(PEER_PAIR_LE_STATE_INITIALISED);
    }
}

void peer_pair_le_start_service(void)
{
    if (PEER_PAIR_LE_STATE_INITIALISED == peer_pair_le_get_state())
    {
        peer_pair_le.data = (peerPairLeRunTimeData*)PanicNull(calloc(1, sizeof(peerPairLeRunTimeData)));

        ConManagerRegisterConnectionsClient(PeerPairLeGetTask());

        peer_pair_le_set_state(PEER_PAIR_LE_STATE_PENDING_LOCAL_ADDR);
    }
}


void peer_pair_le_disconnect(void)
{
    peerPairLeRunTimeData *data = PeerPairLeGetData();

    if (data)
    {
        uint16 cid = data->gatt_cid;
        data->gatt_cid = 0;

        if (cid)
        {
            DEBUG_LOG("peer_pair_le_disconnect. Disconnecting cid:%d", cid);
            GattManagerDisconnectRequest(cid);
            return;
        }
    }
    peer_pair_le_set_state(PEER_PAIR_LE_STATE_INITIALISED);
}


void peer_pair_le_stop_service(void)
{
    peerPairLeRunTimeData *data = PeerPairLeGetData();

    if (data)
    {
        peer_pair_le.data = NULL;
        free(data);
    }
}