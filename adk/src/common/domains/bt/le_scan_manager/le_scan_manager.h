/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   le_scan_manager LE Scan Manager
\ingroup    bt_domain
\brief	    LE scanning component

LE scanning component with support for multiple clients.
Clients can request its own scan interval and advertising filter.

Each advertising filter request is applied.
Which means the a set of passed-through adverts is a combination of all client's requests.

Whereas the scan interval is worked out by the LE Scan Manager.
If at least one client requests fast scan interval then the scan interval will be fast.
Otherwise the scan interval will be slow.

Scan window is not an explicit parameter but it is derived from the scan interval.

*/

#ifndef LE_SCAN_MANAGER_H_
#define LE_SCAN_MANAGER_H_

#include <connection.h>


#define ENABLE_ACTIVE_SCANNING  FALSE
#define RANDOM_OWN_ADDRESS      FALSE
#define WHITE_LIST_ONLY         FALSE

#define SCAN_INTERVAL_SLOW      0x1800  /* 3.84s Tgap(scan_slow_interval1_coded) */
#define SCAN_WINDOW_SLOW        0x36    /* 33.75ms Tgap(scan_slow_window1_coded) */
#define SCAN_INTERVAL_FAST      0x91    /* 90.625ms Tgap(scan_fast_interval_coded) */
#define SCAN_WINDOW_FAST        0x90    /* 90ms Tgap(scan_fast_window_coded) */

#define MAX_ACTIVE_SCANS        2


/*! \brief Opaque type for an le scan handle index object. */
typedef struct le_scan_handle * le_scan_handle_t;

/*! \brief LE scan interval types
*/
typedef enum
{
    le_scan_interval_slow = 0,
    le_scan_interval_fast = 1,
} le_scan_interval_t;

/*! \brief LE advertising report filter. */
typedef struct
{
    ble_ad_type ad_type;
    uint16 interval;
    uint16 size_pattern;
    uint8* pattern;
} le_advertising_report_filter_t;

/*! \brief Confirmation Messages to be sent to Clients for Scan Module Confirmtion */
enum scan_manager_messages
{
    SCAN_MANAGER_INIT_CFM,    /*!< Indicate SM has been initialised */
    SCAN_MANAGER_ENABLE_CFM,  /*!< Sm Module Enabled */
    SCAN_MANAGER_DISABLE_CFM,  /*!< Sm Module Disabled */
    SCAN_MANAGER_START_PENDING_CFM,  /*!< Sm Module Starting the Scan */
    SCAN_MANAGER_START_FAIL_CFM,  /*!< Sm Module Failed the Scan Start Request */
    SCAN_MANAGER_STOP_PENDING_CFM,  /*!< Sm Module Stopping the scan */
    SCAN_MANAGER_PAUSE_PENDING_CFM,  /*!< Sm Module Pausing the Scan */
    SCAN_MANAGER_START_SCANNING_CFM,  /*!< Sm Module Scan Started */
    SCAN_MANAGER_STOP_SCANNING_CFM,  /*!< Sm Module Scan Stopped */
    SCAN_MANAGER_PAUSE_CFM,  /*!< Sm Module Scan Paused */
    SCAN_MANAGER_RESUME_CFM,  /*!< Sm Module Scan Resumed */
    SCAN_MANAGER_ADV_REPORT_CFM, /*!< Sm Module ADV Report */

};

/*! \brief Message to store handle for  Pause API */
typedef struct
{
    /*! Message sent when LeScanManager_Pause() sarts */
    le_scan_handle_t handle;
} SCAN_MANAGER_PAUSE_PENDING_CFM_T;

/*\{*/

/*! \brief Start scanning for an LE device.
    @param fast     scan interval
    @param filter   pointer to advertising filter
    @retval handle  LE scan handle
*/
le_scan_handle_t LeScanManager_Start(le_scan_interval_t scan_interval, le_advertising_report_filter_t * filter);

/*! \brief Stop scanning for an LE device.
    @param handle   LE scan handle
*/
void LeScanManager_Stop(le_scan_handle_t handle);

/*! \brief Checks if there are active scans.
    @retval TRUE, if there are active scans.
*/
bool ScanManager_IsScanActive(void);

/*\}*/

#endif /* LE_SCAN_MANAGER_H_ */
