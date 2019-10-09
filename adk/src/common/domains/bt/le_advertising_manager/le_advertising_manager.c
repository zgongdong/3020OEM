/*!
\copyright  Copyright (c) 2018 - 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Management of Bluetooth Low Energy advertising
*/

#include <adc.h>
#include <panic.h>
#include <stdlib.h>

#include <connection.h>
#include <connection_no_ble.h>
#include <task_list.h>

#include "local_addr.h"

#include "le_advertising_manager.h"
#include "le_advertising_manager_sm.h"
#include "le_advertising_manager_data.h"
#include "le_advertising_manager_advert.h"
#include "le_advertising_manager_clients.h"
#include "le_advertising_manager_private.h"

#include "logging.h"
#include "hydra_macros.h"

/*!< Task information for the advertising manager */
adv_mgr_task_data_t app_adv_manager;

static le_advert_start_params_t start_params;

static le_adv_mgr_state_machine_t * sm = NULL;
static Task task_allow_all;
static Task task_enable_connectable;

static void leAdvertisingManager_HandleEnableAdvertising(const LE_ADV_INTERNAL_MSG_ENABLE_ADVERTISING_T * message);
static void leAdvertisingManager_HandleNotifyRpaAddressChange(void);
static void leAdvertisingManager_HandleInternalStartRequest(const LE_ADV_MGR_INTERNAL_START_T * message);
static bool leAdvertisingManager_Start(const le_advert_start_params_t * params);
static void leAdvertisingManager_ScheduleAdvertisingStart(const le_adv_data_set_t set, const bool send_cfm_msg);

/*! Send a completion message for the current blocking operation (if any).
    If there is no task registered for the message, no message is sent */
void sendBlockingResponse(connection_lib_status sts)
{
    adv_mgr_task_data_t *advMgr = AdvManagerGetTaskData();

    if (APP_ADVMGR_ADVERT_SET_DATA_CFM == advMgr->blockingOperation)
    {
        if (advMgr->blockingTask)
        {
            MAKE_MESSAGE(APP_ADVMGR_ADVERT_SET_DATA_CFM);
            message->status = sts;
            MessageSend(advMgr->blockingTask, advMgr->blockingOperation, message);
            
            advMgr->blockingTask = NULL;
        }
        advMgr->blockingCondition = ADV_SETUP_BLOCK_NONE;
        advMgr->blockingOperation  = 0;
        advMgr->blockingAdvert = NULL;
    }
}

static void startAdvert(const ADV_MANAGER_START_ADVERT_T *message)
{
    AdvManagerGetTaskData()->blockingTask = message->requester;
    AdvManagerGetTaskData()->blockingOperation = APP_ADVMGR_ADVERT_START_CFM;

    leAdvertisingManager_RegisterAdvert(message->advert);

    /*! \todo implement sequencing */
    
    Panic();
}

static void handleSetupAdvert(const ADV_MANAGER_SETUP_ADVERT_T *message)
{
    AdvManagerGetTaskData()->blockingTask = message->requester;
    AdvManagerGetTaskData()->blockingOperation = APP_ADVMGR_ADVERT_SET_DATA_CFM;

    leAdvertisingManager_RegisterAdvert(message->advert);
}

static void handleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch (id)
    {
        case ADV_MANAGER_START_ADVERT:
            DEBUG_LOG_LEVEL_1("ADV_MANAGER_START_ADVERT");
            startAdvert((const ADV_MANAGER_START_ADVERT_T*)message);
            return;

        case ADV_MANAGER_SETUP_ADVERT:
            DEBUG_LOG_LEVEL_1("ADV_MANAGER_SETUP_ADVERT");
            handleSetupAdvert((const ADV_MANAGER_SETUP_ADVERT_T*)message);
            return;
            
        case LE_ADV_INTERNAL_MSG_ENABLE_ADVERTISING:
            DEBUG_LOG_LEVEL_1("LE_ADV_INTERNAL_MSG_ENABLE_ADVERTISING");
            leAdvertisingManager_HandleEnableAdvertising((const LE_ADV_INTERNAL_MSG_ENABLE_ADVERTISING_T*)message);
            return;
            
        case LE_ADV_INTERNAL_MSG_NOTIFY_RPA_CHANGE:
            DEBUG_LOG_LEVEL_1("LE_ADV_INTERNAL_MSG_NOTIFY_RPA_CHANGE");
            leAdvertisingManager_HandleNotifyRpaAddressChange();
            break;
            
        case CL_DM_BLE_SET_ADVERTISE_ENABLE_CFM:
            DEBUG_LOG_LEVEL_1("CL_DM_BLE_SET_ADVERTISE_ENABLE_CFM");
            LeAdvertisingManager_HandleConnectionLibraryMessages(CL_DM_BLE_SET_ADVERTISE_ENABLE_CFM, (const CL_DM_BLE_SET_ADVERTISE_ENABLE_CFM_T*)message, FALSE);
            break;
            
        case LE_ADV_MGR_INTERNAL_START:
            DEBUG_LOG_LEVEL_1("LE_ADV_MGR_INTERNAL_START");
            leAdvertisingManager_HandleInternalStartRequest((const LE_ADV_MGR_INTERNAL_START_T *)message);
            break;
    }
    
}

static bool handleSetAdvertisingDataCfm(const CL_DM_BLE_SET_ADVERTISING_DATA_CFM_T* cfm)
{
    if(AdvManagerGetTaskData()->blockingCondition == ADV_SETUP_BLOCK_ADV_DATA_CFM)
    {
        if (success == cfm->status)
        {
            adv_mgr_advert_t *advert = AdvManagerGetTaskData()->blockingAdvert;
            
            AdvManagerGetTaskData()->blockingCondition = ADV_SETUP_BLOCK_ADV_PARAMS_CFM;

            ConnectionDmBleSetAdvertisingParamsReq(advert->advertising_type,
                                                   advert->use_own_random,
                                                   advert->channel_map_mask,
                                                   &advert->interval_and_filter);
        }
        else
        {
            sendBlockingResponse(fail);
        }
    }

    return TRUE;
}

static void handleSetAdvertisingParamCfm(const CL_DM_BLE_SET_ADVERTISING_PARAMS_CFM_T *cfm)
{
    if (AdvManagerGetTaskData()->blockingCondition == ADV_SETUP_BLOCK_ADV_PARAMS_CFM)
    {
        if (success == cfm->status)
        {
            sendBlockingResponse(cfm->status);
            AdvManagerGetTaskData()->blockingCondition = ADV_SETUP_BLOCK_NONE;
            MessageSend(AdvManagerGetTaskData()->blockingTask, LE_ADV_MGR_SELECT_DATASET_CFM, NULL);
        }
        else
        {
            sendBlockingResponse(fail);
        }
    }
}

