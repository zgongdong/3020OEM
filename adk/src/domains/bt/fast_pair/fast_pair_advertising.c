/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file        fast_pair_advertising.c
\brief      Handles Fast Pair Advertising Data
*/

/*! Firmware and Library Headers */
#include <util.h>
#include <stdlib.h>
#include <message.h>
#include <logging.h>
#include <panic.h>
#include <stdio.h>
#include <connection_manager.h>
#include <connection.h>
#include <string.h>
#include <cryptovm.h>


/*! Application Headers */
#include "le_advertising_manager.h"
#include "fast_pair_advertising.h"
#include "fast_pair_bloom_filter.h"
#include "fast_pair.h"
#include "fast_pair_session_data.h"
#include "tx_power.h"

/*! Global Instance of fastpair advertising data */
fastpair_advert_data_t fastpair_advert;

/*! Module Constants */
#define FAST_PAIR_GOOGLE_IDENTIFIER 0xFE2C
#define FAST_PAIR_MODEL_IDENTIFIER   0x9D893B

#define FAST_PAIR_ADV_ITEM_BLE_FLAGS 0 /* BLE flags required for fast pairing */
#define FAST_PAIR_ADV_ITEM_MODEL_ID 1 /*During BR/EDR Connectable and Discoverable*/
#define FAST_PAIR_ADV_ITEM_BLOOM_FILTER_ID 1 /*During BR/EDR Connectable and Non-Discoverable*/

/*Google Identifier, Model ID in Identifiable mode and Google Identifier, Hashed Account key (Bloom Filter) including Salt in Unidentifiable mode*/
/*Note transmit power needed for fastpair adverts will go as part of Tx_power module*/
#define FAST_PAIR_AD_ITEMS_IDENTIFIABLE 2
#define FAST_PAIR_AD_ITEMS_UNIDENTIFIABLE_WITH_ACCOUNT_KEYS 2

/*All Adv Interval values are in ms (units of 0.625)*/
/* Adv interval when BR/EDR is discoverable should be <=100ms*/
#define FP_ADV_INTERVAL_IDENTIFIABLE_MIN     128
#define FP_ADV_INTERVAL_IDENTIFIABLE_MAX     160

/*FP when BR/EDR non-discoverable/Silent Pairing*/
#define FP_ADV_INTERVAL_UNIDENTIFIABLE_MIN   320
#define FP_ADV_INTERVAL_UNIDENTIFIABLE_MAX   400

/*! Const fastpair advertising data in Length, Tag, Value format*/
#define FP_SIZE_AD_TYPE_FIELD 1 
#define FP_SIZE_LENGTH_FIELD 1
#define FP_SIZE_BLE_FLAGS 1

#define SIZE_GOOGLE_ID 2
#define SIZE_GOOGLE_ID_ADV (FP_SIZE_LENGTH_FIELD+FP_SIZE_AD_TYPE_FIELD+SIZE_GOOGLE_ID)

#define SIZE_BLE_FLAGS_ADV (FP_SIZE_LENGTH_FIELD+FP_SIZE_AD_TYPE_FIELD+FP_SIZE_BLE_FLAGS)

static bool IsHandsetConnAllowed = FALSE;

static const uint8 fp_ble_flags_adv[SIZE_BLE_FLAGS_ADV] = 
{ 
    SIZE_BLE_FLAGS_ADV - 1,
    ble_ad_type_flags, 
    (BLE_FLAGS_GENERAL_DISCOVERABLE_MODE | BLE_FLAGS_DUAL_CONTROLLER | BLE_FLAGS_DUAL_HOST)
};

static const le_adv_data_item_t fp_ble_flags_data_item =
{
    SIZE_BLE_FLAGS_ADV,
    fp_ble_flags_adv
};

static const uint8 fp_google_id_adv[SIZE_GOOGLE_ID_ADV] = 
{ 
    SIZE_GOOGLE_ID_ADV - 1,
    ble_ad_type_service_data, 
    FAST_PAIR_GOOGLE_IDENTIFIER & 0xFF, 
    (FAST_PAIR_GOOGLE_IDENTIFIER >> 8) & 0xFF
};

static const le_adv_data_item_t fp_google_id_data_item =
{
    SIZE_GOOGLE_ID_ADV,
    fp_google_id_adv
};

/*! Const fastpair advertising data in Length, Tag, Value format*/
#define SIZE_MODEL_ID 3
#define SIZE_MODEL_ID_ADV (FP_SIZE_LENGTH_FIELD+FP_SIZE_AD_TYPE_FIELD+SIZE_MODEL_ID + SIZE_GOOGLE_ID)

