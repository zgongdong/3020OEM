/*!
\copyright  Copyright (c) 2005 - 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Component handling synchronisation of keys between peers.
*/

#include "key_sync.h"
#include "key_sync_private.h"
#include "key_sync_marshal_defs.h"

#include <peer_signalling.h>
#include <bt_device.h>

#include <logging.h>
#include <connection.h>

#include "device_properties.h"
#include <device_db_serialiser.h>

#include <device_list.h>

#include <panic.h>
#include <stdlib.h>
#include <stdio.h>

#define MAKE_KEY_SYNC_MESSAGE(TYPE) TYPE##_T *message = PanicUnlessNew(TYPE##_T);

/*! \brief Key Sync task data. */
key_sync_task_data_t key_sync;

/*! \brief Handle confirmation of transmission of a marshalled message. */
static void keySync_HandleMarshalledMsgChannelTxCfm(const PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM_T* cfm)
{
    DEBUG_LOG("keySync_HandleMarshalledMsgChannelTxCfm channel %u status %u", cfm->channel, cfm->status);
    /* free the memory for the marshalled message now confirmed sent */
    free(cfm->msg_ptr);
}

/*! \brief Handle incoming marshalled messages from peer key sync component. */
static void keySync_HandleMarshalledMsgChannelRxInd(PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND_T* ind)
{
    switch (ind->type)
    {
        case MARSHAL_TYPE_key_sync_req_t:
        {
            key_sync_req_t* req = (key_sync_req_t*)ind->msg;

            DEBUG_LOG("keySync_HandleMarshalledMsgChannelRxInd rx key to add");
            /* update local PDL, note the API uses 16-bit words */
            ConnectionSmAddAuthDevice(keySync_GetTask(), &req->bd_addr,
                                      req->bits.trusted,
                                      TRUE,
                                      req->link_key_type,
                                      req->size_link_key/2,
                                      (uint16*)req->link_key);
            free(req);
        }
        break;

        case MARSHAL_TYPE_key_sync_cfm_t:
        {
            key_sync_cfm_t* cfm = (key_sync_cfm_t*)ind->msg;
            DEBUG_LOG("keySync_HandleMarshalledMsgChannelRxInd synced %u", cfm->synced);
            /* mark key as synchronised */
            if (!cfm->synced)
            {
                DEBUG_LOG("keySync_HandleMarshalledMsgChannelRxInd sync failure reported by peer");
            }
            else
            {
                if (!appDeviceSetHandsetAddressForwardReq(&cfm->bd_addr, FALSE))
                {
                    DEBUG_LOG("keySync_HandleMarshalledMsgChannelRxInd FAILED TO CLEAR ADDRESS FWD REQD!");
                }
                else
                {
                    DEBUG_LOG("keySync_HandleMarshalledMsgChannelRxInd cleared ADDRESS FWD REQD");
                }
            }
            free(cfm);
        }
        break;

        default:
        break;
    }
}

static void keySync_HandleClSmGetAuthDeviceConfirm(CL_SM_GET_AUTH_DEVICE_CFM_T* cfm)
{
    bdaddr peer_addr;

    DEBUG_LOG("keySync_HandleClSmGetAuthDeviceConfirm %u size %u", cfm->status, cfm->size_link_key);

    if ((cfm->status == success) &&
        appDeviceGetPeerBdAddr(&peer_addr))
    {
        /* link key size is specified in 16-bit words, so 8 */
        key_sync_req_t* req = PanicUnlessMalloc(sizeof(key_sync_req_t) + ((cfm->size_link_key*2)-1));

        req->bd_addr = cfm->bd_addr;
        req->bits.trusted = cfm->trusted;
        req->link_key_type = cfm->link_key_type;
        req->size_link_key = cfm->size_link_key*2;
        memcpy(req->link_key, cfm->link_key, cfm->size_link_key*2);

        /* send to counterpart on other earbud */
        appPeerSigMarshalledMsgChannelTx(keySync_GetTask(),
                                         PEER_SIG_MSG_CHANNEL_KEY_SYNC,
                                         req, MARSHAL_TYPE_key_sync_req_t);
    }
    else
    {
        DEBUG_LOG("keySync_HandleClSmGetAuthDeviceConfirm no peer to send to");
    }
}

