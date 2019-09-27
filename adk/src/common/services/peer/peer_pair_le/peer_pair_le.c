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
#include <gatt_handler.h>
#include <gatt_root_key_server_uuids.h>
#include <uuid.h>

#include "peer_pair_le.h"
#include "peer_pair_le_init.h"
#include "peer_pair_le_private.h"
#include "pairing.h"


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

/*! \brief Check the advert contains the correct UUID.
    \todo Remove this when the firmware correctly supports filtering
*/
static bool peerPairLe_UuidMatches(const CL_DM_BLE_ADVERTISING_REPORT_IND_T *scan)
{
    uint8 uuid[] = {UUID_128_FORMAT_uint8(UUID128_ROOT_KEY_SERVICE)};
    return ((scan->size_advertising_data >= 22) &&
            (0 == memcmp(uuid, scan->advertising_data + 6, sizeof(uuid))));


    /*! \todo Will only want random - but public for now */
}

void PeerPairLe_HandleFoundDeviceScan(const CL_DM_BLE_ADVERTISING_REPORT_IND_T *scan)
{
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();
    peerPairLeFoundDevice temp;
    PEER_PAIR_LE_STATE state = peer_pair_le_get_state();

    if (   PEER_PAIR_LE_STATE_DISCOVERY != state
        && PEER_PAIR_LE_STATE_SELECTING != state)
    {
        DEBUG_LOG("peerPairLehandleFoundDeviceScan. Advert in unexpected state:%d. No:%d rssi:%d bdaddr %04X:%02X:%06lX",
                        state,
                        scan->num_reports, scan->rssi, 
                        scan->current_taddr.addr.nap,scan->current_taddr.addr.uap,scan->current_taddr.addr.lap);

        return;
    }
    
    /* Eliminate scan results that we are not interested in */
    if (   TYPED_BDADDR_PUBLIC != scan->current_taddr.type
        || 0 == scan->num_reports)
    {
        return;
    }

    if (!peerPairLe_UuidMatches(scan))
    {
        return;
    }

    if (scan->rssi < appConfigPeerPairLeMinRssi())
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


bool PeerPairLe_HandleConnectionLibraryMessages(MessageId id, Message message,
                                              bool already_handled)
{
    UNUSED(already_handled);

    bool handled = FALSE;

    if (   PEER_PAIR_LE_STATE_INITIALISED != peer_pair_le_get_state()
        && PeerPairLeGetData())
    {
        switch (id)
        {
            case CL_DM_BLE_SET_SCAN_PARAMETERS_CFM:
                DEBUG_LOG("PeerPairLeHandleConnectionLibraryMessages. CL_DM_BLE_SET_SCAN_PARAMETERS_CFM");
                break;

            case CL_SM_AUTH_REPLACE_IRK_CFM:
                {
                    const CL_SM_AUTH_REPLACE_IRK_CFM_T *csaric = (const CL_SM_AUTH_REPLACE_IRK_CFM_T *)message;

                    DEBUG_LOG("PeerPairLeHandleConnectionLibraryMessages. CL_SM_AUTH_REPLACE_IRK_CFM. Sts:%d 0x%04x",csaric->status,csaric->bd_addr.lap);
                    handled = TRUE;
                }
                break;

            default:
                break;
        }
    }
    return handled;
}