static const uint8 fp_model_id_adv[SIZE_MODEL_ID_ADV] = 
{ 
    SIZE_MODEL_ID_ADV - 1,
    ble_ad_type_service_data,
    FAST_PAIR_GOOGLE_IDENTIFIER & 0xFF,
    (FAST_PAIR_GOOGLE_IDENTIFIER >> 8) & 0xFF,
    (FAST_PAIR_MODEL_IDENTIFIER>> 16) & 0xFF,
    (FAST_PAIR_MODEL_IDENTIFIER >> 8) & 0xFF,
    FAST_PAIR_MODEL_IDENTIFIER & 0xFF
};

static const le_adv_data_item_t fp_model_id_data_item =
{
    SIZE_MODEL_ID_ADV,
    fp_model_id_adv
};

/*! Static functions for fastpair advertising data */
static unsigned int fastPair_AdvGetNumberOfItems(const le_adv_data_params_t * params);
static le_adv_data_item_t fastPair_AdvGetDataItem(const le_adv_data_params_t * params, unsigned int id);
static void fastPair_ReleaseItems(const le_adv_data_params_t * params);

/*! Callback registered with LE Advertising Manager*/
const le_adv_data_callback_t fastPair_advertising_callback = {
.GetNumberOfItems = &fastPair_AdvGetNumberOfItems, 
.GetItem = &fastPair_AdvGetDataItem,
.ReleaseItems = &fastPair_ReleaseItems
};


/*! \brief Function to initialise the fastpair advertising globals
*/
static void fastPair_InitialiseAdvGlobal(void)
{
    memset(&fastpair_advert, 0, sizeof(fastpair_advert_data_t));
}

/*! \brief Set the identifiable parameter according to the data set returned by Adv Mgr */
static void fastpair_SetIdentifiable(const le_adv_data_set_t data_set)
{
    fastpair_advert.identifiable = (data_set == le_adv_data_set_handset_identifiable)?(TRUE):(FALSE);
}

/*! \brief Query the advertisement interval and check if it in expected range*/
static void fastpair_CheckAdvIntervalInRange(le_adv_data_set_t data_set)
{
    le_adv_common_parameters_t adv_int = {0};

    /* Get the currently used advertising parameters */
    bool status = LeAdvertisingManager_GetAdvertisingInterval(&adv_int);
    DEBUG_LOG("FP ADV: fastpair_CheckAdvIntervalInRange adv_int_min = %d\n, adv_int_max = %d\n", adv_int.le_adv_interval_min, adv_int.le_adv_interval_max);

    if(status)
    {
        switch(data_set)
        {
            case le_adv_data_set_handset_identifiable:
            /*  This is more stringent check as the max value returned might be above 100ms but actual adv interval
                might still be conforming to FASTPAIR discoverable requirement*/
                if(adv_int.le_adv_interval_max > FP_ADV_INTERVAL_IDENTIFIABLE_MAX)
                {
                    DEBUG_LOG("FP ADV: fastpair_CheckAdvIntervalInRange: Adv interval might be non compliant with Fastpair Standard \n");
                }
            break;

            case le_adv_data_set_handset_unidentifiable:
            /*  This is more stringent check as the max value returned might be above 250ms but actual adv interval
                might still be conforming to FASTPAIR non-discoverable requirement*/
                if(adv_int.le_adv_interval_max > FP_ADV_INTERVAL_UNIDENTIFIABLE_MAX)
                {
                    DEBUG_LOG("FP ADV: fastpair_CheckAdvIntervalInRange: Adv interval might be non compliant with Fastpair Standard \n");
                }
            break;

            case le_adv_data_set_peer:
                DEBUG_LOG("FP ADV: fastpair_CheckAdvIntervalInRange: Non-connectable \n");
            break;

            default:
                DEBUG_LOG("FP ADV: fastpair_CheckAdvIntervalInRange: Invalid advertisement dataset\n");
            break;
        }
    }
    else
    {
        DEBUG_LOG("FP ADV: fastpair_CheckAdvIntervalInRange: Failed to get the LE advertising interval values \n");
    }
}


#define FASTPAIR_ADV_PARAMS_REQUESTED(params) ((params->completeness == le_adv_data_completeness_full) && \
            (params->placement == le_adv_data_placement_advert))

#define IS_IDENTIFIABLE(data_set) (data_set == le_adv_data_set_handset_identifiable)
#define IS_UNIDENTIFIABLE(data_set) (data_set == le_adv_data_set_handset_unidentifiable)


