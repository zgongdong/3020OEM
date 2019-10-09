/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Management of an advert
*/

#ifndef LE_ADVERTSING_MANAGER_ADVERT_H_
#define LE_ADVERTSING_MANAGER_ADVERT_H_

#include "le_advertising_manager.h"
#include "le_advertising_manager_private.h"


/*! Get the local name */
#define AdvManagerGetLocalName() (AdvManagerGetTaskData()->localName)

/*! Initialise this module
*/
void leAdvertisingManager_AdvertInit(void);

/*! Convert advert into an LE advertising packet and register it
    with connection library.

    \return TRUE if successful, otherwise FALSE
*/
bool leAdvertisingManager_RegisterAdvert(adv_mgr_advert_t *advert);

/*! Convert scan response into an LE scan response packet and register it
    with connection library.

    \return TRUE if successful, otherwise FALSE
*/
bool leAdvertisingManager_RegisterScanResponse(const struct _le_adv_mgr_scan_response_t * scan_response);

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

/*! Add Individual 16-bit Service UUIDs into the Local Data Structure storing Advert/Scan Response Packet Data 

    \param packet The packet to modify
    \param uuid The uuid to add
*/
void leAdvertisingManager_SetServiceUuid16(struct _le_adv_mgr_packet_t * packet, uint16 uuid);

/*! Add Individual 32-bit Service UUIDs into the Local Data Structure storing Advert/Scan Response Packet Data 

    \param packet The packet to modify
    \param uuid The uuid to add
*/
void leAdvertisingManager_SetServiceUuid32(struct _le_adv_mgr_packet_t * packet, uint32 uuid);

/*! Add Individual 128-bit Service UUIDs into the Local Data Structure storing Advert/Scan Response Packet Data 

    \param packet The packet to modify
    \param uuid The uuid to add
*/
void leAdvertisingManager_SetServiceUuid128(struct _le_adv_mgr_packet_t * packet, const uint8 * uuid);

/*! Add raw data into the Local Data Structure storing Advert/Scan Response Packet Data 

    \param packet   The packet to modify
    \param data     The data to add
    \param data     The size of the data
    
    \return TRUE if successfully added, otherwise FALSE
*/
bool AdvertisingManager_AddRawData(struct _le_adv_mgr_packet_t * packet, const uint8* data, uint8 size);

/*! Free all raw data into from the Data Structure storing Advert/Scan Response Packet Data 

    \param packet   The packet to modify
*/
void AdvertisingManager_FreeRawData(struct _le_adv_mgr_packet_t * packet);

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

#endif /* LE_ADVERTSING_MANAGER_ADVERT_H_ */