bool AdvertisingManager_HandleConnectionLibraryMessages(MessageId id, Message message, bool already_handled)
{
    switch (id)
    {
        case CL_DM_BLE_SET_ADVERTISING_DATA_CFM:
            handleSetAdvertisingDataCfm((const CL_DM_BLE_SET_ADVERTISING_DATA_CFM_T *)message);
            return TRUE;

        case CL_DM_BLE_SET_ADVERTISING_PARAMS_CFM:
            handleSetAdvertisingParamCfm((const CL_DM_BLE_SET_ADVERTISING_PARAMS_CFM_T *)message);
            return TRUE;
    }

    return already_handled;
}

static void leAdvertisingManager_ClearDataSetBitmask(void)
{
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_ClearDataSetBitmask");
    
    start_params.set = 0;    
    
}

/* Local Function to Set Local Data Set Bitmask */
static void leAdvertisingManager_SetDataSetBitmask(le_adv_data_set_t set, bool enable)
{
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_SetDataSetBitmask");
    
    if(enable)
    {
        DEBUG_LOG_LEVEL_2("leAdvertisingManager_SetDataSetBitmask Info, Enable bitmask, Data set is %x", set);
        
        start_params.set |= set;
        
    }
    else
    {
        DEBUG_LOG_LEVEL_2("leAdvertisingManager_SetDataSetBitmask Info, Disable bitmask, Data set is %x", set);
        
        start_params.set &= ~set;
        
    }    
}


/* Local function to retrieve the reference to the handle asssigned to the given data set */
static le_adv_data_set_handle * leAdvertisingManager_GetReferenceToHandleForDataSet(le_adv_data_set_t set)
{
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_GetReferenceToHandleForDataSet");
        
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();
        
    le_adv_data_set_handle * p_handle = NULL;
    
    switch(set)
    {
        case le_adv_data_set_handset_unidentifiable:
        case le_adv_data_set_handset_identifiable:

        p_handle = &adv_task_data->dataset_handset_handle;
        DEBUG_LOG_LEVEL_2("leAdvertisingManager_GetReferenceToHandleForDataSet Info, Pointer to handle assigned to handset data set is %x handle is %x", p_handle, adv_task_data->dataset_handset_handle);
                
        break;
        
        case le_adv_data_set_peer:

        p_handle = &adv_task_data->dataset_peer_handle;
        DEBUG_LOG_LEVEL_2("leAdvertisingManager_GetReferenceToHandleForDataSet Info, Pointer to handle assigned to peer data set is %x handle is %x", p_handle, adv_task_data->dataset_peer_handle);            

        break;
        
        default:
        
        DEBUG_LOG_LEVEL_1("leAdvertisingManager_GetReferenceToHandleForDataSet Failure, Invalid data set %x", set);
        
        break;
    }
    
    return p_handle;
    
}

/* Local Function to Free the Handle Asssigned to the Given Data Set */
static void leAdvertisingManager_FreeHandleForDataSet(le_adv_data_set_t set)
{
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_FreeHandleForDataSet");
        
    le_adv_data_set_handle * p_handle = NULL;
    
    p_handle = leAdvertisingManager_GetReferenceToHandleForDataSet(set);
    PanicNull(p_handle);
    
    DEBUG_LOG_LEVEL_2("leAdvertisingManager_FreeHandleForDataSet Info, Reference to handle is %x, handle is %x, data set is %x", p_handle, *p_handle, set);
    
    free(*p_handle);
    *p_handle = NULL;
    
}

/* Local Function to Retrieve the Task Asssigned to the Given Data Set */
static Task leAdvertisingManager_GetTaskForDataSet(le_adv_data_set_t set)
{
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_GetTaskForDataSet");
        
    le_adv_data_set_handle * p_handle = NULL;
        
    Task task = NULL;
    
    p_handle = leAdvertisingManager_GetReferenceToHandleForDataSet(set);
    PanicNull(p_handle);
    
    if(*p_handle)
    {   
        task = (*p_handle)->task;
        DEBUG_LOG_LEVEL_2("leAdvertisingManager_GetTaskForDataSet Info, Task is %x Data set is %x", task, set);        
    }
    else
    {
        DEBUG_LOG_LEVEL_1("leAdvertisingManager_GetTaskForDataSet Failure, No valid handle exists for data set %x", set);        
    }
    
    return task;    
    
}


/* Local Function to Check If there is a handle already created for a given data set */
static bool leAdvertisingManager_CheckIfHandleExists(le_adv_data_set_t set)
{
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_CheckIfHandleExists");
       
    le_adv_data_set_handle * p_handle = NULL;
    
    bool ret_val = FALSE;
    
    p_handle = leAdvertisingManager_GetReferenceToHandleForDataSet(set);
    PanicNull(p_handle);
    
    if(*p_handle)
    {   
        DEBUG_LOG_LEVEL_2("leAdvertisingManager_CheckIfHandleExists Info, Handle is %x Data set is %x", *p_handle, set);
        ret_val = TRUE;        
    }
    else
    {
        DEBUG_LOG_LEVEL_2("leAdvertisingManager_CheckIfHandleExists Info, No valid handle exists for data set %x", set);        
    }
    
    return ret_val;
    
}

/* Local Function to Check if one of the supported data sets is already selected */
static bool leAdvertisingManager_IsDataSetSelected(void)
{
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_IsDataSetSelected");
        
    if(start_params.set)
    {
        DEBUG_LOG_LEVEL_2("leAdvertisingManager_IsDataSetSelected Info, Selected data set is %x", start_params.set);
        return TRUE;
    }
    
    return FALSE;
}

/* Local Function to Create a New Data Set Handle for a given data set */
static le_adv_data_set_handle leAdvertisingManager_CreateNewDataSetHandle(le_adv_data_set_t set)
{
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_CreateNewDataSetHandle");
        
    le_adv_data_set_handle * p_handle = NULL;
    
    p_handle = leAdvertisingManager_GetReferenceToHandleForDataSet(set);
    PanicNull(p_handle);
    
    *p_handle = PanicUnlessMalloc(sizeof(struct _le_adv_data_set));
    (*p_handle)->set = set;
    
    DEBUG_LOG_LEVEL_2("leAdvertisingManager_CreateNewDataSetHandle Info, Reference to handle is %x, handle is %x, data set is %x" , p_handle, *p_handle, set);
    
    return *p_handle;
    
}    

/* Local Function to Handle Internal Advertising Start Request */
static void leAdvertisingManager_HandleInternalStartRequest(const LE_ADV_MGR_INTERNAL_START_T * msg)
{
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();
    
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleInternalStartRequest");
    
    if(LeAdvertisingManagerSm_IsAdvertisingStarted())
    {
        DEBUG_LOG_LEVEL_2("leAdvertisingManager_HandleInternalStartRequest Info, Advertising already started, suspending and rescheduling");
        
        LeAdvertisingManagerSm_SetState(le_adv_mgr_state_suspending);
        leAdvertisingManager_ClearAdvertData();
        adv_task_data->blockingCondition = ADV_SETUP_BLOCK_ADV_ENABLE_CFM;
        le_adv_data_set_t set = msg->set;
        bool send_cfm_msg = msg->is_select_cfm_msg_needed;
        adv_task_data->is_data_update_required = TRUE;
        
        leAdvertisingManager_ScheduleAdvertisingStart(set, send_cfm_msg);
        return;
    }
    
    LeAdvertisingManagerSm_SetState(le_adv_mgr_state_initialised);
    
    leAdvertisingManager_SetDataSetBitmask(msg->set, TRUE);
    
    start_params.event = le_adv_event_type_connectable_general;
    
    leAdvertisingManager_Start(&start_params);
    
    if(msg->is_select_cfm_msg_needed)
    {
        MAKE_MESSAGE(LE_ADV_MGR_SELECT_DATASET_CFM);
        message->status = le_adv_mgr_status_success;
        MessageSendConditionally(leAdvertisingManager_GetTaskForDataSet(msg->set), LE_ADV_MGR_SELECT_DATASET_CFM, message,  &adv_task_data->blockingCondition );
    }    
}

