/*!
\copyright  Copyright (c) 2017 - 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       device_upgrade.h
\brief      Header file for the earbud upgrade management.
*/

#ifndef DEVICE_UPGRADE_H_
#define DEVICE_UPGRADE_H_

#ifdef INCLUDE_DFU

#include <upgrade.h>
#include <task_list.h>

#include <domain_message.h>

/*! Defines the upgrade client task list initial capacity */
#define THE_UPGRADE_CLIENT_LIST_INIT_CAPACITY 1

/*! Messages that are sent by the device_upgrade module */
typedef enum {
                                            /*! Message sent after the device has restarted. This
                                                indicates that an upgrade has nearly completed
                                                and upgrade mode is needed to allow the
                                                upgrade to be confirmed */
    APP_UPGRADE_REQUESTED_TO_CONFIRM = AV_UPGRADE_MESSAGE_BASE,
    APP_UPGRADE_REQUESTED_IN_PROGRESS,      /*!< Message sent after the device has restarted. This
                                                    indicates that an upgrade is in progress
                                                    and has been interrupted */
    APP_UPGRADE_ACTIVITY,                   /*!< The upgrade module has seen some upgrade activity */
    APP_UPGRADE_STARTED,                    /*!< An upgrade is now in progress. Started or continued. */
    APP_UPGRADE_COMPLETED,                  /*!< An upgrade has been completed */
} av_headet_upgrade_messages;

/*! Types of upgrade context used with appUpgradeSetContext and appUpgradeGetContext */
typedef enum {
    APP_UPGRADE_CONTEXT_UNUSED = 0,
    APP_UPGRADE_CONTEXT_GAIA,
    APP_UPGRADE_CONTEXT_BISTO_OTA
} app_upgrade_context_t;

/*! Structure holding data for the Upgrade module */
typedef struct
{
        /*! Task for handling messaging from upgrade library */
    TaskData        upgrade_task;
        /*! List of tasks to notify of UPGRADE activity. */
    TASK_LIST_WITH_INITIAL_CAPACITY(THE_UPGRADE_CLIENT_LIST_INIT_CAPACITY) client_list;
} upgradeTaskData;

/*!< Task information for UPGRADE support */
extern upgradeTaskData app_upgrade;

/*! Get the info for the applications upgrade support */
#define UpgradeGetTaskData()     (&app_upgrade)

/*! Get the Task info for the applications Upgrade task */
#define UpgradeGetTask()         (&app_upgrade.upgrade_task)

/*! Get the client list for the applications Upgrade task */
#define UpgradeGetClientList()         (task_list_flexible_t *)(&app_upgrade.client_list)


bool appUpgradeInit(Task init_task);


/*! Allow upgrades to be started

    The library used for firmware upgrades will always allow connections.
    However, it is possible to stop upgrades from beginning or completing.

    \param allow    allow or disallow upgrades

    \return TRUE if the request has taken effect. This setting can not be
        changed if an upgrade is in progress in which case the
        function will return FALSE.
 */
extern bool appUpgradeAllowUpgrades(bool allow);


/*! Handler for system messages. All of which are sent to the application.

    This function is called to handle any system messages that this module
    is interested in. If a message is processed then the function returns TRUE.

    \param  id              Identifier of the system message
    \param  message         The message content (if any)
    \param  already_handled Indication whether this message has been processed by
                            another module. The handler may choose to ignore certain
                            messages if they have already been handled.

    \returns TRUE if the message has been processed, otherwise returns the
         value in already_handled
 */
bool appUpgradeHandleSystemMessages(MessageId id, Message message, bool already_handled);


/*! Add a client to the UPGRADE module

    Messages from #av_headet_upgrade_messages will be sent to any task
    registered through this API

    \param task Task to register as a client
 */
void appUpgradeClientRegister(Task task);

/*! Set the context of the UPGRADE module

    The value is stored in the UPGRADE PsKey and hence is non-volatile

    \param app_upgrade_context_t Upgrade context to set
 */
void appUpgradeSetContext(app_upgrade_context_t context);

/*! Get the context of the UPGRADE module

    The value is stored in the UPGRADE PsKey and hence is non-volatile

    \returns The non-volatile context of the UPGRADE module from the UPGRADE PsKey

 */
app_upgrade_context_t appUpgradeGetContext(void);

/*! Abort the ongoing Upgrade if the device is disconnected from GAIA app

    This function is called to abort the device upgrade in both initiator and
    peer device if the initiator device is disconnected from GAIA app

    \param None
 */
void appUpgradeAbortDuringDeviceDisconnect(void);
#else
#define appUpgradeEnteredDfuMode() ((void)(0))
#define appUpgradeHandleSystemMessages(_id, _msg, _handled) (_handled)
#define appUpgradeClientRegister(tsk) ((void)0)
#define appUpgradeSetContext(ctx) ((void)0)
#define appUpgradeAbortDuringDeviceDisconnect() ((void)(0))
#define appUpgradeGetContext() (APP_UPGRADE_CONTEXT_UNUSED)
#endif /* INCLUDE_DFU */

#endif /* DEVICE_UPGRADE_H_ */
