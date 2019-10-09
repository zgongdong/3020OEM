/*!
\copyright  Copyright (c) 2018 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       
\brief      Internal defines used by the advertising manager
*/

#ifndef LE_ADVERTISING_MANAGER_PRIVATE_H_

#define LE_ADVERTISING_MANAGER_PRIVATE_H_

#include "le_advertising_manager.h"
#include "logging.h"

/*! Macro to make a message based on type. */
#define MAKE_MESSAGE(TYPE) TYPE##_T *message = PanicUnlessNew(TYPE##_T);
/*! Macro to make a variable length message based on type. */
#define MAKE_MESSAGE_VAR(VAR, TYPE) TYPE##_T *VAR = PanicUnlessNew(TYPE##_T);

/*! Logging Macros */
#if defined DEBUG_LOG_EXTRA

#define DEBUG_LOG_LEVEL_1 DEBUG_LOG /* Additional Failure Logs */
#define DEBUG_LOG_LEVEL_2 DEBUG_LOG /* Additional Information Logs */

#else

#define DEBUG_LOG_LEVEL_1(...) ((void)(0))
#define DEBUG_LOG_LEVEL_2(...) ((void)(0))

#endif

/* Number of clients supported that can register callbacks for advertising data */
#define MAX_NUMBER_OF_CLIENTS 10

/*! Size of a data element header in advertising data

      * Octet[0]=length,
      * Octet[1]=Tag
      * Octets[2]..[n]=Data
*/
#define AD_DATA_HEADER_SIZE         (0x02)

/*! Maximum data length of an advert if advertising length extensions are not used */
#define MAX_AD_DATA_SIZE_IN_OCTETS  (0x1F)

/*! Given the total space available, returns space available once a header is included

    This makes allowance for being passed negative values, or space remaining
    being less that that needed for a header.

    \param[in] space    Pointer to variable holding the remaining space
    \returns    The usable space, having allowed for a header
*/
#define USABLE_SPACE(space)             ((*space) > AD_DATA_HEADER_SIZE ? (*space) - AD_DATA_HEADER_SIZE : 0)

/*! Helper macro to return total length of a field, once added to advertising data

    \param[in] data_length  Length of field

    \returns    Length of field, including header, in octets
*/
#define AD_FIELD_LENGTH(data_length)    (data_length + 1)

/*! Size of flags field in advertising data */
#define FLAGS_DATA_LENGTH           (0x02)

/*! Calculate value for the maximum possible length of a name in advertising data */
#define MAX_AD_NAME_SIZE_OCTETS     (MAX_AD_DATA_SIZE_IN_OCTETS - AD_DATA_HEADER_SIZE)

/*! Minimum length of the local name being advertised, if we truncate */
#define MIN_LOCAL_NAME_LENGTH       (0x10)

/*! Maximum number of UUID16 services supported in an advert. This can be extended. */
#define MAX_SERVICES_UUID16                 (8)

/*! Maximum number of UUID32 services supported in an advert. This can be extended. */
#define MAX_SERVICES_UUID32                 (4)

/*! Maximum number of UUID128 services supported in an advert. This can be extended. */
#define MAX_SERVICES_UUID128                (2)

/*! Number of octets needed to store a UUID16 service entry */
#define OCTETS_PER_SERVICE                  (0x02)

/*! Number of octets needed to store a UUID32 service entry */
#define OCTETS_PER_32BIT_SERVICE                  (0x04)

/*! Number of octets needed to store a UUID16 service entry */
#define OCTETS_PER_128BIT_SERVICE           (0x10)

/*! Macro to calculate the number of UUID16 service entries that can fit in a given
length of advertising data.

    \param[in] space    Total space available in buffer (octets)

    \returns    Number of services that will fit in the space available
*/
#define NUM_SERVICES_THAT_FIT(space)        (USABLE_SPACE(space) / OCTETS_PER_SERVICE)

/*! Macro to calculate the number of UUID32 service entries that can fit in a given
length of advertising data.

    \param[in] space    Total space available in buffer (octets)

    \returns    Number of services that will fit in the space available
*/
#define NUM_32BIT_SERVICES_THAT_FIT(space)        (USABLE_SPACE(space) / OCTETS_PER_32BIT_SERVICE)

/*! Macro to calculate the number of UUID128 service entries that can fit in a given
length of advertising data.

    \param[in] space    Total space available in buffer (octets)

    \returns    Number of services that will fit in the space available
*/
#define NUM_128BIT_SERVICES_THAT_FIT(space) (USABLE_SPACE(space) / OCTETS_PER_128BIT_SERVICE)

