/*!
\copyright  Copyright (c) 2018 - 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for management of Bluetooth Low Energy advertising settings

Provides control for Bluetooth Low Energy (BLE) advertisements.
The API for this module allows for multiple adverts to be configured.

\ref AdvertisingManager_NewAdvert() is used to get the identifier for
a collection of advertisement settings, and is used in the other API
calls.

Support for multiple simultaneous advertisements is TBD.

Connections intended to use Bluetooth Low Energy should be controlled
through the connection manager (av_headset_con_manager.h). The
adverts associated with a connection are configured here.
*/


#ifndef LE_ADVERTSING_MANAGER_H_
#define LE_ADVERTSING_MANAGER_H_


#include "domain_message.h"

#include <connection.h>
#include <connection_no_ble.h>

/*! \brief Data type to specify the preset advertising interval for advertising events */
typedef enum
{
le_adv_preset_advertising_interval_low,
le_adv_preset_advertising_interval_high,
le_adv_preset_advertising_interval_max = le_adv_preset_advertising_interval_high,
le_adv_preset_advertising_interval_total
} le_adv_preset_advertising_interval_t;

/*! \brief Common LE advertising parameters in units of 0.625 ms */
typedef struct
{
 uint16 le_adv_interval_min;
 uint16 le_adv_interval_max;
} le_adv_common_parameters_t;

/*! \brief Parameter set (one for each type (slow/fast)) */
typedef struct
{
 le_adv_common_parameters_t set_type[le_adv_preset_advertising_interval_total];
} le_adv_parameters_set_t;

/*! \brief LE advertising parameter sets (slow and fast parameters only) */
typedef struct
{
 const le_adv_parameters_set_t *sets;
} le_adv_parameters_t;

/*! \brief Data type for API function return values to indicate success/error status */
typedef enum{
    le_adv_mgr_status_success,
    le_adv_mgr_status_error_unknown
}le_adv_mgr_status_t;

/* Data type for the state of LE Advertising Manager */
typedef enum{
    le_adv_mgr_state_uninitialised,
    le_adv_mgr_state_initialised,
    le_adv_mgr_state_started,
    le_adv_mgr_state_suspended,
}le_adv_mgr_state_t;

/*! Messages sent by the advertising manager.  */
enum adv_mgr_messages_t
{
        /*! Message signalling the battery module initialisation is complete */
    APP_ADVMGR_INIT_CFM = ADV_MANAGER_MESSAGE_BASE,
        /*! Message responding to AdvertisingManager_Start */
    APP_ADVMGR_ADVERT_START_CFM,
        /*! Message responding to AdvertisingManager_SetAdvertData */
    APP_ADVMGR_ADVERT_SET_DATA_CFM,
};


/*! Message sent when AdvertisingManager_Start() completes */
typedef struct
{
    /*! Final result of the AdvertisingManager_Start() operation */
    connection_lib_status   status;
} APP_ADVMGR_ADVERT_START_CFM_T;


/*! Message sent when AdvertisingManager_SetAdvertData() completes */
typedef struct
{
        /*! Final result of the AdvertisingManager_SetAdvertData() operation */
    connection_lib_status   status;
} APP_ADVMGR_ADVERT_SET_DATA_CFM_T;


/*! State of the advertising manager (internal) */
typedef enum _le_advertising_manager_state_t
{
        /*! Initial state of advertising manager */
    ADV_MGR_STATE_STARTING,
        /*! Initialisation completed. Any external information has now been retrieved. */
    ADV_MGR_STATE_INITIALISED,
        /*! There is an active advert (on the stack) */
    ADV_MGR_STATE_ADVERTISING,
} adv_mgr_state_t;


/*! Type of discoverable advertising to use.

    The value of defines from connection.h are used, but note that only
    a subset are available in the enum. */
typedef enum
{
        /*! Not using discoverable advertising */
    ble_discoverable_mode_none = 0,
        /*! LE Limited Discoverable Mode */
    ble_discoverable_mode_limited = BLE_FLAGS_LIMITED_DISCOVERABLE_MODE,
        /*! LE General Discoverable Mode */
    ble_discoverable_mode_general = BLE_FLAGS_GENERAL_DISCOVERABLE_MODE,
} adv_mgr_ble_discoverable_mode_t;


/*! \todo work out what this is for. Note that have removed broadcasting reasons. */
typedef enum
{
    ble_gap_read_name_advertising  = 1,
    ble_gap_read_name_gap_server,// = 2,
    ble_gap_read_name_associating,//= 8,
} adv_mgr_ble_gap_read_name_t;

