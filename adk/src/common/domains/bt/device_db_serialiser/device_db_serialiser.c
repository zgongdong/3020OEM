/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief	    Implementation of the Device Database Serialiser module.
*/

#include "device_db_serialiser.h"
#include "device_properties.h"

#include <connection_no_ble.h>
#include <device_list.h>
#include <panic.h>
#include <stdlib.h>

#define SIZE_OF_TYPE            0x1
#define SIZE_OF_LEN             0x1
#define SIZE_OF_TYPE_AND_LEN    (SIZE_OF_TYPE + SIZE_OF_LEN)

#define TYPE_OFFSET_IN_FRAME    0x0
#define LEN_OFFSET_IN_FRAME     SIZE_OF_TYPE
#define PAYLOAD_OFFSET_IN_FRAME SIZE_OF_TYPE_AND_LEN

#define DEFAULT_PDD_FRAME_LEN_BYTES     32

#define DBS_PDD_FRAME_TYPE      0xDB

#define MAX_NUM_DEVICES_IN_PDL  8

/*! \brief Stores the function pointers for a registered PDDU */
typedef struct
{
    unsigned id;
    get_persistent_device_data_len_bits get_len;
    serialise_persistent_device_data ser;
    deserialise_persistent_device_data deser;

} device_db_serialiser_registered_pddu_t;

device_db_serialiser_registered_pddu_t *registered_pddu_list = NULL;

uint8 num_registered_pddus = 0;

void DeviceDbSerialiser_Init(void)
{
    num_registered_pddus = 0;
}

void DeviceDbSerialiser_RegisterPersistentDeviceDataUser(
        unsigned pddu_id,
        get_persistent_device_data_len_bits get_len,
        serialise_persistent_device_data ser,
        deserialise_persistent_device_data deser)
{
    PanicFalse(get_len);
    PanicFalse(ser);
    PanicFalse(deser);

    if (!registered_pddu_list)
    {
       registered_pddu_list = (device_db_serialiser_registered_pddu_t*)PanicUnlessMalloc(sizeof(device_db_serialiser_registered_pddu_t));
    }
    else
    {
        registered_pddu_list = realloc(registered_pddu_list, sizeof(device_db_serialiser_registered_pddu_t)*(num_registered_pddus + 1));
        PanicNull(registered_pddu_list);
    }

    registered_pddu_list[num_registered_pddus].id = pddu_id;
    registered_pddu_list[num_registered_pddus].get_len = get_len;
    registered_pddu_list[num_registered_pddus].ser = ser;
    registered_pddu_list[num_registered_pddus].deser = deser;

    num_registered_pddus++;
}

static uint8 * deviceDbSerialiser_getAllPddusPayloadLengths(device_t device)
{
    uint8 *pddus_len = (uint8 *)PanicUnlessMalloc(num_registered_pddus*sizeof(uint8));

    for (int i=0; i<num_registered_pddus; i++)
    {
        uint8 pdd_len = registered_pddu_list[i].get_len(device);
        if (pdd_len)
            pdd_len += SIZE_OF_TYPE_AND_LEN;
        pddus_len[registered_pddu_list[i].id] = pdd_len;
    }

    return pddus_len;
}

static uint8 * deviceDbSerialiser_createPddFrame(uint16 total_len)
{
    total_len += SIZE_OF_TYPE_AND_LEN;
    uint8 * buffer = (uint8 *)PanicUnlessMalloc(total_len);

    buffer[TYPE_OFFSET_IN_FRAME] = DBS_PDD_FRAME_TYPE;
    buffer[LEN_OFFSET_IN_FRAME] = total_len;

    return buffer;
}

static void deviceDbSerialiser_addPdduData(device_t device, device_db_serialiser_registered_pddu_t *pddu, uint8 *payload, uint8 len)
{
    payload[TYPE_OFFSET_IN_FRAME] = pddu->id;
    payload[LEN_OFFSET_IN_FRAME] = len;
    pddu->ser(device, &payload[PAYLOAD_OFFSET_IN_FRAME], 0);
}

static void deviceDbSerialiser_populatePddFrame(device_t device, uint8 * pdd_frame, uint8 *pddus_len)
{
    uint8 offset = 0;
    uint8 * payload = pdd_frame + PAYLOAD_OFFSET_IN_FRAME;

    for (int i=0; i<num_registered_pddus; i++)
    {
        device_db_serialiser_registered_pddu_t *curr_pddu = &registered_pddu_list[i];

        deviceDbSerialiser_addPdduData(device, curr_pddu, &payload[offset], pddus_len[curr_pddu->id]);

        offset += pddus_len[curr_pddu->id];
    }
}

static inline uint16 deviceDbSerialiser_sumAllPdduPayloads(uint8 *pddus_payload_lengths)
{
    uint8 sum_of_individual_pddu_frames = 0;
    for (int pddu_index=0; pddu_index<num_registered_pddus; pddu_index++)
        sum_of_individual_pddu_frames += pddus_payload_lengths[pddu_index];
    return sum_of_individual_pddu_frames;
}

