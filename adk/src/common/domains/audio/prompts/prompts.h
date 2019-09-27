/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for handling UI input prompts messages and playing voice prompts.
*/
#ifndef PROMPTS_H
#define PROMPTS_H

#include "kymera.h"

/*! This is a hold off time after a voice prompt is played, in seconds. When a voice prompt
    is played, for the period of time specified here, any repeat of this prompt
    will not be played. If this hold off time is set to zero, then all prompts will be played,
    regardless of whether the prompt was recently played. */
#define prompts_ConfigPromptNoRepeatDelay() D_SEC(5)

/*! A list of prompts supported by the application.
    This list should be used as an index into a configuration array of
    promptConfigs defining the properties of each prompt. The array length should
    be NUMBER_OF_PROMPTS.
*/
typedef enum prompt_name
{
    PROMPT_POWER_ON = 0,
    PROMPT_POWER_OFF,
    PROMPT_PAIRING,
    PROMPT_PAIRING_SUCCESSFUL,
    PROMPT_PAIRING_FAILED,
    PROMPT_CONNECTED,
    PROMPT_DISCONNECTED,
    NUMBER_OF_PROMPTS,
    PROMPT_NONE = 0xffff,
} voicePromptName;

/*! Audio prompt configuration */
typedef struct prompt_configuration
{
    const char *filename; /*!< Prompt filename */
    uint32 rate;          /*!< Prompt sample rate */
    promptFormat format;  /*!< Prompt format */
} promptConfig;

/*! brief Initialise prompts module */
void Prompts_Init(void);

/*! brief Set/reset play_prompt flag. This is flag is used to check if prompts can be played or not. 
  application will set and reset the flag. */
void Prompts_SetPlayPrompt(bool play_prompt);

/*! brief Play the prompt */
void Prompts_PlayPrompt(voicePromptName prompt,
                        bool interruptible, bool queueable,
                        uint16 *client_lock, uint16 client_lock_mask);

#endif // PROMPTS_H







