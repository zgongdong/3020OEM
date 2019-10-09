/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief        Implementation of module managing le scanning.
*/

#include "le_scan_manager_protected.h"

#include <logging.h>
#include <panic.h>
#include <vmtypes.h>
#include <stdlib.h>

#include "task_list.h"
#include "local_addr.h"
#include "bdaddr.h"

#define leScanManagerConfigEnableScanning()  FALSE
#define leScanManagerConfigEnableWhiteList()  FALSE


/*! Macro to make a message based on type. */
#define MAKE_MESSAGE(TYPE) TYPE##_T *message = PanicUnlessNew(TYPE##_T);
#define MAKE_CL_MESSAGE_WITH_LEN(TYPE, LEN) TYPE##_T *message = (TYPE##_T *) PanicUnlessMalloc(sizeof(TYPE##_T) + LEN);
#define MAKE_CL_MESSAGE_WITH_LEN_TO_TASK(TYPE, LEN) TYPE##_T *message_task = (TYPE##_T *) PanicUnlessMalloc(sizeof(TYPE##_T) + LEN);


/*!< SM data structure */
le_scan_manager_data_t  scan_sm;

/*! SM data access Macros */
#define LeScanManagerGetTaskData() (&scan_sm)
#define LeScanManagerGetTask() (&scan_sm.task)
#define LeScanManagerGetState() (scan_sm.state)
#define LeScanManagerSetState(s) (scan_sm.state = s)
#define LeScanManagerSetCurrentCommand(s) (scan_sm.command = s)
#define LeScanManagerGetCurrentCommand() (scan_sm.command)

/*! \brief Scan manager response */
typedef struct
{
    le_scan_result_t status;
    le_scan_handle_t handle;
}le_scan_manager_status_t;

/******************************************************************************
  LE Scan Manager local functions
******************************************************************************/
static void leScanManager_SetScanEnableAndActive(bool is_active);
static bool leScanManager_isDuplicate(Task requester);
static void leScanManager_SetScanParameters(le_scan_interval_t scan_interval);
static uint8 leScanManager_GetEmptySlotIndex(void);
static le_scan_handle_t leScanManager_AcquireScan(le_scan_interval_t scan_interval, le_advertising_report_filter_t * filter,Task task);
static void leScanManager_SetScanAndAddAdvertisingReport(le_scan_interval_t scan_interval, le_advertising_report_filter_t* filter);
static uint8 leScanManager_GetIndexFromHandle(le_scan_handle_t handle);
static bool leScanManager_ReleaseScan(le_scan_handle_t handle);
static bool leScanManager_ReInitialiasePreviousActiveScans(void);
static bool leScanManager_CheckForFastScanInterval(void);
static bool leScanManager_ClearScanOnTask(Task requester);
static void leScanManager_HandleEnable(Task task );
static void leScanManager_HandleStart(Task task, le_scan_interval_t scan_interval,le_advertising_report_filter_t* filter);
static void leScanManager_HandleDisable(Task task );
static void leScanManager_HandleStop(Task task, le_scan_handle_t handle);
static void leScanManager_HandlePause(Task task );
static void leScanManager_HandleResume(Task task);
static void leScanManager_HandleScanEnable(connection_lib_status status);
static void leScanManager_HandleAdverts(const CL_DM_BLE_ADVERTISING_REPORT_IND_T* message);
static void leScanManager_handleScanFailure(scanCommand cmd , Task req);

static bool leScanManager_addClient(Task client);
static bool leScanManager_removeClient(Task client);

static void leScanManager_SendEnableCfm(Task task,le_scan_manager_status_t scan_status)
{
    MAKE_MESSAGE(LE_SCAN_MANAGER_ENABLE_CFM);
    message->status = scan_status.status;
    MessageSend(task,LE_SCAN_MANAGER_ENABLE_CFM,message);
}
static void leScanManager_SendDisableCfm(Task task,le_scan_manager_status_t scan_status)
{
    MAKE_MESSAGE(LE_SCAN_MANAGER_DISABLE_CFM);
    message->status = scan_status.status;
    MessageSend(task,LE_SCAN_MANAGER_DISABLE_CFM,message);
}
static void leScanManager_SendStartCfm(Task task,le_scan_manager_status_t scan_status)
{
    MAKE_MESSAGE(LE_SCAN_MANAGER_START_CFM);
    message->status = scan_status.status;
    message->handle = scan_status.handle;
    MessageSend(task,LE_SCAN_MANAGER_START_CFM,message);
}
static void leScanManager_SendStopCfm(Task task,le_scan_manager_status_t scan_status)
{
    MAKE_MESSAGE(LE_SCAN_MANAGER_STOP_CFM);
    message->status = scan_status.status;
    MessageSend(task,LE_SCAN_MANAGER_STOP_CFM,message);
}
static void leScanManager_SendPauseCfm(Task task,le_scan_manager_status_t scan_status)
{
    MAKE_MESSAGE(LE_SCAN_MANAGER_PAUSE_CFM);
    message->status = scan_status.status;
    MessageSend(task,LE_SCAN_MANAGER_PAUSE_CFM,message);
}
static void leScanManager_SendResumeCfm(Task task,le_scan_manager_status_t scan_status)
{
    MAKE_MESSAGE(LE_SCAN_MANAGER_RESUME_CFM);
    message->status = scan_status.status;
    MessageSend(task,LE_SCAN_MANAGER_RESUME_CFM,message);
}

