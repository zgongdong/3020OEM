/*!
\copyright  Copyright (c) 2015 - 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       gatt_handler.h
\brief      Header file for GATT, GATT Server and GAP Server
*/

#ifndef GATT_HANDLER_H_
#define GATT_HANDLER_H_

#include "le_advertising_manager.h"

#include <gatt_gap_server.h>

#define APP_GATT_SERVER_INSTANCES   (1)

/*! Structure holding information for the gatt task */
typedef struct
{
    /*! Task for handling messaging from GATT Manager */
    TaskData    gatt_task;
    /*! GAP server information. Required for using BLE */
    GGAPS       gap_server;
} gattTaskData;

/*!< App GATT component task */
extern gattTaskData    app_gatt;

/*! Get pointer to the GATT modules task data */
#define GattGetTaskData()                (&app_gatt)

/*! Get pointer to the GATT modules task */
#define GattGetTask()                    (&app_gatt.gatt_task)

/*! Get pointer to the GAP server task */
#define GattGetGapTask()                 (&app_gatt.gatt_task)

/*! Get pointer to instance of the GAP server info  */
#define GattGetGapServer()               (&app_gatt.gap_server)


/*! @brief Initialise GATT handler

    \param init_task    Not used

    \returns TRUE
*/
bool GattHandlerInit(Task init_task);


/*! @brief Finds the public address from a GATT connection ID.

    \param cid          The connection ID
    \param public_addr  The returned public address that was found from the connection ID.

    \returns TRUE if public address if could be found from the connection ID,
    otherwise FALSE.
*/
bool appGattGetPublicAddrFromCid(uint16 cid, bdaddr *public_addr);


#endif /* GATT_HANDLER_H_ */
