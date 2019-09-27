/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Miscellaneous functions for the PEER PAIRING OVER LE service
*/

#include <gatt.h>

#include <bdaddr.h>
#include <panic.h>
#include <logging.h>
#include <gatt_manager.h>
#include <gatt_handler.h>

#include "peer_pair_le.h"
#include "peer_pair_le_init.h"
#include "peer_pair_le_private.h"


/* Data local to the peer pairing module */
peerPairLeTaskData peer_pair_le = {0};


static bool peerPairLe_updateScannedDevice(const CL_DM_BLE_ADVERTISING_REPORT_IND_T *scan,
                                          peerPairLeFoundDevice *device)
{
    bool is_existing_device = BdaddrIsSame(&device->addr, &scan->current_taddr.addr);

    if (is_existing_device)
    {
        if (scan->rssi > device->rssi)
        {
            device->rssi= scan->rssi;
        }
    }
    return is_existing_device;
}


static bool peerPairLe_orderTwoScans(peerPairLeFoundDevice *first, peerPairLeFoundDevice *second)
{
    /* If uninitialised, then the bdaddr will be zero. 
       Checking rssi would require rssi initialisation to be other than 0 */
    if (BdaddrIsZero(&first->addr))
    {
        *first = *second;
        memset(second, 0, sizeof(*second));
        return TRUE;
    }
    else if (second->rssi > first->rssi)
    {
        peerPairLeFoundDevice temp;
        
        temp = *first;
        *first = *second;
        *second = temp;
        return TRUE;
    }
    return FALSE;
}

static void peerPairLe_orderScannedDevices(void)
{
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();

    peerPairLe_orderTwoScans(&ppl->scanned_devices[0], &ppl->scanned_devices[1]);
}

static bool peerPairLe_updateScannedDevices(const CL_DM_BLE_ADVERTISING_REPORT_IND_T *scan)
{
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();

    if (!peerPairLe_updateScannedDevice(scan, &ppl->scanned_devices[0]))
    {
        if (!peerPairLe_updateScannedDevice(scan, &ppl->scanned_devices[1]))
        {
            return FALSE;
        }
        peerPairLe_orderScannedDevices();
    }
    return TRUE;
}


static void peerPairLe_handleFoundDeviceScan(const CL_DM_BLE_ADVERTISING_REPORT_IND_T *scan)
{
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();
    peerPairLeFoundDevice temp;
    PEER_PAIR_LE_STATE state = peer_pair_le_get_state();

    DEBUG_LOG("peerPairLehandleFoundDeviceScan. Advert: No:%d rssi:%d bdaddr %04X:%02X:%06lX",
                    scan->num_reports, scan->rssi, 
                    scan->current_taddr.addr.nap,scan->current_taddr.addr.uap,scan->current_taddr.addr.lap);

    if (   PEER_PAIR_LE_STATE_DISCOVERY != state
        && PEER_PAIR_LE_STATE_SELECTING != state)
    {
        DEBUG_LOG("peerPairLehandleFoundDeviceScan. Advert in unexpected state:%d. No:%d rssi:%d bdaddr %04X:%02X:%06lX",
                        state,
                        scan->num_reports, scan->rssi, 
                        scan->current_taddr.addr.nap,scan->current_taddr.addr.uap,scan->current_taddr.addr.lap);

        return;
    }

    /*! \todo Will only want random - but public for now */
    /* Eliminate scan results that we are not interested in */
    if (   TYPED_BDADDR_PUBLIC != scan->current_taddr.type
        || 0 == scan->num_reports
        || scan->rssi < appConfigPeerPairLeMinRssi())
    {
        return;
    }

    /* See if it a fresh scan for an existing device */
    if (peerPairLe_updateScannedDevices(scan))
    {
        return;
    }

    temp.addr = scan->current_taddr.addr;
    temp.rssi = scan->rssi;

    if (peerPairLe_orderTwoScans(&ppl->scanned_devices[1],&temp))
    {
        peerPairLe_orderScannedDevices();
    }

    if (PEER_PAIR_LE_STATE_DISCOVERY == peer_pair_le_get_state())
    {
        peer_pair_le_set_state(PEER_PAIR_LE_STATE_SELECTING);
    }
}


bool PeerPairLe_Init(Task init_task)
{
    UNUSED(init_task);
    
    peer_pair_le_init();
    
    return TRUE;
}

void PeerPairLe_FindPeer(Task task)
{
    peer_pair_le_start_service();
    PeerPairLeSetClient(task);

    MessageSend(PeerPairLeGetTask(), PEER_PAIR_LE_INTERNAL_FIND_PEER, NULL);
}