static bool leScanManager_addClient(Task client)
{
    le_scan_manager_data_t * sm_data = LeScanManagerGetTaskData();
    return TaskList_AddTask(sm_data->client_list, client);
}

static bool leScanManager_removeClient(Task client)
{
    le_scan_manager_data_t * sm_data = LeScanManagerGetTaskData();
    return TaskList_RemoveTask(sm_data->client_list, client);
}

static void leScanManager_handleScanFailure(scanCommand cmd , Task req)
{
    DEBUG_LOG("leScanManager_handleScanFailure for command %d and client %d",cmd,req);
    switch(cmd)
    {
        case LE_SCAN_MANAGER_CMD_ENABLE:
        {
            MAKE_MESSAGE(LE_SCAN_MANAGER_ENABLE_CFM);
            message->status = LE_SCAN_MANAGER_RESULT_FAILURE;
            MessageSend(req, LE_SCAN_MANAGER_ENABLE_CFM, message);
        }
        break;
        case LE_SCAN_MANAGER_CMD_DISABLE:
        {
            MAKE_MESSAGE(LE_SCAN_MANAGER_DISABLE_CFM);
            message->status = LE_SCAN_MANAGER_RESULT_FAILURE;
            MessageSend(req, LE_SCAN_MANAGER_DISABLE_CFM, message);
        }
        break;
        case LE_SCAN_MANAGER_CMD_START:
        {
            MAKE_MESSAGE(LE_SCAN_MANAGER_START_CFM);
            message->status = LE_SCAN_MANAGER_RESULT_FAILURE;
            message->handle = NULL;
            MessageSend(req, LE_SCAN_MANAGER_START_CFM, message);
        }
        break;
        case LE_SCAN_MANAGER_CMD_STOP:
        {
            MAKE_MESSAGE(LE_SCAN_MANAGER_STOP_CFM);
            message->status = LE_SCAN_MANAGER_RESULT_FAILURE;
            MessageSend(req, LE_SCAN_MANAGER_STOP_CFM, message);
        }
        break;
        case LE_SCAN_MANAGER_CMD_PAUSE:
        {
            MAKE_MESSAGE(LE_SCAN_MANAGER_PAUSE_CFM);
            message->status = LE_SCAN_MANAGER_RESULT_FAILURE;
            MessageSend(req, LE_SCAN_MANAGER_PAUSE_CFM, message);
        }
        break;
        case LE_SCAN_MANAGER_CMD_RESUME:
        {
            MAKE_MESSAGE(LE_SCAN_MANAGER_RESUME_CFM);
            message->status = LE_SCAN_MANAGER_RESULT_FAILURE;
            MessageSend(req, LE_SCAN_MANAGER_RESUME_CFM, message);
        }
        break;
        default:
        break;
    }
    leScanManager_ClearScanOnTask(req);
}

static void leScanManager_SetScanEnableAndActive(bool is_active)
{
    le_scan_manager_data_t * sm_data = LeScanManagerGetTaskData();
    sm_data->is_busy = TRUE;
    ConnectionDmBleSetScanEnableReq(LeScanManagerGetTask(),is_active);
}

/*! \brief Set LE scan parameters based on requested speed.
    \param  scan interval
 */
static void leScanManager_SetScanParameters(le_scan_interval_t scan_interval)
{
    le_scan_manager_data_t * sm_data = LeScanManagerGetTaskData();
    if ((scan_interval == le_scan_interval_slow) && !leScanManager_CheckForFastScanInterval())
    {
        sm_data->scan_parameters.scan_interval = SCAN_INTERVAL_SLOW;
        sm_data->scan_parameters.scan_window = SCAN_WINDOW_SLOW;
    }
    else
    {
        sm_data->scan_parameters.scan_interval = SCAN_INTERVAL_FAST;
        sm_data->scan_parameters.scan_window = SCAN_WINDOW_FAST;
    }
}

/*! \brief Gets the first available empty slot index.
    @retval The index number or MAX_ACTIVE_SCANS if there are no empty slots
*/
static uint8 leScanManager_GetEmptySlotIndex(void)
{
    uint8 handle_index;
    le_scan_manager_data_t* sm_data = LeScanManagerGetTaskData();

    for (handle_index = 0; handle_index < MAX_ACTIVE_SCANS; handle_index++)
    {
        if (sm_data->active_handle[handle_index] == NULL)
        {
            break;
        }
    }

    return handle_index;
}
static bool leScanManager_isDuplicate(Task requester)
{
    le_scan_manager_data_t* sm_data = LeScanManagerGetTaskData();
    int handle_index;

    for (handle_index = 0; handle_index < MAX_ACTIVE_SCANS; handle_index++)
    {
        if ((sm_data->active_handle[handle_index]!= NULL) && (sm_data->active_handle[handle_index]->scan_task == requester))
        {
            return TRUE;
        }
    }
    return FALSE;
}

static void leScanManager_FreeScanHandle(le_scan_handle_t scan_handle)
{
    if(scan_handle->filter.find_tpaddr != NULL)
    {
        free(scan_handle->filter.find_tpaddr);
    }
    free(scan_handle->filter.pattern);
    free(scan_handle);
}

