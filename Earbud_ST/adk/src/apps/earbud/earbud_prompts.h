/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for the Earbud Application user interface audio prompt indications.
*/
#ifndef EARBUD_PROMPTS_H
#define EARBUD_PROMPTS_H

#include "kymera.h"

/*! This is a hold off time after a voice promt is played, in seconds. When a voice prompt
    is played, for the period of time specified here, any repeat of this prompt
    will not be played. If this hold off time is set to zero, then all prompts will be played,
    regardless of whether the prompt was recently played. */
#define appConfigPromptNoRepeatDelay() D_SEC(5)

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

/*! \brief Prompt task structure */
typedef struct
{
    /*! The UI task. */
    TaskData task;
    /*! Cache of the file index of each prompt */
    FILE_INDEX prompt_file_indexes[NUMBER_OF_PROMPTS];
    /*! The last prompt played, used to avoid repeating prompts */
    voicePromptName prompt_last;
} promptTaskData;

/*!< Indicator data structure */
extern promptTaskData  earbud_prompts;

/*! Get pointer to UI data structure */
#define promptsGetTaskData()          (&earbud_prompts)

/*! \brief Play HFP SLC connected prompt */
#define appUiHfpConnected(silent) \
    { if (!(silent)) appUiPlayPrompt(PROMPT_CONNECTED); }

/*! \brief Play HFP SLC disconnected prompt */
#define appUiHfpDisconnected() \
    appUiPlayPrompt(PROMPT_DISCONNECTED)

/*! \brief Play AV connected prompt */
#define appUiAvConnected() \
    appUiPlayPrompt(PROMPT_CONNECTED)

/*! \brief Play AV disconnected prompt */
#define appUiAvDisconnected() \
    appUiPlayPrompt(PROMPT_DISCONNECTED)

/*! \brief Play pairing complete prompt */
#define appUiPairingComplete() \
    appUiPlayPrompt(PROMPT_PAIRING_SUCCESSFUL)

/*! \brief Play pairing failed prompt */
#define appUiPairingFailed() \
    appUiPlayPrompt(PROMPT_PAIRING_FAILED)

void EarbudPrompts_PlayPromptCore(voicePromptName prompt, bool interruptible, bool queueable,
                         uint16 *client_lock, uint16 client_lock_mask);

void EarbudPrompts_Init(void);

/*! \brief Play a prompt to completion */
#define appUiPlayPrompt(prompt) EarbudPrompts_PlayPromptCore(prompt, FALSE, TRUE, NULL, 0)
/*! \brief Play a prompt allowing another tone/prompt/event to interrupt (stop) this prompt
     before completion. */
#define appUiPlayPromptInterruptible(prompt) EarbudPrompts_PlayPromptCore(prompt, TRUE, TRUE, NULL, 0)
/*! \brief Play a prompt to completion. mask bits will be cleared in lock
    when the prompt completes, or if it is not played at all. */
#define appUiPlayPromptClearLock(prompt, lock, mask) EarbudPrompts_PlayPromptCore(prompt, FALSE, TRUE, lock, mask)
/*! \brief Play a prompt allowing another tone/prompt/event to interrupt (stop) this prompt
     before completion. mask bits will be cleared in lock when the prompt completes or
     is interrupted, or if it is not played at all. */
#define appUiPlayPromptInterruptibleClearLock(prompt, lock, mask) EarbudPrompts_PlayPromptCore(prompt, TRUE, TRUE, lock, mask)

#endif // EARBUD_PROMPTS_H