/* Local Function to Handle Internal Enable Advertising Message */
static void leAdvertisingManager_HandleEnableAdvertising(const LE_ADV_INTERNAL_MSG_ENABLE_ADVERTISING_T * message)
{   
    if(TRUE == message->action)
    {                               
        LeAdvertisingManagerSm_SetState(le_adv_mgr_state_starting);
    }
    else
    {                   
        LeAdvertisingManagerSm_SetState(le_adv_mgr_state_suspending);
    }

    AdvManagerGetTaskData()->blockingCondition = ADV_SETUP_BLOCK_ADV_ENABLE_CFM;
    
}

/* Local Function to Handle Internal Notify Rpa Address Change Message */
static void leAdvertisingManager_HandleNotifyRpaAddressChange(void)
{
    le_adv_mgr_client_iterator_t iterator;
    le_adv_mgr_register_handle client_handle = leAdvertisingManager_HeadClient(&iterator);
    
    while(client_handle)
    {
        if(client_handle->task)
        {
            MessageSend(client_handle->task, LE_ADV_MGR_RPA_TIMEOUT_IND, NULL);
        }
        
        client_handle = leAdvertisingManager_NextClient(&iterator);
    }
    
    MessageCancelAll(AdvManagerGetTask(), LE_ADV_INTERNAL_MSG_NOTIFY_RPA_CHANGE);
    MessageSendLater(AdvManagerGetTask(), LE_ADV_INTERNAL_MSG_NOTIFY_RPA_CHANGE, NULL, D_SEC(BLE_RPA_TIMEOUT_DEFAULT));
}

/* Local Function to Cancel Scheduled Enable/Disable Connectable Confirmation Messages */
static bool leAdvertisingManager_CancelPendingEnableDisableConnectableMessages(void)
{
    MessageCancelAll(task_enable_connectable, LE_ADV_MGR_ENABLE_CONNECTABLE_CFM);
    
    return TRUE;
}

/* Local Function to Schedule Enable/Disable Connectable Confirmation Messages */
static bool leAdvertisingManager_ScheduleEnableDisableConnectableMessages(bool enable, le_adv_mgr_status_t status)
{    
    adv_mgr_task_data_t *advMan = AdvManagerGetTaskData();
        
    MAKE_MESSAGE(LE_ADV_MGR_ENABLE_CONNECTABLE_CFM);
    message->enable = enable;
    message->status = status;

    MessageSendConditionally(task_enable_connectable, LE_ADV_MGR_ENABLE_CONNECTABLE_CFM, message, &advMan->blockingCondition);
    
    return TRUE;
}       
            
/* Local Function to Cancel Scheduled Allow/Disallow Messages */
static bool leAdvertisingManager_CancelPendingAllowDisallowMessages(void)
{
    MessageCancelAll(task_allow_all, LE_ADV_MGR_ALLOW_ADVERTISING_CFM);
    
    return TRUE;
}

/* Local Function to Schedule Allow/Disallow Confirmation Messages */
static bool leAdvertisingManager_ScheduleAllowDisallowMessages(bool allow, le_adv_mgr_status_t status)
{   
    adv_mgr_task_data_t *advMan = AdvManagerGetTaskData();
    
    MAKE_MESSAGE(LE_ADV_MGR_ALLOW_ADVERTISING_CFM);
    message->allow = allow;
    message->status = status;
    
    MessageSendConditionally(task_allow_all, LE_ADV_MGR_ALLOW_ADVERTISING_CFM, message, &advMan->blockingCondition);
    
    return TRUE;
}  

/* Local Function to Schedule an Internal Advertising Enable/Disable Message */
static void leAdvertisingManager_ScheduleInternalEnableMessage(bool action)
{
    adv_mgr_task_data_t *advMan = AdvManagerGetTaskData();
        
    MAKE_MESSAGE(LE_ADV_INTERNAL_MSG_ENABLE_ADVERTISING);
    message->action = action;
    MessageSendConditionally(AdvManagerGetTask(), LE_ADV_INTERNAL_MSG_ENABLE_ADVERTISING, message, &advMan->blockingCondition );          
}

/* Local Function to Return the State of Connectable LE Advertising Being Enabled/Disabled */
static bool leAdvertisingManager_IsConnectableAdvertisingEnabled(void)
{
    adv_mgr_task_data_t *advMan = AdvManagerGetTaskData();
    
    const unsigned bitmask_connectable_events = le_adv_event_type_connectable_general | le_adv_event_type_connectable_directed;
        
    return (bitmask_connectable_events == advMan->mask_enabled_events);
}

/* Local Function to Return the State of LE Advertising Being Allowed/Disallowed */
static bool leAdvertisingManager_IsAdvertisingAllowed(void)
{
    adv_mgr_task_data_t *advMan = AdvManagerGetTaskData();
    
    return advMan->is_advertising_allowed;
}

/* Prevent advertising if any below conditions are not met */
static bool leAdvertisingManager_IsAdvertisingPossible(void)
{
    if(!leAdvertisingManager_IsConnectableAdvertisingEnabled())
    {
        return FALSE;
    }
    if(!leAdvertisingManager_IsAdvertisingAllowed())
    {
        return FALSE;
    }
    if(!leAdvertisingManager_IsDataSetSelected())
    {
        return FALSE;
    }
    return TRUE;
}

/* Cancel pending messages and restart enable/disable */
static void leAdvertisingManager_RestartEnableAdvertising(bool enable)
{
    leAdvertisingManager_CancelPendingAllowDisallowMessages();
    leAdvertisingManager_CancelPendingEnableDisableConnectableMessages();
    leAdvertisingManager_ScheduleInternalEnableMessage(enable);
}

/* Local Function to Decide whether to Suspend or Resume Advertising and act as decided */
static void leAdvertisingManager_EnableAdvertising(bool enable)
{
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();
    
    if(enable)
    {
        if(leAdvertisingManager_IsAdvertisingPossible())
        {
            if(LeAdvertisingManagerSm_IsSuspended())
            {
                LeAdvertisingManagerSm_SetState(le_adv_mgr_state_starting);
                
                adv_task_data->blockingCondition = ADV_SETUP_BLOCK_ADV_ENABLE_CFM;
            }
            else if(LeAdvertisingManagerSm_IsSuspending())
            {
                leAdvertisingManager_RestartEnableAdvertising(enable);
            }
        }
    }
    else
    {
        if(LeAdvertisingManagerSm_IsAdvertisingStarting())
        {
            leAdvertisingManager_RestartEnableAdvertising(enable);
        }
        else if(LeAdvertisingManagerSm_IsAdvertisingStarted())
        {
            LeAdvertisingManagerSm_SetState(le_adv_mgr_state_suspending);
            adv_task_data->blockingCondition = ADV_SETUP_BLOCK_ADV_ENABLE_CFM;
        }
    }
}

