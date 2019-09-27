/*******************************************************************************
Copyright (c) 2018 - 2019 Qualcomm Technologies International, Ltd.

FILE NAME
    sink_gatt_server_ams_proxy.h

DESCRIPTION
    Routines to handle messages sent from the GATT AMS Proxy Server Task.
    
NOTES
*/

#ifndef _SINK_GATT_SERVER_AMS_PROXY_H_
#define _SINK_GATT_SERVER_AMS_PROXY_H_

#include <csrtypes.h>
#include <message.h>

bool GattServerAmsProxy_Init(Task init_task);

#endif /* _SINK_GATT_SERVER_AMS_PROXY_H_ */
