/*!
\copyright  Copyright (c) 2008 - 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       power_manager.c
\brief      Power Management
*/

#include <panic.h>
#include <connection.h>
#include <ps.h>
#include <boot.h>
#include <dormant.h>
#include <message.h>
#include <system_clock.h>
#include <task_list.h>
#include <rtime.h>

#include "earbud_init.h"
#include "earbud_log.h"
#include "power_manager.h"
#include "temperature.h"

#include <battery_monitor.h>
#include "battery_monitor_config.h"
#include <charger_monitor.h>
#include "acceleration_config.h"
#include "phy_state.h"
#include "ui.h"
#include "prompts.h"

/*! \brief The client task allows sleep */
#define APP_POWER_ALLOW_SLEEP                       0x00000001
/*! \brief Power is waiting for a response to #POWER_SHUTDOWN_PREPARE_IND from the client task. */
#define APP_POWER_SHUTDOWN_PREPARE_RESPONSE_PENDING 0x00000002
/*! \brief Power is waiting for a response to #POWER_SLEEP_PREPARE_IND from the clienttask. */
#define APP_POWER_SLEEP_PREPARE_RESPONSE_PENDING    0x00000004

/*! \brief Bit set in lock to indicate UI event in progress */
#define APP_POWER_LOCK_UI                           0x0001

#define appPowerGetClients() TaskList_GetBaseTaskList(&PowerGetTaskData()->clients)
#define appPowerGetUiClients() &(PowerGetTaskData()->ui_clients)

#define APP_POWER_SEC_TO_MS(s)    ((rtime_t) ((s) * (rtime_t) US_PER_SEC))

/*!< Information for power_control */
powerTaskData   app_power;

/*! \brief Message IDs internal to the module */
enum app_power_internal_messages
{
    APP_POWER_INTERNAL_UI_COMPLETE, /*!< Message indicating the UI activity is complete. */
};


const message_group_t power_ui_inputs[] = { UI_INPUTS_DEVICE_STATE_MESSAGE_GROUP };

static void appPowerSetState(powerState new_state);


/*! \brief returns power task pointer to requesting component

    \return power task pointer.
*/
Task PowerGetTask(void)
{
    return &app_power.task;
}

/*! \brief provides power module current context to ui module

    \param[in]  void

    \return     ui_provider_ctxt_t - current context of power module.
*/
static unsigned appPowerCurrentContext(void)
{
    power_provider_context_t context = BAD_CONTEXT;
    switch(app_power.state)
    {
    case POWER_STATE_INIT:
        break;
    case POWER_STATE_OK:
        context = context_power_on;
        break;
    case POWER_STATE_TERMINATING_CLIENTS_NOTIFIED:
    case POWER_STATE_TERMINATING_CLIENTS_RESPONDED:
        context = context_powering_off;
        break;
    case POWER_STATE_SOPORIFIC_CLIENTS_NOTIFIED:
    case POWER_STATE_SOPORIFIC_CLIENTS_RESPONDED:
        context = context_entering_sleep;
        break;
    default:
        break;
    }
    return (unsigned)context;

}

/*! \brief Enter dormant mode

    This function is called internally to enter dormant.

    \param extended_wakeup_events Allow accelerometer to wake from dormant
*/
static void appPowerEnterDormantMode(bool extended_wakeup_events)
{
    DEBUG_LOG("appPowerEnterDormantMode");

#ifdef INCLUDE_ACCELEROMETER
    if (extended_wakeup_events)
    {
        powerTaskData *thePower = PowerGetTaskData();
        dormant_config_key key;
        uint32 value;
        /* Register to ensure accelerometer is active */
        appAccelerometerClientRegister(&thePower->task);
        if (appAccelerometerGetDormantConfigureKeyValue(&key, &value))
        {
            /* Since there is only one dormant wake source (not including sys_ctrl),
               just apply the accelerometer's key and value */
            PanicFalse(DormantConfigure(key, value));
        }
        else
        {
            appAccelerometerClientUnregister(&thePower->task);
        }
    }
#else
    UNUSED(extended_wakeup_events);
#endif /* INCLUDE_ACCELEROMETER */

    appPhyStatePrepareToEnterDormant();

    /* An active charge module blocks dormant, regardless of whether
       it has power */
    appChargerForceDisable();

    /* Make sure dormant will ignore any wake up time */
    PanicFalse(DormantConfigure(DEADLINE_VALID,FALSE));

    /* Enter dormant */
    PanicFalse(DormantConfigure(DORMANT_ENABLE,TRUE));

    DEBUG_LOG("appPowerEnterDormantMode FAILED");

    /* If we happen to get here then Dormant didn't work,
     * so make sure the charger is running again (if needed)
     * so we could continue. */
    appChargerRestoreState();

    Panic();
}