static void deviceDbSerialiser_SerialiseDevice(device_t device, void *data)
{
    uint8 pdd_frame_payload_len = 0;
    uint8 *pddus_payload_lengths = NULL;
    bdaddr *device_bdaddr = NULL;
    size_t bdaddr_size;

    UNUSED(data);

    if (!registered_pddu_list)
        return;

    // Only store bluetooth devices in the PDL, so the device must have a bdaddr
    if (!Device_GetProperty(device, device_property_bdaddr, (void *)&device_bdaddr, &bdaddr_size))
        return;

    PanicFalse(bdaddr_size == sizeof(bdaddr));

    pddus_payload_lengths = deviceDbSerialiser_getAllPddusPayloadLengths(device);

    pdd_frame_payload_len = deviceDbSerialiser_sumAllPdduPayloads(pddus_payload_lengths);

    if (pdd_frame_payload_len)
    {
        uint8 *pdd_frame = deviceDbSerialiser_createPddFrame(pdd_frame_payload_len);

        deviceDbSerialiser_populatePddFrame(device, pdd_frame, pddus_payload_lengths);

        ConnectionSmPutAttributeReq(0, TYPED_BDADDR_PUBLIC, device_bdaddr, pdd_frame[LEN_OFFSET_IN_FRAME], pdd_frame);

        free(pdd_frame);
    }

    free(pddus_payload_lengths);
}

void DeviceDbSerialiser_Serialise(void)
{
    DeviceList_Iterate(deviceDbSerialiser_SerialiseDevice, NULL);
}

static void deviceDbSerialiser_checkMemAllocForPddFrame(uint8 pdl_index, uint8 **pdd_frame)
{
    typed_bdaddr taddr = {0};
    uint8 len_pdd_frame = (*pdd_frame)[LEN_OFFSET_IN_FRAME];
    if (len_pdd_frame > DEFAULT_PDD_FRAME_LEN_BYTES)
    {
        *pdd_frame = realloc(*pdd_frame, len_pdd_frame);

        PanicFalse(*pdd_frame);
        PanicFalse(ConnectionSmGetIndexedAttributeNowReq(0, pdl_index, len_pdd_frame, *pdd_frame, &taddr));
        PanicFalse((*pdd_frame)[TYPE_OFFSET_IN_FRAME] == DBS_PDD_FRAME_TYPE);
    }
}

static device_db_serialiser_registered_pddu_t * deviceDbSerialiser_getRegisteredPddu(uint8 id)
{
    device_db_serialiser_registered_pddu_t * pddu = NULL;
    if (registered_pddu_list && num_registered_pddus)
    {
        for (int i=0; i<num_registered_pddus; i++)
        {
            if (registered_pddu_list[i].id == id)
            {
                pddu = &registered_pddu_list[i];
                break;
            }
        }
    }
    return pddu;
}

static void deviceDbSerialiser_deserialisePddFrame(device_t device, uint8 *pdd_frame)
{
    uint8 *end_of_pdd_frame_addr = pdd_frame + pdd_frame[LEN_OFFSET_IN_FRAME];
    uint8 *payload = &pdd_frame[PAYLOAD_OFFSET_IN_FRAME];

    uint8 pddu_frame_counter_watchdog = 1;
    while (payload < end_of_pdd_frame_addr && pddu_frame_counter_watchdog)
    {
        uint8 this_pddu_id = payload[TYPE_OFFSET_IN_FRAME];
        uint8 this_pddu_len = payload[LEN_OFFSET_IN_FRAME];
        device_db_serialiser_registered_pddu_t *curr_pddu = deviceDbSerialiser_getRegisteredPddu(this_pddu_id);

        if (curr_pddu)
        {
            curr_pddu->deser(device, &payload[PAYLOAD_OFFSET_IN_FRAME], 0);
        } // If we don't know the PDDU, ignore it (for now - could panic - it may indicate corrupt data in PDL)

        payload += this_pddu_len;
        pddu_frame_counter_watchdog += 1;
    }
    PanicFalse(pddu_frame_counter_watchdog);
}

static void deviceDbSerialiser_GetDeviceAttributesFromPdlAndDeserialise(uint8 pdl_index, uint8 **pdd_frame)
{
    typed_bdaddr taddr = {0};
    if (ConnectionSmGetIndexedAttributeNowReq(0, pdl_index, DEFAULT_PDD_FRAME_LEN_BYTES, *pdd_frame, &taddr))
    {
        device_t device = NULL;

        PanicFalse((*pdd_frame)[TYPE_OFFSET_IN_FRAME] == DBS_PDD_FRAME_TYPE);

        deviceDbSerialiser_checkMemAllocForPddFrame(pdl_index, pdd_frame);

        device = Device_Create();
        Device_SetProperty(device, device_property_bdaddr, &taddr.addr, sizeof(bdaddr));
        DeviceList_AddDevice(device);

        deviceDbSerialiser_deserialisePddFrame(device, *pdd_frame);
    }
}

void DeviceDbSerialiser_Deserialise(void)
{
    uint8 *pdd_frame = (uint8 *)PanicUnlessMalloc(DEFAULT_PDD_FRAME_LEN_BYTES);
    
    for (uint8 pdl_index = 0; pdl_index < MAX_NUM_DEVICES_IN_PDL; pdl_index++)
    {
        deviceDbSerialiser_GetDeviceAttributesFromPdlAndDeserialise(pdl_index, &pdd_frame);
    }

    free(pdd_frame);
}