/*! \brief Provide the number of items expected to go in adverts for a given mode

      Advertising Manager is expected to retrive the number of items first before the fastPair_AdvGetDataItem() callback

      For fastpair there wont be any adverts in case of le_adv_data_completeness_can_be_shortened/skipped
*/
static unsigned int fastPair_AdvGetNumberOfItems(const le_adv_data_params_t * params)
{
    unsigned int number=0;

    if(params->data_set != le_adv_data_set_peer)
    {
        fastpair_SetIdentifiable(params->data_set);
        /* Add debug logs if existing advertising interval is not in range */
        fastpair_CheckAdvIntervalInRange(params->data_set);

        /*Check for BR/EDR connectable*/
        if (IsHandsetConnAllowed && FASTPAIR_ADV_PARAMS_REQUESTED(params))
        {
            if (IS_IDENTIFIABLE(params->data_set))
            {
                number = FAST_PAIR_AD_ITEMS_IDENTIFIABLE;
            }
            else if (IS_UNIDENTIFIABLE(params->data_set))
            {
                number = FAST_PAIR_AD_ITEMS_UNIDENTIFIABLE_WITH_ACCOUNT_KEYS;
            }
        }
        else
        {
            DEBUG_LOG("FP ADV: fastPair_AdvGetNumberOfItems: Non-connectable \n");
        }
    }

    return number;
}


/*! \brief Provide fastpair advert data when Identifiable (i.e. BR/EDR discoverable)

    Each data item in GetItems will be invoked separately by Adv Mgr, more precisely, one item per AD type.
*/
static le_adv_data_item_t fastPair_GetDataIdentifiable(uint16 data_set_identifier)
{   
    le_adv_data_item_t data_item={0};

    if (data_set_identifier==FAST_PAIR_ADV_ITEM_MODEL_ID)
    {   
        DEBUG_LOG("FP ADV: fastPair_GetDataIdentifiable: Model Id data item \n");
        return fp_model_id_data_item;
    }
    else if(data_set_identifier==FAST_PAIR_ADV_ITEM_BLE_FLAGS)
    {
        DEBUG_LOG("FP ADV: fastPair_GetDataIdentifiable: BLE flags \n");
        return fp_ble_flags_data_item;
    }
    DEBUG_LOG("FP ADV: fastPair_GetDataIdentifiable: Invalid data_set_identifier %d \n", data_set_identifier);
    return data_item;
}


/*! \brief Get the advert data for account key filter
*/
static le_adv_data_item_t fastPairGetAccountKeyFilterAdvData(void)
{
    le_adv_data_item_t data_item;
    uint16 bloom_filter_size = fastPairGetBloomFilterLen();
    uint16 adv_size=0;
    
    if (fastpair_advert.account_key_filter_adv_data)
    {
        free(fastpair_advert.account_key_filter_adv_data);
        fastpair_advert.account_key_filter_adv_data=NULL;
    }
    
    if (bloom_filter_size)
    {
        adv_size=SIZE_GOOGLE_ID_ADV+bloom_filter_size;/*To accomodate service type and length*/
        
        fastpair_advert.account_key_filter_adv_data = PanicUnlessMalloc(adv_size);
        fastpair_advert.account_key_filter_adv_data[0] = adv_size-FP_SIZE_LENGTH_FIELD;
        fastpair_advert.account_key_filter_adv_data[1] = ble_ad_type_service_data;
        fastpair_advert.account_key_filter_adv_data[2] = (uint8)(FAST_PAIR_GOOGLE_IDENTIFIER & 0xFF);
        fastpair_advert.account_key_filter_adv_data[3] = (uint8)((0xFE2C >> 8) & 0xFF);
        memcpy(&fastpair_advert.account_key_filter_adv_data[4], fastPairGetBloomFilterData(), bloom_filter_size);
        DEBUG_LOG("FP ADV: fastPairGetAccountKeyFilterAdvData: bloom_filter_size %d\n", bloom_filter_size);
    }

    data_item.size = adv_size;
    data_item.data = fastpair_advert.account_key_filter_adv_data;
    
    return data_item;
}