/*! \brief Enter power off

    This function is called internally to enter the power off mode.

    It will return FALSE if it can't power-off due to charger detection ongoing.
*/
static bool appPowerDoPowerOff(void)
{
    /* Try to power off.*/
    PsuConfigure(PSU_ALL, PSU_ENABLE, FALSE);

    /* Failed to power off.. */
    DEBUG_LOG("Turning off power supplies was ineffective?");

    /* No need to disable charger for power down, but if a charger is connected
       we want to charge. Check if we failed to power off because charger
       detection is not complete */
    if (!appChargerDetectionIsPending())
    {        
        /* Fall back to Dormant */
        appPowerEnterDormantMode(FALSE);

        /* Should never get here...*/
        Panic();
    }

    DEBUG_LOG("Failed to power off because charger detection ongoing. Check charger again later");

    return FALSE;
}

/*! \brief Query if the device is currently able to be powered off */
static bool appPowerCanPowerOff(void)
{
    return appChargerCanPowerOff();
}

/*! \brief Query if the device is currently able to sleep */
static bool appPowerCanSleep(void)
{
    return appChargerCanPowerOff() && PowerGetTaskData()->allow_dormant;
}

/*! \brief Query if the device should be powered off */
static bool appPowerNeedsToPowerOff(void)
{
    bool battery_too_low = (battery_level_too_low == appBatteryGetState());
    bool user_initiated = PowerGetTaskData()->user_initiated_shutdown;
    bool temperature_extreme = (appTemperatureClientGetState(PowerGetTask()) !=
                                TEMPERATURE_STATE_WITHIN_LIMITS);
    return appPowerCanPowerOff() && (temperature_extreme || user_initiated || battery_too_low);
}

/* \brief Inspect task list data to determine if all clients have flag bits
    cleared in their tasklist data. */
static bool appPowerAllClientsHaveFlagCleared(uint32 flag)
{
    task_list_t *clients = appPowerGetClients();
    Task next_task = NULL;
    task_list_data_t *data = NULL;

    if (TaskList_Size(clients))
    {
        while (TaskList_IterateWithDataRaw(clients, &next_task, &data))
        {
            if (data->u32 & flag)
            {
                return FALSE;
            }
        }
        return TRUE;
    }
    return FALSE;
}

/* \brief Inspect task list data to determine if all clients have flag bits
    cleared in their tasklist data. */
static bool appPowerAllClientsHaveFlagSet(uint32 flag)
{
    task_list_t *clients = appPowerGetClients();
    Task next_task = NULL;
    task_list_data_t *data = NULL;

    if (TaskList_Size(clients))
    {
        while (TaskList_IterateWithDataRaw(clients, &next_task, &data))
        {
            if (0 == (data->u32 & flag))
            {
                return FALSE;
            }
        }
        return TRUE;
    }
    return FALSE;
}

/*! \brief Set a flag in one client's tasklist data */
static bool appPowerSetFlagInClient(Task task, uint32 flag)
{
    task_list_data_t *data = NULL;
    if (TaskList_GetDataForTaskRaw(appPowerGetClients(), task, &data))
    {
        data->u32 |= flag;
        return TRUE;
    }
    return FALSE;
}

/*! \brief Clear a flag in one client's tasklist data */
static bool appPowerClearFlagInClient(Task task, uint32 flag)
{
    task_list_data_t *data = NULL;
    if (TaskList_GetDataForTaskRaw(appPowerGetClients(), task, &data))
    {
        data->u32 &= ~flag;
        return TRUE;
    }
    return FALSE;
}

/*! \brief Set a flag in all client's tasklist data */
static void appPowerSetFlagInAllClients(uint32 flag)
{
    task_list_t *clients = appPowerGetClients();
    Task next_task = NULL;
    task_list_data_t *data = NULL;
    while (TaskList_IterateWithDataRaw(clients, &next_task, &data))
    {
        data->u32 |= flag;
    }
}

