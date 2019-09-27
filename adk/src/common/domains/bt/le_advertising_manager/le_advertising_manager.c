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

#include "local_name.h"

#include "le_advertising_manager.h"
#include "le_advertising_manager_sm.h"
#include "le_advertising_manager_private.h"

#include "logging.h"
#include "hydra_macros.h"

#if defined DEBUG_LOG_EXTRA

#define DEBUG_LOG_LEVEL_1 DEBUG_LOG /* Additional Failure Logs */
#define DEBUG_LOG_LEVEL_2 DEBUG_LOG /* Additional Information Logs */

#else

#define DEBUG_LOG_LEVEL_1(...) ((void)(0))
#define DEBUG_LOG_LEVEL_2(...) ((void)(0))

#endif

/*!< Task information for the advertising manager */
adv_mgr_task_data_t app_adv_manager;

static le_advert_start_params_t start_params;
static struct _adv_mgr_advert_t  advertData[1];
static struct _le_adv_mgr_scan_response_t scan_rsp_packet[1];

static le_adv_mgr_state_machine_t * sm = NULL;
static Task task_allow_all;
static Task task_enable_connectable;

static uint8* leAdvertisingManager_AddServices32(uint8* ad_data, uint8* space, const uint32 *service_list, uint16 services);
static void leAdvertisingManager_HandleEnableAdvertising(const LE_ADV_INTERNAL_MSG_ENABLE_ADVERTISING_T * message);
static void leAdvertisingManager_HandleNotifyRpaAddressChange(void);
static void leAdvertisingManager_HandleInternalStartRequest(const LE_ADV_MGR_INTERNAL_START_T * message);
static bool leAdvertisingManager_Start(const le_advert_start_params_t * params);
static void leAdvertisingManager_ScheduleAdvertisingStart(const le_adv_data_set_t set);

static void initAdvert(adv_mgr_advert_t *advert)
{
    memset(advert,0,sizeof(*advert));
}

adv_mgr_advert_t *AdvertisingManager_NewAdvert(void)
{
    adv_mgr_advert_t *advert = &advertData[0];
    
    if(FALSE == LeAdvertisingManagerSm_IsInitialised())
        return NULL;

    if(advert->in_use)
        return NULL;

    initAdvert(advert);

    advert->channel_map_mask = BLE_ADV_CHANNEL_ALL;
    advert->in_use = TRUE;

    return advert;
}

/*! \todo documented interface needs to check if we are advertising,
        stop advertising and then fail accordingly */
bool AdvertisingManager_DeleteAdvert(adv_mgr_advert_t *advert)
{
    /* Could just return T/F for an invalid advert, but should not happen */
    PanicFalse(VALID_ADVERT_POINTER(advert));

    if (!advert->in_use)
        return TRUE;

    /* Clear advert in case any rogue pointers remain */
    initAdvert(advert);

    advert->in_use = FALSE;
    return TRUE;
}


static bool setAdvertisingData(uint8 size_ad_data, const uint8 *ad_data)
{
    DEBUG_LOG_LEVEL_1("setAdvertisingData");
    
    if (0 == size_ad_data || size_ad_data > BLE_AD_PDU_SIZE || !ad_data)
    {
        DEBUG_LOG_LEVEL_1("setAdvertisingData Failure, Invalid length %d", size_ad_data);

        return FALSE;
    }

    for(int i=0;i<size_ad_data;i++)
    {
        DEBUG_LOG_LEVEL_2("setAdvertisingData Info, Advertising data Item is %x", ad_data[i]);
        
    }

    DEBUG_LOG_LEVEL_2("setAdvertisingData Info, Calling connection library API ConnectionDmBleSetAdvertisingDataReq with size %x", size_ad_data);
    
    ConnectionDmBleSetAdvertisingDataReq(size_ad_data,ad_data);

    return TRUE;
}

/* If the advert has space for the full local device name, or at least
   MIN_LOCAL_NAME_LENGTH, then reduce the space available by that much.

   That allows elements that precede the name to be truncated while
   leaving space for the name */
static uint8 reserveSpaceForLocalName(uint8* space, uint16 name_length)
{
    uint8 required_space = MIN(name_length, MIN_LOCAL_NAME_LENGTH);

    if((*space) >= required_space)
    {
        *space -= required_space;
        return required_space;
    }
    return 0;
}


static void restoreSpaceForLocalName(uint8* space, uint8 reserved_space)
{
    *space += reserved_space;
}


static uint8* addHeaderToAdData(uint8* ad_data, uint8* space, uint8 size, uint8 type)
{
    WRITE_AD_DATA(ad_data, space, size);
    WRITE_AD_DATA(ad_data, space, type);

    return ad_data;
}


static uint8* addUuidToAdData(uint8* ad_data, uint8* space, uint16 uuid)
{
    WRITE_AD_DATA(ad_data, space, uuid & 0xFF);
    WRITE_AD_DATA(ad_data, space, uuid >> 8);

    return ad_data;
}

/*! Add a 128-bit UUID to advertising data in correct format

    \param[in,out] ad_data  Pointer to the advertising data
    \param[in,out] space    Pointer to variable containing the space remaining in the advert
    \param[in] uuid         Pointer to the UUID128 to add
 */
static uint8* addUuid128ToAdData(uint8* ad_data, uint8* space, const le_advertising_manager_uuid128 *uuid)
{
    memcpy(ad_data, uuid->uuid, OCTETS_PER_128BIT_SERVICE);
    ad_data += OCTETS_PER_128BIT_SERVICE;
    *space -= OCTETS_PER_128BIT_SERVICE;

    return ad_data;
}


/*! Send a completion message for the current blocking operation (if any).
    If there is no task registered for the message, no message is sent */
static void sendBlockingResponse(connection_lib_status sts)
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
    else
    {

        
    }
}


static uint8* addName(uint8 *ad_data, uint8* space, uint16 size_local_name, const uint8 * local_name, bool shortened)
{
    uint8 name_field_length;
    uint8 name_data_length = size_local_name;
    uint8 ad_tag = ble_ad_type_complete_local_name;
    uint8 usable_space = USABLE_SPACE(space);

    if((name_data_length == 0) || usable_space <= 1)
        return ad_data;

    if (name_data_length > usable_space)
    {
        ad_tag = ble_ad_type_shortened_local_name;
        name_data_length = usable_space;
    }
    else if (shortened)
    {
        ad_tag = ble_ad_type_shortened_local_name;
    }

    name_field_length = AD_FIELD_LENGTH(name_data_length);
    ad_data = addHeaderToAdData(ad_data, space, name_field_length, ad_tag);

    /* Setup the local name advertising data */
    memmove(ad_data, local_name, name_data_length);
    ad_data += name_data_length;
    *space -= name_data_length;

    return ad_data;
}


