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

#include "le_advertising_manager.h"
#include "le_advertising_manager_private.h"

#include "earbud_init.h"
#include "earbud_log.h"
#include "earbud_test.h"
#include "hydra_macros.h"

/*!< Task information for the advertising manager */
adv_mgr_task_data_t app_adv_manager;

static le_advert_start_params_t start_params;
static struct _adv_mgr_advert_t  advertData[1];
static struct _le_adv_mgr_scan_response_t scan_rsp_packet[1];
static uint8* leAdvertisingManager_AddServices32(uint8* ad_data, uint8* space, const uint32 *service_list, uint16 services);

static void initAdvert(adv_mgr_advert_t *advert)
{
    memset(advert,0,sizeof(*advert));
}

adv_mgr_advert_t *AdvertisingManager_NewAdvert(void)
{
    adv_mgr_advert_t *advert = &advertData[0];
    adv_mgr_task_data_t *advMan = AdvManagerGetTaskData();

    if (advMan->adv_state != le_adv_mgr_state_initialised)
        return NULL;

    if (advert->in_use)
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
    if (0 == size_ad_data || size_ad_data > BLE_AD_PDU_SIZE || !ad_data)
    {
        DEBUG_LOG("setAdvertisingData Bad length.");

        return FALSE;
    }

    for(int i=0;i<size_ad_data;i++)
    {
        DEBUG_LOG("Advertising Data Packet %x = %x",i, ad_data[i]);
    }

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


static void saveLocalName(const CL_DM_LOCAL_NAME_COMPLETE_T *name)
{
    adv_mgr_task_data_t *advMgr = AdvManagerGetTaskData();

    free(advMgr->localName);
    advMgr->localName = NULL;

    if (name->status == hci_success)
    {
        advMgr->localName = PanicNull(calloc(1, name->size_local_name+1));
        memcpy(advMgr->localName,name->local_name,name->size_local_name);
    }

    if (ADV_MGR_STATE_STARTING == advMgr->state)
    {
        advMgr->state = ADV_MGR_STATE_INITIALISED;

        MessageSend(appInitGetInitTask(), APP_ADVMGR_INIT_CFM, NULL);
    }
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

        DEBUG_LOG("sendBlockingResponse. Unexpected blocking operation:%d",
                    advMgr->blockingOperation);
        /*Panic();*/
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
        DEBUG_LOG("addFlags. No space for flags in advert");
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


static void setupAdvert(adv_mgr_advert_t *advert)
{                
    uint8 space = MAX_AD_DATA_SIZE_IN_OCTETS * sizeof(uint8);
    uint8 *ad_start = (uint8*)PanicNull(malloc(space));
    uint8 *ad_head = ad_start;
    unsigned name_len = 0;
    uint8 space_reserved_for_name = 0;

    PanicFalse(VALID_ADVERT_POINTER(advert));
        
    PanicFalse(advert->content.parameters);

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
        
    }

    free(ad_start);
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
    DEBUG_LOG("startAdvert not yet implemented with requester tasks / sequence");
    Panic();
    enableAdvertising(message->advert);
}


static void handleSetupAdvert(const ADV_MANAGER_SETUP_ADVERT_T *message)
{
    DEBUG_LOG("handleSetupAdvert message->requester %x message->advert %x", message->requester, message->advert);

    AdvManagerGetTaskData()->blockingTask = message->requester;
    AdvManagerGetTaskData()->blockingOperation = APP_ADVMGR_ADVERT_SET_DATA_CFM;

    setupAdvert(message->advert);
}


static void handleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch (id)
    {
            /* Connection library message sent directly */
        case CL_DM_LOCAL_NAME_COMPLETE:
            saveLocalName((const CL_DM_LOCAL_NAME_COMPLETE_T*)message);
            return;

            /* Internal messages */
        case ADV_MANAGER_START_ADVERT:
            startAdvert((const ADV_MANAGER_START_ADVERT_T*)message);
            return;

        case ADV_MANAGER_SETUP_ADVERT:
            handleSetupAdvert((const ADV_MANAGER_SETUP_ADVERT_T*)message);
            return;
    }

    DEBUG_LOG("handleMessage. Unhandled message. Id: 0x%X (%d) 0x%p", id, id, message);
}