#if 0
/*! \brief Clear a flag in all client's tasklist data */
static void appPowerClearFlagInAllClients(uint32 flag)
{
    task_list_t *clients = appPowerGetClients();
    Task next_task = NULL;
    task_list_data_t *data = NULL;
    while (TaskList_IterateWithDataRaw(clients, &next_task, &data))
    {
        data->u32 &= ~flag;
    }
}
#endif

static void appPowerExitInit(void)
{
    DEBUG_LOG("appPowerExitPowerStateInit");
    MessageSend(appInitGetInitTask(), APP_POWER_INIT_CFM, NULL);
}

static void appPowerEnterPowerStateOk(void)
{
    DEBUG_LOG("appPowerEnterPowerStateOk");
    PowerGetTaskData()->user_initiated_shutdown = FALSE;
}

static void appPowerExitPowerStateOk(void)
{
    DEBUG_LOG("appPowerExitPowerStateOk");
}

static void appPowerEnterPowerStateTerminatingClientsNotified(void)
{
    DEBUG_LOG("appPowerEnterPowerStateTerminatingClientsNotified");
    if (TaskList_Size(appPowerGetClients()))
    {
        TaskList_MessageSendId(appPowerGetClients(), APP_POWER_SHUTDOWN_PREPARE_IND);
        appPowerSetFlagInAllClients(APP_POWER_SHUTDOWN_PREPARE_RESPONSE_PENDING);
    }
    else
    {
        appPowerSetState(POWER_STATE_TERMINATING_CLIENTS_RESPONDED);
    }
}

static void appPowerExitPowerStateTerminatingClientsNotified(void)
{
    DEBUG_LOG("appPowerExitPowerStateTerminatingClientsNotified");
}

static void appPowerEnterPowerStateTerminatingClientsResponded(void)
{
    DEBUG_LOG("appPowerEnterPowerStateTerminatingClientsResponded");

    PowerGetTaskData()->lock |= APP_POWER_LOCK_UI;
    MessageSendConditionally(PowerGetTask(), APP_POWER_INTERNAL_UI_COMPLETE,
                             NULL, &PowerGetTaskData()->lock);

    Prompts_PlayPrompt(PROMPT_POWER_OFF, FALSE, TRUE, &PowerGetTaskData()->lock, APP_POWER_LOCK_UI);
}

/*! \brief Exiting means shutdown was aborted, tell clients */
static void appPowerExitPowerStateTerminatingClientsResponded(void)
{
    DEBUG_LOG("appPowerExitPowerStateTerminatingClientsResponded");
    TaskList_MessageSendId(appPowerGetClients(), APP_POWER_SHUTDOWN_CANCELLED_IND);
}

static void appPowerEnterPowerStateSoporificClientsNotified(void)
{
    DEBUG_LOG("appPowerEnterPowerStateSoporificClientsNotified");

    TaskList_MessageSendId(appPowerGetClients(), APP_POWER_SLEEP_PREPARE_IND);
    appPowerSetFlagInAllClients(APP_POWER_SLEEP_PREPARE_RESPONSE_PENDING);
}

static void appPowerExitPowerStateSoporificClientsNotified(void)
{
    DEBUG_LOG("appPowerExitPowerStateSoporificClientsNotified");
}

/*! At this point, power has sent #POWER_SLEEP_PREPARE_IND to all clients.
    All clients have responsed. Double check sleep is possible. If so sleep, if not
    return to ok. */
static void appPowerEnterPowerStateSoporificClientsResponded(void)
{
    DEBUG_LOG("appPowerEnterPowerStateSoporificClientsResponded");
    if (appPowerCanSleep())
    {
        appPowerEnterDormantMode(TRUE);
    }
    else
    {
        appPowerSetState(POWER_STATE_OK);
    }
}

/*! \brief Exiting means sleep was aborted, tell clients */
static void appPowerExitPowerStateSoporificClientsResponded(void)
{
    DEBUG_LOG("appPowerExitPowerStateSoporificClientsResponded");
    TaskList_MessageSendId(appPowerGetClients(), APP_POWER_SLEEP_CANCELLED_IND);
}

