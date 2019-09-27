/*!
\copyright  Copyright (c) 2008 - 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       earbud_init.h
\brief      Header file for initialisation module
*/

#ifndef EARBUD_INIT_H
#define EARBUD_INIT_H

#include "init.h"
#include <connection.h>

#include <connection_no_ble.h>

/*! Value used to reduce the chance that a random RAM value might affect a test
    system reading the initialised flag */
#define APP_INIT_COMPLETED_MAGIC (0x2D)


 /*! \brief Initialisation module data */
typedef struct
{
    /*!< Init's local task */
    TaskData task;
    uint8 initialised;      /*!< Flag used to indicate that the full initialisation has completed */
#ifdef USE_BDADDR_FOR_LEFT_RIGHT
    uint8 appInitIsLeft:1;  /*!< Set if this is the left earbud */
#endif
    Task init_task; /*!< The stored init module Task. Set via #appInitSetInitTask. */
} initData;

/*!< Structure used while initialising */
extern initData    app_init;

/*! Get pointer to init data structure */
#define InitGetTaskData()        (&app_init)

/*! \brief Initialise init module
    This function is the start of all initialisation.

    When the initialisation sequence has completed #INIT_CFM will
    be sent to the main app Task.
*/
void appInit(void);

/*! \brief Set the Task to send initialisation complete messages to.

    A utility function to give somewhere for app modules to store the init_task
    passed into the module initialisation function.

    Only modules with asynchronous initialisation sequences may need to use this
    function.

    Note: Only the init_task passed into the module initialisation function
          should be stored here.

    \param init_task The init module task.
*/
void appInitSetInitTask(Task init_task);

/*! \brief Get the Task to send initialisation complete messages to.

    Returns the Task set by the last call to #appInitSetInitTask.

    \return Task the init module task to send or forward the initialistion
                 complete message to.
*/
Task appInitGetInitTask(void);


/*! Let the application initialisation function know that initialisation is complete

    This should be called on receipt of the #APPS_COMMON_INIT_CFM message.
 */
void appInitSetInitialised(void);


/*! Query if the initialisation module has completed all initialisation

    \returns TRUE if all the init tasks have completed
 */
#define appInitCompleted()  (InitGetTaskData()->initialised == APP_INIT_COMPLETED_MAGIC)

#endif /* EARBUD_INIT_H */