/* Local Function to Set/Reset Bitmask for Connectable Advertising Event Types */
static void leAdvertisingManager_SetAllowedAdvertisingBitmaskConnectable(bool action)
{
    adv_mgr_task_data_t *advMan = AdvManagerGetTaskData();

    advMan->mask_enabled_events |= le_adv_event_type_connectable_general;
    advMan->mask_enabled_events |= le_adv_event_type_connectable_directed;
    
    if(FALSE == action)
    {        
        advMan->mask_enabled_events &= ~le_adv_event_type_connectable_general;
        advMan->mask_enabled_events &= ~le_adv_event_type_connectable_directed;;
    }
}

/* Local Function to Set the Allow/Disallow Flag for All Advertising Event Types */
static void leAdvertisingManager_SetAllowAdvertising(bool allow)
{
    adv_mgr_task_data_t *advMan = AdvManagerGetTaskData();
    advMan->is_advertising_allowed = allow;
}


static void leAdvertisingManager_HandleLocalAddressConfigureCfmFailure(void)
{    
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleLocalAddressConfigureCfmFailure");
    
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();
    
    bool ret = 0;
    
    if(adv_task_data->dataset_handset_handle)
    {
        ret = MessageCancelAll(adv_task_data->dataset_handset_handle->task, LE_ADV_MGR_SELECT_DATASET_CFM);
        if(ret)
        {
            DEBUG_LOG_LEVEL_2("leAdvertisingManager_HandleLocalAddressConfigureCfmFailure Info, Reschedule cancelled LE_ADV_MGR_SELECT_DATASET_CFM messages");
            MAKE_MESSAGE(LE_ADV_MGR_SELECT_DATASET_CFM);
            message->status = le_adv_mgr_status_error_unknown;
            MessageSendConditionally(adv_task_data->dataset_handset_handle->task, LE_ADV_MGR_SELECT_DATASET_CFM, message, &adv_task_data->blockingCondition);
        }
    }        
    
    ret = 0;
    
    if(adv_task_data->dataset_peer_handle)
    {
        ret = MessageCancelAll(adv_task_data->dataset_peer_handle->task, LE_ADV_MGR_SELECT_DATASET_CFM);
        if(ret)
        {
            DEBUG_LOG_LEVEL_2("leAdvertisingManager_HandleLocalAddressConfigureCfmFailure Info, Reschedule cancelled LE_ADV_MGR_SELECT_DATASET_CFM messages");
            MAKE_MESSAGE(LE_ADV_MGR_SELECT_DATASET_CFM);
            message->status = le_adv_mgr_status_error_unknown;
            MessageSendConditionally(adv_task_data->dataset_peer_handle->task, LE_ADV_MGR_SELECT_DATASET_CFM, message, &adv_task_data->blockingCondition);
        }
    }        
}

/* Local Function to Handle Connection Library Advertise Enable Fail Response */
static void leAdvertisingManager_HandleSetAdvertisingEnableCfmFailure(void)
{
    if(leAdvertisingManager_CancelPendingAllowDisallowMessages())
    {
        bool adv_allowed = leAdvertisingManager_IsAdvertisingAllowed();
        leAdvertisingManager_ScheduleAllowDisallowMessages(adv_allowed, le_adv_mgr_status_error_unknown);
    }
    
    if(leAdvertisingManager_CancelPendingEnableDisableConnectableMessages())
    {
        bool connectable = leAdvertisingManager_IsConnectableAdvertisingEnabled();
        leAdvertisingManager_ScheduleEnableDisableConnectableMessages(connectable, le_adv_mgr_status_error_unknown);
    }
    
    LeAdvertisingManagerSm_SetState(le_adv_mgr_state_suspended);
    MessageCancelAll(AdvManagerGetTask(), LE_ADV_INTERNAL_MSG_NOTIFY_RPA_CHANGE);
}

/* Local Function to Set Up Advertising Event Type */
static void leAdvertisingManager_SetupAdvertEventType(le_adv_event_type_t event)
{
    ble_adv_type * adv_type = &AdvManagerGetTaskData()->blockingAdvert->advertising_type;

    switch(event)
    {
        case le_adv_event_type_connectable_general:
        {
            *adv_type = ble_adv_ind;
            break;
        }
        case le_adv_event_type_connectable_directed:
        {
            *adv_type = ble_adv_direct_ind;
            break;
        }
        case le_adv_event_type_nonconnectable_discoverable:
        {
            *adv_type = ble_adv_scan_ind;
            break;
        }
        case le_adv_event_type_nonconnectable_nondiscoverable:
        {
            *adv_type = ble_adv_nonconn_ind;
            break;
        }
       
        default:
        {
            
            Panic();
        }
    }
}

/* Local Function to Set Advertising Interval */
static void leAdvertisingManager_SetAdvertisingInterval(void)
{
    adv_mgr_task_data_t * adv_task_data = AdvManagerGetTaskData();
    
    ble_adv_params_t * adv_params = &adv_task_data->blockingAdvert->interval_and_filter;
    le_adv_params_set_handle handle = adv_task_data->params_handle;
    
    if(NULL == handle)
    {
        adv_params->undirect_adv.adv_interval_min = 0xA0;
        adv_params->undirect_adv.adv_interval_max = 0xC0;
    }
    else
    {
        adv_params->undirect_adv.adv_interval_min = handle->params_set->set_type[handle->active_params_set].le_adv_interval_min;
        adv_params->undirect_adv.adv_interval_max = handle->params_set->set_type[handle->active_params_set].le_adv_interval_max;
    }
}

/* Local Function to Set Up Advertising Interval and Filter Policy */
static void leAdvertisingManager_SetupIntervalAndFilter(le_adv_event_type_t event)
{    
    adv_mgr_task_data_t * adv_task_data = AdvManagerGetTaskData();

    memset(&adv_task_data->blockingAdvert->interval_and_filter,0,sizeof(ble_adv_params_t));
    
    if( le_adv_event_type_connectable_directed != event )
        leAdvertisingManager_SetAdvertisingInterval();                
}

/* Local Function to Check if Local Address Setup is Required */
static bool leAdvertisingManager_IsSetupLocalAddressRequired(void)
{      
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();
    
    if(adv_task_data->is_local_address_configured)
        return FALSE;
    
    return TRUE;
}

