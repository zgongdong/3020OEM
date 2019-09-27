/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief	    Implementation of module managing le scanning.
*/

#include "le_scan_manager_protected.h"

#include <logging.h>
#include <panic.h>
#include <vmtypes.h>

#include <stdlib.h>


/*! @brief LE scan parameters. */
typedef struct
{
    uint16  scan_interval;
    uint16  scan_window;
} le_scan_parameters_t;

/* @brief LE scan handle data. */
struct le_scan_handle
{
    le_scan_interval_t scan_interval;
    le_advertising_report_filter_t * filter;
};

static bool scan_is_active = FALSE;
static bool paused = FALSE;
static le_scan_handle_t active_handle[MAX_ACTIVE_SCANS];
static le_scan_parameters_t scan_parameters;

static void leScanManager_SetScanEnableAndActive(bool is_active);
static void leScanManager_SetScanParameters(le_scan_interval_t scan_interval);
static uint8 leScanManager_GetEmptySlotIndex(void);
static le_scan_handle_t leScanManager_AquireScan(le_scan_interval_t scan_interval, le_advertising_report_filter_t * filter);
static void leScanManager_SetScanAndAddAdvertisingReport(le_scan_interval_t scan_interval, le_advertising_report_filter_t * filter);
static uint8 leScanManager_GetIndexFromHandle(le_scan_handle_t handle);
static bool leScanManager_ReleaseScan(le_scan_handle_t handle);
static void leScanManager_ReInitialiasePreviousActiveScans(void);
static bool leScanManager_CheckForFastScanInterval(void);


le_scan_handle_t LeScanManager_Start(le_scan_interval_t scan_interval, le_advertising_report_filter_t * filter)
{
    le_scan_handle_t handle = leScanManager_AquireScan(scan_interval, filter);

    if (handle != NULL)
    {
        DEBUG_LOG("LeScanManager_Start new handle created.");

        leScanManager_SetScanAndAddAdvertisingReport(scan_interval, filter);
    }

    return handle;
}

void LeScanManager_Stop(le_scan_handle_t handle)
{
    PanicNull(handle);

    if (leScanManager_ReleaseScan(handle))
    {
        DEBUG_LOG("LeScanManager_Stop handle released.");

        leScanManager_SetScanEnableAndActive(FALSE);

        ConnectionBleClearAdvertisingReportFilter();
        leScanManager_ReInitialiasePreviousActiveScans();
    }
}

bool ScanManager_IsScanActive(void)
{
    return scan_is_active;
}

void LeScanManager_Pause(Task init)
{
    UNUSED(init);
}

void LeScanManager_Resume(Task init, le_scan_handle_t handle)
{
    UNUSED(init);
    UNUSED(handle);
}

static void leScanManager_SetScanEnableAndActive(bool is_active)
{
    ConnectionDmBleSetScanEnable(is_active);
    scan_is_active = is_active;
}

/*  /brief Set LE scan parameters based on requested speed.
    @param  scan interval
 */
static void leScanManager_SetScanParameters(le_scan_interval_t scan_interval)
{
    if ((scan_interval == le_scan_interval_slow) && !leScanManager_CheckForFastScanInterval())
    {
        scan_parameters.scan_interval = SCAN_INTERVAL_SLOW;
        scan_parameters.scan_window = SCAN_WINDOW_SLOW;
    }
    else
    {
        scan_parameters.scan_interval = SCAN_INTERVAL_FAST;
        scan_parameters.scan_window = SCAN_WINDOW_FAST;
    }
}

/*  /brief Gets the first available empty slot index.
    @retval The index number or MAX_ACTIVE_SCANS if there are no empty slots
*/
static uint8 leScanManager_GetEmptySlotIndex(void)
{
    uint8 handle_index;

    for (handle_index = 0; handle_index < MAX_ACTIVE_SCANS; handle_index++)
    {
        if (active_handle[handle_index] == NULL)
        {
            break;
        }
    }

    return handle_index;
}

