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

/* Utility function to populate the codec configuration to be passed to Audio DSP */
static void audioManager_PopulateVoiceCaptureParam(voice_capture_params_t *voice_params, 
                                                                                                     const audio_manager_audio_config_t  *audio_config)
{
    switch(audio_config->codec_type)
    {
        case audio_manager_codec_sbc:
            {
                sbc_encoder_params_t *sbc = (sbc_encoder_params_t*)(audio_config->codec_config);
                voice_params->encoder_params.sbc = *sbc;
            }
        break;

        default:
            Panic();
        break;
    }
}

/******************************************************************************/
void AudioManager_Init(void)
{
    DEBUG_LOGF("AudioManager_Init");
    if(AudioManager_DataInit())
    {
        AudioManager_SetState(audio_manager_state_idle);
        AudioManager_SetVoiceSource(NULL);
    }
}

/******************************************************************************/
bool AudioManager_StartCapture(voiceDataReceived voice_data_received, const audio_manager_audio_config_t *audio_config)
{
    DEBUG_LOGF("AudioManager_StartCapture");

    RETURN_IF_NOT_INITIALIZED();

    /* Audio Manager shall not entertain invalid input parameters */
    if(!voice_data_received || !audio_config)
        Panic();

    if(AudioManager_IsStartCaptureAllowed())
    {
        voice_capture_params_t params;
        audioManager_PopulateVoiceCaptureParam(&params, audio_config);
        Kymera_StartVoiceCapture(AudioManager_GetTask(), &params);
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
    DEBUG_LOGF("AudioMnager_StopCapture");

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