/* Local Function to Set Up Advertising Parameters */
static void leAdvertisingManager_SetupLocalAddress(void)
{      
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();

    adv_task_data->blockingCondition = ADV_SETUP_BLOCK_ADV_LOCAL_ADDRESS_CFM;
    
    ConnectionDmBleConfigureLocalAddressAutoReq(ble_local_addr_generate_resolvable, NULL, BLE_RPA_TIMEOUT_DEFAULT);
}

/* Local Function to Set Up Own Address Parameters */
static void leAdvertisingManager_SetupOwnAddressType(void)
{
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();
    
    adv_task_data->blockingAdvert->use_own_random = LocalAddr_GetBleType();

}

/* Local Function to Set Up Mask for Advertising Channels */
static void leAdvertisingManager_SetupChannelMask(void)
{
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();
    
    adv_task_data->blockingAdvert->channel_map_mask = BLE_ADV_CHANNEL_ALL;
}

/* Local Function to Set Up Advertising Parameters */
static void leAdvertisingManager_SetupAdvertParams(const le_advert_start_params_t * params)
{           
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_SetupAdvertParams");
        
    if(leAdvertisingManager_IsSetupLocalAddressRequired())
    {        
        DEBUG_LOG_LEVEL_2("leAdvertisingManager_SetupAdvertParams Info, Local address setup is required");
        
        leAdvertisingManager_SetupLocalAddress();
        return;
    }   
    
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();

    if(NULL == adv_task_data->blockingAdvert)
    {
        DEBUG_LOG_LEVEL_2("leAdvertisingManager_SetupAdvertParams Info, adv_task_data->blockingAdvert is %x", adv_task_data->blockingAdvert);
        
        adv_task_data->blockingAdvert = AdvertisingManager_NewAdvert();
        
        PanicNull(adv_task_data->blockingAdvert);
    }
    
    leAdvertisingManager_SetupAdvertEventType(params->event);
        
    leAdvertisingManager_SetupOwnAddressType();
    
    leAdvertisingManager_SetupChannelMask();

    leAdvertisingManager_SetupIntervalAndFilter(params->event);

    adv_task_data->blockingCondition = ADV_SETUP_BLOCK_ADV_PARAMS_CFM;

    DEBUG_LOG_LEVEL_2("leAdvertisingManager_SetupAdvertParams Info, Request advertising parameters set, blocking condition is %x", adv_task_data->blockingCondition);
    
    adv_mgr_advert_t *advert = adv_task_data->blockingAdvert;
        
    ConnectionDmBleSetAdvertisingParamsReq(advert->advertising_type,
                                                   advert->use_own_random,
                                                   advert->channel_map_mask,
                                                   &advert->interval_and_filter);
}

/* Local function to handle CL_DM_BLE_SET_ADVERTISING_DATA_CFM message */
static void leAdvertisingManager_HandleSetAdvertisingDataCfm(const CL_DM_BLE_SET_ADVERTISING_DATA_CFM_T* cfm)
{    
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleSetAdvertisingDataCfm");    
    
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();
    
    if(adv_task_data->blockingCondition == ADV_SETUP_BLOCK_ADV_DATA_CFM)
    {    
        if (success == cfm->status)
        {
            DEBUG_LOG_LEVEL_2("leAdvertisingManager_HandleSetAdvertisingDataCfm Info, CL_DM_BLE_SET_ADVERTISING_DATA_CFM received with success");    
            
            if(TRUE == leAdvertisingManager_SetupScanResponseData(le_adv_data_set_handset_identifiable))
            {
                DEBUG_LOG_LEVEL_2("leAdvertisingManager_HandleSetAdvertisingDataCfm Info, Scan response data exists");    
                
                return;
            }
            
            else
            {
                DEBUG_LOG_LEVEL_2("leAdvertisingManager_HandleSetAdvertisingDataCfm Info, Scan response data does not exist");    
                
                leAdvertisingManager_SetupAdvertParams(&start_params);
            }
        }
        else
        {
            DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleSetAdvertisingDataCfm Failure, CL_DM_BLE_SET_ADVERTISING_DATA_CFM received with failure");    

            sendBlockingResponse(fail);
        }
    }
    else
    {
        DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleSetAdvertisingDataCfm Failure, Message Received in Unexpected Blocking Condition %x", adv_task_data->blockingCondition);    
        
        Panic();
    }
}

/* Local function to handle CL_DM_BLE_SET_SCAN_RESPONSE_DATA_CFM message */
static void leAdvertisingManager_HandleSetScanResponseDataCfm(const CL_DM_BLE_SET_SCAN_RESPONSE_DATA_CFM_T * cfm)
{    
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleSetScanResponseDataCfm");    
    
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();
    
    if (adv_task_data->blockingCondition == ADV_SETUP_BLOCK_ADV_SCAN_RESPONSE_DATA_CFM)
    {        

        DEBUG_LOG_LEVEL_2("leAdvertisingManager_HandleSetScanResponseDataCfm Info, adv_task_data->blockingCondition is %x cfm->status is %x", adv_task_data->blockingCondition, cfm->status);    

        if (success == cfm->status)
        {            
            DEBUG_LOG_LEVEL_2("leAdvertisingManager_HandleSetScanResponseDataCfm Info, CL_DM_BLE_SET_SCAN_RESPONSE_DATA_CFM received with success");    
            
            leAdvertisingManager_SetupAdvertParams(&start_params);
        }
        else
        {
            
            DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleSetScanResponseDataCfm Failure, CL_DM_BLE_SET_SCAN_RESPONSE_DATA_CFM received with failure");    

            sendBlockingResponse(fail);
        }
    }
    else
    {
        DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleSetScanResponseDataCfm Failure, Message Received in Unexpected Blocking Condition %x", adv_task_data->blockingCondition);    
            
        Panic();
    }
}

/* Local function to handle CL_DM_BLE_SET_ADVERTISING_PARAMS_CFM message */
static void leAdvertisingManager_HandleSetAdvertisingParamCfm(const CL_DM_BLE_SET_ADVERTISING_PARAMS_CFM_T *cfm)
{
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleSetAdvertisingParamCfm");    
    
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();
    
    if (adv_task_data->blockingCondition == ADV_SETUP_BLOCK_ADV_PARAMS_CFM)
    {            
        DEBUG_LOG_LEVEL_2("leAdvertisingManager_HandleSetAdvertisingParamCfm Info, adv_task_data->blockingCondition is %x cfm->status is %x", adv_task_data->blockingCondition, cfm->status);    
            
        if (success == cfm->status)
        {            
            DEBUG_LOG_LEVEL_2("leAdvertisingManager_HandleSetAdvertisingParamCfm Info, CL_DM_BLE_SET_ADVERTISING_PARAMS_CFM received with success");    
            
            LeAdvertisingManagerSm_SetState(le_adv_mgr_state_starting);
            adv_task_data->blockingCondition = ADV_SETUP_BLOCK_ADV_ENABLE_CFM;

        }
        else
        {            
            DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleSetAdvertisingParamCfm Failure, CL_DM_BLE_SET_ADVERTISING_PARAMS_CFM received with failure");    
            
            Panic();
        }
    }
    else
    {      

        DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleSetAdvertisingParamCfm Failure, Message Received in Unexpected Blocking Condition %x", adv_task_data->blockingCondition);    
                                    
        Panic();
    }
}