static bool leScanManager_ClearScanOnTask(Task requester)
{
    le_scan_manager_data_t* sm_data = LeScanManagerGetTaskData();
    int handle_index;

    for (handle_index = 0; handle_index < MAX_ACTIVE_SCANS; handle_index++)
    {
        if ((sm_data->active_handle[handle_index]!= NULL)&&(sm_data->active_handle[handle_index]->scan_task == requester))
        {
            leScanManager_FreeScanHandle(sm_data->active_handle[handle_index]);
            sm_data->active_handle[handle_index] = NULL;

            return TRUE;
        }
    }
    return FALSE;
}

/*! \brief Aquire handle to start scanning for an LE device.
    \param  scan interval
    \param  pointer to filter parameters
    @retval handle
*/

static le_scan_handle_t leScanManager_AcquireScan(le_scan_interval_t scan_interval, le_advertising_report_filter_t* filter, Task task)
{
    le_scan_handle_t handle;
    le_scan_manager_data_t * sm_data = LeScanManagerGetTaskData();
    uint8 handle_index = leScanManager_GetEmptySlotIndex();
    
    if (handle_index < MAX_ACTIVE_SCANS)
    {
        DEBUG_LOG("leScanManager_AcquireScan handle available.");
        sm_data->active_handle[handle_index] = PanicUnlessMalloc(sizeof(struct le_scan_handle));
        sm_data->active_handle[handle_index]->scan_interval = scan_interval;
        sm_data->active_handle[handle_index]->filter.ad_type = filter->ad_type;
        sm_data->active_handle[handle_index]->filter.interval =filter->interval;
        sm_data->active_handle[handle_index]->filter.size_pattern =filter->size_pattern;
 
        sm_data->active_handle[handle_index]->filter.pattern = PanicUnlessMalloc(filter->size_pattern);

        memcpy(sm_data->active_handle[handle_index]->filter.pattern , filter->pattern , sizeof(uint8)*(filter->size_pattern));
        if(filter->find_tpaddr != NULL)
        {
            sm_data->active_handle[handle_index]->filter.find_tpaddr = PanicUnlessMalloc(sizeof(tp_bdaddr));
            memcpy(sm_data->active_handle[handle_index]->filter.find_tpaddr , filter->find_tpaddr , sizeof(tp_bdaddr));
        }
        else
        {
            sm_data->active_handle[handle_index]->filter.find_tpaddr = NULL;
        }
        
        sm_data->active_handle[handle_index]->scan_task = task;

        handle = sm_data->active_handle[handle_index];
    }
    else
    {
        DEBUG_LOG("leScanManager_AcquireScan handle unavailable.");
        handle = NULL;
    }
    return handle;
}

/*! \brief Sets scan, connection parameters and then adds advertising filter and enables scan.
    \param  scan interval
    \param  pointer to filter parameters
 */
static void leScanManager_SetScanAndAddAdvertisingReport(le_scan_interval_t scan_interval, le_advertising_report_filter_t* filter)
{
    le_scan_manager_data_t * sm_data = LeScanManagerGetTaskData();
    leScanManager_SetScanParameters(scan_interval);
    
    ConnectionDmBleSetScanParametersReq(leScanManagerConfigEnableScanning(),
                                        LocalAddr_GetBleType(),
                                        leScanManagerConfigEnableWhiteList(),
                                        sm_data->scan_parameters.scan_interval,
                                        sm_data->scan_parameters.scan_window);

    ConnectionBleAddAdvertisingReportFilter(filter->ad_type,
                                            filter->interval,
                                            filter->size_pattern,
                                            filter->pattern);
}

/*! \brief Matches a handle to its represetative index number.
    \param  pointer to handle to be released
    @retval The index number or MAX_ACTIVE_SCANS if a handle does not match
*/
static uint8 leScanManager_GetIndexFromHandle(le_scan_handle_t handle)
{
    le_scan_manager_data_t * sm_data = LeScanManagerGetTaskData();
    uint8 handle_index;
    for (handle_index = 0; handle_index < MAX_ACTIVE_SCANS; handle_index++)
    {
        if ((sm_data->active_handle[handle_index] == handle) && (handle != NULL))
        {
            break;
        }
    }

    return handle_index;
}

/*! \brief Release handle scanning for an LE device.
    \param  pointer to handle to be released
    @retval TRUE if the handle is released
*/
static bool leScanManager_ReleaseScan(le_scan_handle_t handle)
{
    bool released = FALSE;
    le_scan_manager_data_t * sm_data = LeScanManagerGetTaskData();
    uint8 handle_index = leScanManager_GetIndexFromHandle(handle);

    if ((handle_index < MAX_ACTIVE_SCANS) && (handle != NULL))
    {
        DEBUG_LOG("leScanManager_ReleaseScan handle released");
             
        leScanManager_FreeScanHandle(sm_data->active_handle[handle_index]);
        sm_data->active_handle[handle_index] = NULL;
   
        released = TRUE;
    }

    return released;
}

/*! \brief Finds out the last active scan and re-initialises it. */
static bool leScanManager_ReInitialiasePreviousActiveScans(void)
{
    le_scan_manager_data_t * sm_data = LeScanManagerGetTaskData();
    uint8 handle_index;
    bool result = FALSE;
    for (handle_index = 0; handle_index < MAX_ACTIVE_SCANS; handle_index++)
    {
        if(sm_data->active_handle[handle_index] != NULL)
        {            
            leScanManager_SetScanAndAddAdvertisingReport(sm_data->active_handle[handle_index]->scan_interval, &sm_data->active_handle[handle_index]->filter);
            result = TRUE;
        }
    }

    return result;
}