static bool handleSetAdvertisingDataCfm(const CL_DM_BLE_SET_ADVERTISING_DATA_CFM_T* cfm)
{
    if (AdvManagerGetTaskData()->blockingCondition == ADV_SETUP_BLOCK_ADV_DATA_CFM)
    {
        
        if (success == cfm->status)
        {
            
            adv_mgr_advert_t *advert = AdvManagerGetTaskData()->blockingAdvert;

            DEBUG_LOG("handleSetAdvertisingDataCfm: success");

            AdvManagerGetTaskData()->blockingCondition = ADV_SETUP_BLOCK_ADV_PARAMS_CFM;

            ConnectionDmBleSetAdvertisingParamsReq(advert->advertising_type,
                                                   advert->use_own_random,
                                                   advert->channel_map_mask,
                                                   &advert->interval_and_filter);
        }
        else
        {
            DEBUG_LOG("handleSetAdvertisingDataCfm: failed:%d", cfm->status);

            sendBlockingResponse(fail);
        }
    }
    else
    {
        DEBUG_LOG("handleSetAdvertisingDataCfm. RECEIVED in unexpected blocking state %d",
                                    AdvManagerGetTaskData()->blockingCondition);
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
            DEBUG_LOG("handleSetAdvertisingDataCfm: failed:%d", cfm->status);

            sendBlockingResponse(fail);
        }
    }
    else
    {
        DEBUG_LOG("handleSetAdvertisingParamsCfm. RECEIVED in unexpected blocking state %d",
                                    AdvManagerGetTaskData()->blockingCondition);
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
        DEBUG_LOG("AdvertisingManager_HandleConnectionLibraryMessages. Unhandled message. Id: 0x%X (%d) 0x%p", id, id, message);
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
        DEBUG_LOG("Standalone adverts not implemented");
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

    DEBUG_LOG("AdvertisingManager_Start not implemented");
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
    uint8 *local_name = AdvManagerGetTaskData()->localName;

    if (NULL == local_name)
    {
        DEBUG_LOG("AdvertisingManager_UseLocalName No name set, using empty name");
        local_name = (uint8 *)"";
    }

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


/* Local Function to get the internal state of LE Advertising Manager */
static le_adv_mgr_status_t leAdvertisingManager_GetState(le_adv_mgr_state_t* state)
{    
    le_adv_mgr_status_t retVal = le_adv_mgr_status_error_unknown;
    
    adv_mgr_task_data_t *advMan = AdvManagerGetTaskData();
    
    if(le_adv_mgr_state_uninitialised == advMan->adv_state)
    {
        return retVal;
    }
    
    *state = advMan->adv_state;

    retVal = le_adv_mgr_status_success;
    
    return retVal;
}

/* Local Function to check if there is an active advertising operation already in progress */
static bool leAdvertisingManager_IsAdvertisingStarted(void)
{
    le_adv_mgr_status_t status = le_adv_mgr_status_error_unknown;
    le_adv_mgr_state_t state;
    status = leAdvertisingManager_GetState(&state);

    if((le_adv_mgr_status_error_unknown == status)||(le_adv_mgr_state_uninitialised == state))
    {
        return FALSE;
    }

    return (le_adv_mgr_state_started == state);
}

/* Local Function to check if Initialisation has already been completed with success */
static bool leAdvertisingManager_IsInitialised(void)
{
    le_adv_mgr_status_t status = le_adv_mgr_status_error_unknown;
    le_adv_mgr_state_t state;
    status = leAdvertisingManager_GetState(&state);

    if((le_adv_mgr_status_error_unknown == status)||(le_adv_mgr_state_uninitialised == state))
        return FALSE;

    return TRUE;
}

/* Local Function to check if the advertising is in suspended state */
static bool leAdvertisingManager_IsSuspended(void)
{
    le_adv_mgr_status_t status = le_adv_mgr_status_error_unknown;
    le_adv_mgr_state_t state;
    status = leAdvertisingManager_GetState(&state);

    if((le_adv_mgr_status_error_unknown == status)||(le_adv_mgr_state_uninitialised == state))
        return FALSE;

    return (le_adv_mgr_state_suspended == state);
}

/* Local Function to Suspend Advertising and Store the Existing Advertising Data and Parameter Set */
static void leAdvertisingManager_SuspendAdvertising(void)
{
    adv_mgr_task_data_t *advMan = AdvManagerGetTaskData();
    
    advMan->adv_state = le_adv_mgr_state_suspended;
    
    ConnectionDmBleSetAdvertiseEnable(FALSE);
}
           
/* Local Function to Resume Advertising with the Existing Advertising Data and Parameter Set */
static void leAdvertisingManager_ResumeAdvertising(void)
{    
    ConnectionDmBleSetAdvertiseEnable(TRUE);
    
}

/* Local Function to Decide whether to Suspend or Resume Advertising and act as decided */
static void leAdvertisingManager_SuspendOrResumeAdvertising(bool action)
{
    const unsigned bitmask_connectable_events = le_adv_event_type_connectable_general | le_adv_event_type_connectable_directed;
    
    adv_mgr_task_data_t *advMan = AdvManagerGetTaskData();
    
    if(TRUE == action)
    {
        if(leAdvertisingManager_IsSuspended())
        {
            if(bitmask_connectable_events == advMan->mask_enabled_events)
            {
                leAdvertisingManager_ResumeAdvertising();
            }
        }
    }
    else
    {
        if(leAdvertisingManager_IsAdvertisingStarted())
        {
            leAdvertisingManager_SuspendAdvertising();
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
        DEBUG_LOG("Unexpected Scan Response Packet Payload");
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
        DEBUG_LOG("Unexpected Advertising Packet Payload");
        Panic();
    }
    }
}

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
    for(int client_index=0;client_index<MAX_NUMBER_OF_CLIENTS;client_index++)
    {        
        if(FALSE == database[client_index].in_use)
            continue;
        
        size_t size = database[client_index].callback.GetNumberOfItems(data_params);

        for(int data_index = 0;data_index<size;data_index++)
        {
            *advert_data = database[client_index].callback.GetItem(data_params, data_index);
            leAdvertisingManager_AddDataItem(advert_data);
            
        }
        
        if(0 != size)
            database[client_index].callback.ReleaseItems(data_params);
    }
}

