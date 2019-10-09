/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       audio_manager_data.h
\brief      file for audio manager instance data manupilation
*/

#ifndef _AUDIO_MANAGER_DATA_
#define _AUDIO_MANAGER_DATA_

#include "audio_manager.h"

/* Defines the different states that audio manager can be in */
typedef enum
{
    audio_manager_state_idle = 1,
    audio_manager_state_capture_initiated,
    audio_manager_state_capture_in_progress,
    audio_manager_state_capture_stopping,
    audio_manager_state_max
}audio_manager_state_t;

/* Audio Manager Instance structure */
typedef struct
{
    /* Audio Manager task */
    TaskData task;
    /* Current state of audio manager */
    audio_manager_state_t state;
    /* function pointer to handle received voice data */
    voiceDataReceived callback_for_voice_data;
    /* voice data source */
    Source voice_source;
}audio_manager_t;


/***************************************************************************
DESCRIPTION
    Utility function to initializes the Audio Manager data
*/
bool AudioManager_DataInit(void);

/***************************************************************************
DESCRIPTION
    Checks if the Audio Data is initialized

RETURNS
    TRUE if Audio Manager data is initialized else FALSE
*/
bool AudioManager_IsDataInitialized(void);

/***************************************************************************
DESCRIPTION
    Utility function to set the state of Audio Manager data
 
PARAMS
    new_state - new state value to be set
*/
void AudioManager_SetState(audio_manager_state_t new_state);

/***************************************************************************
DESCRIPTION
    Utility function to get the state of Audio Manager data
 
RETURNS
    Current state of Audio Manager
*/
audio_manager_state_t AudioManager_GetState(void);

/***************************************************************************
DESCRIPTION
    Utility function to set the voice data source
 
PARAMS
    voice_source - Source pointer to voice data

*/
void AudioManager_SetVoiceSource(Source voice_source);

/***************************************************************************
DESCRIPTION
    Utility function to get the voice data source
 
RETURNS
    Voice source
*/
Source AudioManager_GetVoiceSource(void);

/***************************************************************************
DESCRIPTION
    Utility function to get the Task for Audio Manager

RETURNS
    Pointer to Audio Manager TaskData
*/
Task AudioManager_GetTask(void);

/***************************************************************************
DESCRIPTION
    Utility function to register the callback for voice data notification
 
PARAMS
    callback - function pointer which needs to be triggered on receiving voice data
*/
void AudioManager_RegisterVoiceDataReceivedCallback(voiceDataReceived callback);

/***************************************************************************
DESCRIPTION
    Utility function to trigger the registered callback when the voice data is received
*/
void AudioManager_TriggerVoiceDataReceivedCallback(void);

/***************************************************************************
DESCRIPTION
    Utility function to check if Audio Manager can allow Start Capture of voice data
 
RETURNS
    TRUE if we can allow Start Voice Capture else FALSE
*/
bool AudioManager_IsStartCaptureAllowed(void);

/***************************************************************************
DESCRIPTION
    Utility function to check if Audio Manager can allow Stop Capture of voice data
 
RETURNS
    TRUE if we can allow Stop Voice Capture else FALSE
*/
bool AudioManager_IsStopCaptureAllowed(void);

/***************************************************************************
DESCRIPTION
    Utility function to check if there is any pending Stop voice capture request

RETURNS
    TRUE if there is pending Stop Voice Capture else FALSE
*/
bool AudioManager_AnyPendingStopRequest(void);

/***************************************************************************
DESCRIPTION
    Utility function to check if voice capture is still in progress

RETURNS
    TRUE if there is pending Stop Voice Capture else FALSE
*/
bool AudioManager_IsCaptureInProgress(void);

/***************************************************************************
DESCRIPTION
    This interfaces Deinitializes the Audio Manager data

*/
void AudioManager_DataDeInit(void);

/***************************************************************************
DESCRIPTION
    Utility function to trigger initiate start capture event in Audio Manager state machine
*/
void AudioManager_StartCaptureInitiated(void);

/***************************************************************************
DESCRIPTION
    Utility function to trigger capture started event in Audio Manager state machine
*/
void AudioManager_CaptureStarted(void);

/***************************************************************************
DESCRIPTION
    Utility function to trigger initiate capture stop event in Audio Manager state machine
*/
void AudioManager_StopCaptureInitiated(void);

/***************************************************************************
DESCRIPTION
    Utility function to trigger capture stopped event in Audio Manager state machine
*/
void AudioManager_CaptureStopped(void);

/*@}*/

#endif /* _AUDIO_MANAGER_DATA_ */