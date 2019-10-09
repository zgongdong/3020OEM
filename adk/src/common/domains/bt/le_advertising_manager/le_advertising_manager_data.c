/*!
\copyright  Copyright (c) 2018 - 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Manage execution of callbacks to construct adverts and scan response
*/

#include "le_advertising_manager_data.h"
#include "le_advertising_manager_advert.h"
#include "le_advertising_manager_format.h"
#include "le_advertising_manager_clients.h"

#include <stdlib.h>
#include <panic.h>

static struct _le_adv_mgr_scan_response_t scan_rsp_packet[1];

/* Local Function to initialise the local instance of scan response data structure */
void leAdvertisingManager_InitScanResponse(void)
{
    memset(scan_rsp_packet,0,sizeof(struct _le_adv_mgr_scan_response_t));
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
            PanicFalse(AdvertisingManager_AddRawData(&packet, advert_data->data, advert_data->size));
            break;
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
            PanicFalse(AdvertisingManager_AddRawData(&packet, advert_data->data, advert_data->size));
            break;
        }
    }
}

/* Local Function to Collect the Data for Scan Response */
bool leAdvertisingManager_SetupScanResponseData(le_adv_data_set_t set)
{
    le_adv_mgr_client_iterator_t iterator;
    le_adv_mgr_register_handle client_handle = leAdvertisingManager_HeadClient(&iterator);
    
    le_adv_data_item_t * item = PanicUnlessMalloc(sizeof(le_adv_data_item_t));
    
    le_adv_data_params_t data_params;
    data_params.data_set = set;
    data_params.placement = le_adv_data_placement_scan_response;
    data_params.completeness = le_adv_data_completeness_full;    
    
    while(client_handle)
    {
        if(client_handle->in_use)
        {
            size_t num_items = client_handle->callback.GetNumberOfItems(&data_params);
            
            for(int data_index = 0; data_index < num_items; data_index++)
            {
                *item = client_handle->callback.GetItem(&data_params, data_index);
                leAdvertisingManager_AddDataItemToScanResponse(item);
            }
            
            if(num_items)
            {
                client_handle->callback.ReleaseItems(&data_params);
            }
        }
        
        client_handle = leAdvertisingManager_NextClient(&iterator);
    }
    
    free(item);
    item = NULL;
    
    bool result = leAdvertisingManager_RegisterScanResponse(scan_rsp_packet);
    
    struct _le_adv_mgr_packet_t packet;
    packet.packet_type = _le_adv_mgr_packet_type_scan_response;
    packet.u.scan_response = scan_rsp_packet;
    AdvertisingManager_FreeRawData(&packet);
    
    return result;
}

/* Local Function to Walk Through the Clients Data Items with the Given Parameters and Add the Item into Advertising Packet */
static void leAdvertisingManager_ProcessItemsAndAddIntoAdvertisingPacket(const le_adv_data_params_t * data_params, le_adv_data_item_t * advert_data)
{
    le_adv_mgr_client_iterator_t iterator;
    le_adv_mgr_register_handle client_handle = leAdvertisingManager_HeadClient(&iterator);
    
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_ProcessItemsAndAddIntoAdvertisingPacket");
    
    while(client_handle)
    {
        if(client_handle->in_use)
        {
            size_t num_items = client_handle->callback.GetNumberOfItems(data_params);

            for(int data_index = 0; data_index < num_items; data_index++)
            {
                *advert_data = client_handle->callback.GetItem(data_params, data_index);
                PanicNull(advert_data);
                leAdvertisingManager_AddDataItem(advert_data);
            }
            
            if(num_items)
            {
                DEBUG_LOG_LEVEL_2("leAdvertisingManager_ProcessItemsAndAddIntoAdvertisingPacket Info, Release items");
                client_handle->callback.ReleaseItems(data_params);
            }
        }
        
        client_handle = leAdvertisingManager_NextClient(&iterator);
    }
}

/* Local Function to Collect the Data for Advertising */
bool leAdvertisingManager_SetupAdvertData(le_adv_data_set_t set)
{    
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_SetupAdvertData");
    
    adv_mgr_task_data_t * adv_task_data = AdvManagerGetTaskData();
    
    adv_task_data->blockingCondition = ADV_SETUP_BLOCK_ADV_DATA_CFM;
    adv_task_data->blockingOperation = APP_ADVMGR_ADVERT_START_CFM;
    
    if(adv_task_data->blockingAdvert)
    {
        DEBUG_LOG_LEVEL_1("leAdvertisingManager_SetupAdvertData Failure, Blocking advert is non-NULL");
    }
    
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
        
        if(0 == (set & data_params.data_set))
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
    
    bool result = leAdvertisingManager_RegisterAdvert(adv_task_data->blockingAdvert);
    
    if(!result)
    {
        DEBUG_LOG_LEVEL_2("leAdvertisingManager_SetupAdvertData Info, There is currently no data");
        adv_task_data->blockingCondition = ADV_SETUP_BLOCK_NONE;
        AdvertisingManager_DeleteAdvert(adv_task_data->blockingAdvert );
    }
    else
    {
        DEBUG_LOG_LEVEL_2("leAdvertisingManager_SetupAdvertData Info, Setting up of data is complete");
    }
    
    struct _le_adv_mgr_packet_t packet;
    packet.packet_type = _le_adv_mgr_packet_type_advert;
    packet.u.advert = adv_task_data->blockingAdvert;
    AdvertisingManager_FreeRawData(&packet);
    
    free(advert_data);
    
    return result;
}

void leAdvertisingManager_ClearAdvertData(void)
{
    adv_mgr_task_data_t * adv_task_data = AdvManagerGetTaskData();
    if(adv_task_data->blockingAdvert)
    {
        PanicFalse(AdvertisingManager_DeleteAdvert(adv_task_data->blockingAdvert));
        adv_task_data->blockingAdvert = NULL;
    }
}