/* Local Function to Collect the Data for Advertising */
static bool leAdvertisingManager_SetupAdvertData(le_adv_data_set_t set)
{    
    adv_mgr_task_data_t * adv_task_data = AdvManagerGetTaskData();
    
    if(FALSE == adv_task_data->is_data_update_required)
        return FALSE;
    
    adv_task_data->is_data_update_required = FALSE;
    le_adv_data_item_t * advert_data = PanicUnlessMalloc(sizeof(le_adv_data_item_t));
    
    le_adv_data_params_t data_params;
    data_params.data_set = set;
    data_params.placement = le_adv_data_placement_advert;
    data_params.completeness = le_adv_data_completeness_full;    

    leAdvertisingManager_ProcessItemsAndAddIntoAdvertisingPacket(&data_params, advert_data);
    
    data_params.placement = le_adv_data_placement_dont_care;
    
    leAdvertisingManager_ProcessItemsAndAddIntoAdvertisingPacket(&data_params, advert_data);
    
    setupAdvert(adv_task_data->blockingAdvert);
        
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
            DEBUG_LOG("Invalid LE Advertising Event Type %x" , event);
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

/* Local Function to Set Up Advertising Parameters */
static void leAdvertisingManager_SetupAdvertParams(const le_advert_start_params_t * params)
{      
    adv_mgr_task_data_t *advMan = AdvManagerGetTaskData();
          
    advMan->blockingTask = advMan->dataset_handle->task;
    advMan->blockingCondition = ADV_SETUP_BLOCK_ADV_DATA_CFM;
    advMan->blockingOperation = APP_ADVMGR_ADVERT_START_CFM;
    advMan->blockingAdvert = AdvertisingManager_NewAdvert();

    PanicNull(advMan->blockingAdvert);
   
    advMan->blockingAdvert->channel_map_mask = BLE_ADV_CHANNEL_ALL;
    advMan->blockingAdvert->use_own_random = FALSE;

    memset(&advMan->blockingAdvert->interval_and_filter,0,sizeof(ble_adv_params_t));

    leAdvertisingManager_SetupAdvertEventType(params->event);
        
    if( le_adv_event_type_connectable_directed != params->event )
        leAdvertisingManager_SetupIntervalAndFilter();

    advMan->blockingAdvert->content.parameters = TRUE;
}

/* Local function to handle CL_DM_BLE_SET_ADVERTISING_DATA_CFM message */
static void leAdvertisingManager_HandleSetAdvertisingDataCfm(const CL_DM_BLE_SET_ADVERTISING_DATA_CFM_T* cfm)
{    
    if (AdvManagerGetTaskData()->blockingCondition == ADV_SETUP_BLOCK_ADV_DATA_CFM)
    {        
        if (success == cfm->status)
        {
            if(TRUE == leAdvertisingManager_SetupScanResponseData(le_adv_data_set_cooperative_identifiable))
            {
                return;
            }
            
            else
            {
                adv_mgr_advert_t *advert = AdvManagerGetTaskData()->blockingAdvert;
                DEBUG_LOG("leAdvertisingManager_HandleSetAdvertisingDataCfm: success");
                AdvManagerGetTaskData()->blockingCondition = ADV_SETUP_BLOCK_ADV_PARAMS_CFM;
                ConnectionDmBleSetAdvertisingParamsReq(advert->advertising_type,
                                                   advert->use_own_random,
                                                   advert->channel_map_mask,
                                                   &advert->interval_and_filter);
            }
        }
        else
        {
            
            DEBUG_LOG("leAdvertisingManager_HandleSetAdvertisingDataCfm: failed:%d", cfm->status);

            sendBlockingResponse(fail);
        }
    }
    else
    {
        DEBUG_LOG("leAdvertisingManager_HandleSetAdvertisingDataCfm. RECEIVED in unexpected blocking state %d",
                                    AdvManagerGetTaskData()->blockingCondition);
    }
}

/* Local function to handle CL_DM_BLE_SET_SCAN_RESPONSE_DATA_CFM message */
static void leAdvertisingManager_HandleSetScanResponseDataCfm(const CL_DM_BLE_SET_SCAN_RESPONSE_DATA_CFM_T * cfm)
{    
    if (AdvManagerGetTaskData()->blockingCondition == ADV_SETUP_BLOCK_ADV_SCAN_RESPONSE_DATA_CFM)
    {        
        if (success == cfm->status)
        {
            
            adv_mgr_advert_t *advert = AdvManagerGetTaskData()->blockingAdvert;
            DEBUG_LOG("handleSetAdvertisingDataCfm: success");
            AdvManagerGetTaskData()->blockingCondition = ADV_SETUP_BLOCK_ADV_PARAMS_CFM;
            ConnectionDmBleSetAdvertisingParamsReq(advert->advertising_type,
                                                   advert->use_own_random,
                                                   advert->channel_map_mask,
                                                   &advert->interval_and_filter);
        }
        else
        {
            
            DEBUG_LOG("leAdvertisingManager_HandleSetScanResponseDataCfm: failed:%d", cfm->status);

            sendBlockingResponse(fail);
        }
    }
    else
    {
        DEBUG_LOG("leAdvertisingManager_HandleSetScanResponseDataCfm. RECEIVED in unexpected blocking state %d",
                                    AdvManagerGetTaskData()->blockingCondition);
    }
}

/* Local function to handle CL_DM_BLE_SET_ADVERTISING_PARAMS_CFM message */
static void leAdvertisingManager_HandleSetAdvertisingParamCfm(const CL_DM_BLE_SET_ADVERTISING_PARAMS_CFM_T *cfm)
{
    adv_mgr_task_data_t *advMan = AdvManagerGetTaskData();
    
    if (advMan->blockingCondition == ADV_SETUP_BLOCK_ADV_PARAMS_CFM)
    {
                    
        if (success == cfm->status)
        {
            advMan->blockingOperation  = 0;
            advMan->blockingAdvert = NULL;
            advMan->blockingCondition = ADV_SETUP_BLOCK_NONE;
    
            ConnectionDmBleSetAdvertiseEnable(TRUE);
            
            /* TODO: B-287453 */
            MAKE_MESSAGE(LE_ADV_MGR_SELECT_DATASET_CFM);
            message->status = le_adv_mgr_status_success;            
            MessageSend(advMan->blockingTask, LE_ADV_MGR_SELECT_DATASET_CFM, message);

        }
        else
        {
            DEBUG_LOG("leAdvertisingManager_HandleSetAdvertisingParamCfm: failed:%d", cfm->status);

            sendBlockingResponse(fail);
        }
    }
    else
    {
        DEBUG_LOG("leAdvertisingManager_HandleSetAdvertisingParamCfm. RECEIVED in unexpected blocking state %d",
                                    advMan->blockingCondition);
    }
}

/* Local Function to Start Advertising */
static bool leAdvertisingManager_Start(const le_advert_start_params_t * params)
{    
    DEBUG_LOG("leAdvertisingManager_Start");

    adv_mgr_task_data_t *advMan = AdvManagerGetTaskData();
    
    if(FALSE == leAdvertisingManager_IsInitialised())
    {
        return FALSE;
    }
    
    if(TRUE == leAdvertisingManager_IsAdvertisingStarted())
    {
        return FALSE;
    }

    if(NULL == params)
    {
        return FALSE;
    }
    
    if(( FALSE == advMan->is_advertising_allowed))
    {
        return FALSE;
    }
    else
    {
        if(0UL == (advMan->mask_enabled_events & params->event))
            return FALSE;
        
    }

    if( (NULL == database[0].callback.GetItem) || (NULL == database[0].callback.GetNumberOfItems) )
    {
        return FALSE;
    }
    
    leAdvertisingManager_SetupAdvertParams(params);

    if(FALSE == leAdvertisingManager_SetupAdvertData(params->set))
    {
        adv_mgr_advert_t *advert = AdvManagerGetTaskData()->blockingAdvert;
        AdvManagerGetTaskData()->blockingCondition = ADV_SETUP_BLOCK_ADV_PARAMS_CFM;
        ConnectionDmBleSetAdvertisingParamsReq(advert->advertising_type,
                                                   advert->use_own_random,
                                                   advert->channel_map_mask,
                                                   &advert->interval_and_filter);
    }
        
    advMan->adv_state = le_adv_mgr_state_started;

    return TRUE;    
}

/* Local Function to Stop Advertising */
static bool leAdvertisingManager_Stop(void)
{
    DEBUG_LOG("leAdvertisingManager_Stop");
    
    adv_mgr_task_data_t *advMan = AdvManagerGetTaskData();
    
    PanicFalse(AdvertisingManager_DeleteAdvert(advertData));
    advMan->blockingAdvert = NULL;
    
    advMan->adv_state = le_adv_mgr_state_initialised;
    
    ConnectionDmBleSetAdvertiseEnable(FALSE);
    
    return TRUE;
}

/* API Function to Register Callbacks for Advertising Data Items */
le_adv_mgr_register_handle LeAdvertisingManager_Register(Task task, const le_adv_data_callback_t * callback)
{   
    UNUSED(task);
    
    DEBUG_LOG("LeAdvertisingManager_Register");
    
    if(FALSE == leAdvertisingManager_IsInitialised())
        return NULL;

    if( (NULL == callback) || (NULL == callback->GetNumberOfItems) || (NULL == callback->GetItem) )
        return NULL;
    
    int i;
    for(i=0; i<MAX_NUMBER_OF_CLIENTS; i++)
    {
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
        break;
    }

    if(MAX_NUMBER_OF_CLIENTS == i)
        return NULL;
    
    return &database[i];
}

/* API Function to Initialise LE Advertising Manager */
bool LeAdvertisingManager_Init(Task init_task)
{
    UNUSED(init_task);

    DEBUG_LOG("LeAdvertisingManager_Init");

    adv_mgr_task_data_t *advMan = AdvManagerGetTaskData();
    
    memset(advMan, 0, sizeof(*advMan));
    advMan->task.handler = handleMessage;
    
    initAdvert(advertData);
    leAdvertisingManager_InitScanResponse();

    advMan->dataset_handle = NULL;
    advMan->params_handle = NULL;
    
    advMan->adv_state = le_adv_mgr_state_initialised;
    
    for(int i=0; i<MAX_NUMBER_OF_CLIENTS; i++)
    {

        database[i].callback.GetNumberOfItems = NULL;
        database[i].callback.GetItem = NULL;
        database[i].callback.ReleaseItems = NULL;
        database[i].in_use = FALSE;
    }
    
    return TRUE;
}

/* API Function to De-Initialise LE Advertising Manager */
bool LeAdvertisingManager_DeInit(void)
{
    DEBUG_LOG("LeAdvertisingManager_DeInit");

    adv_mgr_task_data_t *advMan = AdvManagerGetTaskData();
    
    if(le_adv_mgr_state_uninitialised == advMan->adv_state)
        return FALSE;

    if(NULL != advMan->params_handle)
    {
        free(advMan->params_handle);
        advMan->params_handle = NULL;
    }
    
    if(NULL != advMan->dataset_handle)
    {
        free(advMan->dataset_handle);
        advMan->dataset_handle = NULL;
    }
    
    memset(advMan, 0, sizeof(adv_mgr_task_data_t));

    advMan->adv_state = le_adv_mgr_state_uninitialised;

    return TRUE;        
}

/* API Function to enable/disable connectable LE advertising */
bool LeAdvertisingManager_EnableConnectableAdvertising(Task task, bool enable)
{
    DEBUG_LOG("LeAdvertisingManager_EnableConnectableAdvertising");
    
    if(NULL == task)
        return FALSE;
    
    if(FALSE == leAdvertisingManager_IsInitialised())
        return FALSE;
       
    leAdvertisingManager_SetAllowedAdvertisingBitmaskConnectable(enable);
       
    leAdvertisingManager_SuspendOrResumeAdvertising(enable);
    
    MAKE_MESSAGE(LE_ADV_MGR_ENABLE_CONNECTABLE_CFM);
    message->enable = enable;
    message->status = le_adv_mgr_status_success;
    
    MessageSend(task, LE_ADV_MGR_ENABLE_CONNECTABLE_CFM, message);
    
    return TRUE;
}

/* API Function to enable/disable all LE advertising */
bool LeAdvertisingManager_AllowAdvertising(Task task, bool allow)
{
    DEBUG_LOG("LeAdvertisingManager_AllowAdvertising");
        
    if(NULL == task)
        return FALSE;
    
    if(FALSE == leAdvertisingManager_IsInitialised())
        return FALSE;
    
    leAdvertisingManager_SetAllowAdvertising(allow);
    
    leAdvertisingManager_SuspendOrResumeAdvertising(allow);
    
    MAKE_MESSAGE(LE_ADV_MGR_ALLOW_ADVERTISING_CFM);
    message->allow = allow;
    message->status = le_adv_mgr_status_success;
    
    MessageSend(task, LE_ADV_MGR_ALLOW_ADVERTISING_CFM, message);
    return TRUE;
}

/* API function to use as the handler for connection library messages */
bool LeAdvertisingManager_HandleConnectionLibraryMessages(MessageId id, Message message, bool already_handled)
{
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

        /* TODO: B-287453 */         
#define CL_DM_BLE_SET_ADVERTISE_ENABLE_CFM 20600
        case CL_DM_BLE_SET_ADVERTISE_ENABLE_CFM:
            return TRUE;            
            
    }

    if (!already_handled)
    {
        DEBUG_LOG("AdvertisingManager_HandleConnectionLibraryMessages. Unhandled message. Id: 0x%X (%d) 0x%p", id, id, message);
    }

    return already_handled;
}