static void appPowerSetState(powerState new_state)
{
    powerState current_state = PowerGetTaskData()->state;

    // Handle exiting states
    switch (current_state)
    {
        case POWER_STATE_INIT:
            appPowerExitInit();
            break;
        case POWER_STATE_OK:
            appPowerExitPowerStateOk();
            break;
        case POWER_STATE_TERMINATING_CLIENTS_NOTIFIED:
            appPowerExitPowerStateTerminatingClientsNotified();
            break;
        case POWER_STATE_TERMINATING_CLIENTS_RESPONDED:
            appPowerExitPowerStateTerminatingClientsResponded();
            break;
        case POWER_STATE_SOPORIFIC_CLIENTS_NOTIFIED:
            appPowerExitPowerStateSoporificClientsNotified();
            break;
        case POWER_STATE_SOPORIFIC_CLIENTS_RESPONDED:
            appPowerExitPowerStateSoporificClientsResponded();
            break;
        default:
            Panic();
            break;
    }

    PowerGetTaskData()->state = new_state;

    // Handle entering states
    switch (new_state)
    {
        case POWER_STATE_OK:
            appPowerEnterPowerStateOk();
            break;
        case POWER_STATE_TERMINATING_CLIENTS_NOTIFIED:
            appPowerEnterPowerStateTerminatingClientsNotified();
            break;
        case POWER_STATE_TERMINATING_CLIENTS_RESPONDED:
            appPowerEnterPowerStateTerminatingClientsResponded();
            break;
        case POWER_STATE_SOPORIFIC_CLIENTS_NOTIFIED:
            appPowerEnterPowerStateSoporificClientsNotified();
            break;
        case POWER_STATE_SOPORIFIC_CLIENTS_RESPONDED:
            appPowerEnterPowerStateSoporificClientsResponded();
            break;
        default:
            Panic();
            break;
    }
}

/*! At the point, power has sent a #POWER_SHUTDOWN_PREPARE_IND to its
    clients and all have responded. The power off UI activity is complete.
    Double check power still needs to be off. If so power off, if not
    return to ok. */
static void appPowerHandleInternalUIComplete(void)
{
    DEBUG_LOG("appPowerHandleInternalUIComplete");
    if (appPowerNeedsToPowerOff() && appPowerDoPowerOff())
    {
        // Does not return
    }
    appPowerSetState(POWER_STATE_OK);
}

static void appPowerHandlePowerEvent(void)
{
    switch (PowerGetTaskData()->state)
    {
        case POWER_STATE_INIT:
            if (appPowerNeedsToPowerOff() && appPowerDoPowerOff())
            {
                // Does not return
            }
            appPowerSetState(POWER_STATE_OK);
        break;

        case POWER_STATE_OK:
            if (appPowerNeedsToPowerOff())
            {
                DEBUG_LOG("appPowerHandlePowerEvent need to shutdown");
                appPowerSetState(POWER_STATE_TERMINATING_CLIENTS_NOTIFIED);
            }
            else if (appPowerAllClientsHaveFlagSet(APP_POWER_ALLOW_SLEEP))
            {
                if (appPowerCanSleep())
                {
                    DEBUG_LOG("appPowerHandlePowerEvent can sleep");
                    appPowerSetState(POWER_STATE_SOPORIFIC_CLIENTS_NOTIFIED);
                }
            }
        break;

        default:
        break;
    }
}

/*! @brief Power control message handler
 */
static void appPowerHandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);

    if (isMessageUiInput(id))
        return;

    switch (id)
    {
        case APP_POWER_INTERNAL_UI_COMPLETE:
            appPowerHandleInternalUIComplete();
            break;
        case CHARGER_MESSAGE_ATTACHED:
        case CHARGER_MESSAGE_DETACHED:
        case MESSAGE_BATTERY_LEVEL_UPDATE_STATE:
        case TEMPERATURE_STATE_CHANGED_IND:
            appPowerHandlePowerEvent();
            break;
        default:
            break;
    }
}

void appPowerOn(void)
{
    DEBUG_LOG("appPowerOn");

    Prompts_PlayPrompt(PROMPT_POWER_ON, FALSE, TRUE, NULL, 0);
}

