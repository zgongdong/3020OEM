/*******************************************************************************
Copyright (c) 2018 - 2019 Qualcomm Technologies International, Ltd.

FILE NAME
    sink_gatt_server_ancs_proxy.h

DESCRIPTION
    Routines to handle messages sent from the GATT ANCS_PROXY Server Task.
*/

#ifndef _SINK_GATT_SERVER_ANCS_PROXY_H_
#define _SINK_GATT_SERVER_ANCS_PROXY_H_

#include <csrtypes.h>
#include <message.h>


bool GattServerAncsProxy_Init(Task init_task);

#endif /* _SINK_GATT_SERVER_ANCS_PROXY_H_ */