/* Local Function to Handle Connection Library CL_DM_BLE_CONFIGURE_LOCAL_ADDRESS_CFM Message */
static void leAdvertisingManager_HandleLocalAddressConfigureCfm(const CL_DM_BLE_CONFIGURE_LOCAL_ADDRESS_CFM_T *cfm)
{
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleLocalAddressConfigureCfm");    
    
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();
    
    if (adv_task_data->blockingCondition == ADV_SETUP_BLOCK_ADV_LOCAL_ADDRESS_CFM)
    {            
        DEBUG_LOG_LEVEL_2("leAdvertisingManager_HandleLocalAddressConfigureCfm Info, adv_task_data->blockingCondition is %x cfm->status is %x", adv_task_data->blockingCondition, cfm->status);    

        if (success == cfm->status)
        {
            DEBUG_LOG_LEVEL_2("leAdvertisingManager_HandleLocalAddressConfigureCfm Info, CL_DM_BLE_CONFIGURE_LOCAL_ADDRESS_CFM received with success");    
            
            adv_task_data->is_local_address_configured = TRUE;
            
            leAdvertisingManager_SetupAdvertParams(&start_params);
            
        }
        else
        {                            
            DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleLocalAddressConfigureCfm Failure, CL_DM_BLE_CONFIGURE_LOCAL_ADDRESS_CFM received with failure");    
            
            leAdvertisingManager_HandleLocalAddressConfigureCfmFailure();
            
            adv_task_data->blockingOperation  = 0;
            adv_task_data->blockingAdvert = NULL;
            adv_task_data->blockingCondition = ADV_SETUP_BLOCK_NONE;
            
        }            
    }
    else
    {                            
         DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleLocalAddressConfigureCfm Failure, Message Received in Unexpected Blocking Condition %x", adv_task_data->blockingCondition);    
        
        /* TO DO: Pending on Local Address Management Component */                            
        /*Panic();*/
    }
    
    
    
}

/* Local function to handle CL_DM_BLE_SET_ADVERTISE_ENABLE_CFM message */
static void leAdvertisingManager_HandleSetAdvertisingEnableCfm(const CL_DM_BLE_SET_ADVERTISE_ENABLE_CFM_T *cfm)
{
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleSetAdvertisingEnableCfm");
    
    if (adv_task_data->blockingCondition == ADV_SETUP_BLOCK_ADV_ENABLE_CFM)
    {
        if(LeAdvertisingManagerSm_IsSuspending())
        {
            /* Assume failure to disable is due to advertising already being disabled */
            LeAdvertisingManagerSm_SetState(le_adv_mgr_state_suspended);
            MessageCancelAll(AdvManagerGetTask(), LE_ADV_INTERNAL_MSG_NOTIFY_RPA_CHANGE);
            DEBUG_LOG_LEVEL_2("leAdvertisingManager_HandleSetAdvertisingEnableCfm Info, CL_DM_BLE_SET_ADVERTISE_ENABLE_CFM received with success");    
                DEBUG_LOG_LEVEL_2("leAdvertisingManager_HandleSetAdvertisingEnableCfm Info, State machine is in suspending state");    
                DEBUG_LOG_LEVEL_2("leAdvertisingManager_HandleSetAdvertisingEnableCfm Info, State machine is in starting state");    
        }
        else if(LeAdvertisingManagerSm_IsAdvertisingStarting() && cfm->status == success)
        {
            LeAdvertisingManagerSm_SetState(le_adv_mgr_state_started); 
            MessageSendLater(AdvManagerGetTask(), LE_ADV_INTERNAL_MSG_NOTIFY_RPA_CHANGE, NULL, D_SEC(BLE_RPA_TIMEOUT_DEFAULT));
        }
        else
        {
            leAdvertisingManager_HandleSetAdvertisingEnableCfmFailure();
        }

        leAdvertisingManager_ClearAdvertData();
        adv_task_data->blockingOperation  = 0;
        adv_task_data->blockingCondition = ADV_SETUP_BLOCK_NONE;
    }
    else
    {
        Panic();
    }
}

/* Local Function to Start Advertising */
static bool leAdvertisingManager_Start(const le_advert_start_params_t * params)
{    
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_Start");
    
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();
            
    if(FALSE == LeAdvertisingManagerSm_IsInitialised())
    {
        DEBUG_LOG_LEVEL_1("leAdvertisingManager_Start Failure, State Machine is not Initialised");
        
        return FALSE;
    }
    
    if(TRUE == LeAdvertisingManagerSm_IsAdvertisingStarting())
    {
        DEBUG_LOG_LEVEL_1("leAdvertisingManager_Start Failure, Advertising is already in a process of starting");
        
        return FALSE;
    }

    if(NULL == params)
    {
        
        return FALSE;
    }
    
    if( FALSE == leAdvertisingManager_IsAdvertisingAllowed())
    {
        
        DEBUG_LOG_LEVEL_1("leAdvertisingManager_Start Failure, Advertising is currently not allowed");
        
        return FALSE;
    }
    else
    {        
                
        if(0UL == (adv_task_data->mask_enabled_events & params->event))
        {
            DEBUG_LOG_LEVEL_1("leAdvertisingManager_Start Failure, Advertising for the requested advertising event is currently not enabled");
            
            return FALSE;
        }
        
    }

    if(leAdvertisingManager_ClientListIsEmpty())
    {
        DEBUG_LOG_LEVEL_1("leAdvertisingManager_Start Failure, Database is empty");
        return FALSE;
    }
        
    if(adv_task_data->is_data_update_required)
    {
        DEBUG_LOG_LEVEL_2("leAdvertisingManager_Start Info, Data update is needed");        
        
        if(FALSE == leAdvertisingManager_SetupAdvertData(start_params.set))
        {
            DEBUG_LOG_LEVEL_2("leAdvertisingManager_Start Info, There is no data to advertise");        
        
            return FALSE;
        }
    }
    else
    {   
        
        DEBUG_LOG_LEVEL_2("leAdvertisingManager_Start Info, Data update is not needed, advertising parameters need to be configured");        
                
        leAdvertisingManager_SetupAdvertParams(params);        
        
    }
    
    LeAdvertisingManagerSm_SetState(le_adv_mgr_state_starting);
    
    return TRUE;    
}

/* Local Function to schedule advertising start operation */
static void leAdvertisingManager_ScheduleAdvertisingStart(const le_adv_data_set_t set, bool is_select_cfm_msg_needed)
{
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_ScheduleAdvertisingStart");
    
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();
    
    MAKE_MESSAGE(LE_ADV_MGR_INTERNAL_START);
    message->set = set;
    message->is_select_cfm_msg_needed = is_select_cfm_msg_needed;
    
    MessageSendConditionally(AdvManagerGetTask(), LE_ADV_MGR_INTERNAL_START, message,  &adv_task_data->blockingCondition );
    
}

