/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
*/

#include "connection_manager_params.h"

#define LE_FIXED_PARAMS \
    .scan_interval              = 320, \
    .scan_window                = 80, \
    .supervision_timeout        = 400, \
    .conn_attempt_timeout       = 50, \
    .conn_latency_max           = 64, \
    .supervision_timeout_min    = 400, \
    .supervision_timeout_max    = 400, \
    .own_address_type           = TYPED_BDADDR_PUBLIC

/* Taken from sink app slave parameters */
#define LE_LP_CONNECTION_PARAMS \
    .conn_interval_min  = 72, \
    .conn_interval_max  = 88, \
    .conn_latency       = 4

static const ble_connection_params low_power_connection_params = 
{
    LE_FIXED_PARAMS, LE_LP_CONNECTION_PARAMS
};

/* Taken from sink app master initial parameters */
#define LE_LL_CONNECTION_PARAMS \
    .conn_interval_min  = 24, \
    .conn_interval_max  = 40, \
    .conn_latency       = 0

static const ble_connection_params low_latency_connection_params = 
{
    LE_FIXED_PARAMS, LE_LL_CONNECTION_PARAMS
};

/* Taken from sink app Bisto parameters */
#define LE_AUDIO_CONNECTION_PARAMS \
    .conn_interval_min  = 12, \
    .conn_interval_max  = 24, \
    .conn_latency       = 4

static const ble_connection_params audio_connection_params = 
{
    LE_FIXED_PARAMS, LE_AUDIO_CONNECTION_PARAMS
};

const ble_connection_params* cm_qos_params[cm_qos_max] = 
{
    [cm_qos_invalid]        = NULL,
    [cm_qos_low_power]      = &low_power_connection_params,
    [cm_qos_low_latency]    = &low_latency_connection_params,
    [cm_qos_audio]          = &audio_connection_params,
    [cm_qos_passive]        = &low_power_connection_params
};
