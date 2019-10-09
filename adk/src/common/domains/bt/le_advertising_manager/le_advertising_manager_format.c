/*!
\copyright  Copyright (c) 2018 - 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Turn advertising data into a properly formatted LTV packet
*/

#include "le_advertising_manager_format.h"

#include <connection.h>

/*! Write a single value to the advertising data buffer

    This macro adds a single value into the buffer, incrementing the pointer
    into the buffer and reducing the space available.

    \param[in,out] ad_data Pointer to the next position in the advertising data
    \param[in,out] space   Variable with the amount of remaining space in buffer
    \param[in]     value   Value (octet) to add to the advertising data
*/
#define WRITE_AD_DATA(ad_data, space, value) do \
                                                { \
                                                    *ad_data = value; \
                                                    ad_data++; \
                                                    (*space)--; \
                                                } while(0)

uint8 reserveSpaceForLocalName(uint8* space, uint16 name_length)
{
    uint8 required_space = MIN(name_length, MIN_LOCAL_NAME_LENGTH);

    if((*space) >= required_space)
    {
        *space -= required_space;
        return required_space;
    }
    return 0;
}

void restoreSpaceForLocalName(uint8* space, uint8 reserved_space)
{
    *space += reserved_space;
}

uint8* addHeaderToAdData(uint8* ad_data, uint8* space, uint8 size, uint8 type)
{
    WRITE_AD_DATA(ad_data, space, size);
    WRITE_AD_DATA(ad_data, space, type);

    return ad_data;
}

uint8* addUuidToAdData(uint8* ad_data, uint8* space, uint16 uuid)
{
    WRITE_AD_DATA(ad_data, space, uuid & 0xFF);
    WRITE_AD_DATA(ad_data, space, uuid >> 8);

    return ad_data;
}

uint8* leAdvertisingManager_AddUuid32ToAdData(uint8* ad_data, uint8* space, uint32 uuid)
{
    WRITE_AD_DATA(ad_data, space, uuid & 0xFF);
    WRITE_AD_DATA(ad_data, space, (uuid >> 8) & 0xFF);
    WRITE_AD_DATA(ad_data, space, (uuid >> 16) & 0xFF);
    WRITE_AD_DATA(ad_data, space, (uuid >> 24) & 0xFF);

    return ad_data;
}

uint8* addUuid128ToAdData(uint8* ad_data, uint8* space, const le_advertising_manager_uuid128 *uuid)
{
    memcpy(ad_data, uuid->uuid, OCTETS_PER_128BIT_SERVICE);
    ad_data += OCTETS_PER_128BIT_SERVICE;
    *space -= OCTETS_PER_128BIT_SERVICE;

    return ad_data;
}

uint8* addName(uint8 *ad_data, uint8* space, uint16 size_local_name, const uint8 * local_name, bool shortened)
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

uint8* addFlags(uint8* ad_data, uint8* space, uint8 flags)
{
    uint8 usable_space = USABLE_SPACE(space);

    if (usable_space >= 1)
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

uint8* addServices(uint8* ad_data, uint8* space, const uint16 *service_list, uint16 services)
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

uint8* leAdvertisingManager_AddServices32(uint8* ad_data, uint8* space, const uint32 *service_list, uint16 services)
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

uint8* addServices128(uint8* ad_data, uint8* space, const le_advertising_manager_uuid128 *service_list, uint16 services)
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

uint8* addRawData(uint8* ad_data, uint8* space, uint8* raw_data)
{
    if(raw_data)
    {
        uint8 size = raw_data[0] + 1;
        if(*space >= size)
        {
            memcpy(ad_data, raw_data, size);
            *space -= size;
            ad_data += size;
        }
    }
    return ad_data;
}