/*! Helper macro to calculate the length needed to add UUID16 service to advertising data

    \param num_services     Number of UUID16 services to be added

    \returns Total length needed, in octets
 */
#define SERVICE_DATA_LENGTH(num_services)   ((num_services) * OCTETS_PER_SERVICE)

/*! Helper macro to calculate the length needed to add UUID32 service to advertising data

    \param num_services     Number of UUID32 services to be added

    \returns Total length needed, in octets
 */
#define SERVICE32_DATA_LENGTH(num_services)   ((num_services) * OCTETS_PER_32BIT_SERVICE)

/*! Helper macro to calculate the length needed to add UUID128 services to advertising data

    \param num_services     Number of UUID128 services to be added

    \returns Total length needed, in octets
 */
#define SERVICE128_DATA_LENGTH(num_services) ((num_services) * OCTETS_PER_128BIT_SERVICE)

typedef struct
{
    bool   action;
} LE_ADV_INTERNAL_MSG_ENABLE_ADVERTISING_T;

/*! Structure to hold a 128 bit UUID as not convinced by uuid library
    which doesn't seem to be used anyway */
typedef struct
{
    uint8 uuid[16];
} le_advertising_manager_uuid128;


/*! Enumerated type for messages sent within the advertising manager only. */
enum adv_mgr_internal_messages_t
{
        /*! Start advertising using this advert */
    ADV_MANAGER_START_ADVERT = 1,
        /*! Set advertising data using this advert. Used for connections (from Gatt) */
    ADV_MANAGER_SETUP_ADVERT,
    LE_ADV_INTERNAL_MSG_ENABLE_ADVERTISING,
    LE_ADV_INTERNAL_MSG_NOTIFY_RPA_CHANGE,
    LE_ADV_MGR_INTERNAL_START
};

#define MAX_RAW_DATA 10

/*! Implementation of the anonymous structure for advertisements.

    This structure is not visible outside of the advertising module.*/
struct _adv_mgr_advert_t
{
    bool    in_use;     /*!< Is this advertising entry being used */
        /*! Grouping of flags to indicate which information has been configured for
            the advert.
            This helps remove the need to have a defined NOT_SET value, where 0
            is a valid value for a setting. */
    struct
        {
            /*! Has a name been configured in the advert */
        bool    local_name:1;
            /*! Has the name in the advert been shortened before adding */
        bool    local_name_shortened:1;
            /*! There are flags to be included in the advert */
        bool    flags:1;
            /*! There are services to be included in the advert */
        bool    services:1;
            /*! The advertising parameters have been set (interval & filter) */
        bool    parameters;
            /*! Whether the type of advert has been set (mandatory) */
        bool    advert_type:1;
        }   content;
            /*! The name to use in the advert */
    uint8   local_name[MAX_AD_NAME_SIZE_OCTETS+1];
            /*! The flags to include in advertisements. See the Bluetooth Low Energy GAP flags
                in connection.h, such as BLE_FLAGS_DUAL_HOST. These are not set directly. */
    uint8   flags;
            /*! The channel map to use for advertising. See connection.h for the values,
                such as BLE_ADV_CHANNEL_ALL */
    uint8               channel_map_mask;
            /*! The reason for advertising, which affects the flags used in the advert.*/
    adv_mgr_ble_gap_read_name_t reason;
            /*! Number of GATT services (uuid16) that are to be included in the advert */
    uint16              num_services_uuid16;
            /*! Array of GATT services to be included in the advertisement */
    uint16              services_uuid16[MAX_SERVICES_UUID16];
                /*! Number of GATT services (uuid32) that are to be included in the advert */
    uint16              num_services_uuid32;
            /*! Array of GATT services to be included in the advertisement */
    uint32              services_uuid32[MAX_SERVICES_UUID32];
            /*! Number of GATT services (uuid128) that are to be included in the advert */
    uint16              num_services_uuid128;
            /*! Array of GATT services to be included in the advertisement */
    le_advertising_manager_uuid128 services_uuid128[MAX_SERVICES_UUID128];
            /*! Flag indicating whether we want to use a random address in our
                advertisements. This will actually be a resolvable private address (RPA) */
    uint8                use_own_random;
            /*! Advertising settings that affect the filter and advertising rate */
    ble_adv_params_t    interval_and_filter;
            /*! The type of advertising to use, when advertising is started */
    ble_adv_type        advertising_type;
    /*! Number of raw data entries */
    uint16              num_raw_data;
    /*! Array of raw data entries */
    uint8*              raw_data[MAX_RAW_DATA];
};

