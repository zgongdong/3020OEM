/*!
\copyright  Copyright (c) 2018 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       audio_connect_params.h
\brief      Connect parameters structure.
*/

#ifndef AUDIO_CONNECT_PARAMS_H_
#define AUDIO_CONNECT_PARAMS_H_

#include <audio_plugin_if.h>

typedef struct
{
    Task audio_plugin;
    Sink audio_sink;
    AUDIO_SINK_T sink_type;
    int16 volume;
    uint32 rate;
    AudioPluginFeatures features;
    AUDIO_MODE_T mode;
    AUDIO_ROUTE_T route;
    void * params;
    Task app_task;
} audio_connect_parameters_t;

#endif /* AUDIO_CONNECT_PARAMS_H_ */