/*! \brief Opaque type for LE Advertising Manager data set object. */
struct _le_adv_data_set;
/*! \brief Handle to LE Advertising Manager data set object. */
typedef struct _le_adv_data_set * le_adv_data_set_handle;

/*! \brief Opaque type for LE Advertising Manager params set object. */
struct _le_adv_params_set;
/*! \brief Handle to LE Advertising Manager params set object. */
typedef struct _le_adv_params_set * le_adv_params_set_handle;

struct _adv_mgr_advert_t;

/*! Definition for an advert. The structure will contain all needed settings.

    This is an anonymous type as no external code should need direct access.
*/
typedef struct _adv_mgr_advert_t adv_mgr_advert_t;

/*! Advertising manager task structure */
typedef struct
{
        /*! Task for advertisement management */
    TaskData                    task;
        /*! Local state for advertising manager */
    adv_mgr_state_t             state;
        /*! Local state for advertising manager */
    le_adv_mgr_state_t           adv_state;
        /*! Bitmask for allowed advertising event types */
    uint8           mask_enabled_events;
    /*! Flag to indicate enabled/disabled state of all advertising event types */
    bool            is_advertising_allowed;
    /*! Flag to indicate if data update is required */
    bool            is_data_update_required;
    /*! Selected advertising data set for the undirected advertising */
    le_adv_data_set_handle dataset_handle;
    /*! Configured advertising parameter set for the undirected advertising */
    le_adv_params_set_handle params_handle;
        /*! Flag for Allocated storage for the device name */
    uint8*                      localName;
        /*! An advert that is currently in the middle of an operation
            that blocks other activity */
    adv_mgr_advert_t           *blockingAdvert;
        /*! The message that will be sent once the current blocking operation
            is completed */
    MessageId                   blockingOperation;
        /*! The condition (internal) that the blocked operation is waiting for */
    uint16                      blockingCondition;
        /*! The task to send a message to once the blocking operation completes */
    Task                        blockingTask;
} adv_mgr_task_data_t;

/*!< Task information for the advertising manager */
extern adv_mgr_task_data_t  app_adv_manager;

/*! Get the advertising manager data structure */
#define AdvManagerGetTaskData()      (&app_adv_manager)

/*! Get the advertising manager task */
#define AdvManagerGetTask()  (&app_adv_manager.task)

/*! Get the local name */
#define AdvManagerGetLocalName() (AdvManagerGetTaskData()->localName)


/*! Retrieve a handle for a new advertisement

    New adverts reset all setting to zero or equivalent. The exception
    is for the advertising channels to use, where all channels are selected.

    \return NULL if no unused advertisements are available
*/
adv_mgr_advert_t *AdvertisingManager_NewAdvert(void);


/*! Stop using an advertisement

    This releases one of the adverts for re-use.

    Advertising using the advert should be stopped before calling this function.
    If it is not and the advert is in use, then the advert \b will be stopped, but
    the function may return FALSE if the advert cannot be stopped synchronously.

    \param advert   The advert-settings to be released

    \return FALSE if the advertisement could not be released.
*/
bool AdvertisingManager_DeleteAdvert(adv_mgr_advert_t *advert);


/*! Set the name to use in the specified advert

    The name will be shorted if necessary to fit into the advertising packet

    \param advert   The advert-settings to be updated
    \param name     The name string to use (null terminated)

    \return TRUE if name set successfully
*/
bool AdvertisingManager_SetName(adv_mgr_advert_t *advert, uint8 *name);


/*! Use the device name in the specified advert

    The name will be shortened if necessary to fit into the advertising packet

    If no local name can be found found, a blank name will be set.

    \param advert   The advert-settings to be updated
*/
void AdvertisingManager_UseLocalName(adv_mgr_advert_t *advert);


/*! Set the flags to use in the specified advert based on modes to use

    \param advert   The advert-settings to be updated
    \param discoverable_mode    The type of discoverable advert

    \return TRUE if flags set successfully
*/
bool AdvertisingManager_SetDiscoverableMode(adv_mgr_advert_t *advert,
                                      adv_mgr_ble_discoverable_mode_t discoverable_mode);