static void keySync_AddDeviceAttributes(bdaddr* bd_addr, uint16 tws_version, uint16 flags)
{
    device_t handset_device = BtDevice_GetDeviceCreateIfNew(bd_addr, DEVICE_TYPE_HANDSET);

    flags |= DEVICE_FLAGS_JUST_PAIRED;
    Device_SetPropertyU16(handset_device, device_property_tws_version, tws_version);
    Device_SetPropertyU16(handset_device, device_property_flags, flags);
    Device_SetPropertyU8(handset_device, device_property_a2dp_volume, A2dpProfile_GetDefaultVolume());
    Device_SetPropertyU8(handset_device, device_property_hfp_profile, hfp_handsfree_107_profile);
}

static void keySync_HandleClSmAddAuthDeviceConfirm(CL_SM_ADD_AUTH_DEVICE_CFM_T* cfm)
{
    bdaddr peer_addr;

    DEBUG_LOG("keySync_HandleClSmAddAuthDeviceConfirm %u", cfm->status);

    if (appDeviceGetPeerBdAddr(&peer_addr))
    {
        key_sync_cfm_t* key_cfm = PanicUnlessMalloc(sizeof(key_sync_cfm_t));
        key_cfm->bd_addr = cfm->bd_addr;
        key_cfm->synced = (cfm->status == success) ? TRUE : FALSE;

        /* send confirmation key received and stored back to counterpart */
        appPeerSigMarshalledMsgChannelTx(keySync_GetTask(),
                                         PEER_SIG_MSG_CHANNEL_KEY_SYNC,
                                         key_cfm, MARSHAL_TYPE_key_sync_cfm_t);

        keySync_AddDeviceAttributes(&cfm->bd_addr, DEVICE_TWS_STANDARD, DEVICE_FLAGS_PRE_PAIRED_HANDSET);
        BtDevice_PrintAllDevices();
    }
    else
    {
        DEBUG_LOG("keySync_HandleClSmAddAuthDeviceConfirm no peer to send to");
    }
}

/*! Key Sync Message Handler. */
static void keySync_HandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);
    
    switch (id)
    {
            /* marshalled messaging */
        case PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND:
            keySync_HandleMarshalledMsgChannelRxInd((PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND_T*)message);
            break;
        case PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM:
            keySync_HandleMarshalledMsgChannelTxCfm((PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM_T*)message);
            break;

        case CL_SM_GET_AUTH_DEVICE_CFM:
            keySync_HandleClSmGetAuthDeviceConfirm((CL_SM_GET_AUTH_DEVICE_CFM_T*)message);
            break;
        case CL_SM_ADD_AUTH_DEVICE_CFM:
            keySync_HandleClSmAddAuthDeviceConfirm((CL_SM_ADD_AUTH_DEVICE_CFM_T*)message);
            break;

        default:
            break;
    }
}

bool KeySync_Init(Task init_task)
{
    key_sync_task_data_t *ks = keySync_GetTaskData();

    UNUSED(init_task);

    DEBUG_LOG("KeySync_Init");

    /* Initialise component task data */
    memset(ks, 0, sizeof(*ks));
    ks->task.handler = keySync_HandleMessage;

    /* Register with peer signalling to use the key sync msg channel */
    appPeerSigMarshalledMsgChannelTaskRegister(keySync_GetTask(), 
                                               PEER_SIG_MSG_CHANNEL_KEY_SYNC,
                                               key_sync_marshal_type_descriptors,
                                               NUMBER_OF_MARSHAL_OBJECT_TYPES);
    return TRUE;
}

static void keySync_FindHandsetsAndCheckForSync(device_t device, void *data)
{
    uint16 tws_version = DEVICE_TWS_UNKNOWN;
    uint16 flags = 0;

    UNUSED(data);

    if (BtDevice_GetDeviceType(device) != DEVICE_TYPE_HANDSET)
    {
        return;
    }

    Device_GetPropertyU16(device, device_property_tws_version, &tws_version);
    if (tws_version != DEVICE_TWS_STANDARD)
    {
        return;
    }

    Device_GetPropertyU16(device, device_property_flags, &flags);
    if (flags & DEVICE_FLAGS_HANDSET_ADDRESS_FORWARD_REQD)
    {
        bdaddr handset = DeviceProperties_GetBdAddr(device);

        DEBUG_LOG("KeySync_Sync found key to sync 0x%04x", handset.lap);
        ConnectionSmGetAuthDevice(keySync_GetTask(), &handset);
    }
}

void KeySync_Sync(void)
{
    DeviceList_Iterate(keySync_FindHandsetsAndCheckForSync, NULL);
}