/*  /brief Aquire handle to start scanning for an LE device.
    @param  scan interval
    @param  pointer to filter parameters
    @retval handle
*/
static le_scan_handle_t leScanManager_AquireScan(le_scan_interval_t scan_interval, le_advertising_report_filter_t * filter)
{
    le_scan_handle_t handle;
    uint8 handle_index = leScanManager_GetEmptySlotIndex();

    if (handle_index < MAX_ACTIVE_SCANS)
    {
        DEBUG_LOG("leScanManager_AquireScan handle available.");

        active_handle[handle_index] = PanicUnlessMalloc(sizeof(struct le_scan_handle));
        active_handle[handle_index]->scan_interval = scan_interval;
        active_handle[handle_index]->filter = filter;

        handle = active_handle[handle_index];
    }
    else
    {
        DEBUG_LOG("leScanManager_AquireScan handle unavailable.");

        handle = NULL;
    }

    return handle;
}

/*  /brief Sets scan, connection parameters and then adds advertising filter and enables scan.
    @param  scan interval
    @param  pointer to filter parameters
 */
static void leScanManager_SetScanAndAddAdvertisingReport(le_scan_interval_t scan_interval, le_advertising_report_filter_t * filter)
{
    leScanManager_SetScanParameters(scan_interval);

    ConnectionDmBleSetScanParametersReq(ENABLE_ACTIVE_SCANNING,
                                        RANDOM_OWN_ADDRESS,
                                        WHITE_LIST_ONLY,
                                        scan_parameters.scan_interval,
                                        scan_parameters.scan_window);

    ConnectionBleAddAdvertisingReportFilter(filter->ad_type,
                                            filter->interval,
                                            filter->size_pattern,
                                            filter->pattern);

    if (!paused)
    {
        leScanManager_SetScanEnableAndActive(TRUE);
    }
}

/*  /brief Matches a handle to its represetative index number.
    @param  pointer to handle to be released
    @retval The index number or MAX_ACTIVE_SCANS if a handle does not match
*/
static uint8 leScanManager_GetIndexFromHandle(le_scan_handle_t handle)
{
    uint8 handle_index;

    for (handle_index = 0; handle_index < MAX_ACTIVE_SCANS; handle_index++)
    {
        if ((active_handle[handle_index] == handle) && (handle != NULL))
        {
            break;
        }
    }

    return handle_index;
}

/*  /brief Release handle scanning for an LE device.
    @param  pointer to handle to be released
    @retval TRUE if the handle is released
*/
static bool leScanManager_ReleaseScan(le_scan_handle_t handle)
{
    bool released = FALSE;
    uint8 handle_index = leScanManager_GetIndexFromHandle(handle);

    if (handle_index < MAX_ACTIVE_SCANS)
    {
        DEBUG_LOG("leScanManager_ReleaseScan handle released.");

        memset(handle, 0, sizeof(le_scan_handle_t));
        free(handle);
        active_handle[handle_index] = NULL;

        released = TRUE;
    }

    return released;
}

/*  /brief Finds out the last active scan and re-initialises it. */
static void leScanManager_ReInitialiasePreviousActiveScans(void)
{
    uint8 handle_index;

    for (handle_index = 0; handle_index < MAX_ACTIVE_SCANS; handle_index++)
    {
        if (active_handle[handle_index] != NULL)
        {
            leScanManager_SetScanAndAddAdvertisingReport(active_handle[handle_index]->scan_interval, active_handle[handle_index]->filter);
        }
    }
}

/*  /brief Check active handles for fast scan interval.
    @retval TRUE if a fast scan interval exists
*/
static bool leScanManager_CheckForFastScanInterval(void)
{
    bool fast_scan_exists = FALSE;
    uint8 handle_index;

    for (handle_index = 0; handle_index < MAX_ACTIVE_SCANS; handle_index++)
    {
        if (active_handle[handle_index] != NULL)
        {
            if (active_handle[handle_index]->scan_interval == le_scan_interval_fast)
            {
                fast_scan_exists = TRUE;
                break;
            }
        }
    }

    return fast_scan_exists;
}