static void peerPairLe_handleSmIoCapabilityReqInd(const CL_SM_IO_CAPABILITY_REQ_IND_T *iocap_req_ind)
{
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();
    bool accept = BdaddrIsSame(&ppl->peer, &iocap_req_ind->tpaddr.taddr.addr);

    DEBUG_LOG("peerPairLe_handleSmIoCapabilityReqInd");

    uint16 key_dist = (KEY_DIST_RESPONDER_ENC_CENTRAL |
                       KEY_DIST_RESPONDER_ID |
                       KEY_DIST_INITIATOR_ENC_CENTRAL |
                       KEY_DIST_INITIATOR_ID);

    if (!accept)
    {
        DEBUG_LOG("peerPairLe_handleSmIoCapabilityReqInd. CL_SM_IO_CAPABILITY_REQ_IND - addr mismatch. %x vs %x expected",
                        iocap_req_ind->tpaddr.taddr.addr.lap,
                        ppl->peer.lap);
    }
    ConnectionSmIoCapabilityResponse(&iocap_req_ind->tpaddr,
                                     accept ? cl_sm_io_cap_no_input_no_output : cl_sm_reject_request,
                                     mitm_not_required,
                                     TRUE,
                                     accept ? key_dist : KEY_DIST_NONE,
                                     oob_data_none,
                                     0,
                                     0);
}


static void peerPairLe_handleSmRemoteIoCapabilityInd(const CL_SM_REMOTE_IO_CAPABILITY_IND_T *iocap_ind)
{
    DEBUG_LOG("peerPairLe_handleSmRemoteIoCapabilityInd %lx auth:%x io:%x oib:%x", 
                                    iocap_ind->tpaddr.taddr.addr.lap,
                                    iocap_ind->authentication_requirements,
                                    iocap_ind->io_capability,
                                    iocap_ind->oob_data_present);
}


static void peerPairLe_handleSmAuthenticateCfm(const CL_SM_AUTHENTICATE_CFM_T *authenticate_cfm)
{
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();
    bool accept = BdaddrIsSame(&ppl->peer, &authenticate_cfm->bd_addr);

    if (!accept)
    {
        DEBUG_LOG("peerPairLe_handleSmAuthenticateCfm - addr mismatch. %x vs %x expected",
                        authenticate_cfm->bd_addr.lap,
                        ppl->peer.lap);
    }

    DEBUG_LOG("peerPairLe_handleSmAuthenticateCfm - sts:%d x%04x", authenticate_cfm->status, 
                                                                   authenticate_cfm->bd_addr.lap);
}


static void peerPairLe_handleSmEncryptionChangeInd(const CL_SM_ENCRYPTION_CHANGE_IND_T *encrypt_ind)
{
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();
    bool accept = BdaddrIsSame(&ppl->peer, &encrypt_ind->tpaddr.taddr.addr);

    if (!accept)
    {
        DEBUG_LOG("peerPairLe_handleSmEncryptionChangeInd - addr mismatch. %x vs %x expected",
                        encrypt_ind->tpaddr.taddr.addr.lap,
                        ppl->peer.lap);
    }

    DEBUG_LOG("peerPairLe_handleSmEncryptionChangeInd - Encrypted:%d", encrypt_ind->encrypted);
}


static void peerPairLe_handleSmBleSimplePairingCompleteInd(const CL_SM_BLE_SIMPLE_PAIRING_COMPLETE_IND_T *pair_complete_ind)
{
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();
    bool accept = BdaddrIsSame(&ppl->peer, &pair_complete_ind->tpaddr.taddr.addr);

    if (!accept)
    {
        DEBUG_LOG("peerPairLe_handleSmBleSimplePairingCompleteInd - addr mismatch. %x vs %x expected",
                        pair_complete_ind->tpaddr.taddr.addr.lap,
                        ppl->peer.lap);
        Panic();
    }
    DEBUG_LOG("peerPairLe_handleSmBleSimplePairingCompleteInd. Sts:%d",
                pair_complete_ind->status);

    if (success == pair_complete_ind->status)
    {
        switch (peer_pair_le_get_state())
        {
            case PEER_PAIR_LE_STATE_PAIRING_AS_CLIENT:
                peer_pair_le_set_state(PEER_PAIR_LE_STATE_NEGOTIATE_C_ROLE);
                break;

            case PEER_PAIR_LE_STATE_PAIRING_AS_SERVER:
                peer_pair_le_set_state(PEER_PAIR_LE_STATE_NEGOTIATE_P_ROLE);
                break;

            default:
                DEBUG_LOG("peerPairLe_handleSmBleSimplePairingCompleteInd. success in unexpected state:%d",
                            peer_pair_le_get_state());
                Panic();
                break;
        }
    }
}


static void peerPairLe_handleSmGetAuthDeviceCfm(const CL_SM_GET_AUTH_DEVICE_CFM_T *get_device_cfm)
{
    DEBUG_LOG("peerPairLe_handleSmGetAuthDeviceCfm. sts:%d Key[type:%d, size:%d] x%04x",
                                get_device_cfm->status, 
                                get_device_cfm->link_key_type, get_device_cfm->size_link_key, 
                                get_device_cfm->bd_addr.lap);
    
    ConnectionSmAddAuthDevice(PeerPairLeGetTask(), &PeerPairLeGetData()->local_addr , TRUE, TRUE/*?*/, 
                                get_device_cfm->link_key_type, 
                                get_device_cfm->size_link_key, get_device_cfm->link_key);
}