le_adv_mgr_register_handle LeAdvertisingManager_Register(Task task, const le_adv_data_callback_t * callback)
{       
    DEBUG_LOG("LeAdvertisingManager_Register");
    
    if(FALSE == LeAdvertisingManagerSm_IsInitialised())
        return NULL;
    
    return LeAdvertisingManager_NewClient(task, callback);
}

/* API Function to Initialise LE Advertising Manager */
bool LeAdvertisingManager_Init(Task init_task)
{
    UNUSED(init_task);

    DEBUG_LOG("LeAdvertisingManager_Init");
    
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();
    
    memset(adv_task_data, 0, sizeof(*adv_task_data));
    adv_task_data->task.handler = handleMessage;
    
    leAdvertisingManager_AdvertInit();
    leAdvertisingManager_InitScanResponse();
    leAdvertisingManager_ClearDataSetBitmask();

    adv_task_data->dataset_handset_handle = NULL;
    adv_task_data->dataset_peer_handle = NULL;
    adv_task_data->params_handle = NULL;
    
    sm = LeAdvertisingManagerSm_Init();
    PanicNull(sm);
    LeAdvertisingManagerSm_SetState(le_adv_mgr_state_initialised);
    
    leAdvertisingManager_ClientsInit();
    
    task_allow_all = 0;
    task_enable_connectable = 0;
    
    return TRUE;
}

/* API Function to De-Initialise LE Advertising Manager */
bool LeAdvertisingManager_DeInit(void)
{
    DEBUG_LOG("LeAdvertisingManager_DeInit");
    
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();

    if(NULL != adv_task_data->params_handle)
    {
        free(adv_task_data->params_handle);
        adv_task_data->params_handle = NULL;
    }
    
    if(NULL != adv_task_data->dataset_handset_handle)
    {
        free(adv_task_data->dataset_handset_handle);
        adv_task_data->dataset_handset_handle = NULL;
    }
    
    if(NULL != adv_task_data->dataset_peer_handle)
    {
        free(adv_task_data->dataset_peer_handle);
        adv_task_data->dataset_peer_handle = NULL;
    }
    
    memset(adv_task_data, 0, sizeof(adv_mgr_task_data_t));

    if(NULL != sm)
    {
        LeAdvertisingManagerSm_SetState(le_adv_mgr_state_uninitialised);
        
    }
    
    return TRUE;        
}

/* API Function to enable/disable connectable LE advertising */
bool LeAdvertisingManager_EnableConnectableAdvertising(Task task, bool enable)
{
    DEBUG_LOG("LeAdvertisingManager_EnableConnectableAdvertising");
        
    if(NULL == task)
        return FALSE;
    
    if(FALSE == LeAdvertisingManagerSm_IsInitialised())
        return FALSE;
    
    if(0 == task_enable_connectable)
    {
        task_enable_connectable = task;
    }
    else
    {
        if(task_enable_connectable != task)
            return FALSE;
    }
           
    leAdvertisingManager_SetAllowedAdvertisingBitmaskConnectable(enable);
       
    leAdvertisingManager_EnableAdvertising(enable);

    leAdvertisingManager_ScheduleEnableDisableConnectableMessages(enable, le_adv_mgr_status_success);    
    
    return TRUE;
}

/* API Function to enable/disable all LE advertising */
bool LeAdvertisingManager_AllowAdvertising(Task task, bool allow)
{
    DEBUG_LOG("LeAdvertisingManager_AllowAdvertising");

    if(NULL == task)
        return FALSE;
    
    if(FALSE == LeAdvertisingManagerSm_IsInitialised())
        return FALSE;
    
    task_allow_all = task;

    leAdvertisingManager_SetAllowAdvertising(allow);
    
    leAdvertisingManager_EnableAdvertising(allow);

    leAdvertisingManager_ScheduleAllowDisallowMessages(allow, le_adv_mgr_status_success);
    
    return TRUE;
}

/* API function to use as the handler for connection library messages */
bool LeAdvertisingManager_HandleConnectionLibraryMessages(MessageId id, Message message, bool already_handled)
{
    DEBUG_LOG("LeAdvertisingManager_HandleConnectionLibraryMessages");
    
    switch (id)
    {
        case CL_DM_BLE_SET_ADVERTISING_DATA_CFM:
            
            leAdvertisingManager_HandleSetAdvertisingDataCfm((const CL_DM_BLE_SET_ADVERTISING_DATA_CFM_T *)message);
            return TRUE;

        case CL_DM_BLE_SET_SCAN_RESPONSE_DATA_CFM:
            
            leAdvertisingManager_HandleSetScanResponseDataCfm((const CL_DM_BLE_SET_SCAN_RESPONSE_DATA_CFM_T *)message);
            return TRUE;
            
        case CL_DM_BLE_SET_ADVERTISING_PARAMS_CFM:
            
            leAdvertisingManager_HandleSetAdvertisingParamCfm((const CL_DM_BLE_SET_ADVERTISING_PARAMS_CFM_T *)message);
            return TRUE;

        case CL_DM_BLE_SET_ADVERTISE_ENABLE_CFM:
            
            leAdvertisingManager_HandleSetAdvertisingEnableCfm((const CL_DM_BLE_SET_ADVERTISE_ENABLE_CFM_T *)message);
            return TRUE;
            
        case CL_DM_BLE_CONFIGURE_LOCAL_ADDRESS_CFM:
            
            leAdvertisingManager_HandleLocalAddressConfigureCfm((const CL_DM_BLE_CONFIGURE_LOCAL_ADDRESS_CFM_T *)message);
            return TRUE;            
    }

    if (!already_handled)
    {
        
    }

    return already_handled;
}

/* API function to select the data set for undirected advertising */
le_adv_data_set_handle LeAdvertisingManager_SelectAdvertisingDataSet(Task task, const le_adv_select_params_t * params)
{
    DEBUG_LOG("LeAdvertisingManager_SelectAdvertisingDataSet");

    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();
    
    if(FALSE == LeAdvertisingManagerSm_IsInitialised())
    {
        DEBUG_LOG("LeAdvertisingManager_SelectAdvertisingDataSet Failure, State Machine is not Initialised");
            
        return NULL;
    }
    
    if( (NULL == params) || (NULL == task) )
    {
        DEBUG_LOG("LeAdvertisingManager_SelectAdvertisingDataSet Failure, Invalid Input Arguments");
        
        return NULL;
    }
    else
    {
        DEBUG_LOG_LEVEL_2("LeAdvertisingManager_SelectAdvertisingDataSet Info, Task is %x Selected Data Set is %x" , task, params->set);
    }
    
    if(leAdvertisingManager_CheckIfHandleExists(params->set))
    {
        DEBUG_LOG("LeAdvertisingManager_SelectAdvertisingDataSet Failure, Dataset Handle Already Exists");
        
        return NULL;
        
    }
    else
    {
        adv_task_data->is_data_update_required = TRUE;
    
        le_adv_data_set_handle handle = leAdvertisingManager_CreateNewDataSetHandle(params->set);
        
        handle->task = task;
        
        leAdvertisingManager_ScheduleAdvertisingStart(params->set, TRUE);  
        
        DEBUG_LOG_LEVEL_2("LeAdvertisingManager_SelectAdvertisingDataSet Info, Handle does not exist, create new handle, handle->task is %x, handle->set is %x", handle->task, handle->set);
        
        return handle;        
    }
}

