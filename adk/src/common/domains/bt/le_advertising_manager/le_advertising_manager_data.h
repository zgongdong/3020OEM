/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Manage execution of callbacks to construct adverts and scan response
*/

#ifndef LE_ADVERTSING_MANAGER_DATA_H_
#define LE_ADVERTSING_MANAGER_DATA_H_

#include "le_advertising_manager.h"
#include "le_advertising_manager_private.h"

/*!
    Initialise (zero) scan response
 */
void leAdvertisingManager_InitScanResponse(void);

/*!
    Get scan response data from clients and add it to our scan response
    
    \param set Mask of the data set(s) to obtain data for
    
    \returns TRUE if successful, FALSE otherwise
 */
bool leAdvertisingManager_SetupScanResponseData(le_adv_data_set_t set);

/*!
    Get advertising data from clients and add it to our adverts
    
    \param set Mask of the data set(s) to obtain data for
    
    \returns TRUE if successful, FALSE otherwise
 */
bool leAdvertisingManager_SetupAdvertData(le_adv_data_set_t set);

/*!
    Clear advert data
 */
void leAdvertisingManager_ClearAdvertData(void);

#endif /* LE_ADVERTSING_MANAGER_DATA_H_ */
