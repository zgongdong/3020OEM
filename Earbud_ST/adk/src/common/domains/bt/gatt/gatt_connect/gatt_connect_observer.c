/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Application support for observice GATT connect/disconnect
*/

#include "gatt_connect.h"
#include "gatt_connect_observer.h"

#include <panic.h>

#define MAX_NUMBER_GATT_CONNECT_OBSERVERS   5

typedef struct
{
    gatt_connect_observer_callback_t callbacks[MAX_NUMBER_GATT_CONNECT_OBSERVERS];
    unsigned number_registered;
} gatt_connect_observer_registry_t;

static gatt_connect_observer_registry_t gatt_connect_observer_registry = {0};

void GattConnect_RegisterObserver(const gatt_connect_observer_callback_t *callback)
{
    if(gatt_connect_observer_registry.number_registered < MAX_NUMBER_GATT_CONNECT_OBSERVERS)
    {
        /* The register requires that all callback functions have been supplied */
        PanicNull((void*)callback);
        PanicNull((void*)callback->OnConnection);
        PanicNull((void*)callback->OnDisconnection);
        
        gatt_connect_observer_registry.callbacks[gatt_connect_observer_registry.number_registered].OnConnection = callback->OnConnection;
        gatt_connect_observer_registry.callbacks[gatt_connect_observer_registry.number_registered].OnDisconnection = callback->OnDisconnection;
        gatt_connect_observer_registry.number_registered++;
    }
    else
    {
        /*  No space to store a new observer, as this is a fixed number at compile time.
            Cause a Panic so that more space can be reserved for observers. 
        */
        Panic();
    }
}

void GattConnect_ObserverNotifyOnConnection(uint16 cid)
{
    uint8 index = 0;

    while (index < gatt_connect_observer_registry.number_registered)
    {
        gatt_connect_observer_callback_t * callback = &gatt_connect_observer_registry.callbacks[index];

        callback->OnConnection(cid);
        
        index++;
    }
}

void GattConnect_ObserverNotifyOnDisconnection(uint16 cid)
{
    uint8 index = 0;

    while (index < gatt_connect_observer_registry.number_registered)
    {
        gatt_connect_observer_callback_t * callback = &gatt_connect_observer_registry.callbacks[index];

        callback->OnDisconnection(cid);

        index++;
    }
}

void GattConnect_ObserverInit(void)
{
    memset(&gatt_connect_observer_registry, 0, sizeof(gatt_connect_observer_registry));
    
}