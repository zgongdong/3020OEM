/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Source file for the Earbud Application user interface audio prompt indications.
*/
#include "earbud_prompts.h"
#include "earbud_ui.h"
#include "earbud_sm.h"

promptTaskData  earbud_prompts;

/*! User interface internal messasges */
enum ui_internal_messages
{
    /*! Message sent later when a prompt is played. Until this message is delivered
        repeat prompts will not be played */
    UI_INTERNAL_CLEAR_LAST_PROMPT,
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

/*! \brief Message Handler

    This function is the main message handler for the indicator instance.It recieves
    messages from ui module and invokes further handler which basically calls respective
    functions for different ui inputs.
*/
static void earbudPrompts_HandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);
    promptTaskData *thePrompts = promptsGetTaskData();

    if(id == UI_INTERNAL_CLEAR_LAST_PROMPT)
    {
        thePrompts->prompt_last = PROMPT_NONE;
    }
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
void EarbudPrompts_PlayPromptCore(voicePromptName prompt, bool interruptible, bool queueable,
                         uint16 *client_lock, uint16 client_lock_mask)
{
#ifndef INCLUDE_PROMPTS
    UNUSED(prompt);
    UNUSED(interruptible);
    UNUSED(queueable);
#else
    promptTaskData *theIndicator = promptsGetTaskData();
    PanicFalse(prompt < NUMBER_OF_PROMPTS);
    /* Only play prompt if it can be heard */
    if ((PHY_STATE_IN_EAR == appPhyStateGetState()) && (prompt != theIndicator->prompt_last) &&
        (queueable || !appKymeraIsTonePlaying()))
    {
        const promptConfig *config = &prompt_config[prompt];
        FILE_INDEX *index = theIndicator->prompt_file_indexes + prompt;
        if (*index == FILE_NONE)
        {
            const char* name = config->filename;
            *index = FileFind(FILE_ROOT, name, strlen(name));
            /* Prompt not found */
            PanicFalse(*index != FILE_NONE);
        }
        appKymeraPromptPlay(*index, config->format, config->rate,
                            interruptible, client_lock, client_lock_mask);

        if (appConfigPromptNoRepeatDelay())
        {
            MessageCancelFirst(&theIndicator->task, UI_INTERNAL_CLEAR_LAST_PROMPT);
            MessageSendLater(&theIndicator->task, UI_INTERNAL_CLEAR_LAST_PROMPT, NULL,
                             appConfigPromptNoRepeatDelay());
            theIndicator->prompt_last = prompt;
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

/*! brief Initialise prompts module */
void EarbudPrompts_Init(void)
{
    promptTaskData *theIndicator = promptsGetTaskData();
    /* Set up task handler */
    theIndicator->task.handler = earbudPrompts_HandleMessage;

    memset(theIndicator->prompt_file_indexes, FILE_NONE, sizeof(theIndicator->prompt_file_indexes));

    theIndicator->prompt_last = PROMPT_NONE;
}