/*! \brief Check active handles for fast scan interval.
    @retval TRUE if a fast scan interval exists
*/
static bool leScanManager_CheckForFastScanInterval(void)
{
    le_scan_manager_data_t * sm_data = LeScanManagerGetTaskData();
    bool fast_scan_exists = FALSE;
    uint8 handle_index;

    for (handle_index = 0; handle_index < MAX_ACTIVE_SCANS; handle_index++)
    {
        if (sm_data->active_handle[handle_index] != NULL) 
        {
            if (sm_data->active_handle[handle_index]->scan_interval == le_scan_interval_fast)
            {
                fast_scan_exists = TRUE;
                break;
            }
        }
    }

    return fast_scan_exists;
}

void leScanManager_Handler(Task task, MessageId id, Message message)
{
    UNUSED(task);
    connection_lib_status status = ((CL_DM_BLE_SET_SCAN_ENABLE_CFM_T*)message)->status;
    switch (id)
    {
        case CL_DM_BLE_SET_SCAN_ENABLE_CFM:
            leScanManager_HandleScanEnable(status);
            break;
        default:
            break;
    }
}

static bool leScanManager_FilterAddress(const typed_bdaddr* scan_taddr)
{
    tp_bdaddr current_addr;
    tp_bdaddr public_addr;
    le_scan_manager_data_t *sm_data = LeScanManagerGetTaskData();

    /* If client task is interested in finding specific device then resolve all RPA devices to check for a match */
    if(sm_data->confirmation_handle->filter.find_tpaddr != NULL)
    {
        memset(&public_addr, 0, sizeof(tp_bdaddr));
        
        current_addr.transport = TRANSPORT_BLE_ACL;
        memcpy(&(current_addr.taddr), scan_taddr, sizeof(typed_bdaddr));
        
        /* Retrieve permanent address if this is a random address */
        if(current_addr.taddr.type == TYPED_BDADDR_RANDOM)
        {
            if(!VmGetPublicAddress(&current_addr, &public_addr))
                return FALSE;
        }
        else
        {
            /* Provided address is PUBLIC address, copy it */
            public_addr.transport = TRANSPORT_BLE_ACL;
            memcpy(&(public_addr.taddr), scan_taddr, sizeof(typed_bdaddr));
        }

        /* If matching address is found send the message only to requested task */
        if(BdaddrTpIsSame(sm_data->confirmation_handle->filter.find_tpaddr, &public_addr))
            return TRUE;        
    }
    return FALSE;
}

static void leScanManager_HandleAdverts(const CL_DM_BLE_ADVERTISING_REPORT_IND_T* scan)
{
    scanState current_state = LeScanManagerGetState();
    le_scan_manager_data_t *sm_data = LeScanManagerGetTaskData();
    size_t size = sizeof(CL_DM_BLE_ADVERTISING_REPORT_IND_T)+(sizeof(uint8) * (scan->size_advertising_data - 1));
    if(current_state == LE_SCAN_MANAGER_STATE_SCANNING)
    {

        DEBUG_LOG("leScanManager_HandleAdverts. Advert: No:%d rssi:%d bdaddr %04X:%02X:%06lX",
                   scan->num_reports, scan->rssi,
                   scan->current_taddr.addr.nap,scan->current_taddr.addr.uap,scan->current_taddr.addr.lap);

        /* Eliminate scan results that we are not interested in */
        if (0 == scan->num_reports || scan->rssi < scanManagerMinRssi())
        {
           return;
        }

        
        MAKE_CL_MESSAGE_WITH_LEN(LE_SCAN_MANAGER_ADV_REPORT_IND,size);

        message->num_reports = scan->num_reports;
        message->event_type =  scan->event_type;
        message->rssi = scan->rssi;
        message->size_advertising_data = scan->size_advertising_data;
        
        memcpy(message->advertising_data, scan->advertising_data, scan->size_advertising_data);

        message->current_taddr.type =  scan->current_taddr.type;
        message->current_taddr.addr.nap =  scan->current_taddr.addr.nap;
        message->current_taddr.addr.uap =  scan->current_taddr.addr.uap;
        message->current_taddr.addr.lap =  scan->current_taddr.addr.lap;

        message->permanent_taddr.type =  scan->permanent_taddr.type;
        message->permanent_taddr.addr.nap =  scan->permanent_taddr.addr.nap;
        message->permanent_taddr.addr.uap =  scan->permanent_taddr.addr.uap;
        message->permanent_taddr.addr.lap =  scan->permanent_taddr.addr.lap;
        
        TaskList_MessageSendWithSize(sm_data->client_list, LE_SCAN_MANAGER_ADV_REPORT_IND, message,size);

        /* If client task is interested in finding specific device then resolve all RPA devices to check for a match */
        if(leScanManager_FilterAddress(&(scan->current_taddr)))
        {
            MAKE_CL_MESSAGE_WITH_LEN_TO_TASK(LE_SCAN_MANAGER_ADV_REPORT_IND,size);

            message_task->num_reports = scan->num_reports;
            message_task->event_type =  scan->event_type;
            message_task->rssi = scan->rssi;
            message_task->size_advertising_data = scan->size_advertising_data;
            
            memcpy(message_task->advertising_data, scan->advertising_data, scan->size_advertising_data);

            message_task->current_taddr.type =  scan->current_taddr.type;
            message_task->current_taddr.addr.nap =  scan->current_taddr.addr.nap;
            message_task->current_taddr.addr.uap =  scan->current_taddr.addr.uap;
            message_task->current_taddr.addr.lap =  scan->current_taddr.addr.lap;
        
            message_task->permanent_taddr.type =  sm_data->confirmation_handle->filter.find_tpaddr->taddr.type;
            message_task->permanent_taddr.addr.nap = sm_data->confirmation_handle->filter.find_tpaddr->taddr.addr.nap;
            message_task->permanent_taddr.addr.uap = sm_data->confirmation_handle->filter.find_tpaddr->taddr.addr.uap;
            message_task->permanent_taddr.addr.lap = sm_data->confirmation_handle->filter.find_tpaddr->taddr.addr.lap;

            MessageSend(sm_data->confirmation_handle->scan_task, LE_SCAN_MANAGER_ADV_REPORT_IND, message_task);
            free(sm_data->confirmation_handle->filter.find_tpaddr);
            sm_data->confirmation_handle->filter.find_tpaddr = NULL;
        }
    }
}