/* API function to release the data set for undirected advertising */
bool LeAdvertisingManager_ReleaseAdvertisingDataSet(le_adv_data_set_handle handle)
{    
    DEBUG_LOG("LeAdvertisingManager_ReleaseAdvertisingDataSet");
    
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();
    
    if(NULL == handle)
    {
        DEBUG_LOG_LEVEL_1("LeAdvertisingManager_ReleaseAdvertisingDataSet Failure, Invalid data set handle");
        
        return FALSE;
    }
    else
    {
        DEBUG_LOG_LEVEL_2("LeAdvertisingManager_ReleaseAdvertisingDataSet Info, Data set handle is %x", handle);
    }
    
    leAdvertisingManager_SetDataSetBitmask(handle->set, FALSE);
    
    leAdvertisingManager_EnableAdvertising(FALSE);
    
    MAKE_MESSAGE(LE_ADV_MGR_RELEASE_DATASET_CFM);
    message->status = le_adv_mgr_status_success;         
        
    MessageSendConditionally(leAdvertisingManager_GetTaskForDataSet(handle->set), LE_ADV_MGR_RELEASE_DATASET_CFM, message,  &adv_task_data->blockingCondition );
    
    MessageCancelAll(AdvManagerGetTask(), LE_ADV_MGR_INTERNAL_START);
    
    leAdvertisingManager_FreeHandleForDataSet(handle->set);
    leAdvertisingManager_ClearAdvertData();
    
    if(start_params.set)
    {
        DEBUG_LOG_LEVEL_2("LeAdvertisingManager_ReleaseAdvertisingDataSet Info, Local start parameters contain a valid set, reschedule advertising start with the set %x", start_params.set);
        adv_task_data->is_data_update_required = TRUE;
        
        bool send_cfm_msg = MessageCancelFirst(leAdvertisingManager_GetTaskForDataSet(start_params.set), LE_ADV_MGR_SELECT_DATASET_CFM);
        leAdvertisingManager_ScheduleAdvertisingStart(start_params.set, send_cfm_msg);  
    }
        
    return TRUE;
}

/* API function to notify a change in the data */
bool LeAdvertisingManager_NotifyDataChange(Task task, const le_adv_mgr_register_handle handle)
{    
    DEBUG_LOG("LeAdvertisingManager_NotifyDataChange");
        
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();
        
    if(!leAdvertisingManager_ClientHandleIsValid(handle))
    {
        DEBUG_LOG_LEVEL_1("LeAdvertisingManager_NotifyDataChange Failure, Invalid Handle");
        return FALSE;
    }
    
    adv_task_data->is_data_update_required = TRUE;
    
    if(LeAdvertisingManagerSm_IsAdvertisingStarting() || LeAdvertisingManagerSm_IsAdvertisingStarted())
    {
        DEBUG_LOG_LEVEL_2("LeAdvertisingManager_NotifyDataChange Info, Advertising in progress, suspend and reschedule advertising");
        
        leAdvertisingManager_EnableAdvertising(FALSE);
        leAdvertisingManager_ClearAdvertData();
        leAdvertisingManager_ScheduleAdvertisingStart(start_params.set, FALSE);  
    }
        
    MAKE_MESSAGE(LE_ADV_MGR_NOTIFY_DATA_CHANGE_CFM);
    message->status = le_adv_mgr_status_success;
    
    MessageSend(task, LE_ADV_MGR_NOTIFY_DATA_CHANGE_CFM, message);
    return TRUE;
}

/* API function to register LE advertising parameter sets */
bool LeAdvertisingManager_ParametersRegister(const le_adv_parameters_t *params)
{
    DEBUG_LOG("LeAdvertisingManager_ParametersRegister");
    
    if(NULL == params)
        return FALSE;
    
    if(FALSE == LeAdvertisingManagerSm_IsInitialised())
        return FALSE;
    
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();
    
    adv_task_data->params_handle =  PanicUnlessMalloc(sizeof(struct _le_adv_params_set));
    memset(adv_task_data->params_handle, 0, sizeof(struct _le_adv_params_set));
    
    adv_task_data->params_handle->params_set = (le_adv_parameters_set_t *)params->sets;    
    
    return TRUE;
    
}

/* API function to select the active LE advertising parameter set */
bool LeAdvertisingManager_ParametersSelect(uint8 index)
{
    DEBUG_LOG("LeAdvertisingManager_ParametersSelect index is %d", index);
    
    if(FALSE == LeAdvertisingManagerSm_IsInitialised())
    {
        return FALSE;
    }
    
    if((le_adv_preset_advertising_interval_low != index) && (le_adv_preset_advertising_interval_high != index))
    {
        return FALSE;
    }

    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();
    
    if(NULL == adv_task_data->params_handle)
    {
        return FALSE;
    }
    
    if(index != adv_task_data->params_handle->active_params_set)
    {
        adv_task_data->params_handle->active_params_set = index;
        if(LeAdvertisingManagerSm_IsAdvertisingStarting() || LeAdvertisingManagerSm_IsAdvertisingStarted())
        {
            leAdvertisingManager_EnableAdvertising(FALSE);
            
            leAdvertisingManager_ScheduleAdvertisingStart(start_params.set, FALSE);
        }        
    }
        
    return TRUE;    
}

/* API function to retrieve LE advertising interval minimum/maximum value pair */
bool LeAdvertisingManager_GetAdvertisingInterval(le_adv_common_parameters_t * interval)
{
    DEBUG_LOG("LeAdvertisingManager_GetAdvertisingInterval");
    
    if(NULL == interval)
    {
        return FALSE;
    }

    if(FALSE == LeAdvertisingManagerSm_IsInitialised())
    {
        return FALSE;
    }
    
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();
    le_adv_params_set_handle handle = adv_task_data->params_handle;
    
    if(NULL == adv_task_data->params_handle)
    {
        return FALSE;
    }
    
    if(le_adv_preset_advertising_interval_max >= adv_task_data->params_handle->active_params_set)
    {
        interval->le_adv_interval_min = handle->params_set->set_type[handle->active_params_set].le_adv_interval_min;
        interval->le_adv_interval_max = handle->params_set->set_type[handle->active_params_set].le_adv_interval_max;
    }
    
    return TRUE;
}

/* API function to retrieve LE advertising own address configuration */
bool LeAdvertisingManager_GetOwnAddressConfig(le_adv_own_addr_config_t * own_address_config)
{
    DEBUG_LOG("LeAdvertisingManager_GetOwnAddressConfig");
        
    if(FALSE == LeAdvertisingManagerSm_IsInitialised())
    {
        return FALSE;
    }
    
    own_address_config->own_address_type = le_adv_own_address_type_public;
    own_address_config->timeout = BLE_RPA_TIMEOUT_DEFAULT;
    
    return TRUE;
}