/*! Set the reason for the advert (based on GAP)

    \param advert   The advert-settings to be updated
    \param reason   The reason for this advert

    \return TRUE if the reason was saved successfully
*/
bool AdvertisingManager_SetReadNameReason(adv_mgr_advert_t *advert,
                                    adv_mgr_ble_gap_read_name_t reason);


/*! Add a service UUID to data that can be advertised

    \param advert   The advert-settings to be updated
    \param uuid     The service UUID to add

    \return TRUE if the UUID was added successfully. Note that the
    number of services supported is limited.
*/
bool AdvertisingManager_SetService(adv_mgr_advert_t *advert, uint16 uuid);


bool AdvertisingManager_SetServiceUuid128(adv_mgr_advert_t *advert, const uint8 *uuid);


/*! Set the parameters for advertising.

    This allows the setting of parameters for directed and undirected
    advertising.

    For undirected advertising the advertising interval and
    filtering policy can be specified.

    For directed advertising only the target address (and its type)
    can be specified.

    See the description for ble_adv_params_t in connection.h

    \param advert   The advert-settings to be updated
    \param[in]  adv_params  Pointer to structure with parameters
*/
void AdvertisingManager_SetAdvertParams(adv_mgr_advert_t *advert, ble_adv_params_t *adv_params);


/*! Set the type of advert to be used

    \param advert   The advert-settings to be updated
    \param advert_type The type of advertising that will be performed
*/
void AdvertisingManager_SetAdvertisingType(adv_mgr_advert_t *advert, ble_adv_type advert_type);


/*! Set the advertising channels to be used for the advert

    \param advert   The advert-settings to be updated
    \param channel_mask A mask specifying the channels to be used. The define
        BLE_ADV_CHANNEL_ALL will select all channels
*/
void AdvertisingManager_SetAdvertisingChannels(adv_mgr_advert_t *advert, uint8 channel_mask);


/*! Select whether to use a random address as our address

    \param advert   The advert-settings to be updated
    \param use_random_address Set this if wish to use a random address when advertising
*/
void AdvertisingManager_SetUseOwnRandomAddress(adv_mgr_advert_t *advert, bool use_random_address);


/*! Handler for connection library messages not sent directly

    This function is called to handle any connection library messages sent to
    the application that the advertising module is interested in. If a message
    is processed then the function returns TRUE.

    \note Some connection library messages can be sent directly as the
        request is able to specify a destination for the response.

    \param  id              Identifier of the connection library message
    \param  message         The message content (if any)
    \param  already_handled Indication whether this message has been processed by
                            another module. The handler may choose to ignore certain
                            messages if they have already been handled.

    \returns TRUE if the message has been processed, otherwise returns the
        value in already_handled
 */
extern bool AdvertisingManager_HandleConnectionLibraryMessages(MessageId id, Message message, bool already_handled);


/*! Enable advertising with the specified advert

    If multiple adverts are enabled simultaneously, then the advertising
    manager will schedule them.

    If the function returns TRUE then when the advert has been configured
    and started the message APP_ADVMGR_ADVERT_START_CFM is sent to the
    requester task. The message can indicate a failure.

    \note As this is an asynchronous command, requests will be serialised and may
    be delayed by the advertising manager.

    \param advert       The advert-to enable
    \param requester    Task to send response to

    \return FALSE if there was a problem with the advert.
            TRUE the advert appears correct and the message APP_ADVMGR_ADVERT_START_CFM
            will be sent with the final status
*/
bool AdvertisingManager_Start(adv_mgr_advert_t *advert, Task requester);


/*! Set-up the advertising data to match the specified advert

    This is intended for use with connectable adverts which are managed by
    GATT.

    If the function returns TRUE then the advertising data is configured
    and the message APP_ADVMGR_ADVERT_SET_DATA_CFM is sent to the
    requester task. The message can indicate a failure.

    \note As this is an asynchronous command, requests will be serialised and may
    be delayed by the advertising manager.

    \param advert       The advert-to enable
    \param requester    Task to send response to

    \return FALSE if there was a problem with the advert.
            TRUE the advert settings appear correct and the message
            APP_ADVMGR_ADVERT_SET_DATA_CFM will be sent with the final status.
*/
bool AdvertisingManager_SetAdvertData(adv_mgr_advert_t *advert, Task requester);