static void leScanManager_HandleEnable(Task task)
{
    bool respond = FALSE;
    le_scan_manager_status_t scan_result;
    scanState current_state = LeScanManagerGetState();
    DEBUG_LOG("leScanManager_HandleEnable Current State %d",current_state);
    le_scan_manager_data_t * sm_data = LeScanManagerGetTaskData();
    LeScanManagerSetCurrentCommand(LE_SCAN_MANAGER_CMD_ENABLE);
    
    if(sm_data->is_busy)
    {
        DEBUG_LOG("leScanManager_HandleEnable CL is busy!");
        scan_result.status = LE_SCAN_MANAGER_RESULT_BUSY;
        respond = TRUE;
    }
    else
    {
        switch (current_state)
        {
            case LE_SCAN_MANAGER_STATE_ENABLED:
            case LE_SCAN_MANAGER_STATE_SCANNING:
            case LE_SCAN_MANAGER_STATE_PAUSED:
            {
                /* Unexpected request */
                DEBUG_LOG("leScanManager_HandleEnable no action in (%d) state", current_state);
                respond = TRUE;
            }
            break;
            case LE_SCAN_MANAGER_STATE_DISABLED:
            {
                if (sm_data->is_paused)
                {
                    LeScanManagerSetState(LE_SCAN_MANAGER_STATE_PAUSED);
                    respond = TRUE;
                }
                else
                {
                    /* Change state and resume scanning */
                    if (leScanManager_ReInitialiasePreviousActiveScans())
                    {
                        sm_data->requester = task;
                        leScanManager_SetScanEnableAndActive(TRUE);
                        respond = FALSE;
                    }
                    else
                    {
                        LeScanManagerSetState(LE_SCAN_MANAGER_STATE_ENABLED);
                        respond = TRUE;
                    }
                }
            }
            break;
            default:
            break;
        }
        scan_result.status = LE_SCAN_MANAGER_RESULT_SUCCESS;
    }
    
    if(respond)
    {
        leScanManager_SendEnableCfm(task,scan_result);
    }
}

static void leScanManager_HandleDisable(Task task)
{
    scanState current_state = LeScanManagerGetState();
    DEBUG_LOG("leScanManager_HandleDisable Current State %d",current_state);
    bool respond = FALSE;
    le_scan_manager_status_t scan_result = {0};
    le_scan_manager_data_t * sm_data = LeScanManagerGetTaskData();
    LeScanManagerSetCurrentCommand(LE_SCAN_MANAGER_CMD_DISABLE);
    
    if(sm_data->is_busy)
    {
        DEBUG_LOG("leScanManager_HandleDisable CL is Busy!");
        scan_result.status = LE_SCAN_MANAGER_RESULT_BUSY;
        respond = TRUE;
    }
    else
    {
        switch (current_state)
        {
            case LE_SCAN_MANAGER_STATE_DISABLED:
            case LE_SCAN_MANAGER_STATE_PAUSED:
            case LE_SCAN_MANAGER_STATE_ENABLED:
            {
                /* Respond and change the state to Disabled */
                LeScanManagerSetState(LE_SCAN_MANAGER_STATE_DISABLED);
                scan_result.status = LE_SCAN_MANAGER_RESULT_SUCCESS;
                respond = TRUE;
            }
            break;
            case LE_SCAN_MANAGER_STATE_SCANNING:
            {
                /* Stop the scan */
                sm_data->requester = task;
                leScanManager_SetScanEnableAndActive(FALSE);
            }
            default:
            break;
        }
    }
   if(respond)
   {  
      leScanManager_SendDisableCfm(task,scan_result);
   }
}

