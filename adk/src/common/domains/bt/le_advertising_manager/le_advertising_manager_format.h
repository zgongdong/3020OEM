/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Turn advertising data into a properly formatted LTV packet
*/

#ifndef LE_ADVERTSING_MANAGER_FORMAT_H_
#define LE_ADVERTSING_MANAGER_FORMAT_H_

#include "le_advertising_manager_private.h"

/*! If the advert has space for the full local device name, or at least
    MIN_LOCAL_NAME_LENGTH, then reduce the space available by that much.

    That allows elements that precede the name to be truncated while
    leaving space for the name 

    \param[in,out] space        Pointer to variable containing the space remaining in the advert
    \param[in]     name_length  Length of the local name
    
    \returns The space reserved for the local name
*/
uint8 reserveSpaceForLocalName(uint8* space, uint16 name_length);

/*! Restore space reserved for the local name

    \param[in,out] space            Pointer to variable containing the space remaining in the advert
    \param[in]     reserved_space   Value returned from reserveSpaceForLocalName
    
    \returns The space reserved for the local name
*/
void restoreSpaceForLocalName(uint8* space, uint8 reserved_space);

/*! Add a header indicating field size and type to advertising data in correct format

    \param[in,out] ad_data  Pointer to the advertising data
    \param[in,out] space    Pointer to variable containing the space remaining in the advert
    \param[in] size         The size of the field
    \param[in] type         The type of the field
 */
uint8* addHeaderToAdData(uint8* ad_data, uint8* space, uint8 size, uint8 type);

/*! Add a 16-bit UUID to advertising data in correct format

    \param[in,out] ad_data  Pointer to the advertising data
    \param[in,out] space    Pointer to variable containing the space remaining in the advert
    \param[in] uuid         The UUID16 to add
 */
uint8* addUuidToAdData(uint8* ad_data, uint8* space, uint16 uuid);

/*! Add a 32-bit UUID to advertising data in correct format

    \param[in,out] ad_data  Pointer to the advertising data
    \param[in,out] space    Pointer to variable containing the space remaining in the advert
    \param[in] uuid         The UUID32 to add
 */
uint8* leAdvertisingManager_AddUuid32ToAdData(uint8* ad_data, uint8* space, uint32 uuid);

/*! Add a 128-bit UUID to advertising data in correct format

    \param[in,out] ad_data  Pointer to the advertising data
    \param[in,out] space    Pointer to variable containing the space remaining in the advert
    \param[in] uuid         Pointer to the UUID128 to add
 */
uint8* addUuid128ToAdData(uint8* ad_data, uint8* space, const le_advertising_manager_uuid128 *uuid);

/*! Add a Local Name to advertising data in correct format

    \param[in,out] ad_data      Pointer to the advertising data
    \param[in,out] space        Pointer to variable containing the space remaining in the advert
    \param[in] size_local_name  Size of the local name
    \param[in] local_name       Pointer to the local name
    \param[in] shortened        Inidcates whether local_name has been shortened
 */
uint8* addName(uint8 *ad_data, uint8* space, uint16 size_local_name, const uint8 * local_name, bool shortened);

/*! Add Flags to advertising data in correct format

    \param[in,out] ad_data  Pointer to the advertising data
    \param[in,out] space    Pointer to variable containing the space remaining in the advert
    \param[in] flags        Flags 
 */
uint8* addFlags(uint8* ad_data, uint8* space, uint8 flags);

/*! Add 16-bit services to advertising data in correct format

    \param[in,out] ad_data  Pointer to the advertising data
    \param[in,out] space    Pointer to variable containing the space remaining in the advert
    \param[in] service_list List of service UUIDs to advertise
    \param[in] services     Number of services in the list
 */
uint8* addServices(uint8* ad_data, uint8* space, const uint16 *service_list, uint16 services);

/*! Add 32-bit services to advertising data in correct format

    \param[in,out] ad_data  Pointer to the advertising data
    \param[in,out] space    Pointer to variable containing the space remaining in the advert
    \param[in] service_list List of service UUIDs to advertise
    \param[in] services     Number of services in the list
 */
uint8* leAdvertisingManager_AddServices32(uint8* ad_data, uint8* space, const uint32 *service_list, uint16 services);

/*! Add 128-bit services to advertising data in correct format

    \param[in,out] ad_data  Pointer to the advertising data
    \param[in,out] space    Pointer to variable containing the space remaining in the advert
    \param[in] service_list List of service UUIDs to advertise
    \param[in] services     Number of services in the list
 */
uint8* addServices128(uint8* ad_data, uint8* space, const le_advertising_manager_uuid128 *service_list, uint16 services);

/*! Add raw data to advertising data in correct format

    \param[in,out] ad_data  Pointer to the advertising data
    \param[in,out] space    Pointer to variable containing the space remaining in the advert
    \param[in] raw_data     Raw advertising data which must be properly formatted with data 
                            length as raw_data[0]
 */
uint8* addRawData(uint8* ad_data, uint8* space, uint8* raw_data);

#endif /* LE_ADVERTSING_MANAGER_FORMAT_H_ */