/*! \brief Data type to define whether the data is strictly needed in full or can be skipped/shortened for LE advertising */
typedef enum
{
 le_adv_data_completeness_full,
 le_adv_data_completeness_can_be_shortened,
 le_adv_data_completeness_can_be_skipped
} le_adv_data_completeness_t;
/*! \brief Data type to define the advertisement packet types where the data item is aimed at */
typedef enum
{
 le_adv_data_placement_advert,
 le_adv_data_placement_scan_response,
 le_adv_data_placement_dont_care
} le_adv_data_placement_t;
/*! \brief Data type for the supported advertising data sets */
typedef enum
{
 le_adv_data_set_cooperative_identifiable,
 le_adv_data_set_cooperative_unidentifiable,
 le_adv_data_set_exclusive
} le_adv_data_set_t;
/*! \brief Data structure to specify the attributes for the individual data items */
typedef struct
{
 le_adv_data_set_t data_set;
 le_adv_data_completeness_t completeness;
 le_adv_data_placement_t placement;
}le_adv_data_params_t;

/*! \brief Data structure for the individual data items */
typedef struct
{
    unsigned size;
    const uint8 * data;
}le_adv_data_item_t;

/*! \brief Data structure to specify the callback functions for the data items to be registered */
typedef struct
{
 unsigned int (*GetNumberOfItems)(const le_adv_data_params_t * params);
 le_adv_data_item_t (*GetItem)(const le_adv_data_params_t * params, unsigned int);
 void (*ReleaseItems)(const le_adv_data_params_t * params);
}le_adv_data_callback_t;

/*! \brief Opaque type for LE Advertising Manager registry object */
struct _le_adv_mgr_register;
/*! \brief Handle to LE Advertising Manager registry object */
typedef struct _le_adv_mgr_register * le_adv_mgr_register_handle;

/*! \brief Data type for the message identifiers */
typedef enum
{
    LE_ADV_MGR_SELECT_DATASET_CFM = ADV_MANAGER_MESSAGE_BASE,
    LE_ADV_MGR_RELEASE_DATASET_CFM,
    LE_ADV_MGR_ENABLE_CONNECTABLE_CFM,
    LE_ADV_MGR_ALLOW_ADVERTISING_CFM,
    LE_ADV_MGR_NOTIFY_DATA_CHANGE_CFM
}le_adv_mgr_message_id_t;

/*! \brief Data structure for the confirmation message LE_ADV_MGR_SELECT_DATASET_CFM */
typedef struct
{
 le_adv_mgr_status_t status;
}LE_ADV_MGR_SELECT_DATASET_CFM_T;

/*! \brief Data structure for the confirmation message LE_ADV_MGR_RELEASE_DATASET_CFM */
typedef struct
{
 le_adv_mgr_status_t status;
}LE_ADV_MGR_RELEASE_DATASET_CFM_T;

/*! \brief Data structure for the confirmation message LE_ADV_MGR_ADVERT_ENABLE_CONNECTABLE_CFM */
typedef struct
{
    le_adv_mgr_status_t status;
    bool enable;

}LE_ADV_MGR_ENABLE_CONNECTABLE_CFM_T;

/*! \brief Data structure for the confirmation message LE_ADV_MGR_ADVERT_ALLOW_CFM */
typedef struct
{
    le_adv_mgr_status_t status;
    bool allow;

}LE_ADV_MGR_ALLOW_ADVERTISING_CFM_T;

/*! \brief Data structure for the confirmation message LE_ADV_MGR_NOTIFY_DATA_CHANGE_CFM */
typedef struct
{
 le_adv_mgr_status_t status;
}LE_ADV_MGR_NOTIFY_DATA_CHANGE_CFM_T;

/*! \brief Data structure to specify the input parameters for LeAdvertisingManager_SelectAdvertisingDataSet() API function */
typedef struct
{
    le_adv_data_set_t set;
}le_adv_select_params_t;

/*! Initialise LE Advertising Manager

    \param init_task
           Task to send init completion message to

    \return TRUE to indicate successful initialisation.
            FALSE otherwise.
*/
bool LeAdvertisingManager_Init(Task init_task);

/*! Deinitialise LE Advertising Manager

    \param

    \return TRUE to indicate successful uninitialisation.
            FALSE to indicate failure.
*/
bool LeAdvertisingManager_DeInit(void);

/*! \brief Public API to allow/disallow LE advertising events

    \param[in] task Task to send confirmation message
    
    \param[in] allow Boolean value, TRUE for allow operation, FALSE for disallow operation.

    \return status_t API status
    \return Sends LE_ADV_MGR_ADVERT_ALLOW_CFM message to the task

*/
bool LeAdvertisingManager_AllowAdvertising(Task task, bool allow);

