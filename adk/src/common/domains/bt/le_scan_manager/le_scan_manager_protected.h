/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief	    Protected interface to module managing le scanning.
*/

#ifndef LE_SCAN_MANAGER_PROTECTED_H_
#define LE_SCAN_MANAGER_PROTECTED_H_

#include "le_scan_manager.h"


/*! /brief Pauses active scan.
    @retval void
*/
void LeScanManager_Pause(Task init);

/*! /brief Resumes paused scan.
    @param  handle   LE scan handle returned by ScanManager_Pause()
    @retval void
*/
void LeScanManager_Resume(Task init,le_scan_handle_t handle);

#endif /* LE_SCAN_MANAGER_PROTECTED_H_ */
