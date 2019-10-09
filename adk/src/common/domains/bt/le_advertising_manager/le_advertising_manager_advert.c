/*!
\copyright  Copyright (c) 2018 - 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Management of an advert
*/

#include "le_advertising_manager_advert.h"
#include "le_advertising_manager_format.h"
#include "le_advertising_manager_sm.h"

#include "local_name.h"

#include <stdlib.h>
#include <panic.h>

/*! Helper macro to check if a pointer to advert settings is valid */
#define VALID_ADVERT_POINTER(x) ((x)== &advertData[0])

static struct _adv_mgr_advert_t  advertData[1];

static void initAdvert(adv_mgr_advert_t *advert)
{
    memset(advert,0,sizeof(*advert));
}

void leAdvertisingManager_AdvertInit(void)
{
    initAdvert(advertData);
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

bool leAdvertisingManager_RegisterAdvert(adv_mgr_advert_t *advert)
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
    
    for(int i = 0; i < advert->num_raw_data; i++)
    {
        ad_head = addRawData(ad_head, &space, advert->raw_data[i]);
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


bool leAdvertisingManager_RegisterScanResponse(const struct _le_adv_mgr_scan_response_t * scan_response)
{           
    bool result = FALSE;
    uint8 space = MAX_AD_DATA_SIZE_IN_OCTETS * sizeof(uint8);
    uint8 *ad_start = (uint8*)PanicNull(malloc(space));
    uint8 *ad_head = ad_start;
    unsigned name_len = 0;
    uint8 space_reserved_for_name = 0;
    
    PanicNull((void*)scan_response);

    name_len = strlen((char *)scan_response->local_name);
    
    if(name_len)
    {
        space_reserved_for_name = reserveSpaceForLocalName(&space, name_len);
        
        restoreSpaceForLocalName(&space, space_reserved_for_name);
                
        ad_head = addName(ad_head, &space, name_len, scan_response->local_name, FALSE);   
    }
    
    if ((scan_response->num_services_uuid16) || (scan_response->num_services_uuid32) || (scan_response->num_services_uuid128))
    {
        ad_head = addServices(ad_head, &space,
                              scan_response->services_uuid16,
                              scan_response->num_services_uuid16);
                                      
        ad_head = leAdvertisingManager_AddServices32(ad_head, &space,
                              scan_response->services_uuid32,
                              scan_response->num_services_uuid32);
                              
        ad_head = addServices128(ad_head, &space,
                                 scan_response->services_uuid128,
                                 scan_response->num_services_uuid128);
    }

    for(int i = 0; i < scan_response->num_raw_data; i++)
    {
        ad_head = addRawData(ad_head, &space, scan_response->raw_data[i]);
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

/* Local Function to Add Individual 16-bit Service UUIDs into the Local Data Structure storing Advert/Scan Response Packet Data */
void leAdvertisingManager_SetServiceUuid16(struct _le_adv_mgr_packet_t * packet, uint16 uuid)
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
void leAdvertisingManager_SetServiceUuid32(struct _le_adv_mgr_packet_t * packet, uint32 uuid)
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
void leAdvertisingManager_SetServiceUuid128(struct _le_adv_mgr_packet_t * packet, const uint8 * uuid)
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


bool AdvertisingManager_AddRawData(struct _le_adv_mgr_packet_t * packet, const uint8* data, uint8 size)
{
    if(packet->packet_type == _le_adv_mgr_packet_type_advert)
    {
        adv_mgr_advert_t *advert = packet->u.advert;
        PanicFalse(VALID_ADVERT_POINTER(advert));

        if (advert->num_raw_data >= MAX_RAW_DATA)
            return FALSE;

        advert->raw_data[advert->num_raw_data] = PanicUnlessMalloc(size);
        memcpy(advert->raw_data[advert->num_raw_data], data, size);
        
        advert->num_raw_data++;
    }
    else if(packet->packet_type == _le_adv_mgr_packet_type_scan_response)
    {
        struct _le_adv_mgr_scan_response_t* scan_response = packet->u.scan_response;
        
        if (scan_response->num_raw_data >= MAX_RAW_DATA)
            return FALSE;

        scan_response->raw_data[scan_response->num_raw_data] = PanicUnlessMalloc(size);
        memcpy(scan_response->raw_data[scan_response->num_raw_data], data, size);
        
        scan_response->num_raw_data++;
    }
    else
    {
        Panic();
        return FALSE;
    }
    
    return TRUE;
}

void AdvertisingManager_FreeRawData(struct _le_adv_mgr_packet_t * packet)
{
    if(packet->packet_type == _le_adv_mgr_packet_type_advert)
    {
        adv_mgr_advert_t *advert = packet->u.advert;
        for(int i = 0; i < advert->num_raw_data; i++)
        {
            free(advert->raw_data[i]);
            advert->raw_data[i] = NULL;
        }
        advert->num_raw_data = 0;
    }
    else if(packet->packet_type == _le_adv_mgr_packet_type_scan_response)
    {
        struct _le_adv_mgr_scan_response_t* scan_response = packet->u.scan_response;
        for(int i = 0; i < scan_response->num_raw_data; i++)
        {
            free(scan_response->raw_data[i]);
            scan_response->raw_data[i] = NULL;
        }
        scan_response->num_raw_data = 0;
    }
    else
    {
        Panic();
    }
    
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