static uint8* addFlags(uint8* ad_data, uint8* space, uint8 flags)
{
    uint8 usable_space = USABLE_SPACE(space);

    if (usable_space < 1)
    {
        
    }
    else
    {
        /* According to CSSv6 Part A, section 1.3 "FLAGS" states:
            "The Flags data type shall be included when any of the Flag bits are non-zero and the advertising packet
            is connectable, otherwise the Flags data type may be omitted"
         */
        ad_data = addHeaderToAdData(ad_data, space, FLAGS_DATA_LENGTH, ble_ad_type_flags);
        WRITE_AD_DATA(ad_data, space, flags);
    }

    return ad_data;
}


static uint8* addServices(uint8* ad_data, uint8* space, const uint16 *service_list, uint16 services)
{
    uint8 num_services_that_fit = NUM_SERVICES_THAT_FIT(space);

    if (services && num_services_that_fit)
    {
        uint8 service_data_length;
        uint8 service_field_length;
        uint8 ad_tag = ble_ad_type_complete_uuid16;

        if(services > num_services_that_fit)
        {
            ad_tag = ble_ad_type_more_uuid16;
            services = num_services_that_fit;
        }

        /* Setup AD data for the services */
        service_data_length = SERVICE_DATA_LENGTH(services);
        service_field_length = AD_FIELD_LENGTH(service_data_length);
        ad_data = addHeaderToAdData(ad_data, space, service_field_length, ad_tag);

        while ((*space >= OCTETS_PER_SERVICE) && services--)
        {
            ad_data = addUuidToAdData(ad_data, space, *service_list++);
        }

        return ad_data;
    }
    return ad_data;
}

static uint8* addServices128(uint8* ad_data, uint8* space, 
                             const le_advertising_manager_uuid128 *service_list, 
                             uint16 services)
{
    uint8 num_services_that_fit = NUM_128BIT_SERVICES_THAT_FIT(space);

    if (services && num_services_that_fit)
    {
        uint8 service_data_length;
        uint8 service_field_length;
        uint8 ad_tag = ble_ad_type_complete_uuid128;

        if(services > num_services_that_fit)
        {
            ad_tag = ble_ad_type_more_uuid128;
            services = num_services_that_fit;
        }

        /* Setup AD data for the services */
        service_data_length = SERVICE128_DATA_LENGTH(services);
        service_field_length = AD_FIELD_LENGTH(service_data_length);
        ad_data = addHeaderToAdData(ad_data, space, service_field_length, ad_tag);

        while ((*space >= OCTETS_PER_128BIT_SERVICE) && services--)
        {
            ad_data = addUuid128ToAdData(ad_data, space, service_list++);
        }

        return ad_data;
    }
    return ad_data;
}


static bool setupAdvert(adv_mgr_advert_t *advert)
{                
    uint8 space = MAX_AD_DATA_SIZE_IN_OCTETS * sizeof(uint8);
    uint8 *ad_start = (uint8*)PanicNull(malloc(space));
    uint8 *ad_head = ad_start;
    unsigned name_len = 0;
    uint8 space_reserved_for_name = 0;

    PanicFalse(VALID_ADVERT_POINTER(advert));

    if (advert->content.flags)
    {
        ad_head = addFlags(ad_head, &space, advert->flags);
    }

    if (advert->content.local_name)
    {
        name_len = strlen((char *)advert->local_name);
        space_reserved_for_name = reserveSpaceForLocalName(&space, name_len);
    }

    if (advert->content.services)
    {
        ad_head = addServices(ad_head, &space,
                              advert->services_uuid16,
                              advert->num_services_uuid16);
                                      
        ad_head = leAdvertisingManager_AddServices32(ad_head, &space,
                              advert->services_uuid32,
                              advert->num_services_uuid32);
                              
        ad_head = addServices128(ad_head, &space,
                                 advert->services_uuid128,
                                 advert->num_services_uuid128);
                                 

    }

    if (advert->content.local_name)
    {
        restoreSpaceForLocalName(&space, space_reserved_for_name);

        ad_head = addName(ad_head, &space, name_len, advert->local_name, advert->content.local_name_shortened);
        

    }
            
    if (setAdvertisingData(ad_head - ad_start, ad_start))
    {                
        AdvManagerGetTaskData()->blockingAdvert = advert;
        AdvManagerGetTaskData()->blockingCondition = ADV_SETUP_BLOCK_ADV_DATA_CFM;
    }
    else
    {
                
        sendBlockingResponse(fail);
        free(ad_start);
        return FALSE;
        
    }

    free(ad_start);
    return TRUE;
}


static void enableAdvertising(adv_mgr_advert_t *advert)
{
    /*! \todo Need to deal with gatt as well as advertising set this way */
    UNUSED(advert);
}


static void startAdvert(const ADV_MANAGER_START_ADVERT_T *message)
{
    AdvManagerGetTaskData()->blockingTask = message->requester;
    AdvManagerGetTaskData()->blockingOperation = APP_ADVMGR_ADVERT_START_CFM;

    setupAdvert(message->advert);

    /*! \todo implement sequencing */
    
    Panic();
    enableAdvertising(message->advert);
}