/* API function to select the data set for undirected advertising */
le_adv_data_set_handle LeAdvertisingManager_SelectAdvertisingDataSet(Task task, const le_adv_select_params_t * params)
{
    DEBUG_LOG("LeAdvertisingManager_SelectAdvertisingDataSet");

    adv_mgr_task_data_t *advMan = AdvManagerGetTaskData();
    
    if(FALSE == leAdvertisingManager_IsInitialised())
    {
        return NULL;
    }
    
    if( (NULL == params) || (NULL == task) )
    {
        return NULL;
    }
    
    if(NULL != advMan->dataset_handle)
        return NULL;
    
    advMan->dataset_handle = PanicUnlessMalloc(sizeof(struct _le_adv_data_set));
    memset(advMan->dataset_handle,0, sizeof(struct _le_adv_data_set));
    advMan->dataset_handle->task = task;
    advMan->is_data_update_required = TRUE;
    
    start_params.set = params->set;
    start_params.event = le_adv_event_type_connectable_general;
    
    leAdvertisingManager_Start(&start_params);
    
    return advMan->dataset_handle;
    
}

/* API function to release the data set for undirected advertising */
bool LeAdvertisingManager_ReleaseAdvertisingDataSet(le_adv_data_set_handle handle)
{
    DEBUG_LOG("LeAdvertisingManager_ReleaseAdvertisingDataSet");

    adv_mgr_task_data_t *advMan = AdvManagerGetTaskData();
    
    if(NULL == handle)
        return FALSE;

    leAdvertisingManager_Stop();
    
    MAKE_MESSAGE(LE_ADV_MGR_RELEASE_DATASET_CFM);
    message->status = le_adv_mgr_status_success; 
    
    MessageSend(advMan->dataset_handle->task, LE_ADV_MGR_RELEASE_DATASET_CFM, message);
    
    free(advMan->dataset_handle);
    advMan->dataset_handle = NULL;
    
    return TRUE;
}