static void peerPairLe_handleSmBleLinkSecurityInd(const CL_SM_BLE_LINK_SECURITY_IND_T *ble_security_ind)
{
    DEBUG_LOG("peerPairLe_handleSmBleLinkSecurityInd. Link security(sc?):%d x%04x",
                        ble_security_ind->le_link_sc, ble_security_ind->bd_addr.lap);
}


static void peerPairLe_handleDmBleSecurityCfm(const CL_DM_BLE_SECURITY_CFM_T *ble_security_cfm)
{
    DEBUG_LOG("peerPairLe_handleDmBleSecurityCfm: CL_DM_BLE_SECURITY_CFM. sts:%d x%4x",
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


/*! \todo The entire function can be deleted once pairing is delegated to 
          the pairing module */
static bool peerPairLe_handlePairingRelated(MessageId id, Message message,
                                              bool already_handled)
{
    if (already_handled)
    {
        return FALSE;
    }

    if (!peer_pair_le_in_pairing_state())
    {
        DEBUG_LOG("peerPairLe_handlePairingRelated Not in a pairing state:%d", peer_pair_le_get_state());
        return FALSE;
    }

    switch (id)
    {
    case CL_SM_IO_CAPABILITY_REQ_IND:
        peerPairLe_handleSmIoCapabilityReqInd((const CL_SM_IO_CAPABILITY_REQ_IND_T*) message);
        break;

    case CL_SM_REMOTE_IO_CAPABILITY_IND:
        peerPairLe_handleSmRemoteIoCapabilityInd((const CL_SM_REMOTE_IO_CAPABILITY_IND_T *)message);
        break;

    case CL_SM_AUTHENTICATE_CFM:
        peerPairLe_handleSmAuthenticateCfm((const CL_SM_AUTHENTICATE_CFM_T *)message);
        break;

    case CL_SM_ENCRYPTION_CHANGE_IND:
        peerPairLe_handleSmEncryptionChangeInd((const CL_SM_ENCRYPTION_CHANGE_IND_T *)message);
        break;

    case CL_SM_BLE_SIMPLE_PAIRING_COMPLETE_IND:
        peerPairLe_handleSmBleSimplePairingCompleteInd((const CL_SM_BLE_SIMPLE_PAIRING_COMPLETE_IND_T*)message);
        break;

    case CL_SM_GET_AUTH_DEVICE_CFM:
        peerPairLe_handleSmGetAuthDeviceCfm((const CL_SM_GET_AUTH_DEVICE_CFM_T *)message);
        break;

    case CL_SM_BLE_LINK_SECURITY_IND:
        peerPairLe_handleSmBleLinkSecurityInd((const CL_SM_BLE_LINK_SECURITY_IND_T *)message);
        break;

    case CL_DM_BLE_SECURITY_CFM:
        peerPairLe_handleDmBleSecurityCfm((const CL_DM_BLE_SECURITY_CFM_T *)message);
        break;

    default:
        return FALSE;
    }
    return TRUE;
}


bool PeerPairLe_HandleConnectionLibraryMessages(MessageId id, Message message,
                                              bool already_handled)
{
    UNUSED(already_handled);

    if (   PEER_PAIR_LE_STATE_INITIALISED != peer_pair_le_get_state()
        && PeerPairLeGetData())
    {
        switch (id)
        {
            case CL_DM_BLE_ADVERTISING_REPORT_IND:
                peerPairLe_handleFoundDeviceScan((const CL_DM_BLE_ADVERTISING_REPORT_IND_T *)message);
                return TRUE;

            case CL_DM_BLE_SET_SCAN_PARAMETERS_CFM:
                DEBUG_LOG("PeerPairLeHandleConnectionLibraryMessages. CL_DM_BLE_SET_SCAN_PARAMETERS_CFM");
                break;

            case CL_SM_AUTH_REPLACE_IRK_CFM:
                {
                    const CL_SM_AUTH_REPLACE_IRK_CFM_T *csaric = (const CL_SM_AUTH_REPLACE_IRK_CFM_T *)message;

                    DEBUG_LOG("PeerPairLeHandleConnectionLibraryMessages. CL_SM_AUTH_REPLACE_IRK_CFM. Sts:%d 0x%04x",csaric->status,csaric->bd_addr.lap);
                    return TRUE;
                }

            case CL_SM_IO_CAPABILITY_REQ_IND:
            case CL_SM_REMOTE_IO_CAPABILITY_IND:
            case CL_SM_AUTHENTICATE_CFM:
            case CL_SM_ENCRYPTION_CHANGE_IND:
            case CL_SM_BLE_SIMPLE_PAIRING_COMPLETE_IND:
            case CL_DM_BLE_SECURITY_CFM:
            case CL_SM_BLE_LINK_SECURITY_IND:
            case CL_SM_GET_AUTH_DEVICE_CFM:
                return peerPairLe_handlePairingRelated(id, message, already_handled);

            default:
                break;
        }
    }

    return FALSE;
}