void appPowerReboot(void)
{
    /* Reboot now */
    BootSetMode(BootGetMode());
    DEBUG_LOG("appPowerReboot, post Reboot");

    /* BootSetMode returns control on some devices, although should reboot
       Wait here for 1 seconds and then Panic() */
    rtime_t start = SystemClockGetTimerTime();

    while (1)
    {
        rtime_t elapsed = rtime_sub(SystemClockGetTimerTime(),start);
        if (rtime_gt(elapsed,APP_POWER_SEC_TO_MS(1)))
        {
            DEBUG_LOG("appPowerReboot, PANICKED");
            Panic();
        }
    }
}

bool appPowerOffRequest(void)
{
    DEBUG_LOG("appPowerOffRequest");
    if (appPowerCanPowerOff())
    {
        switch (PowerGetTaskData()->state)
        {
            case POWER_STATE_INIT:
                /* Cannot power off during initialisation */
                break;
            case POWER_STATE_OK:
                PowerGetTaskData()->user_initiated_shutdown = TRUE;
                appPowerSetState(POWER_STATE_TERMINATING_CLIENTS_NOTIFIED);
                return TRUE;
            case POWER_STATE_TERMINATING_CLIENTS_NOTIFIED:
            case POWER_STATE_TERMINATING_CLIENTS_RESPONDED:
                /* Already shutting down, accept the request, but do nothing. */
                return TRUE;
            case POWER_STATE_SOPORIFIC_CLIENTS_NOTIFIED:
            case POWER_STATE_SOPORIFIC_CLIENTS_RESPONDED:
                /* Cannot power off when already entering sleep */
                break;
        }
    }
    return FALSE;
}

void appPowerClientRegister(Task task)
{
    task_list_data_t data = {.u32 = 0};

    DEBUG_LOGF("appPowerClientRegister %p", task);

    PanicFalse(TaskList_AddTaskWithData(appPowerGetClients(), task, &data));

    switch (PowerGetTaskData()->state)
    {
        case POWER_STATE_INIT:
            /* Cannot register during initialisation */
            Panic();
            break;
        case POWER_STATE_OK:
            break;
        case POWER_STATE_TERMINATING_CLIENTS_NOTIFIED:
            /* Notify the new client  */
            appPowerSetFlagInClient(task, APP_POWER_SHUTDOWN_PREPARE_RESPONSE_PENDING);
            MessageSend(task, APP_POWER_SHUTDOWN_PREPARE_IND, NULL);
            break;
        case POWER_STATE_TERMINATING_CLIENTS_RESPONDED:
            /* Already shutting down, the new client will not be informed of the
               shutdown, unless it is cancelled */
            break;
        case POWER_STATE_SOPORIFIC_CLIENTS_NOTIFIED:
            /* Notify the new client  */
            appPowerSetFlagInClient(task, APP_POWER_SLEEP_PREPARE_RESPONSE_PENDING);
            MessageSend(task, APP_POWER_SLEEP_PREPARE_IND, NULL);
            break;
        case POWER_STATE_SOPORIFIC_CLIENTS_RESPONDED:
            /* This should never happen, since entering this state results in
               the chip going to sleep. */
            break;
    }
}

void appPowerClientUnregister(Task task)
{
    DEBUG_LOGF("appPowerClientUnregister %p", task);

    /* Tidy up any outstanding actions the client may have. */
    appPowerClientAllowSleep(task);
    appPowerShutdownPrepareResponse(task);
    appPowerSleepPrepareResponse(task);

    MessageCancelAll(task, APP_POWER_SLEEP_PREPARE_IND);
    MessageCancelAll(task, APP_POWER_SLEEP_CANCELLED_IND);
    MessageCancelAll(task, APP_POWER_SHUTDOWN_PREPARE_IND);
    MessageCancelAll(task, APP_POWER_SHUTDOWN_CANCELLED_IND);

    TaskList_RemoveTask(appPowerGetClients(), task);
}

void appPowerClientAllowSleep(Task task)
{
    DEBUG_LOGF("appPowerClientAllowSleep %p", task);

    appPowerSetFlagInClient(task, APP_POWER_ALLOW_SLEEP);

    if (POWER_STATE_OK == PowerGetTaskData()->state)
    {
        if (appPowerAllClientsHaveFlagSet(APP_POWER_ALLOW_SLEEP))
        {
            if (appPowerCanSleep())
            {
                appPowerSetState(POWER_STATE_SOPORIFIC_CLIENTS_NOTIFIED);
            }
        }
    }
}