static void handleSetupAdvert(const ADV_MANAGER_SETUP_ADVERT_T *message)
{
    

    AdvManagerGetTaskData()->blockingTask = message->requester;
    AdvManagerGetTaskData()->blockingOperation = APP_ADVMGR_ADVERT_SET_DATA_CFM;

    setupAdvert(message->advert);
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
    else
    {
        
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
    else
    {
        
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

    if (!already_handled)
    {
        
    }

    return already_handled;
}

/*! Stopping any ongoing adverts.

    \todo message if successful ?  with task ?
*/
static void stopAllAdverts(void)
{
    adv_mgr_task_data_t *advMan = AdvManagerGetTaskData();

    if (ADV_MGR_STATE_ADVERTISING == advMan->state)
    {
        /*! \todo Stop advertising !!!! */
        
        Panic();
        advMan->state = ADV_MGR_STATE_INITIALISED;
    }
}


/*! \todo Somewhat more extensive validation */
static bool checkAdvert(adv_mgr_advert_t *advert)
{
    return VALID_ADVERT_POINTER(advert);
}


bool AdvertisingManager_SetAdvertData(adv_mgr_advert_t *advert, Task requester)
{
    MAKE_MESSAGE(ADV_MANAGER_SETUP_ADVERT);

    message->advert = advert;
    message->requester = requester;
    MessageSendConditionally(AdvManagerGetTask(), ADV_MANAGER_SETUP_ADVERT, message, &AdvManagerGetTaskData()->blockingCondition);

    return TRUE;
}



bool AdvertisingManager_Start(adv_mgr_advert_t *advert, Task requester)
{
    MAKE_MESSAGE(ADV_MANAGER_START_ADVERT);

    /*! \todo Need to decide if adv manager has multiple adverts or not */

    
    Panic();

    stopAllAdverts();

    if (!checkAdvert(advert))
        return FALSE;

    message->advert = advert;
    message->requester = requester;
    MessageSendConditionally(AdvManagerGetTask(), ADV_MANAGER_START_ADVERT, message, &AdvManagerGetTaskData()->blockingCondition);

    return TRUE;
}


bool AdvertisingManager_SetName(adv_mgr_advert_t *advert,uint8 *name)
{
    PanicFalse(VALID_ADVERT_POINTER(advert));
    PanicNull(name);

    unsigned name_len = strlen((char *)name);
    unsigned max_name_len = sizeof(advert->local_name)-1;   /* Avoid #define */
    bool shorten_name = name_len > max_name_len;

    advert->content.local_name = TRUE;
    advert->content.local_name_shortened = shorten_name;

    if (shorten_name)
    {
        name_len = max_name_len;
    }

    memset(advert->local_name,0,sizeof(advert->local_name));
    memcpy(advert->local_name,name,name_len);

    return TRUE;
}


void AdvertisingManager_UseLocalName(adv_mgr_advert_t *advert)
{
    uint8 *local_name = LocalName_GetPrefixedName();
    AdvertisingManager_SetName(advert, local_name);
}


bool AdvertisingManager_SetDiscoverableMode(adv_mgr_advert_t *advert,
                           adv_mgr_ble_discoverable_mode_t discoverable_mode)
{
    PanicFalse(VALID_ADVERT_POINTER(advert));

    uint16 flags = advert->flags & ~(BLE_FLAGS_GENERAL_DISCOVERABLE_MODE|BLE_FLAGS_LIMITED_DISCOVERABLE_MODE);

    if (discoverable_mode == ble_discoverable_mode_general)
    {
        flags |= BLE_FLAGS_GENERAL_DISCOVERABLE_MODE;
    }
    else if (discoverable_mode == ble_discoverable_mode_limited)
    {
        flags |= BLE_FLAGS_LIMITED_DISCOVERABLE_MODE;
    }

    advert->flags = flags;
    advert->content.flags = !!flags;

    return TRUE;
}


bool AdvertisingManager_SetReadNameReason(adv_mgr_advert_t *advert,
                                    adv_mgr_ble_gap_read_name_t reason)
{
    PanicFalse(VALID_ADVERT_POINTER(advert));

    uint16 flags = advert->flags & ~BLE_FLAGS_DUAL_HOST;

    /* broadcasting would set flag here also, but not supported */
    if(reason == ble_gap_read_name_associating)
    {
        flags |= BLE_FLAGS_DUAL_HOST;
    }
    advert->reason = reason;

    return TRUE;
}


static bool serviceExistsUuid16(adv_mgr_advert_t *advert, uint16 uuid)
{
    if (advert && advert->content.services)
    {
        unsigned service = advert->num_services_uuid16;

        while (service--)
        {
            if (advert->services_uuid16[service] == uuid)
            {
                return TRUE;
            }
        }
    }
    return FALSE;
}


static bool serviceExistsUuid128(adv_mgr_advert_t *advert, le_advertising_manager_uuid128 *uuid)
{
    if (advert && advert->content.services)
    {
        unsigned service = advert->num_services_uuid128;

        while (service--)
        {
            if (0 == memcmp(advert->services_uuid128[service].uuid, uuid->uuid, sizeof(uuid->uuid)))
            {
                return TRUE;
            }
        }
    }

    return FALSE;
}


bool AdvertisingManager_SetService(adv_mgr_advert_t *advert, uint16 uuid)
{
    PanicFalse(VALID_ADVERT_POINTER(advert));

    if (advert->num_services_uuid16 >= MAX_SERVICES_UUID16)
        return FALSE;

    if (serviceExistsUuid16(advert, uuid))
        return TRUE;

    advert->services_uuid16[advert->num_services_uuid16++] = uuid;
    advert->content.services = TRUE;

    return TRUE;
}


bool AdvertisingManager_SetServiceUuid128(adv_mgr_advert_t *advert, const uint8 *uuid)
{
    le_advertising_manager_uuid128 new_uuid;
    PanicFalse(VALID_ADVERT_POINTER(advert));

    if (advert->num_services_uuid128 >= MAX_SERVICES_UUID128)
        return FALSE;

    memcpy(new_uuid.uuid, uuid, sizeof(new_uuid.uuid));

    if (serviceExistsUuid128(advert, &new_uuid))
        return TRUE;

    advert->services_uuid128[advert->num_services_uuid128++] = new_uuid;
    advert->content.services = TRUE;

    return TRUE;
}


void AdvertisingManager_SetAdvertParams(adv_mgr_advert_t *advert, ble_adv_params_t *adv_params)
{
    PanicFalse(VALID_ADVERT_POINTER(advert));
    PanicFalse(adv_params);

    advert->interval_and_filter = *adv_params;
    advert->content.parameters = TRUE;
}


void AdvertisingManager_SetAdvertisingType(adv_mgr_advert_t *advert, ble_adv_type advert_type)
{
    PanicFalse(VALID_ADVERT_POINTER(advert));

    advert->advertising_type = advert_type;
    advert->content.advert_type = TRUE;
}


void AdvertisingManager_SetAdvertisingChannels(adv_mgr_advert_t *advert, uint8 channel_mask)
{
    PanicFalse(VALID_ADVERT_POINTER(advert));

    advert->channel_map_mask = channel_mask;
}


void AdvertisingManager_SetUseOwnRandomAddress(adv_mgr_advert_t *advert, bool use_random_address)
{
    PanicFalse(VALID_ADVERT_POINTER(advert));

    advert->use_own_random = use_random_address;
}

/* Number of clients supported that can register callbacks for advertising data */
#define MAX_NUMBER_OF_CLIENTS 10

/* Local database to store the callback information clients register */
static struct _le_adv_mgr_register database[MAX_NUMBER_OF_CLIENTS];

/* Local Function to initialise the local instance of scan response data structure */
static void leAdvertisingManager_InitScanResponse(void)
{
    memset(scan_rsp_packet,0,sizeof(struct _le_adv_mgr_scan_response_t));
}

/* Local function to check if the handle matches one of the existing entries in the registry */
static bool leAdvertisingManager_IsValidRegisterHandle(const struct _le_adv_mgr_register * handle)
{
    for(int i=0; i<MAX_NUMBER_OF_CLIENTS; i++)
    {
        if(&database[i] == handle)
        {
            return TRUE;
        }
    }
    
    return FALSE;   
    
}

/* Local Function to add 32-bit UUID into the packet */
static uint8* leAdvertisingManager_AddUuid32ToAdData(uint8* ad_data, uint8* space, uint32 uuid)
{
    WRITE_AD_DATA(ad_data, space, uuid & 0xFF);
    WRITE_AD_DATA(ad_data, space, (uuid >> 8) & 0xFF);
    WRITE_AD_DATA(ad_data, space, (uuid >> 16) & 0xFF);
    WRITE_AD_DATA(ad_data, space, (uuid >> 24) & 0xFF);

    return ad_data;
}

/* Local Function to add 32-bit UUID into the packet */
static uint8* leAdvertisingManager_AddServices32(uint8* ad_data, uint8* space, const uint32 *service_list, uint16 services)
{    
    uint8 num_services_that_fit = NUM_32BIT_SERVICES_THAT_FIT(space);

    if (services && num_services_that_fit)
    {
        uint8 service_data_length;
        uint8 service_field_length;
        uint8 ad_tag = ble_ad_type_complete_uuid32;

        if(services > num_services_that_fit)
        {
            ad_tag = ble_ad_type_more_uuid32;
            services = num_services_that_fit;
        }

        /* Setup AD data for the services */
        service_data_length = SERVICE32_DATA_LENGTH(services);
        service_field_length = AD_FIELD_LENGTH(service_data_length);
        ad_data = addHeaderToAdData(ad_data, space, service_field_length, ad_tag);

        while ((*space >= OCTETS_PER_32BIT_SERVICE) && services--)
        {
            ad_data = leAdvertisingManager_AddUuid32ToAdData(ad_data, space, *service_list++);
        }

        return ad_data;
    }
    return ad_data;
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
        
        PanicFalse(AdvertisingManager_DeleteAdvert(advertData));
        adv_task_data->blockingAdvert = NULL;
        LeAdvertisingManagerSm_SetState(le_adv_mgr_state_suspending);
        adv_task_data->blockingCondition = ADV_SETUP_BLOCK_ADV_ENABLE_CFM;
        le_adv_select_params_t params;
        params.set =  msg->set;
        leAdvertisingManager_ScheduleAdvertisingStart(params.set);
        return;
    }
    
    LeAdvertisingManagerSm_SetState(le_adv_mgr_state_initialised);
    
    leAdvertisingManager_SetDataSetBitmask(msg->set, TRUE);
    
    start_params.event = le_adv_event_type_connectable_general;
    
    leAdvertisingManager_Start(&start_params);
    
    MAKE_MESSAGE(LE_ADV_MGR_SELECT_DATASET_CFM);
    
    message->status = le_adv_mgr_status_success;
    
    MessageSendConditionally(leAdvertisingManager_GetTaskForDataSet(msg->set), LE_ADV_MGR_SELECT_DATASET_CFM, message,  &adv_task_data->blockingCondition );
    
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
    
    for(int i =0;i < MAX_NUMBER_OF_CLIENTS; i++)
    {
        
        Task task = database[i].task;
        if(NULL == task)
            continue;
        
        MessageSend(task, LE_ADV_MGR_RPA_TIMEOUT_IND, NULL);
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

/* Local Function to Decide whether to Suspend or Resume Advertising and act as decided */
static void leAdvertisingManager_SuspendOrResumeAdvertising(bool action)
{    
    
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();    
    
    if(TRUE == action)
    {            
        if(LeAdvertisingManagerSm_IsSuspended())
        {                
            
                
            if((leAdvertisingManager_IsConnectableAdvertisingEnabled()) && (leAdvertisingManager_IsAdvertisingAllowed()) && (leAdvertisingManager_IsDataSetSelected()))
            {
                
                LeAdvertisingManagerSm_SetState(le_adv_mgr_state_starting);
                adv_task_data->blockingCondition = ADV_SETUP_BLOCK_ADV_ENABLE_CFM;
            }
        }
        else if(LeAdvertisingManagerSm_IsSuspending())
        {                    
            if((leAdvertisingManager_IsConnectableAdvertisingEnabled()) && (leAdvertisingManager_IsAdvertisingAllowed()) && (leAdvertisingManager_IsDataSetSelected()))
            {     
                
                
                leAdvertisingManager_CancelPendingAllowDisallowMessages();
                
                leAdvertisingManager_CancelPendingEnableDisableConnectableMessages();
            
                leAdvertisingManager_ScheduleInternalEnableMessage(action);
            }
        }
    }
    else
    {                
        
                
        if(LeAdvertisingManagerSm_IsAdvertisingStarting())
        {                
            
                
            leAdvertisingManager_CancelPendingAllowDisallowMessages();
            leAdvertisingManager_CancelPendingEnableDisableConnectableMessages();
            leAdvertisingManager_ScheduleInternalEnableMessage(action);
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
static void leAdvertisingManager_SetAllowAdvertising(bool action)
{
    adv_mgr_task_data_t *advMan = AdvManagerGetTaskData();    
    
    advMan->is_advertising_allowed = TRUE;
    
    if(FALSE == action)
    {        
        advMan->is_advertising_allowed = FALSE;
    }
}

/* Local Function to convert octets to 16-bit UUID */
static uint16 leAdvertisingManager_ConvertToUuid16(const uint8 * buf)
{
    PanicNull((uint8*)buf);
    uint16 uuid = ( ( buf[1]<<8) & 0xFF00 ) | buf[0];
    return uuid;
}

/* Local Function to convert octets to 32-bit UUID */
static uint32 leAdvertisingManager_ConvertToUuid32(const uint8 * buf)
{
    PanicNull((uint8*)buf);
    uint32 uuid = ( ( buf[3]<<24) & 0xFF000000UL ) | (buf[2]<<16 & 0xFF0000UL) | ( ( buf[1]<<8) & 0xFF00 ) | buf[0];
    return uuid;
}

static bool leAdvertisingManager_ServiceExistsUuid32(adv_mgr_advert_t *advert, uint32 uuid)
{
    if (advert && advert->content.services)
    {
        unsigned service = advert->num_services_uuid32;

        while (service--)
        {
            if (advert->services_uuid32[service] == uuid)
            {
                return TRUE;
            }
        }
    }
    return FALSE;
}

/* Local Function to Add Individual 16-bit Service UUIDs into the Local Data Structure storing Advert/Scan Response Packet Data */
static void leAdvertisingManager_SetServiceUuid16(struct _le_adv_mgr_packet_t * packet, uint16 uuid)
{
    
    if(_le_adv_mgr_packet_type_scan_response == packet->packet_type)
    {
        
        
        packet->u.scan_response->services_uuid16[packet->u.scan_response->num_services_uuid16++] = uuid;
        
    }
    else if(_le_adv_mgr_packet_type_advert == packet->packet_type)
    {
        
        
        PanicFalse(VALID_ADVERT_POINTER(packet->u.advert));
        
        
            
        if (packet->u.advert->num_services_uuid16 >= MAX_SERVICES_UUID16)
            Panic();

        
        
        if (serviceExistsUuid16(packet->u.advert, uuid))
            return;

        packet->u.advert->services_uuid16[packet->u.advert->num_services_uuid16++] = uuid;
        packet->u.advert->content.services = TRUE;
    }

}

/* Local Function to Add Individual 32-bit Service UUIDs into the Local Data Structure storing Advert/Scan Response Packet Data */
static void leAdvertisingManager_SetServiceUuid32(struct _le_adv_mgr_packet_t * packet, uint32 uuid)
{
    if(_le_adv_mgr_packet_type_scan_response == packet->packet_type)
    {
        packet->u.scan_response->services_uuid32[packet->u.scan_response->num_services_uuid32++] = uuid;
        
    }
    else if(_le_adv_mgr_packet_type_advert == packet->packet_type)
    {
        PanicFalse(VALID_ADVERT_POINTER(packet->u.advert));
        
        if (packet->u.advert->num_services_uuid32 >= MAX_SERVICES_UUID32)
            Panic();

        if (leAdvertisingManager_ServiceExistsUuid32(packet->u.advert, uuid))
            return;

        packet->u.advert->services_uuid32[packet->u.advert->num_services_uuid32++] = uuid;
        packet->u.advert->content.services = TRUE;
    }
}

/* Local Function to Add Individual 128-bit Service UUIDs into the Local Data Structure storing Advert/Scan Response Packet Data */
static void leAdvertisingManager_SetServiceUuid128(struct _le_adv_mgr_packet_t * packet, const uint8 * uuid)
{
    if(_le_adv_mgr_packet_type_scan_response == packet->packet_type)
    {
        le_advertising_manager_uuid128 uuid_128;        
        memcpy(uuid_128.uuid, uuid, sizeof(uuid_128.uuid));        
        packet->u.scan_response->services_uuid128[packet->u.scan_response->num_services_uuid128++] = uuid_128;
        
    }
    else if(_le_adv_mgr_packet_type_advert == packet->packet_type)
    {
        le_advertising_manager_uuid128 new_uuid;
        PanicFalse(VALID_ADVERT_POINTER(packet->u.advert));
        
        if (packet->u.advert->num_services_uuid128 >= MAX_SERVICES_UUID128)
            Panic();
        
        memcpy(new_uuid.uuid, uuid, sizeof(new_uuid.uuid));

        if (serviceExistsUuid128(packet->u.advert, &new_uuid))
            return;

        packet->u.advert->services_uuid128[packet->u.advert->num_services_uuid128++] = new_uuid;
        packet->u.advert->content.services = TRUE;
    }
}

/* Local Function to walk through all 16-bit UUIDs in the data item */
static void leAdvertisingManager_Process16BitUuids(const uint8* data, const unsigned number, struct _le_adv_mgr_packet_t * packet )
{
    for(int i=0; i<number; i++)
    {
        uint16 uuid = leAdvertisingManager_ConvertToUuid16(&data[0 + (i*OCTETS_PER_SERVICE)]);
        leAdvertisingManager_SetServiceUuid16(packet, uuid);
        
    }
}

/* Local Function to walk through all 32-bit UUIDs in the data item */
static void leAdvertisingManager_Process32BitUuids(const uint8* data, const unsigned number, struct _le_adv_mgr_packet_t * packet  )
{
    for(int i=0;i < number;i++)
    {
        uint32 uuid_32 = leAdvertisingManager_ConvertToUuid32(&data[0 + (i*OCTETS_PER_32BIT_SERVICE)]);
        leAdvertisingManager_SetServiceUuid32(packet, uuid_32);
            
    }       
}

/* Local Function to walk through all 128-bit UUIDs in the data item */
static void leAdvertisingManager_Process128BitUuids(const uint8* data, const unsigned number, struct _le_adv_mgr_packet_t * packet  )
{
    for(int i=0;i < number;i++)
    {
        leAdvertisingManager_SetServiceUuid128(packet, data);
            
    }       
}

/* Local Function to add a single data item into scan response packet */
static void leAdvertisingManager_AddDataItemToScanResponse(const le_adv_data_item_t * advert_data)
{            
    if(0 == advert_data->size)
    {
        return;
    }
    
    if( 0 == (advert_data->data[0] ))
    {
        return;
    }
    
    if( advert_data->size != (advert_data->data[0] + 1 ))
    {
        return;
    }
     
    struct _le_adv_mgr_packet_t packet;
    packet.packet_type = _le_adv_mgr_packet_type_scan_response;
    packet.u.scan_response = scan_rsp_packet;
        
    switch(advert_data->data[1])
    {
        
    case ble_ad_type_complete_uuid16:
    {
        leAdvertisingManager_Process16BitUuids(&advert_data->data[2], (advert_data->size - 2)/OCTETS_PER_SERVICE, &packet);        
        break;
    }
    case ble_ad_type_complete_uuid32:
    {
        leAdvertisingManager_Process32BitUuids(&advert_data->data[2], (advert_data->size - 2)/OCTETS_PER_32BIT_SERVICE, &packet );
        break;
    }
    
    case ble_ad_type_complete_uuid128:
    {        
        leAdvertisingManager_Process128BitUuids(&advert_data->data[2], (advert_data->size - 2)/OCTETS_PER_128BIT_SERVICE, &packet );
        break;
    }
    
    case ble_ad_type_complete_local_name:
    {
        for(int i =0; i < (advert_data->data[0]-1) ; i++)
            scan_rsp_packet->local_name[i] = advert_data->data[i+2] ;
        break;
    }

    default:
    {
        
        Panic();
    }
    }
}
       
/* Local Function to add a single advertising data item into advetising packet */
static void leAdvertisingManager_AddDataItem(const le_adv_data_item_t * advert_data)
{        
    if(0 == advert_data->size)
    {
        return;
    }
    
    if( 0 == (advert_data->data[0] ))
    {
        return;
    }
    
    if( advert_data->size != (advert_data->data[0] + 1 ))
    {
        return;
    }
 
    adv_mgr_task_data_t *advMan = AdvManagerGetTaskData();
    struct _le_adv_mgr_packet_t packet;
    packet.packet_type = _le_adv_mgr_packet_type_advert;
    packet.u.advert = advMan->blockingAdvert;
    
    switch(advert_data->data[1])
    {
    case 0x0:
    {
        break;
    }    
    case ble_ad_type_complete_uuid16:
    {
        leAdvertisingManager_Process16BitUuids(&advert_data->data[2], (advert_data->size - 2)/OCTETS_PER_SERVICE, &packet );
        break;
    }
    
    case ble_ad_type_complete_uuid32:
    {
        leAdvertisingManager_Process32BitUuids(&advert_data->data[2], (advert_data->size - 2)/OCTETS_PER_32BIT_SERVICE, &packet );
        break;
    }

    case ble_ad_type_complete_uuid128:
    {
        leAdvertisingManager_Process128BitUuids(&advert_data->data[2], (advert_data->size - 2)/OCTETS_PER_128BIT_SERVICE, &packet );
        break;
    }

    case ble_ad_type_flags:
    {
        AdvertisingManager_SetDiscoverableMode(advMan->blockingAdvert, ble_discoverable_mode_limited);
        break;
    }
    
    case ble_ad_type_complete_local_name:
    {
        for(int i =0; i < (advert_data->data[0]-1) ; i++)
            advMan->blockingAdvert->local_name[i] = advert_data->data[i+2] ;
        advMan->blockingAdvert->content.local_name = TRUE;
        break;
    }

    default:
    {
        
        Panic();
    }
    }
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
    bool ret = leAdvertisingManager_CancelPendingAllowDisallowMessages();
    
    if(ret)
    {
        leAdvertisingManager_ScheduleAllowDisallowMessages(leAdvertisingManager_IsAdvertisingAllowed(), le_adv_mgr_status_error_unknown);
    }
    
    ret = leAdvertisingManager_CancelPendingEnableDisableConnectableMessages();
    
    if(ret)
    {
        leAdvertisingManager_ScheduleEnableDisableConnectableMessages(leAdvertisingManager_IsConnectableAdvertisingEnabled(), le_adv_mgr_status_error_unknown);

    }    
    
    LeAdvertisingManagerSm_SetState(le_adv_mgr_state_suspended);
    MessageCancelAll(AdvManagerGetTask(), LE_ADV_INTERNAL_MSG_NOTIFY_RPA_CHANGE);

}

/* Local Function to Send Scan Response Packet */
static bool leAdvertisingManager_SendScanResponsePacket(const struct _le_adv_mgr_scan_response_t * packet)
{           
    bool result = FALSE;
    uint8 space = MAX_AD_DATA_SIZE_IN_OCTETS * sizeof(uint8);
    uint8 *ad_start = (uint8*)PanicNull(malloc(space));
    uint8 *ad_head = ad_start;
    unsigned name_len = 0;
    uint8 space_reserved_for_name = 0;
    
    PanicNull((void*)packet);

    name_len = strlen((char *)packet->local_name);
    
    if(name_len)
    {
        space_reserved_for_name = reserveSpaceForLocalName(&space, name_len);
        
        restoreSpaceForLocalName(&space, space_reserved_for_name);
                
        ad_head = addName(ad_head, &space, name_len, packet->local_name, FALSE);   
    }
    
    if ((packet->num_services_uuid16) || (packet->num_services_uuid32) || (packet->num_services_uuid128))
    {
        ad_head = addServices(ad_head, &space,
                              packet->services_uuid16,
                              packet->num_services_uuid16);
                                      
        ad_head = leAdvertisingManager_AddServices32(ad_head, &space,
                              packet->services_uuid32,
                              packet->num_services_uuid32);
                              
        ad_head = addServices128(ad_head, &space,
                                 packet->services_uuid128,
                                 packet->num_services_uuid128);
                                 

    }

    if(ad_head > ad_start)
    {
        ConnectionDmBleSetScanResponseDataReq(ad_head - ad_start, ad_start );
        AdvManagerGetTaskData()->blockingCondition = ADV_SETUP_BLOCK_ADV_SCAN_RESPONSE_DATA_CFM;
        result = TRUE;
    }
    else
    {
        sendBlockingResponse(fail);
        
    }

    free(ad_start);
    return result;
}

/* Local Function to Collect the Data for Scan Response */
static bool leAdvertisingManager_SetupScanResponseData(le_adv_data_set_t set)
{
    bool result = FALSE;
    le_adv_data_item_t * item = PanicUnlessMalloc(sizeof(le_adv_data_item_t));
            
    le_adv_data_params_t data_params;
    data_params.data_set = set;
    data_params.placement = le_adv_data_placement_scan_response;
    data_params.completeness = le_adv_data_completeness_full;    
    
    for(int client_index=0;client_index<MAX_NUMBER_OF_CLIENTS;client_index++)
    {
        if(FALSE == database[client_index].in_use)
            continue;
        
        size_t size = database[client_index].callback.GetNumberOfItems(&data_params);
                            
        for(int data_index=0; data_index<size; data_index++)
        {
                    
            *item = database[client_index].callback.GetItem(&data_params, data_index);
            leAdvertisingManager_AddDataItemToScanResponse(item);
            
        }
        
        if(0 != size)
            database[client_index].callback.ReleaseItems(&data_params);
            
    }
    
    if(TRUE == leAdvertisingManager_SendScanResponsePacket(scan_rsp_packet))
        result = TRUE;
            
    free(item);
    item = NULL;
            
    return result;
}

/* Local Function to Walk Through the Clients Data Items with the Given Parameters and Add the Item into Advertising Packet */
static void leAdvertisingManager_ProcessItemsAndAddIntoAdvertisingPacket(const le_adv_data_params_t * data_params, le_adv_data_item_t * advert_data)
{           
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_ProcessItemsAndAddIntoAdvertisingPacket");
    
    for(int client_index=0;client_index<MAX_NUMBER_OF_CLIENTS;client_index++)
    {
        DEBUG_LOG_LEVEL_2("leAdvertisingManager_ProcessItemsAndAddIntoAdvertisingPacket Info, Client database index is %x", client_index);
        
        if(FALSE == database[client_index].in_use)
        {
            DEBUG_LOG_LEVEL_2("leAdvertisingManager_ProcessItemsAndAddIntoAdvertisingPacket Info, Client database index not in use, skip the database entry");
            
            continue;
        }
        
        size_t size = database[client_index].callback.GetNumberOfItems(data_params);

        for(int data_index = 0;data_index<size;data_index++)
        {
            DEBUG_LOG_LEVEL_2("leAdvertisingManager_ProcessItemsAndAddIntoAdvertisingPacket Info, Data item index is %x", data_index);
                    
            *advert_data = database[client_index].callback.GetItem(data_params, data_index);
            
            PanicNull(advert_data);
            
            leAdvertisingManager_AddDataItem(advert_data);
            
        }
        
        if(0 != size)
        {
            DEBUG_LOG_LEVEL_2("leAdvertisingManager_ProcessItemsAndAddIntoAdvertisingPacket Info, Release items");
                        
            database[client_index].callback.ReleaseItems(data_params);
        }
    }
}

/* Local Function to Collect the Data for Advertising */
static bool leAdvertisingManager_SetupAdvertData(void)
{    
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_SetupAdvertData");
    
    adv_mgr_task_data_t * adv_task_data = AdvManagerGetTaskData();
    
    adv_task_data->blockingCondition = ADV_SETUP_BLOCK_ADV_DATA_CFM;
    adv_task_data->blockingOperation = APP_ADVMGR_ADVERT_START_CFM;
    adv_task_data->blockingAdvert = AdvertisingManager_NewAdvert();
    PanicNull(adv_task_data->blockingAdvert);
    
    if(FALSE == adv_task_data->is_data_update_required)
    {        
        DEBUG_LOG_LEVEL_2("leAdvertisingManager_SetupAdvertData Info, Data Update is not Needed");
        
        return FALSE;
    }
    
    adv_task_data->is_data_update_required = FALSE;
    le_adv_data_item_t * advert_data = PanicUnlessMalloc(sizeof(le_adv_data_item_t));
    
    le_adv_data_params_t data_params;

    data_params.completeness = le_adv_data_completeness_full;    

    for(data_params.data_set = le_adv_data_set_handset_identifiable; data_params.data_set <= le_adv_data_set_peer; (data_params.data_set)<<=1)
    {
        DEBUG_LOG_LEVEL_2("leAdvertisingManager_SetupAdvertData Info, Walk through selected data sets, current data set index is %x", data_params.data_set);
        
        if(0 == (start_params.set & data_params.data_set))
        {
            DEBUG_LOG_LEVEL_2("leAdvertisingManager_SetupAdvertData Info, No match for the selected data sets");
            continue;
        }
        
        for(data_params.placement = le_adv_data_placement_advert; data_params.placement <= le_adv_data_placement_dont_care; data_params.placement++)
        {
            DEBUG_LOG_LEVEL_2("leAdvertisingManager_SetupAdvertData Info, Walk through data placement types, current placement type index is %x", data_params.placement);
            
            if(le_adv_data_placement_scan_response == data_params.placement )
            {
                DEBUG_LOG_LEVEL_2("leAdvertisingManager_SetupAdvertData Info, Skip scan response placement type while building advert packet");
                continue;
            }
            
            DEBUG_LOG_LEVEL_2("leAdvertisingManager_SetupAdvertData Info, Data placement is %x, Current set to collect data is %x", data_params.placement, data_params.data_set);
            leAdvertisingManager_ProcessItemsAndAddIntoAdvertisingPacket(&data_params, advert_data);    
        }
    }
    
    if(FALSE == setupAdvert(adv_task_data->blockingAdvert))
    {
        DEBUG_LOG_LEVEL_2("leAdvertisingManager_SetupAdvertData Info, There is currently no data, free temporary advert data");
        free(advert_data);
        adv_task_data->blockingCondition = ADV_SETUP_BLOCK_NONE;
        return FALSE;
    }
        
    DEBUG_LOG_LEVEL_2("leAdvertisingManager_SetupAdvertData Info, Setting up of data is complete, free temporary advert data");
        
    free(advert_data);
                
    return TRUE;
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
static void leAdvertisingManager_SetupIntervalAndFilter(void)
{    
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
static void leAdvertisingManager_SetupOwnAddressType(const le_adv_data_set_t set)
{
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();
    
    PanicNull(adv_task_data->blockingAdvert);
    
    if(set == le_adv_data_set_peer)
    {
        adv_task_data->blockingAdvert->use_own_random = OWN_ADDRESS_PUBLIC;
    }
    else
    {
        adv_task_data->blockingAdvert->use_own_random = OWN_ADDRESS_RANDOM;
    }
}

/* Local Function to Set Up Advertising Parameters */
static void leAdvertisingManager_SetupAdvertParams(const le_advert_start_params_t * params)
{           
    
        
    if(leAdvertisingManager_IsSetupLocalAddressRequired())
    {        
        
        
        leAdvertisingManager_SetupLocalAddress();
        return;
    }   
    
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();

    PanicNull(adv_task_data->blockingAdvert);
   
    adv_task_data->blockingAdvert->channel_map_mask = BLE_ADV_CHANNEL_ALL;
    
    leAdvertisingManager_SetupOwnAddressType(params->set);

    memset(&adv_task_data->blockingAdvert->interval_and_filter,0,sizeof(ble_adv_params_t));

    leAdvertisingManager_SetupAdvertEventType(params->event);
        
    if( le_adv_event_type_connectable_directed != params->event )
        leAdvertisingManager_SetupIntervalAndFilter();

    adv_mgr_advert_t *advert = adv_task_data->blockingAdvert;
    AdvManagerGetTaskData()->blockingCondition = ADV_SETUP_BLOCK_ADV_PARAMS_CFM;

    
        
    ConnectionDmBleSetAdvertisingParamsReq(advert->advertising_type,
                                                   advert->use_own_random,
                                                   advert->channel_map_mask,
                                                   &advert->interval_and_filter);
}

/* Local function to handle CL_DM_BLE_SET_ADVERTISING_DATA_CFM message */
static void leAdvertisingManager_HandleSetAdvertisingDataCfm(const CL_DM_BLE_SET_ADVERTISING_DATA_CFM_T* cfm)
{    
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleSetAdvertisingDataCfm");    
    
    if(AdvManagerGetTaskData()->blockingCondition == ADV_SETUP_BLOCK_ADV_DATA_CFM)
    {    
        if (success == cfm->status)
        {
            if(TRUE == leAdvertisingManager_SetupScanResponseData(le_adv_data_set_handset_identifiable))
            {
                return;
            }
            
            else
            {
                leAdvertisingManager_SetupAdvertParams(&start_params);
            }
        }
        else
        {
            

            sendBlockingResponse(fail);
        }
    }
    else
    {
        
        if(cfm->status != success)
        Panic();
    }
}

/* Local function to handle CL_DM_BLE_SET_SCAN_RESPONSE_DATA_CFM message */
static void leAdvertisingManager_HandleSetScanResponseDataCfm(const CL_DM_BLE_SET_SCAN_RESPONSE_DATA_CFM_T * cfm)
{    
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleSetScanResponseDataCfm");    
    
    if (AdvManagerGetTaskData()->blockingCondition == ADV_SETUP_BLOCK_ADV_SCAN_RESPONSE_DATA_CFM)
    {        
        if (success == cfm->status)
        {            
            DEBUG_LOG_LEVEL_2("leAdvertisingManager_HandleSetScanResponseDataCfm Info, CL_DM_BLE_SET_SCAN_RESPONSE_DATA_CFM received with success, configure advertising parameters");    
            
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
        DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleSetScanResponseDataCfm Failure, Message Received in Unexpected Blocking Condition %x", AdvManagerGetTaskData()->blockingCondition);    
            
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
        
            
        if (success == cfm->status)
        {            
            
            
            LeAdvertisingManagerSm_SetState(le_adv_mgr_state_starting);
            adv_task_data->blockingCondition = ADV_SETUP_BLOCK_ADV_ENABLE_CFM;

        }
        else
        {            
            
            
            

            Panic();
        }
    }
    else
    {        
        
        
        
                                    
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
        if (success == cfm->status)
        {
            adv_task_data->is_local_address_configured = TRUE;
            
            leAdvertisingManager_SetupAdvertParams(&start_params);
            
        }
        else
        {                        
            leAdvertisingManager_HandleLocalAddressConfigureCfmFailure();
            
            adv_task_data->blockingOperation  = 0;
            adv_task_data->blockingAdvert = NULL;
            adv_task_data->blockingCondition = ADV_SETUP_BLOCK_NONE;
                
            

        }
            
    }
    else
    {                    
        
        
        /* TO DO: Pending on Local Address Management Component */                            
        /*Panic();*/
    }
    
    
    
}

/* Local function to handle CL_DM_BLE_SET_ADVERTISE_ENABLE_CFM message */
static void leAdvertisingManager_HandleSetAdvertisingEnableCfm(const CL_DM_BLE_SET_ADVERTISE_ENABLE_CFM_T *cfm)
{        
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleSetAdvertisingEnableCfm");    
        
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();
    
    if (adv_task_data->blockingCondition == ADV_SETUP_BLOCK_ADV_ENABLE_CFM)
    {        
        
        
        if (success == cfm->status)
        {            
            
            
            if(TRUE == LeAdvertisingManagerSm_IsSuspending())
            {                
                
                
                LeAdvertisingManagerSm_SetState(le_adv_mgr_state_suspended);
                MessageCancelAll(AdvManagerGetTask(), LE_ADV_INTERNAL_MSG_NOTIFY_RPA_CHANGE);
                            

            }
            else if(TRUE == LeAdvertisingManagerSm_IsAdvertisingStarting())
            {                
                
                
                LeAdvertisingManagerSm_SetState(le_adv_mgr_state_started);   
                MessageSendLater(AdvManagerGetTask(), LE_ADV_INTERNAL_MSG_NOTIFY_RPA_CHANGE, NULL, D_SEC(BLE_RPA_TIMEOUT_DEFAULT));                

            }
            
        }
        else
        {            
            
            
            

            leAdvertisingManager_HandleSetAdvertisingEnableCfmFailure();
            adv_task_data->blockingAdvert = NULL;
        }

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

    if( (NULL == database[0].callback.GetItem) || (NULL == database[0].callback.GetNumberOfItems) )
    {
        DEBUG_LOG_LEVEL_1("leAdvertisingManager_Start Failure, Database is empty");
                
        return FALSE;
    }
        
    if(adv_task_data->is_data_update_required)
    {
        DEBUG_LOG_LEVEL_2("leAdvertisingManager_Start Info, Data update is needed");        
        
        if(FALSE == leAdvertisingManager_SetupAdvertData())
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
static void leAdvertisingManager_ScheduleAdvertisingStart(const le_adv_data_set_t set)
{
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_ScheduleAdvertisingStart");
    
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();
    
    MAKE_MESSAGE(LE_ADV_MGR_INTERNAL_START);
    message->set = set;
    
    MessageSendConditionally(AdvManagerGetTask(), LE_ADV_MGR_INTERNAL_START, message,  &adv_task_data->blockingCondition );
    
}

/* API Function to Register Callbacks for Advertising Data Items */
le_adv_mgr_register_handle LeAdvertisingManager_Register(Task task, const le_adv_data_callback_t * callback)
{       
    DEBUG_LOG("LeAdvertisingManager_Register");
    
    if(FALSE == LeAdvertisingManagerSm_IsInitialised())
        return NULL;

    if( (NULL == callback) || (NULL == callback->GetNumberOfItems) || (NULL == callback->GetItem) )
        return NULL;
    
    int i;
    for(i=0; i<MAX_NUMBER_OF_CLIENTS; i++)
    {
        DEBUG_LOG_LEVEL_2("LeAdvertisingManager_Register Info, Wak thorugh the client data base to register a new client");
        
        if(TRUE == database[i].in_use)
        {
            if((callback->GetNumberOfItems == database[i].callback.GetNumberOfItems) || (callback->GetItem ==  database[i].callback.GetItem))
                return NULL;
            else
                continue;
        }
        database[i].callback.GetNumberOfItems = callback->GetNumberOfItems;
        database[i].callback.GetItem = callback->GetItem;
        database[i].callback.ReleaseItems = callback->ReleaseItems;
        database[i].in_use = TRUE;
        
        database[i].task = task;
        break;
    }

    if(MAX_NUMBER_OF_CLIENTS == i)
    {
        DEBUG_LOG_LEVEL_1("LeAdvertisingManager_Register Failure, Reached Maximum Number of Clients");
        return NULL;
    }
    
    return &database[i];
}

/* API Function to Initialise LE Advertising Manager */
bool LeAdvertisingManager_Init(Task init_task)
{
    UNUSED(init_task);

    DEBUG_LOG("LeAdvertisingManager_Init");
    
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();
    
    memset(adv_task_data, 0, sizeof(*adv_task_data));
    adv_task_data->task.handler = handleMessage;
    
    initAdvert(advertData);
    leAdvertisingManager_InitScanResponse();
    leAdvertisingManager_ClearDataSetBitmask();

    adv_task_data->dataset_handset_handle = NULL;
    adv_task_data->dataset_peer_handle = NULL;
    adv_task_data->params_handle = NULL;
    
    sm = LeAdvertisingManagerSm_Init();
    PanicNull(sm);
    LeAdvertisingManagerSm_SetState(le_adv_mgr_state_initialised);
    
    for(int i=0; i<MAX_NUMBER_OF_CLIENTS; i++)
    {

        database[i].callback.GetNumberOfItems = NULL;
        database[i].callback.GetItem = NULL;
        database[i].callback.ReleaseItems = NULL;
        database[i].in_use = FALSE;
    }
    
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
       
    leAdvertisingManager_SuspendOrResumeAdvertising(enable);

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
    
    leAdvertisingManager_SuspendOrResumeAdvertising(allow);

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
        
        leAdvertisingManager_ScheduleAdvertisingStart(params->set);  
        
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
    
    leAdvertisingManager_SuspendOrResumeAdvertising(FALSE);
    
    MAKE_MESSAGE(LE_ADV_MGR_RELEASE_DATASET_CFM);
    message->status = le_adv_mgr_status_success;         
        
    MessageSendConditionally(leAdvertisingManager_GetTaskForDataSet(handle->set), LE_ADV_MGR_RELEASE_DATASET_CFM, message,  &adv_task_data->blockingCondition );
    
    leAdvertisingManager_FreeHandleForDataSet(handle->set);
    
    PanicFalse(AdvertisingManager_DeleteAdvert(advertData));
    adv_task_data->blockingAdvert = NULL;
    
    if(start_params.set)
    {
        DEBUG_LOG_LEVEL_2("LeAdvertisingManager_ReleaseAdvertisingDataSet Info, Local start parameters contain a valid set, reschedule advertising start with the set %x", start_params.set);
        adv_task_data->is_data_update_required = TRUE;
        leAdvertisingManager_ScheduleAdvertisingStart(start_params.set);  
    }
        
    return TRUE;
}

/* API function to notify a change in the data */
bool LeAdvertisingManager_NotifyDataChange(Task task, const le_adv_mgr_register_handle handle)
{    
    DEBUG_LOG("LeAdvertisingManager_NotifyDataChange");
        
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();
        
    if(FALSE == leAdvertisingManager_IsValidRegisterHandle(handle))
    {
        DEBUG_LOG_LEVEL_1("LeAdvertisingManager_NotifyDataChange Failure, Invalid Handle");
        
        return FALSE;
    }
    
    adv_task_data->is_data_update_required = TRUE;
    
    if(LeAdvertisingManagerSm_IsAdvertisingStarting() || LeAdvertisingManagerSm_IsAdvertisingStarted())
    {
        DEBUG_LOG_LEVEL_2("LeAdvertisingManager_NotifyDataChange Info, Advertising in progress, suspend and reschedule advertising");
        
        leAdvertisingManager_SuspendOrResumeAdvertising(FALSE);
        
        PanicFalse(AdvertisingManager_DeleteAdvert(advertData));
        adv_task_data->blockingAdvert = NULL;

        leAdvertisingManager_ScheduleAdvertisingStart(start_params.set);  
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
            leAdvertisingManager_SuspendOrResumeAdvertising(FALSE);
            
            leAdvertisingManager_ScheduleAdvertisingStart(start_params.set);
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
    
    own_address_config->own_address_type = le_adv_own_address_type_random;
    own_address_config->timeout = BLE_RPA_TIMEOUT_DEFAULT;
    
    return TRUE;
}