/* API function to notify a change in the data */
bool LeAdvertisingManager_NotifyDataChange(Task task, const le_adv_mgr_register_handle handle)
{
    DEBUG_LOG("LeAdvertisingManager_NotifyDataChange");
        
    adv_mgr_task_data_t *advMan = AdvManagerGetTaskData();
        
    if(FALSE == leAdvertisingManager_IsValidRegisterHandle(handle))
        return FALSE;
    
    if(TRUE == leAdvertisingManager_IsAdvertisingStarted())
    {
        advMan->is_data_update_required = TRUE;
        leAdvertisingManager_Stop();
        leAdvertisingManager_Start(&start_params);
        MAKE_MESSAGE(LE_ADV_MGR_NOTIFY_DATA_CHANGE_CFM);
        message->status = le_adv_mgr_status_success;
        MessageSend(task, LE_ADV_MGR_NOTIFY_DATA_CHANGE_CFM, message);
        return TRUE;
    }
    
    return TRUE;
        
}

/* API function to register LE advertising parameter sets */
bool LeAdvertisingManager_ParametersRegister(const le_adv_parameters_t *params)
{
    DEBUG_LOG("LeAdvertisingManager_ParametersRegister");
    
    if(NULL == params)
        return FALSE;
    
    if(FALSE == leAdvertisingManager_IsInitialised())
        return FALSE;
    
    adv_mgr_task_data_t *advMan = AdvManagerGetTaskData();
    
    advMan->params_handle =  PanicUnlessMalloc(sizeof(struct _le_adv_params_set));
    memset(advMan->params_handle, 0, sizeof(struct _le_adv_params_set));
    
    advMan->params_handle->params_set = (le_adv_parameters_set_t *)params->sets;    
    
    return TRUE;
    
}