static void leScanManager_HandleStart(Task task, le_scan_interval_t scan_interval,le_advertising_report_filter_t* filter)
{
    scanState current_state = LeScanManagerGetState();
    DEBUG_LOG("leScanManager_HandleStart Current State is:: %d", current_state);
    bool respond = FALSE;
    le_scan_manager_status_t scan_result;
    le_scan_handle_t handle = NULL;
    le_scan_manager_data_t * sm_data = LeScanManagerGetTaskData();
    LeScanManagerSetCurrentCommand(LE_SCAN_MANAGER_CMD_START);

    if(leScanManager_isDuplicate(task))
    {
        DEBUG_LOG("Found Duplicate for Task %d",task);
        scan_result.status = LE_SCAN_MANAGER_RESULT_FAILURE;
        respond = TRUE;
    }
    else if(sm_data->is_busy)
    {
        DEBUG_LOG("leScanManager_HandleStart CL is Busy!");
        scan_result.status = LE_SCAN_MANAGER_RESULT_BUSY;
        respond = TRUE;
    }
    else
    {
        switch (current_state)
        {
            case LE_SCAN_MANAGER_STATE_DISABLED:
            case LE_SCAN_MANAGER_STATE_PAUSED:
            {
                /* Save the scan parameters and respond. Scan shall start on resume/enable */
                DEBUG_LOG("leScanManager_HandleStart Cannot start scanning in state %d!", current_state);
                handle = leScanManager_AcquireScan(scan_interval,filter,task);
                
                if(handle)
                {
                    DEBUG_LOG("leScanManager_HandleStart new handle created.");
                    sm_data->confirmation_handle = handle;
                }
                scan_result.status = LE_SCAN_MANAGER_RESULT_SUCCESS;
                respond = TRUE;
            }
            break;

            case LE_SCAN_MANAGER_STATE_ENABLED:
            {
                /* Acquire the Scan and save the Filter Details in Local Structure, Handle is returned and sent to Client */
                handle = leScanManager_AcquireScan(scan_interval,filter,task);

                if(handle)
                {
                    sm_data->confirmation_handle = handle;
                    sm_data->requester = task;
                    DEBUG_LOG("leScanManager_HandleStart new handle created.");
                    leScanManager_SetScanAndAddAdvertisingReport(scan_interval,filter);
                    leScanManager_SetScanEnableAndActive(TRUE);
                }
            }
            break;

            case LE_SCAN_MANAGER_STATE_SCANNING:
            {
                /* Save the scan parameters and respond. Scan shall start on resume */
                handle = leScanManager_AcquireScan(scan_interval,filter,task);

                if(handle)
                {
                    sm_data->confirmation_handle = handle;
                    sm_data->requester = task;
                    DEBUG_LOG("leScanManager_HandleStart new handle created.");
                    leScanManager_SetScanEnableAndActive(FALSE);
                }
                else
                {
                    scan_result.status = LE_SCAN_MANAGER_RESULT_FAILURE;
                    respond = TRUE;
                }
            }

            default:
            break;
        }
    }
    if(respond)
    {
        scan_result.handle = handle;
        leScanManager_SendStartCfm(task,scan_result);
    }
}

static void leScanManager_HandleStop(Task task, le_scan_handle_t handle)
{
    scanState current_state = LeScanManagerGetState();
    DEBUG_LOG("leScanManager_HandleStop Current State is:: %d", current_state);

    bool respond = FALSE;
    le_scan_manager_status_t scan_result = {0};

    le_scan_manager_data_t * sm_data = LeScanManagerGetTaskData();
    LeScanManagerSetCurrentCommand(LE_SCAN_MANAGER_CMD_STOP);

    if(sm_data->is_busy)
    {
        DEBUG_LOG("leScanManager_HandleStop CL is Busy!");
        scan_result.status = LE_SCAN_MANAGER_RESULT_BUSY;
        respond = TRUE;
    }
    else
    {
        switch (current_state)
        {
            case LE_SCAN_MANAGER_STATE_DISABLED:
            case LE_SCAN_MANAGER_STATE_ENABLED:
            case LE_SCAN_MANAGER_STATE_PAUSED:
            {
                leScanManager_ReleaseScan(handle);
                scan_result.status = LE_SCAN_MANAGER_RESULT_SUCCESS;
                respond = TRUE;
            }
            break;

            case LE_SCAN_MANAGER_STATE_SCANNING:
            {
                /* Stop the scan and respond once scan is stopped. */
                if (leScanManager_ReleaseScan(handle))
                {
                    sm_data->requester = task;
                    DEBUG_LOG("leScanManager_HandleStop handle released.");
                    leScanManager_SetScanEnableAndActive(FALSE);
                }
                else
                {
                    /* Handle not found */
                    DEBUG_LOG("leScanManager_HandleStop cannot release scan");
                    scan_result.status = LE_SCAN_MANAGER_RESULT_SUCCESS;
                    respond = TRUE;
                }
            }

            default:
            break;
        }
   }

   if(respond)
   {
       leScanManager_SendStopCfm(task,scan_result);
   }
}

static void leScanManager_HandlePause(Task task )
{
    scanState current_state = LeScanManagerGetState();
    DEBUG_LOG("leScanManager_HandlePause Current State is:: %d", current_state);
    bool respond = FALSE;
    le_scan_manager_status_t scan_result = {0};
    le_scan_manager_data_t * sm_data = LeScanManagerGetTaskData();
    LeScanManagerSetCurrentCommand(LE_SCAN_MANAGER_CMD_PAUSE);
    sm_data->is_paused = TRUE;

    if(sm_data->is_busy)
    {
        DEBUG_LOG("leScanManager_HandlePause CL is Busy!");
        scan_result.status = LE_SCAN_MANAGER_RESULT_BUSY;
        respond = TRUE;
    }
    else
    {
        switch (current_state)
        {
            case LE_SCAN_MANAGER_STATE_DISABLED:
            case LE_SCAN_MANAGER_STATE_PAUSED:
            {
                respond = TRUE;
                scan_result.status = LE_SCAN_MANAGER_RESULT_SUCCESS;
                DEBUG_LOG("leScanManager_HandlePause cant be handled in this state %d",current_state);
            }
            break;

            case LE_SCAN_MANAGER_STATE_ENABLED:
            {
                /* Respond and change state to Paused */
                respond = TRUE;
                scan_result.status = LE_SCAN_MANAGER_RESULT_SUCCESS;
                LeScanManagerSetState(LE_SCAN_MANAGER_STATE_PAUSED);
            }
            break;

            case LE_SCAN_MANAGER_STATE_SCANNING:
            {
                sm_data->requester = task;
                respond = FALSE;
                DEBUG_LOG("leScanManager_HandlePause scanning paused.");
                leScanManager_SetScanEnableAndActive(FALSE);
            }
            break;

            default:
            break;
        }
    }
   if(respond)
   {
       /* Reply back with confirmation */
       leScanManager_SendPauseCfm(task,scan_result);
    }
}

