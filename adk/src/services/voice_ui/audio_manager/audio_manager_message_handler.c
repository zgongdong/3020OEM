/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       audio_manager_message_handler.c
\brief      Implementation of functions to handle messages from Audio DSP
*/

#include "audio_manager_message_handler.h"
#include "audio_manager_data.h"
#include <kymera.h>
#include <kymera_voice_capture.h>
#include <logging.h>

/* No More interested in message from the source*/
static void audioManager_DeregisterSource(void)
{
    if(AudioManager_GetVoiceSource())
        MessageStreamTaskFromSource(AudioManager_GetVoiceSource(), NULL);
}

/* Register to get MORE_DATA message from Audio DSP */
static void audioManager_RegisterSource(Source new_src)
{
    if(new_src)
    {
        MessageStreamTaskFromSource(new_src, AudioManager_GetTask());
        SourceConfigure(new_src, VM_SOURCE_MESSAGES, VM_MESSAGES_SOME);
    }
}

/* Utility function to handle Start Cfm message from DSP */
static void audioManager_HandleVoiceStartCfm(KYMERA_START_VOICE_CAPTURE_CFM_T *cfm)
{
    if(cfm)
    {
        if(cfm->status)
        {
            if(!AudioManager_AnyPendingStopRequest())
            {
                /* Register the message handler to the source to get MORE_DATA messages */
                audioManager_RegisterSource(cfm->capture_source);
                AudioManager_SetVoiceSource(cfm->capture_source);
                 /* Record the state */
                AudioManager_CaptureStarted();
                AudioManager_TriggerVoiceDataReceivedCallback();
            }
        }
        else
        {
            /* Failed to start the capture, update the state machine */
            AudioManager_CaptureStopped();
       }
    }
}

/* Getting more voice data, pass it on to the registered call back */
static void audioManager_HandleVoiceDataReceived(MessageMoreData *msg)
{
    /* we are safe to handle the data, provided we are still capturing voice and we are getting 
        voice data from the correct source */
    if(AudioManager_IsCaptureInProgress() &&
        (msg->source == AudioManager_GetVoiceSource()))
    {
        /* Send the source to registered callback */
        AudioManager_TriggerVoiceDataReceivedCallback();
    }
}

/* Utility function to handle Stop Cfm message from DSP */
static void audioManager_HandleVoiceStopCfm(KYMERA_STOP_VOICE_CAPTURE_CFM_T *cfm)
{
    if(cfm)
    {
            /* No need of any more message from the voice source */
            audioManager_DeregisterSource();
            /* No more require the source */
            AudioManager_SetVoiceSource(NULL);
            /* Post the event to update the state machine */
            AudioManager_CaptureStopped();
    }
}

/*************************************************************************/
void AudioManager_MsgHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);
    switch(id)
    {
        case KYMERA_START_VOICE_CAPTURE_CFM:
            audioManager_HandleVoiceStartCfm((KYMERA_START_VOICE_CAPTURE_CFM_T*)(message));
            break;

        case KYMERA_STOP_VOICE_CAPTURE_CFM:
            audioManager_HandleVoiceStopCfm((KYMERA_STOP_VOICE_CAPTURE_CFM_T*)(message));
            break;

        case MESSAGE_MORE_DATA:
            audioManager_HandleVoiceDataReceived((MessageMoreData *) message);
            break;
    }
}