/* API function to select the active LE advertising parameter set */
bool LeAdvertisingManager_ParametersSelect(uint8 index)
{
    DEBUG_LOG("LeAdvertisingManager_ParametersSelect index is %d", index);

    if(FALSE == leAdvertisingManager_IsInitialised())
    {
        return FALSE;
    }
    
    if((le_adv_preset_advertising_interval_low != index) && (le_adv_preset_advertising_interval_high != index))
    {
        return FALSE;
    }

    adv_mgr_task_data_t *advMan = AdvManagerGetTaskData();
    
    if(NULL == advMan->params_handle)
    {
        return FALSE;
    }
    
    if(index != advMan->params_handle->active_params_set)
    {
        advMan->params_handle->active_params_set = index;
        if(leAdvertisingManager_IsAdvertisingStarted())
        {
            leAdvertisingManager_Stop();
            leAdvertisingManager_Start(&start_params);
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

    if(FALSE == leAdvertisingManager_IsInitialised())
    {
        return FALSE;
    }
    
    adv_mgr_task_data_t *advMan = AdvManagerGetTaskData();
    le_adv_params_set_handle handle = advMan->params_handle;
    
    if(NULL == advMan->params_handle)
    {
        return FALSE;
    }
    
    if(le_adv_preset_advertising_interval_max >= advMan->params_handle->active_params_set)
    {
        interval->le_adv_interval_min = handle->params_set->set_type[handle->active_params_set].le_adv_interval_min;
        interval->le_adv_interval_max = handle->params_set->set_type[handle->active_params_set].le_adv_interval_max;
    }
    
    return TRUE;
}