static void leScanManager_HandleResume(Task task)
{
    scanState current_state = LeScanManagerGetState();
    DEBUG_LOG("leScanManager_HandleResume Current State is:: %d", current_state);
    
    bool respond = FALSE;
    le_scan_manager_status_t scan_result = {0};

    le_scan_manager_data_t * sm_data = LeScanManagerGetTaskData();
    LeScanManagerSetCurrentCommand(LE_SCAN_MANAGER_CMD_RESUME);
    sm_data->is_paused = FALSE;

    if(sm_data->is_busy)
    {
        DEBUG_LOG("leScanManager_HandleResume CL is Busy!");
        scan_result.status = LE_SCAN_MANAGER_RESULT_BUSY;
        respond = TRUE;
    }
    else
    {
        switch (current_state)
        {
            case LE_SCAN_MANAGER_STATE_PAUSED:
            {
                if (leScanManager_ReInitialiasePreviousActiveScans())
                {
                    sm_data->requester = task;
                    leScanManager_SetScanEnableAndActive(TRUE);
                    DEBUG_LOG("LeScanManager_Resume scanning resumed.");
                }
                else
                {
                    LeScanManagerSetState(LE_SCAN_MANAGER_STATE_ENABLED);
                    scan_result.status = LE_SCAN_MANAGER_RESULT_SUCCESS;
                }
            }
            break;

            case LE_SCAN_MANAGER_STATE_DISABLED:
            case LE_SCAN_MANAGER_STATE_ENABLED:
            break;

            case LE_SCAN_MANAGER_STATE_SCANNING:
            {
                /* Unexpected to receive Resume during scanning */
                scan_result.status = LE_SCAN_MANAGER_RESULT_FAILURE;
                DEBUG_LOG("LeScanManager_Resume cant be handled in state %d",current_state);
                respond = TRUE;
            }
            break;
            default:
            break;
        }
    }
   if(respond)
   {
       /* Reply back with confirmation */
       leScanManager_SendResumeCfm(task,scan_result);
   }
}

static void leScanManager_HandleScanEnable(connection_lib_status status)
{
    le_scan_manager_status_t scan_result;
    scanState current_state = LeScanManagerGetState();
    scanCommand scan_command = LeScanManagerGetCurrentCommand();
    DEBUG_LOG("Scan State before Getting Scan Enable %d for Command %d",current_state,scan_command);
    le_scan_manager_data_t * sm_data = LeScanManagerGetTaskData();
    if(status == fail)
    {
         leScanManager_handleScanFailure(scan_command,sm_data->requester);
    }
    else if(status == success)
    {
        switch(current_state)
        {
            case LE_SCAN_MANAGER_STATE_ENABLED:
            {
               if(scan_command == LE_SCAN_MANAGER_CMD_START)
               {
                    DEBUG_LOG("Respond to Scan Start %d",sm_data->requester);
                    LeScanManagerSetState(LE_SCAN_MANAGER_STATE_SCANNING);
                    leScanManager_addClient(sm_data->requester);

                    scan_result.handle = sm_data->confirmation_handle;
                    scan_result.status = LE_SCAN_MANAGER_RESULT_SUCCESS;
                    leScanManager_SendStartCfm(sm_data->requester,scan_result);
               }
               else if(scan_command == LE_SCAN_MANAGER_CMD_STOP)
               {
                   DEBUG_LOG("Scan Started for other clients after stopping the req %d",sm_data->requester);
                   scan_result.status = LE_SCAN_MANAGER_RESULT_SUCCESS;
                   leScanManager_SendStopCfm(sm_data->requester,scan_result);
               }
            }
            break;
            case LE_SCAN_MANAGER_STATE_DISABLED:
            {
                if(scan_command == LE_SCAN_MANAGER_CMD_ENABLE)
                {
                    scan_result.status = LE_SCAN_MANAGER_RESULT_SUCCESS;
                    leScanManager_SendEnableCfm(sm_data->requester, scan_result);
                    leScanManager_addClient(sm_data->requester);
                    LeScanManagerSetState(LE_SCAN_MANAGER_STATE_SCANNING);
                }
            }
            break;
            case LE_SCAN_MANAGER_STATE_SCANNING:
            {
                if(scan_command == LE_SCAN_MANAGER_CMD_DISABLE)
                {
                   LeScanManagerSetState(LE_SCAN_MANAGER_STATE_DISABLED);
                   leScanManager_removeClient(sm_data->requester);
                   scan_result.status = LE_SCAN_MANAGER_RESULT_SUCCESS;
                   leScanManager_SendDisableCfm(sm_data->requester,scan_result);
                }
                else if(scan_command == LE_SCAN_MANAGER_CMD_PAUSE)
                {
                   LeScanManagerSetState(LE_SCAN_MANAGER_STATE_PAUSED);
                   leScanManager_removeClient(sm_data->requester);
                   scan_result.status = LE_SCAN_MANAGER_RESULT_SUCCESS;
                   leScanManager_SendPauseCfm(sm_data->requester,scan_result);
                }
                else if(scan_command == LE_SCAN_MANAGER_CMD_START)
                {
                    LeScanManagerSetState(LE_SCAN_MANAGER_STATE_ENABLED);
                    ConnectionBleClearAdvertisingReportFilter();
                    leScanManager_ReInitialiasePreviousActiveScans();
                    leScanManager_SetScanEnableAndActive(TRUE);
                }
                else if(scan_command == LE_SCAN_MANAGER_CMD_STOP)
                {
                    LeScanManagerSetState(LE_SCAN_MANAGER_STATE_ENABLED);
                    ConnectionBleClearAdvertisingReportFilter();
                    if (leScanManager_ReInitialiasePreviousActiveScans())
                    {
                        leScanManager_SetScanEnableAndActive(TRUE);
                    }
                    else
                    {
                        LeScanManagerSetState(LE_SCAN_MANAGER_STATE_ENABLED);
                        scan_result.status  = LE_SCAN_MANAGER_RESULT_SUCCESS;
                        leScanManager_SendStopCfm(sm_data->requester, scan_result);
                    }
                }
            }
            case LE_SCAN_MANAGER_STATE_PAUSED:
            {
                if(scan_command == LE_SCAN_MANAGER_CMD_RESUME)
                {
                   LeScanManagerSetState(LE_SCAN_MANAGER_STATE_SCANNING);
                   scan_result.status = LE_SCAN_MANAGER_RESULT_SUCCESS;
                   leScanManager_SendResumeCfm(sm_data->requester, scan_result);
                   leScanManager_addClient(sm_data->requester);
                }
            }
            break;
            default:
            break;
        }
    }
    sm_data->is_busy = FALSE;
}

