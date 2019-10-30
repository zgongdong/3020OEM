/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Source file for handling UI input prompts messages and playing voice prompts.
*/
#include "prompts.h"
#include "ui_inputs.h"
#include "ui.h"
#include "logging.h"

/*! \brief prompts task structure */
typedef struct
{
    /*! The  task. */
    TaskData task;

    /*! Cache of the file index of each prompt */
    FILE_INDEX prompt_file_indexes[NUMBER_OF_PROMPTS];

    bool play_prompt;

    /*! The last prompt played, used to avoid repeating prompts */
    voicePromptName prompt_last;
} promptsTaskData;

static promptsTaskData prompts;

/*! User interface internal messasges */
enum ui_internal_messages
{
    /*! Message sent later when a prompt is played. Until this message is delivered
        repeat prompts will not be played */
    UI_INTERNAL_CLEAR_LAST_PROMPT,
};

/*! User interface internal messages in which prompts module is interested*/
const message_group_t ui_inputs_prompt_messages[] =
{
    UI_INPUTS_PROMPT_MESSAGE_GROUP,
};

/*! Configuration of all audio prompts - a configuration  must be defined for
each of the NUMBER_OF_PROMPTS in the system. */
const promptConfig prompt_config[] =
{
    [PROMPT_POWER_ON] = {
        .filename = "power_on.sbc",
        .rate = 48000,
        .format = PROMPT_FORMAT_SBC,
    },
    [PROMPT_POWER_OFF] = {
        .filename = "power_off.sbc",
        .rate = 48000,
        .format = PROMPT_FORMAT_SBC,
    },
    [PROMPT_PAIRING] = {
        .filename = "pairing.sbc",
        .rate = 48000,
        .format = PROMPT_FORMAT_SBC,
    },
    [PROMPT_PAIRING_SUCCESSFUL] = {
        .filename = "pairing_successful.sbc",
        .rate = 48000,
        .format = PROMPT_FORMAT_SBC,
    },
    [PROMPT_PAIRING_FAILED] = {
        .filename = "pairing_failed.sbc",
        .rate = 48000,
        .format = PROMPT_FORMAT_SBC,
    },
    [PROMPT_CONNECTED] = {
        .filename = "connected.sbc",
        .rate = 48000,
        .format = PROMPT_FORMAT_SBC,
    },
    [PROMPT_DISCONNECTED] = {
        .filename = "disconnected.sbc",
        .rate = 48000,
        .format = PROMPT_FORMAT_SBC,
    },
};

static promptsTaskData* prompts_GetTaskData(void)
{
    return &prompts;
}

/*! \brief Play prompt.

    \param prompt The prompt to play.
    \param interruptible If TRUE, always play to completion, if FALSE, the prompt may be
    interrupted before completion. 
    \param client_lock If not NULL, bits set in client_lock_mask will be cleared
    in client_lock when the prompt finishes - either on completion, when interrupted,
    or if the prompt is not played at all, because the UI is not currently playing prompts.
    \param client_lock_mask A mask of bits to clear in the client_lock.   
*/
void Prompts_PlayPrompt(voicePromptName prompt, bool interruptible, bool queueable,uint16 *client_lock, uint16 client_lock_mask)
{
    DEBUG_LOG("Prompts_PlayPrompt prompt name %d play_prompt %d", prompt , prompts.play_prompt );
#ifndef INCLUDE_PROMPTS
    UNUSED(prompt);
    UNUSED(interruptible);
    UNUSED(queueable);
#else
    UNUSED(interruptible);
    promptsTaskData *thePrompts = prompts_GetTaskData();
    
    PanicFalse(prompt < NUMBER_OF_PROMPTS);
    
    /* Playing the prompt. It depends on play_prompt status which is controlled 
    by APP like EB in ear or not. And checking if not repeating the prompt*/
    if(thePrompts->play_prompt && (prompt != thePrompts->prompt_last) &&
        (queueable || !appKymeraIsTonePlaying()))
    {
        const promptConfig *config = &prompt_config[prompt];
        FILE_INDEX *index = thePrompts->prompt_file_indexes + prompt;

        if (*index == FILE_NONE)
        {
            const char* name = config->filename;
            *index = FileFind(FILE_ROOT, name, strlen(name));
            /* Prompt not found */
            PanicFalse(*index != FILE_NONE);
        }

        appKymeraPromptPlay(*index, config->format, config->rate,
                            interruptible, client_lock, client_lock_mask);

        if(prompts_ConfigPromptNoRepeatDelay())
        {
            MessageCancelFirst(&thePrompts->task, UI_INTERNAL_CLEAR_LAST_PROMPT);
            MessageSendLater(&thePrompts->task, UI_INTERNAL_CLEAR_LAST_PROMPT, NULL,
                             prompts_ConfigPromptNoRepeatDelay());
            thePrompts->prompt_last = prompt;
        }
    }
    else
#endif
    {
        if (client_lock)
        {
            *client_lock &= ~client_lock_mask;
        }
    }
}

/*! \brief brief Set/reset play_prompt flag. This is flag is used to check if prompts 
  can be played or not. Application will set and reset the flag. Scenarios like earbud 
  is in ear or not and etc.  

    \param play_prompt If TRUE, prompt can be played, if FALSE, the prompt can not be 
    played.  
*/
void Prompts_SetPlayPrompt(bool play_prompt)
{
    prompts.play_prompt = play_prompt;
}

static void prompts_HandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);

    switch (id)
    {
        case UI_INTERNAL_CLEAR_LAST_PROMPT:
            prompts.prompt_last = PROMPT_NONE;
            break;    
    
        case ui_input_prompt_pairing:
            Prompts_PlayPrompt(PROMPT_PAIRING, FALSE, TRUE, NULL, 0);
            break;

        case ui_input_prompt_pairing_successful:
            Prompts_PlayPrompt(PROMPT_PAIRING_SUCCESSFUL, FALSE, TRUE, NULL, 0);
            break;

        case ui_input_prompt_pairing_failed:
            Prompts_PlayPrompt(PROMPT_PAIRING_FAILED, FALSE, TRUE, NULL, 0);
            break;

        case ui_input_prompt_connected:
            Prompts_PlayPrompt(PROMPT_CONNECTED, FALSE, TRUE, NULL, 0);
            break;

        case ui_input_prompt_disconnected:
            Prompts_PlayPrompt(PROMPT_DISCONNECTED, FALSE, TRUE, NULL, 0);
            break;

        default:
            break;
    }
}

/*! brief Initialisation of prompts module */
void Prompts_Init(void)
{
    promptsTaskData *thePrompts = prompts_GetTaskData();

    memset(thePrompts, 0, sizeof(*thePrompts));

    /* Set the play_prompt */
    thePrompts->play_prompt = TRUE;

    /* Set the last played prompt to invalid */
    thePrompts->prompt_last = PROMPT_NONE;

    /* Set up task handler */
    thePrompts->task.handler = prompts_HandleMessage;

    memset(thePrompts->prompt_file_indexes, FILE_NONE, sizeof(thePrompts->prompt_file_indexes));

    Ui_RegisterUiInputConsumer(
                &thePrompts->task,
                ui_inputs_prompt_messages,
                ARRAY_DIM(ui_inputs_prompt_messages));
}