void appPowerClientProhibitSleep(Task task)
{
    DEBUG_LOGF("appPowerClientProhibitSleep %p", task);

    appPowerClearFlagInClient(task, APP_POWER_ALLOW_SLEEP);
}

void appPowerShutdownPrepareResponse(Task task)
{
    DEBUG_LOGF("appPowerShutdownPrepareResponse 0x%x %p", PowerGetTaskData()->state, task);

    if (POWER_STATE_TERMINATING_CLIENTS_NOTIFIED == PowerGetTaskData()->state)
    {
        if (appPowerClearFlagInClient(task, APP_POWER_SHUTDOWN_PREPARE_RESPONSE_PENDING))
        {
            if (appPowerAllClientsHaveFlagCleared(APP_POWER_SHUTDOWN_PREPARE_RESPONSE_PENDING))
            {
                appPowerSetState(POWER_STATE_TERMINATING_CLIENTS_RESPONDED);
            }
        }
    }
    // Ignore response in wrong state.
}

void appPowerSleepPrepareResponse(Task task)
{
    DEBUG_LOGF("appPowerSleepPrepareResponse 0x%x %p", PowerGetTaskData()->state, task);

    if (POWER_STATE_SOPORIFIC_CLIENTS_NOTIFIED == PowerGetTaskData()->state)
    {
        if (appPowerClearFlagInClient(task, APP_POWER_SLEEP_PREPARE_RESPONSE_PENDING))
        {
            if (appPowerAllClientsHaveFlagCleared(APP_POWER_SLEEP_PREPARE_RESPONSE_PENDING))
            {
                appPowerSetState(POWER_STATE_SOPORIFIC_CLIENTS_RESPONDED);
            }
        }
    }
    // Ignore response in wrong state.
}

void appPowerPerformanceProfileRequest(void)
{
    powerTaskData *thePower = PowerGetTaskData();

    if (0 == thePower->performance_req_count)
    {
        VmRequestRunTimeProfile(VM_PERFORMANCE);
        DEBUG_LOG("appPowerPerformanceProfileRequest VM_PERFORMANCE");
    }
    thePower->performance_req_count++;
    /* Unsigned overflowed request count */
    PanicZero(thePower->performance_req_count);
}

void appPowerPerformanceProfileRelinquish(void)
{
    powerTaskData *thePower = PowerGetTaskData();

    /* Unsigned underflow request count */
    PanicZero(thePower->performance_req_count);
    thePower->performance_req_count--;
    if (0 == thePower->performance_req_count)
    {
        VmRequestRunTimeProfile(VM_BALANCED);
        DEBUG_LOG("appPowerPerformanceProfileRelinquish VM_BALANCED");
    }
}

bool appPowerInit(Task init_task)
{
    batteryRegistrationForm batteryMonitoringForm;
    powerTaskData *thePower = PowerGetTaskData();

    memset(thePower, 0, sizeof(*thePower));

    thePower->task.handler = appPowerHandleMessage;
    thePower->allow_dormant = TRUE;
    thePower->user_initiated_shutdown = FALSE;
    thePower->performance_req_count = 0;
    thePower->state = POWER_STATE_INIT;

    TaskList_WithDataInitialise(&thePower->clients);

    VmRequestRunTimeProfile(VM_BALANCED);

    appChargerClientRegister(PowerGetTask());

    batteryMonitoringForm.task = PowerGetTask();
    batteryMonitoringForm.representation = battery_level_repres_state;
    batteryMonitoringForm.hysteresis = appConfigSmBatteryHysteresisMargin();
    appBatteryRegister(&batteryMonitoringForm);

    /* Need to power off when temperature is outside battery's operating range */
    if (!appTemperatureClientRegister(PowerGetTask(),
                                      appConfigBatteryDischargingTemperatureMin(),
                                      appConfigBatteryDischargingTemperatureMax()))
    {
        DEBUG_LOG("appPowerInit no temperature support");
    }

    appInitSetInitTask(init_task);

    /* Register power module as ui provider*/
    Ui_RegisterUiProvider(ui_provider_power,appPowerCurrentContext);

    Ui_RegisterUiInputConsumer(PowerGetTask(), power_ui_inputs, ARRAY_DIM(power_ui_inputs));

    return TRUE;
}
