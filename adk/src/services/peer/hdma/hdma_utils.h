/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Hearder for Utility functions required for HDMA Core.
*/

#ifdef INCLUDE_HDMA

#ifndef HDMA_UTILS_H
#define HDMA_UTILS_H

#ifdef DEBUG_HDMA_UT
#include "types.h"
#include <stdlib.h>
#include <stdio.h>
#endif
#ifndef DEBUG_HDMA_UT
#include <macros.h>
#include <logging.h>
#endif

#ifdef DEBUG_HDMA_UT
    #define TRUE 1
    #define FALSE 0
    #define DEBUG 0
    #define MAX( a, b ) ( ( a > b) ? a : b )
    #define MIN( a, b ) ( ( a < b) ? a : b )
#endif


#ifdef DEBUG_HDMA_UT
    #define HDMA_DEBUG_LOG(...) printf(__VA_ARGS__)
#else
    #define HDMA_DEBUG_LOG(...) DEBUG_LOG(__VA_ARGS__)
#endif

#ifdef DEBUG_HDMA_UT
    #define HDMA_MALLOC(T) (T*)malloc(sizeof(T))
#else
    #define HDMA_MALLOC(T) PanicUnlessMalloc(sizeof(T))
#endif

#define FORTYEIGHTKB 49152
#define BUFFER_LEN 10
#define MIN_UPDATE_INT_MS 0
#define MIN_HANDOVER_RETRY_TIME_LOW_MS 0
#define MIN_HANDOVER_RETRY_TIME_HIGH_MS 0
#define MIN_HANDOVER_RETRY_TIME_CRITICAL_MS 0
#define OUT_OF_EAR_TIME_BEFORE_HANDOVER_MS 1500
#define INVALID_TIMESTAMP 0xFFFFFFF
#define HDMA_UNKNOWN 0xFF
#define HDMA_UNKNOWN_QUALITY 0xFF

#define IN_EAR_FALLBACK TRUE
#define HDMA_INVALID -1

/*! \brief Returns rounded value to nearest integer.

*/
#define ROUND(num,denom) ((num < 0)? ((((num << 1)/denom) - 1) >> 1): ((((num << 1)/denom) + 1)>> 1))
typedef struct{
    int16 critical;
    int16 high;
    int16 low;
}hdma_urgency_thresholds_t;

/*  Following filters specify half life, max age, and thresholds by urgency: critical, high, low for RSSI Quality  */
extern hdma_urgency_thresholds_t rssiHalfLife_ms;
extern hdma_urgency_thresholds_t rssiMaxAge_ms;
extern hdma_urgency_thresholds_t absRSSIThreshold;
extern hdma_urgency_thresholds_t relRSSIThreshold;

/*  Following filters specify half life, max age, and thresholds by urgency: critical, high, low for Voice Quality  */
extern hdma_urgency_thresholds_t vqHalfLife_ms ;
extern hdma_urgency_thresholds_t vqMaxAge_ms;
extern hdma_urgency_thresholds_t absVQThreshold ;
extern hdma_urgency_thresholds_t relVQThreshold;

#endif

#endif /* INCLUDE_HDMA */