/*! The anonymous structure for scan response data.

    This structure is not visible outside of the advertising module.*/
struct _le_adv_mgr_scan_response_t
{
            /*! The name to use in the scan response */
    uint8   local_name[MAX_AD_NAME_SIZE_OCTETS+1];
            /*! Number of GATT services (uuid16) that are to be included in the scan response */
    uint16              num_services_uuid16;
            /*! Array of GATT services to be included in the scan response */
    uint16              services_uuid16[MAX_SERVICES_UUID16];
                /*! Number of GATT services (uuid32) that are to be included in the scan response */
    uint16              num_services_uuid32;
            /*! Array of GATT services to be included in the scan response */
    uint32              services_uuid32[MAX_SERVICES_UUID32];
            /*! Number of GATT services (uuid128) that are to be included in the scan response */
    uint16              num_services_uuid128;
            /*! Array of GATT services to be included in the scan response */
    le_advertising_manager_uuid128 services_uuid128[MAX_SERVICES_UUID128];
    /*! Number of raw data entries */
    uint16              num_raw_data;
    /*! Array of raw data entries */
    uint8*              raw_data[MAX_RAW_DATA];
};

typedef enum
{
    _le_adv_mgr_packet_type_advert,
    _le_adv_mgr_packet_type_scan_response
}_le_adv_mgr_packet_type_t;

struct _le_adv_mgr_packet_t
{
    _le_adv_mgr_packet_type_t packet_type;
    union
    {
        struct _adv_mgr_advert_t * advert;
        struct _le_adv_mgr_scan_response_t * scan_response;        
    }u;
};

struct _le_adv_data_set
{
    Task task;
    le_adv_data_set_t set;
};

struct _le_adv_params_set
{
    /*! Registered advertising parameter sets */
    le_adv_parameters_set_t * params_set;
    /*! Selected advertising parameter set */
    le_adv_preset_advertising_interval_t active_params_set;
};

/*! Generic message used for messages sent by the advertising manager */
typedef struct {
    adv_mgr_advert_t    *advert;        /*!< The advert that the message applies to */
    Task                requester;      /*!< The task that requested the operation (can be NULL) */
} ADV_MANAGER_ADVERT_INTERNAL_T;

typedef struct
{
    le_adv_data_set_t set;
    bool is_select_cfm_msg_needed;
}
LE_ADV_MGR_INTERNAL_START_T;


/*! Enumerated type used to note reason for blocking

    Advertising operations can be delayed while a previous operation completes.
    The reason for the delay is recorded using these values */
typedef enum {
    ADV_SETUP_BLOCK_NONE,               /*!< No blocking operation at present */
    ADV_SETUP_BLOCK_ADV_DATA_CFM = 1,   /*!< Blocked pending appAdvManagerSetAdvertisingData() completing */
    ADV_SETUP_BLOCK_ADV_PARAMS_CFM = 2, /*!< Blocked pending appAdvManagerHandleSetAdvertisingDataCfm completing */
    ADV_SETUP_BLOCK_ADV_SCAN_RESPONSE_DATA_CFM = 3,
    ADV_SETUP_BLOCK_ADV_ENABLE_CFM = 4,
    ADV_SETUP_BLOCK_ADV_LOCAL_ADDRESS_CFM = 5,
    ADV_SETUP_BLOCK_INVALID = 0xFF
} adv_mgr_blocking_state_t;

/*! Structure used for internal message #ADV_MANAGER_START_ADVERT */
typedef ADV_MANAGER_ADVERT_INTERNAL_T ADV_MANAGER_START_ADVERT_T;

/*! Structure used for internal message #ADV_MANAGER_SETUP_ADVERT */
typedef ADV_MANAGER_ADVERT_INTERNAL_T ADV_MANAGER_SETUP_ADVERT_T;

/*! Data type for the supported advertising events */
typedef enum
{
    le_adv_event_type_connectable_general = 1UL<<0,
    le_adv_event_type_connectable_directed = 1UL<<1,
    le_adv_event_type_nonconnectable_discoverable = 1UL<<2,
    le_adv_event_type_nonconnectable_nondiscoverable = 1UL<<3

} le_adv_event_type_t;

/*! Data structure to specify the input parameters for leAdvertisingManager_Start() API function */
typedef struct
{
    le_adv_data_set_t set;
    le_adv_event_type_t event;    
}le_advert_start_params_t;

void sendBlockingResponse(connection_lib_status sts);

#endif