/*! \brief Provide fastpair advert data when Unidentifiable (i.e. BR/EDR non-discoverable)

    Each data item in GetItems will be invoked separately by Adv Mgr, more precisely, one item per AD type.
*/
static le_adv_data_item_t fastPair_GetDataUnIdentifiable(uint16 data_set_identifier)
{
    le_adv_data_item_t data_item={0};

    DEBUG_LOG("FP ADV: fastPair_GetDataUnIdentifiable: data_set_identifier %d \n", data_set_identifier);
    
    if(data_set_identifier==FAST_PAIR_ADV_ITEM_BLE_FLAGS)
    {
        DEBUG_LOG("FP ADV: fastPair_GetDataIdentifiable: BLE flags \n");
        return fp_ble_flags_data_item;
    }
    else if (data_set_identifier==FAST_PAIR_ADV_ITEM_BLOOM_FILTER_ID)
    {
        if(!fastPair_GetNumAccountKeys())
        {
            DEBUG_LOG("FP ADV: fastPair_GetDataUnIdentifiable: Google Id data item with empty Account Key\n");
            return fp_google_id_data_item;
        }
        else
        {
            DEBUG_LOG("FP ADV: fastPair_GetDataUnIdentifiable: Google Id data item with Account Key\n");
            data_item = fastPairGetAccountKeyFilterAdvData();
            /*Generate new bloom filter and keep it ready for advertisements in BR/EDR Connectable and non-discoverable mode
            This will ensure next callback would have new Salt*/
            DEBUG_LOG("FP ADV: fastPair_GetDataUnIdentifiable: fastPair_GenerateBloomFilter\n");
            fastPair_GenerateBloomFilter();
        }
    }
    return data_item;
}


/*! \brief Provide the advertisement data expected to go in adverts for a given mode

    Each data item in GetItems will be invoked separately by Adv Mgr, more precisely, one item per AD type.
*/
static le_adv_data_item_t fastPair_AdvGetDataItem(const le_adv_data_params_t * params, unsigned int id)
{
    le_adv_data_item_t data_item={0};

    if(params->data_set != le_adv_data_set_peer)
    {
        if (IsHandsetConnAllowed && FASTPAIR_ADV_PARAMS_REQUESTED(params))
        {
            if (IS_IDENTIFIABLE(params->data_set))
            {
                return fastPair_GetDataIdentifiable(id);
            }
            else if (IS_UNIDENTIFIABLE(params->data_set))
            {
                return fastPair_GetDataUnIdentifiable(id);
            }
        }
    }

    return data_item;
}


/*! \brief Release any allocated fastpair data

      Advertising Manager is expected to retrive the number of items first before the fastPair_AdvGetDatatems() callback
*/
static void fastPair_ReleaseItems(const le_adv_data_params_t * params)
{
    if (FASTPAIR_ADV_PARAMS_REQUESTED(params) && IS_UNIDENTIFIABLE(params->data_set))
    {        
        if (fastpair_advert.account_key_filter_adv_data)
        {
            free(fastpair_advert.account_key_filter_adv_data);
            fastpair_advert.account_key_filter_adv_data=NULL;
        }
    }
}


/*! @brief Private API to initialise fastpair
 */
void fastPair_SetUpAdvertising(void)
{
    fastPairTaskData *fast_pair_task_data = fastPair_GetTaskData();

    /*Initialise fastpair advertising globals*/
    fastPair_InitialiseAdvGlobal();

    /*Initialise fastpair bloom filter globals*/
    fastPair_InitBloomFilter();
    
    /*PreCalculate Bloom Filter with available account filters, if any*/
    DEBUG_LOG("FP ADV: fastPair_SetUpAdvertising: fastPair_GenerateBloomFilter\n");
    fastPair_GenerateBloomFilter();

    /*Mandate use of Transmit Power in fastpair adverts*/
    TxPower_Mandatory(TRUE, le_client_fast_pair);

    if (fastpair_advert.adv_register_handle!=NULL)
    {
        DEBUG_LOG("FP ADV: fastPair_SetUpAdvertising: Adv Handle NOT NULL \n");
    }

    /*Register callback with Advertising Manager*/    
    fastpair_advert.adv_register_handle = LeAdvertisingManager_Register(&fast_pair_task_data->task, &fastPair_advertising_callback);
}


/*! @brief Private API to handle change in Connectable state and notify the LE Advertising Manager
 */
bool fastPair_AdvNotifyChangeInConnectableState(uint16 ind)
{
    fastPairTaskData *fast_pair_task_data = fastPair_GetTaskData();
    bool status = FALSE;

    IsHandsetConnAllowed = (ind == CON_MANAGER_HANDSET_CONNECT_ALLOW_IND)? TRUE : FALSE;

    if (fastpair_advert.adv_register_handle)
    {
        status = LeAdvertisingManager_NotifyDataChange(&fast_pair_task_data->task, fastpair_advert.adv_register_handle);
    }
    else
    {
        DEBUG_LOG("FP ADV: Invalid handle in fastpair_AdvNotifyChangeInConnectableState ");
    }

    return status;
}

/*! @brief Private API to provide BR/EDR discoverablity information
 */
bool fastPair_AdvIsBrEdrDiscoverable(void)
{
    DEBUG_LOG("FP ADV: fastpair_AdvIsBrEdrDiscoverable %d \n", fastpair_advert.identifiable);
    return fastpair_advert.identifiable;
}

