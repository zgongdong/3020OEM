/*!
\copyright  Copyright (c) 2017 - 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       gaia_handler.h
\brief      Header file for handling the GAIA transport interface
*/

#ifndef GAIA_HANDLER_H_
#define GAIA_HANDLER_H_

#ifdef INCLUDE_DFU


#include <gaia.h>
#include <task_list.h>

#include "domain_message.h"


/*! Minor version number used for the GAIA interface */
#define GAIA_API_MINOR_VERSION 5

/*! Messages that are sent by the gaia_handler module */
typedef enum {
    APP_GAIA_INIT_CFM = AV_GAIA_MESSAGE_BASE,   /*!< Application GAIA module has initialised */
    APP_GAIA_CONNECTED,                         /*!< A GAIA connection has been made */
    APP_GAIA_DISCONNECTED,                      /*!< A GAIA connection has been lost */
    APP_GAIA_UPGRADE_CONNECTED,                 /*!< An upgrade protocol has connected through GAIA */
    APP_GAIA_UPGRADE_DISCONNECTED,              /*!< An upgrade protocol has disconnected through GAIA */
    APP_GAIA_UPGRADE_ACTIVITY,                  /*!< The GAIA module has seen some upgrade activity */
} av_headet_gaia_messages;


/*! Data used by the GAIA module */
typedef struct
{
        /*! Task for handling messaging from upgrade library */
    TaskData        gaia_task;
        /*! The current transport (if any) for GAIA */
    GAIA_TRANSPORT *transport;
        /*! Whether a GAIA connection is allowed, or will be rejected immediately */
    bool            connections_allowed;
        /*! List of tasks to notify of GAIA activity. */
    task_list_t    *client_list;

} gaiaTaskData;

/*!< Task information for GAIA support */
extern gaiaTaskData    app_gaia;

/*! Get the info for the applications Gaia support */
#define GaiaGetTaskData()                (&app_gaia)

/*! Get the Task info for the applications Gaia task */
#define GaiaGetTask()            (&app_gaia.gaia_task)

/*! Get the transport for the current GAIA connection */
#define GaiaGetTransport()           (GaiaGetTaskData()->transport)

/*! Set the transport for the current GAIA connection */
#define GaiaSetTransport(_transport)  do { \
                                            GaiaGetTaskData()->transport = (_transport);\
                                           } while(0)

/*! Initialise the GAIA Module */
extern bool appGaiaInit(Task init_task);

/*! Add a client to the GAIA module

    Messages from #av_headet_gaia_messages will be sent to any task
    registered through this API

    \param task Task to register as a client
 */
extern void appGaiaClientRegister(Task task);


/*! \brief Disconnect any active gaia connection
 */
extern void appGaiaDisconnect(void);


/*! \brief Let GAIA know whether to allow any connections

    \param  allow A new gaia connection is allowed
 */
extern void appGaiaAllowNewConnections(bool allow);


/*! \brief Initialise the GATT Gaia Server.

    \param  init_task    Task to send init completion message to
 */
extern bool appGaiaGattServerInit(Task init_task);


#endif /* INCLUDE_DFU */
#endif /* GAIA_HANDLER_H_ */
