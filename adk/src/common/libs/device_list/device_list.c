/*!
\copyright  Copyright (c) 2018 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\brief      A collection of device objects that can be retrieved by property type and value.
*/

#include <string.h>

#include <device_list.h>
#include <panic.h>


#define DEVICE_LIST_MAX_NUM_DEVICES 8

static device_t device_list[DEVICE_LIST_MAX_NUM_DEVICES];


void DeviceList_Init(void)
{
    memset(&device_list, 0, sizeof(device_list));
}

unsigned DeviceList_GetNumOfDevices(void)
{
    int i;
    unsigned count = 0;

    for (i = 0; i < DEVICE_LIST_MAX_NUM_DEVICES; i++)
    {
        if (device_list[i])
            count++;
    }

    return count;
}

void DeviceList_RemoveAllDevices(void)
{
    int i;

    for (i = 0; i < DEVICE_LIST_MAX_NUM_DEVICES; i++)
    {
        if (device_list[i])
        {
            /* Should the device be destroyed by this function? */
            device_list[i] = 0;
        }
    }
}

bool DeviceList_AddDevice(device_t device)
{
    int i;
    bool added = FALSE;

    for (i = 0; i < DEVICE_LIST_MAX_NUM_DEVICES; i++)
    {
        if (device_list[i] == 0)
        {
            device_list[i] = device;
            added = TRUE;
            break;
        }
        else if (device_list[i] == device)
        {
            added = FALSE;
            break;
        }
    }

    return added;
}

void DeviceList_RemoveDevice(device_t device)
{
    int i;

    for (i = 0; i < DEVICE_LIST_MAX_NUM_DEVICES; i++)
    {
        if (device_list[i] == device)
        {
            device_list[i] = 0;
            /* Should the device be destroyed by this function? */
            break;
        }
    }
}

static void deviceList_FindMatchingDevices(device_property_t id, void *value, size_t size, bool just_first, device_t *device_array, unsigned *len_device_array)
{
    int i;
    unsigned num_found_devices = 0;

    for (i = 0; i < DEVICE_LIST_MAX_NUM_DEVICES; i++)
    {
        if (device_list[i])
        {
            void *property;
            size_t property_size;

            if (Device_GetProperty(device_list[i], id, &property, &property_size))
            {
                if ((size == property_size)
                    && !memcmp(value, property, size))
                {
                    device_array[num_found_devices] = device_list[i];
                    num_found_devices += 1;

                    if (just_first)
                        break;
                }
            }
        }
    }
    *len_device_array = num_found_devices;
}

device_t DeviceList_GetFirstDeviceWithPropertyValue(device_property_t id, void *value, size_t size)
{
    device_t found_devices[1] = {0};
    unsigned num_found_devices = 0;

    deviceList_FindMatchingDevices(id, value, size, TRUE, found_devices, &num_found_devices);
    PanicFalse(num_found_devices <= 1);

    return found_devices[0];
}

void DeviceList_GetAllDevicesWithPropertyValue(device_property_t id, void *value, size_t size, device_t **device_array, unsigned *len_device_array)
{
    unsigned num_found_devices = 0;
    device_t found_devices[DEVICE_LIST_MAX_NUM_DEVICES] = { 0 };

    deviceList_FindMatchingDevices(id, value, size, FALSE, found_devices, &num_found_devices);

    if (num_found_devices)
    {
        *device_array = (device_t*)PanicUnlessMalloc(num_found_devices*sizeof(device_t));
        memcpy(*device_array, found_devices, num_found_devices*sizeof(device_t));
    }
    else
    {
        *device_array = NULL;
    }
    *len_device_array = num_found_devices;
}

void DeviceList_Iterate(device_list_iterate_callback_t action, void *action_data)
{
    int i;

    PanicFalse(action);

    for (i = 0; i < DEVICE_LIST_MAX_NUM_DEVICES; i++)
    {
        if (device_list[i])
        {
            action(device_list[i], action_data);
        }
    }
}
