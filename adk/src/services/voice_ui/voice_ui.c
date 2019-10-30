/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       voice_ui.c
\brief      Implementation of voice ui service.
*/

#include "logging.h"
#include <task_list.h>
#include <csrtypes.h>
#include <panic.h>
#include <vmtypes.h>
#include <hfp_profile.h>

#include "ui.h"

#include "voice_ui.h"
#include "voice_assistant_state.h"
#include "audio_manager.h"
#include "voice_assistant_container.h"
#include "telephony_messages.h"


typedef struct
{
    TaskData task;
} voice_ui_service_data;

static voice_ui_service_data voice_ui_service;

/* Ui Inputs in which voice ui service is interested*/
static const uint16 voice_ui_inputs[] =
{
    ID_TO_MSG_GRP(UI_INPUTS_VOICE_UI_MESSAGE_BASE),
};

static Task voiceUi_GetTask(void)
{
  return (Task)&voice_ui_service.task;
}

static void voiceUi_HandleUiInput(MessageId ui_input)
{
    voice_assistant_handle_t* handle = VoiceAssistant_GetActiveVa();

    if(handle && (handle->voice_assistant_state != VOICE_ASSISTANT_STATE_IDLE))
    {
        DEBUG_LOGF("voiceUi_HandleUiInput, state %u", handle->voice_assistant_state );

        switch(ui_input)
        {
            case ui_input_va_1:
            case ui_input_va_2:
            case ui_input_va_3:
            case ui_input_va_4:
            case ui_input_va_5:
            case ui_input_va_6:
                VoiceAssistants_UiEventHandler(handle,(ui_input_t)ui_input);

            default:
                /* Un handled UI input */
                break;
        }
    }
    else
    {
        switch(ui_input)
        {
            case ui_input_audio_tuning_mode_toggle:
                handle = VoiceAssistants_GetHandleFromProvider(voice_assistant_provider_audio_tuning);
                VoiceAssistants_UiEventHandler(handle,(ui_input_t)ui_input);
                break;
            default:
                /*Un handled UI input*/
                break;
    }
    }
}

static unsigned voiceUi_GetUiContext(void)
{
    voice_assistant_state_t state = VoiceAssistant_GetState(VoiceAssistant_GetActiveVa());
    voice_ui_context_t context = BAD_CONTEXT;

    switch(state)
    {
        case VOICE_ASSISTANT_STATE_IDLE:
            context = context_voice_ui_idle;
            break;
        case VOICE_ASSISTANT_STATE_CONNECTED:
            context = context_voice_ui_available;
            break;
        case VOICE_ASSISTANT_STATE_ACTIVE:
            context = context_voice_ui_capture_in_progress;
            break;
        default:
            break;
    }
    DEBUG_LOGF("voiceUi_GetUiContext, context %d, state %d", context,state);
    
    return (unsigned)context;
}

static void voiceUi_HandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);

    if (isMessageUiInput(id))
    {
        voiceUi_HandleUiInput(id);
        return;
    }

    /* Handle HFP profile messages */
    switch (id)
    {
        case TELEPHONY_INCOMING_CALL:
        case APP_HFP_SCO_CONNECTED_IND:
            VoiceAssistants_Suspend(VoiceAssistant_GetActiveVa());
            break;


        case APP_HFP_SCO_DISCONNECTED_IND:
        default:
            DEBUG_LOGF("voiceUi_HandleMessage: Unhandled id 0x%x", id);
            break;
    }
}

bool VoiceUi_Init(Task init_task)
{
    DEBUG_LOGF("VoiceUi_Init()");

    UNUSED(init_task);

    AudioManager_Init();
    VoiceAssistants_Init();
    VoiceAssistant_InitMessages();
    
    voice_ui_service.task.handler = voiceUi_HandleMessage;
	
	/* register with HFP for changes in sco state */
    appHfpStatusClientRegister(voiceUi_GetTask());

    /* Register av task call back as ui provider*/
    Ui_RegisterUiProvider(ui_provider_voice_ui, voiceUi_GetUiContext);

    Ui_RegisterUiInputConsumer(voiceUi_GetTask(), (uint16*)voice_ui_inputs, sizeof(voice_ui_inputs)/sizeof(uint16));

    return TRUE;
}

static void voiceAssistant_RegisterMessageGroup(Task task, message_group_t group)
{
    PanicFalse(group == VOICE_UI_SERVICE_MESSAGE_GROUP);
    TaskList_AddTask(VoiceAssistant_GetMessageClients(), task);
}

MESSAGE_BROKER_GROUP_REGISTRATION_MAKE(VOICE_UI_SERVICE, voiceAssistant_RegisterMessageGroup, NULL);
