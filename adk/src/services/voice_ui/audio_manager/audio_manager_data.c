/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       audio_manager_data.c
\brief      Implementation of Audio Manager Instance manupilation functions
*/

#include "audio_manager_data.h"
#include "audio_manager_message_handler.h"
#include <logging.h>
#include <stdlib.h>
#include <panic.h>

const char * const states[4] = {
    "IDLE",
    "CAPTURE_INITIATED",
    "CAPTURING",
    "STOP_CAPTURING"
};

static audio_manager_t *audio_manager = NULL;

/***************************************************/
bool AudioManager_DataInit(void)
{
    if(!audio_manager)
    {
        audio_manager = (audio_manager_t*) PanicUnlessMalloc(sizeof(audio_manager_t));
        memset(audio_manager, 0, sizeof(audio_manager_t));
        audio_manager->task.handler = AudioManager_MsgHandler;
        return TRUE;
    }
    return FALSE;
}

/********************************************************************/
bool AudioManager_IsDataInitialized(void)
{
    return ((audio_manager != NULL) ? TRUE : FALSE);
}

/*************************************************************/
void AudioManager_SetState(audio_manager_state_t new_state)
{
    DEBUG_LOGF("audioManager_SetState: Changing from %s to %s", states[audio_manager->state -1], states[new_state-1]);
    audio_manager->state = new_state;
}

/***************************************************/
audio_manager_state_t AudioManager_GetState(void)
{
    return audio_manager->state;
}

/********************************************************************/
void AudioManager_SetVoiceSource(Source voice_source)
{
    audio_manager->voice_source = voice_source;
}

/********************************************************************/
Source AudioManager_GetVoiceSource(void)
{
    return audio_manager->voice_source;
}

/*************************************************************/
Task AudioManager_GetTask(void)
{
    return &(audio_manager->task);
}

/*************************************************************/
void AudioManager_RegisterVoiceDataReceivedCallback(voiceDataReceived callback)
{
    audio_manager->callback_for_voice_data = callback;
}

/********************************************************************/
void AudioManager_TriggerVoiceDataReceivedCallback(void)
{
    audio_manager->callback_for_voice_data(audio_manager->voice_source);
}

/********************************************************************/
bool AudioManager_IsStartCaptureAllowed(void)
{
    return (audio_manager->state == audio_manager_state_idle) ? TRUE : FALSE;
}

/********************************************************************/
bool AudioManager_IsStopCaptureAllowed(void)
{
    if((audio_manager->state == audio_manager_state_capture_initiated) ||
        (audio_manager->state == audio_manager_state_capture_in_progress))
            return TRUE;

    return FALSE;
}

/********************************************************************/
bool AudioManager_AnyPendingStopRequest(void)
{
    return (audio_manager->state == audio_manager_state_capture_stopping) ? TRUE : FALSE;
}

/********************************************************************/
bool AudioManager_IsCaptureInProgress(void)
{
    return (audio_manager->state == audio_manager_state_capture_in_progress) ? TRUE : FALSE;
}

/********************************************************************/
void AudioManager_DataDeInit(void)
{
        if(audio_manager)
        {
            free(audio_manager);
            audio_manager = NULL;
        }
}

/**************************************************************/
void AudioManager_StartCaptureInitiated(void)
{
    AudioManager_SetState(audio_manager_state_capture_initiated);
}

/**************************************************************/
void AudioManager_CaptureStarted(void)
{
    AudioManager_SetState(audio_manager_state_capture_in_progress);
}

/**************************************************************/
void AudioManager_StopCaptureInitiated(void)
{
    AudioManager_SetState(audio_manager_state_capture_stopping);
}

/**************************************************************/
void AudioManager_CaptureStopped(void)
{
    AudioManager_SetState(audio_manager_state_idle);
}




