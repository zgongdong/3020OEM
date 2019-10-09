/*!
\copyright  Copyright (c) 2015 - 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       gatt_handler.c
\brief      Application support for GATT, GATT Server and GAP Server
*/

#include "gatt_handler.h"
#include "local_name.h"
#include "gatt_handler_db.h"
#include "earbud_log.h"
#include "earbud_config.h"
#include "earbud_init.h"
#include "earbud_sm.h"

#include "earbud_test_le.h"

#include <bdaddr.h>
#include <gatt.h>
#include <gatt_manager.h>
#include <gatt_gap_server.h>
#include <panic.h>
#include <app\bluestack\l2cap_prim.h>

/*!< App GATT component task */
gattTaskData    app_gatt;

/*! Earbud GATT database, for the required GATT and GAP servers. */
extern const uint16 gattDatabase[];


static void appGattHandleGattGapReadDeviceNameInd(const GATT_GAP_SERVER_READ_DEVICE_NAME_IND_T *ind)
{
    uint8 *name = LocalName_GetPrefixedName();
    uint16 namelen = strlen((char *)name);

    /* The indication can request a portion of our name by specifying the start offset */
    if (ind->name_offset)
    {
        /* Check that we haven't been asked for an entry off the end of the name */
        
        if (ind->name_offset >= namelen)
        {
            namelen = 0;
            name = NULL;
        /*  \todo return gatt_status_invalid_offset  */
        }
        else
        {
            namelen -= ind->name_offset;
            name += ind->name_offset;
        /*  \todo return gatt_status_success  */
        }
    }

    PanicFalse(GattGetGapServer() == ind->gap_server);

    GattGapServerReadDeviceNameResponse(GattGetGapServer(), ind->cid,
                                        namelen, name);
}

static void appGattMessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);

    DEBUG_LOGF("appGattMessageHandler id:%d 0x%x", id, id);

    switch (id)
    {
        /******************** GAP MESSAGES ******************/
        case GATT_GAP_SERVER_READ_DEVICE_NAME_IND:
            appGattHandleGattGapReadDeviceNameInd((const GATT_GAP_SERVER_READ_DEVICE_NAME_IND_T*)message);
            break;


        default:
            DEBUG_LOGF("appGattMessageHandler. Unhandled message id:0x%x", id);
            break;
    }
}


bool GattHandlerInit(Task init_task)
{
    gattTaskData *gatt = GattGetTaskData();

    if (!GattManagerRegisterConstDB(&gattDatabase[0], GattGetDatabaseSize()/sizeof(uint16)))
    {
        DEBUG_LOG("appGattInit. Failed to register GATT database");
        Panic();
    }
    
    memset(gatt,0,sizeof(*gatt));

    /* setup the GATT tasks */
    gatt->gatt_task.handler = appGattMessageHandler;

    if (GattGapServerInit(GattGetGapServer(), GattGetGapTask(),
                                    HANDLE_GAP_SERVICE, HANDLE_GAP_SERVICE_END)
                        != gatt_gap_server_status_success)
    {
        DEBUG_LOG("APP:GATT: GAP server init failed");
        Panic();
    }

    appInitSetInitTask(init_task);
    
    return TRUE;
}


bool appGattGetPublicAddrFromCid(uint16 cid, bdaddr *public_addr)
{
    tp_bdaddr client_tpaddr;
    tp_bdaddr public_tpaddr;
    bool addr_found = FALSE;
    
    if (VmGetBdAddrtFromCid(cid, &client_tpaddr))
    {
        if (client_tpaddr.taddr.type == TYPED_BDADDR_RANDOM)
        {
            VmGetPublicAddress(&client_tpaddr, &public_tpaddr);
        }
        else
        {
            memcpy(&public_tpaddr, &client_tpaddr, sizeof(tp_bdaddr));
        }
        if (!BdaddrIsZero(&public_tpaddr.taddr.addr))
        {
            *public_addr = public_tpaddr.taddr.addr;
            addr_found = TRUE;
        }
    }
    
    return addr_found;
}
