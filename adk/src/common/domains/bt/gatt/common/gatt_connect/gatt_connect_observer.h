/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for GATT connect observer
*/

#ifndef GATT_CONNECT_OBSERVER_H_
#define GATT_CONNECT_OBSERVER_H_

/*! @brief Called when a GATT connection occurs.

    \param cid          The GATT connection ID

*/
void GattConnect_ObserverNotifyOnConnection(uint16 cid);

/*! @brief Called when a GATT disconnection occurs.

    \param cid          The GATT connection ID

*/
void GattConnect_ObserverNotifyOnDisconnection(uint16 cid);

/*! @brief Initialise gatt_connect_observer

*/
void GattConnect_ObserverInit(void);

#endif 
