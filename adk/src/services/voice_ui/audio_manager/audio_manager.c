/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       audio_manager.c
\brief      Implementation of voice assistant Audio Manager interfaces.
*/

#include "audio_manager.h"
#include "audio_manager_data.h"
#include <kymera.h>
#include <kymera_voice_capture.h>
#include <logging.h>

/* Utility Macros for checking conditions */
#define RETURN_IF_NOT_INITIALIZED()    if(!AudioManager_IsDataInitialized()) \
                                                                 { \
                                                                    return FALSE; \
                                                                 }

/******************************************************************************/
void AudioManager_Init(void)
{
    DEBUG_LOG("AudioManager_Init");
    if(AudioManager_DataInit())
    {
        AudioManager_SetState(audio_manager_state_idle);
        AudioManager_SetVoiceSource(NULL);
    }
}

/******************************************************************************/
bool AudioManager_StartCapture(voiceDataReceived voice_data_received, const va_audio_voice_capture_params_t *audio_config)
{
    DEBUG_LOG("AudioManager_StartCapture");

    RETURN_IF_NOT_INITIALIZED();

    /* Audio Manager shall not entertain invalid input parameters */
    if(!voice_data_received || !audio_config)
        Panic();

    if(AudioManager_IsStartCaptureAllowed())
    {
        Kymera_StartVoiceCapture(AudioManager_GetTask(), audio_config);
        AudioManager_RegisterVoiceDataReceivedCallback(voice_data_received);
        /* Record the state change */
        AudioManager_StartCaptureInitiated();
        return TRUE;
    }
    
    return FALSE;
}

/******************************************************************************/
bool AudioManager_StopCapture(void)
{
    DEBUG_LOG("AudioMnager_StopCapture");

    RETURN_IF_NOT_INITIALIZED();

    if(AudioManager_IsStopCaptureAllowed())
    {
        Kymera_StopVoiceCapture(AudioManager_GetTask());
         /* Record the state change */
        AudioManager_StopCaptureInitiated();
        return TRUE;
    }
    /* Client should expect success if there is ongoing capture stop request */
    else if(AudioManager_AnyPendingStopRequest())
        return TRUE;
    
    return FALSE;
}

/******************************************************/
void AudioManager_DeInit(void)
{
    AudioManager_DataDeInit();
}
