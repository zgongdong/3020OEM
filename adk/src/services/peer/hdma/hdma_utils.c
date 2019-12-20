/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Utility functions and constants required for HDMA Core.
*/

#ifdef INCLUDE_HDMA

#include "hdma_utils.h"


#ifdef INCLUDE_HDMA_RSSI_EVENT
/*  Following filters specify half life, max age, and thresholds by urgency: critical, high, low for RSSI */
const hdma_urgency_thresholds_t rssiHalfLife_ms = {1000, 2000, 5000};
const hdma_urgency_thresholds_t rssiMaxAge_ms = {3000, 10000, 30000};
const hdma_urgency_thresholds_t absRSSIThreshold = {-80, -75, -70};
const hdma_urgency_thresholds_t relRSSIThreshold = {8, 10, 15};
#endif

#ifdef INCLUDE_HDMA_MIC_QUALITY_EVENT
/*  Following filters specify half life, max age, and thresholds by urgency: critical, high, low for Voice Quality  */
const hdma_urgency_thresholds_t vqHalfLife_ms = {1000, 2000, 5000};
const hdma_urgency_thresholds_t vqMaxAge_ms = {3000, 10000, 30000};
const hdma_urgency_thresholds_t absVQThreshold = {4, 7, 15};
const hdma_urgency_thresholds_t relVQThreshold = {2, 2, 4};
#endif

#endif /* INCLUDE_HDMA */