/*! \brief Public API to enable/disable connectable LE advertising events

    \param[in] task Task to send confirmation message
    
    \param[in] enable Boolean value, TRUE for enable operation, FALSE for disable operation.

    \return status_t API status
    \return Sends LE_ADV_MGR_ADVERT_ENABLE_CONNECTABLE_CFM message to the task               

*/
bool LeAdvertisingManager_EnableConnectableAdvertising(Task task, bool enable);

/*! \brief Public API to register callback functions for advertising data
    \param[in] task The task to receive messages from LE advertising manager when advertising states change
    \param[in] callback Pointer to the data structure of type le_adv_data_callback_t to specify function pointers for LE Advertising Manager to use to collect the data items to be advertised
    \return Valid pointer to the handle of type le_adv_mgr_register_handle, NULL otherwise.
*/
le_adv_mgr_register_handle LeAdvertisingManager_Register(Task task, const le_adv_data_callback_t * callback);

/*! \brief Handler for connection library messages not sent directly

    This function is called to handle any connection library messages sent to
    the application that the advertising module is interested in. If a message
    is processed then the function returns TRUE.

    \note Some connection library messages can be sent directly as the
        request is able to specify a destination for the response.

    \param[in]  id              Identifier of the connection library message
    \param[in]  message         A valid pointer to the instance of message structure if message has any payload, NULL otherwise.
    \param[in]  already_handled Indication whether this message has been processed by
                            another module. The handler may choose to ignore certain
                            messages if they have already been handled.

    \return TRUE if the message has been processed, otherwise returns the
        value in already_handled
 */
extern bool LeAdvertisingManager_HandleConnectionLibraryMessages(MessageId id, Message message, bool already_handled);

/*! \brief Select LE Advertising Data Set
\param[in] task The task to receive LE_ADV_MGR_SELECT_DATASET_CFM message from LE Advertising Manager
\param[in] params Pointer to an instance of advertising select parameters of type le_adv_select_params_t
\return Valid pointer to indicate successful LE advertising data set select operation, NULL otherwise.
\return Sends LE_ADV_MGR_SELECT_DATASET_CFM message when LE advertising gets started for the selected data set.
*/
le_adv_data_set_handle LeAdvertisingManager_SelectAdvertisingDataSet(Task task, const le_adv_select_params_t * params);

/*! \brief Release LE Advertising Data Set
    \param Handle to the advertising data set object returned from the call to the API LeAdvertisingManager_SelectAdvertisingDataSet().

    \return TRUE to indicate successful LE advertising data set release operation, FALSE otherwise.
    \return Sends LE_ADV_MGR_RELEASE_CFM message when LE advertising gets terminated for the released data set. 
*/
bool LeAdvertisingManager_ReleaseAdvertisingDataSet(le_adv_data_set_handle handle);

/*! \brief Public API to notify the changes in the advertising data
\param[in] task The task to receive message LE_ADV_MGR_NOTIFY_DATA_CHANGE_CFM from LE advertising manager
\param[in] handle Handle returned from the call to the API LeAdvertisingManager_Register().
\return TRUE for a succesful call to API for notification, FALSE otherwise.
\return Sends LE_ADV_MGR_NOTIFY_DATA_CHANGE_CFM when LE advertising gets started with the modified data items.
*/
bool LeAdvertisingManager_NotifyDataChange(Task task, const le_adv_mgr_register_handle handle);

/*! \brief Register the parameters to be used by LE advertising manager.
\param[in] params Pointer to LE advertising parameters
\return TRUE for a succesful call to API for parameter register operation, FALSE otherwise.
*/
bool LeAdvertisingManager_ParametersRegister(const le_adv_parameters_t *params);

/*! \brief Select the use of set in the array of LE advertising parameters.
\param[in] index The index to activate
\return TRUE for a succesful call to API for parameter select operation, FALSE otherwise.
*/
bool LeAdvertisingManager_ParametersSelect(uint8 index);

/*! \brief Public API to get LE Advertising interval min and max values
\param[out] interval Pointer to an instance of advertising interval min and max values of the type le_adv_common_parameters_t
\return TRUE to indicate successful advertising interval parameter get operation, FALSE otherwise.
*/
bool LeAdvertisingManager_GetAdvertisingInterval(le_adv_common_parameters_t * interval);

#endif /* LE_ADVERTSING_MANAGER_H_ */
