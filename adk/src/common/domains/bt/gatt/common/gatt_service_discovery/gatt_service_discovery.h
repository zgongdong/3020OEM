/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
All Rights Reserved.
Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for Gatt Service Discovery
*/

#ifndef GATT_SERVICE_DISCOVERY_H
#define GATT_SERVICE_DISCOVERY_H

#include "gatt_client.h"

/*! Enumerated type for service discovery status */
typedef enum
{
    gsd_uninitialised,   /*! Uninitialised state */
    gsd_in_idle,         /*! Idle State - Ready to start dicovering */
    gsd_in_progress,     /*! Service discovery in progress */
    gsd_complete_failure /*! Service discovery failed */
}service_discovery_status;

/*! Function to initialise the GATT service discovery.

    \param prioritised list which defines the order in which the
           clients will be processed.

    \return TRUE if initialised successfully, FALSE otherwise.
*/
bool GattServiceDiscovery_Init(const client_id_t* gatt_client_prioritised_id[],uint8 num_elements);

/*! Function to initiate the GATT service procedure.

    \param connection id in which the discovery procedure has to be started.

    \return TRUE if service discovery started successfully, FALSE otherwise.
*/
bool GattServiceDiscovery_StartDiscovery(uint16 cid);

/*! Function to remove the clients.

    \param cid Connection in which the Service discovery should start.

    \return TRUE if all the clients destroyed successfully, FALSE otherwise.
*/
bool GattServiceDiscovery_DestroyClients(uint16 cid);

/*! Function to get the GATT Service discovery component status.

    \param none.

    \return Gatt service discovery component's present status.
*/
service_discovery_status GattServiceDiscovery_GetStatus(void);

/*! Function to handle the GATT discovery messages.

    \param task    Task to which GATT discovery messages needs to be posted.
    \param id      GATT Primitive message identifiers.
    \param message GATT Service discovery message from lower layers.

*/
void GattServiceDisovery_HandleMessage(Task task, MessageId id, Message message);

#endif
