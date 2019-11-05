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
#include <panic.h>
#include <app\bluestack\l2cap_prim.h>

/*!< App GATT component task */
gattTaskData    app_gatt;

/*! Earbud GATT database, for the required GATT and GAP servers. */
extern const uint16 gattDatabase[];


bool GattHandlerInit(Task init_task)
{
    gattTaskData *gatt = GattGetTaskData();

    if (!GattManagerRegisterConstDB(&gattDatabase[0], GattGetDatabaseSize()/sizeof(uint16)))
    {
        DEBUG_LOG("appGattInit. Failed to register GATT database");
        Panic();
    }
    
    memset(gatt,0,sizeof(*gatt));

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

uint16 GattHandler_GetGattStartHandle(gatt_server_t gatt_server)
{
    uint16 start_handle = 0;
    switch(gatt_server)
    {
        case gatt_server_gaa:
#ifdef INCLUDE_GAA_COMM_SERVER
            start_handle = HANDLE_GAA_COMM_SERVICE;
#endif
            break;

        default:
            Panic();
            break;
    }

    return start_handle;
}

uint16 GattHandler_GetGattEndHandle(gatt_server_t gatt_server)
{
    uint16 end_handle = 0;
    switch(gatt_server)
    {
        case gatt_server_gaa:
#ifdef INCLUDE_GAA_COMM_SERVER
            end_handle = HANDLE_GAA_COMM_SERVICE_END;
#endif
            break;

        default:
            Panic();
            break;
    }

    return end_handle;
}
