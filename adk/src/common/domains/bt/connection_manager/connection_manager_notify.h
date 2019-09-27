/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       connection_manager_notify.h
\brief      Header file for Connection Manager Notify
*/

#ifndef __CON_MANAGER_NOTIFY_H
#define __CON_MANAGER_NOTIFY_H

#include <connection_manager.h>

/*! \brief Notify observers of connection/disconnection
    \param tpaddr The Transport Bluetooth Address of the connection
    \param connected TRUE if connected, FALSE if disconnected
    \param reason Reason for disconnection if disconnected
*/
void conManagerNotifyObservers(const tp_bdaddr *tpaddr, bool connected, hci_status reason);

/*! \brief Initialise notify */
void conManagerNotifyInit(void);

#endif
