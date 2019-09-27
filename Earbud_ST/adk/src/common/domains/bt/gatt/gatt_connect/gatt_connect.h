/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\defgroup   gatt_connect
\ingroup    gatt
\file
\brief      Header file for GATT connect

The gatt_connect module is used to notify any interested modules when GATT
is connected or disconnected. Modules can also express a preferred GATT MTU to 
exchange after connection and can get the actual MTU for a given connection ID
*/

#ifndef GATT_CONNECT_H_
#define GATT_CONNECT_H_

#include "domain_message.h"

#define GATT_CONNECT_MTU_INVALID 0

#define GATT_CONNECT_MAX_CONNECTIONS 2

/*! Messages sent by the gatt_connect module */
typedef enum 
{
    GATT_CONNECT_SERVER_INIT_COMPLETE_CFM = APP_GATT_MESSAGE_BASE  /*!< GattConnect_ServerInitComplete confirmation */
} gatt_connect_message_t;

/*! Callback structure used when an observer registers with the GATT connect module.

A callback is used here (instead of TaskList), to ensure that the connection indications
are delivered before any server access messages.
All callback functions MUST be supplied when an observer registers. 
It is assumed an observer will need to know about connections and disconnections.
 */
typedef struct
{
    void (*OnConnection)(uint16 cid);
    void (*OnDisconnection)(uint16 cid);
} gatt_connect_observer_callback_t;

/*! \brief Initialise the gatt_connect module.

    \param init_task Unused
    \return TRUE if init was successful, otherwise FALSE
 */
bool GattConnect_Init(Task init_task);

/*! \brief Notify gatt_connect all servers have been initialised

    \param init_task Task to receive GATT_CONNECT_SERVER_INIT_COMPLETE_CFM
    \return TRUE if init was successful, otherwise FALSE
 */
bool GattConnect_ServerInitComplete(Task init_task);

/*! \brief Update the minimum acceptable MTU
    Multiple calls to this function will update the local MTU
    to be the MAX from all calls and the DEFAULT_MTU
    
    \param mtu The minimum acceptable MTU for the caller
 */
void GattConnect_UpdateMinAcceptableMtu(unsigned mtu);

/*! \brief Get the MTU for a GATT connection
    
    \param cid The connection ID for which to get the MTU
    
    \return The MTU if cid is connected, otherwise GATT_CONNECT_MTU_INVALID
 */
unsigned GattConnect_GetMtu(unsigned cid);


/*! @brief Register an observer with that gatt_connect module.

    \param callback     Callback funtions to register
*/
void GattConnect_RegisterObserver(const gatt_connect_observer_callback_t *callback);

#endif 
