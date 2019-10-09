/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       voice_assistant_state.c
\brief      Implementation of voice assistant state 
*/

#include <task_list.h>
#include <panic.h>
#include "ui.h"
#include "logging.h"
#include "voice_ui.h"
#include "voice_assistant_state.h"

static voice_assistant_handle_t *active_va = NULL;
static task_list_t * client_list;

/*! \brief Send voice assistant message to client lists
 */
static void voiceAssistant_SendMessage(voice_ui_msg_id_t msg_id, void* handle)
{
    voice_ui_msg_t * const message = (voice_ui_msg_t *)PanicUnlessMalloc(sizeof(voice_ui_msg_t));
    message->vui_handle = handle;
    TaskList_MessageSendWithSize(VoiceAssistant_GetMessageClients(), msg_id, message, sizeof(voice_ui_msg_t));
}

/*! \brief Send voice assistant message to client lists
 */
static void voiceAssistant_SendContext(voice_ui_context_t context)
{
    Ui_InformContextChange(ui_provider_voice_ui, context);
}

task_list_t * VoiceAssistant_GetMessageClients(void)
{
    return client_list;
}

voice_assistant_handle_t* VoiceAssistant_GetActiveVa(void)
{
    return active_va;
}

void VoiceAssistant_SetActiveVa(voice_assistant_handle_t *va_handle)
{
    active_va = va_handle;
}

voice_assistant_state_t VoiceAssistant_GetState(voice_assistant_handle_t *va_handle)
{
    voice_assistant_state_t state = VOICE_ASSISTANT_STATE_IDLE;

    if(va_handle == VoiceAssistant_GetActiveVa())
    {
        state = va_handle->voice_assistant_state;
    }
    return state;
}

bool VoiceAssistant_InitMessages(void)
{
    client_list = TaskList_Create();
    return TRUE;
}

void VoiceAssistant_HandleState(voice_assistant_handle_t *handle, voice_assistant_state_t state)
{
    voice_ui_context_t context = context_voice_ui_idle;
    voice_ui_msg_id_t va_msg_id = VOICE_UI_IDLE;

    DEBUG_LOGF("VoiceUi_handleVaState, va_handle -%u, state - %u", handle, state);

    switch(state)
    {
        case VOICE_ASSISTANT_STATE_IDLE:
            {
                context = context_voice_ui_idle;
                va_msg_id = VOICE_UI_IDLE;
            }
            break;

        case VOICE_ASSISTANT_STATE_CONNECTED:
            VoiceAssistant_SetActiveVa(handle);
            context = context_voice_ui_available;
            va_msg_id = VOICE_UI_CONNECTED;
            break;

        case VOICE_ASSISTANT_STATE_ACTIVE:
            {
                context = context_voice_ui_capture_in_progress;
                va_msg_id = VOICE_UI_ACTIVE;
            }
            break;
    }

    /* The va ui context is updated to the application */
    voiceAssistant_SendContext(context);

    voiceAssistant_SendMessage(va_msg_id, VoiceAssistant_GetActiveVa());

}