/******************************************************************************
  LE Scan Manager API functions
******************************************************************************/
bool LeScanManager_Init(Task init_task)
{
    UNUSED(init_task);
    le_scan_manager_data_t *scanTask = LeScanManagerGetTaskData();
    memset(scanTask, 0, sizeof(*scanTask));
    scanTask->task.handler = leScanManager_Handler;
    LeScanManagerSetState(LE_SCAN_MANAGER_STATE_INITIALIZED);
    if(scanTask->client_list == NULL)
    {
        scanTask->client_list = TaskList_Create();
    }
    /* Enabling the scan */
    LeScanManagerSetState(LE_SCAN_MANAGER_STATE_ENABLED);

    scanTask->is_paused = FALSE;
    scanTask->is_busy = FALSE;
    return TRUE;
}

void LeScanManager_Enable(Task task)
{
    DEBUG_LOG("LeScanManager_Enable");
    PanicNull(task);
    PanicFalse(LeScanManagerGetState() > LE_SCAN_MANAGER_STATE_UNINITIALIZED);
    leScanManager_HandleEnable(task);
}

void LeScanManager_Disable(Task task)
{
    DEBUG_LOG("LeScanManager_Disable");
    PanicNull(task);
    PanicFalse(LeScanManagerGetState() > LE_SCAN_MANAGER_STATE_UNINITIALIZED);

    leScanManager_HandleDisable(task);
}

void LeScanManager_Start(Task task, le_scan_interval_t scan_interval, le_advertising_report_filter_t* filter)
{
    DEBUG_LOG("LeScanManager_Start from Requester %d and scan interval %d",task,scan_interval);
    PanicNull(task);
    UNUSED(scan_interval);
    PanicFalse(LeScanManagerGetState() > LE_SCAN_MANAGER_STATE_UNINITIALIZED);

    leScanManager_HandleStart(task,scan_interval,filter);
}

void LeScanManager_Stop(Task task, le_scan_handle_t handle)
{
    DEBUG_LOG("LeScanManager_Stop");
    PanicNull(task);
    PanicNull(handle);
    PanicFalse(LeScanManagerGetState() > LE_SCAN_MANAGER_STATE_UNINITIALIZED);

    leScanManager_HandleStop(task,handle);

}

void  LeScanManager_Pause(Task task)
{
    DEBUG_LOG("LeScanManager_Pause");
    PanicNull(task);
    PanicFalse(LeScanManagerGetState() > LE_SCAN_MANAGER_STATE_UNINITIALIZED);

    leScanManager_HandlePause(task);
}

void LeScanManager_Resume(Task task)
{
    DEBUG_LOG("LeScanManager_Resume");
    PanicNull(task);
    PanicFalse(LeScanManagerGetState() > LE_SCAN_MANAGER_STATE_UNINITIALIZED);

    leScanManager_HandleResume(task);
}
/* To be used when filtering moves from connection lib */
bool LeScanManager_HandleConnectionLibraryMessages(MessageId id, Message message,
                                                          bool already_handled)
{
    UNUSED(already_handled);

    switch (id)
    {
        case CL_DM_BLE_ADVERTISING_REPORT_IND:
            leScanManager_HandleAdverts((const CL_DM_BLE_ADVERTISING_REPORT_IND_T *)message);
            return TRUE;

        case CL_DM_BLE_SET_SCAN_PARAMETERS_CFM:
            break;

        default:
            break;
    }

    return FALSE;
}

