/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   voice_assistant container
\ingroup    Service
\brief      A component responsible for managing voice assistants.

Responsibilities:
- Service component which creates and manages handle.
*/

#ifndef VOICE_ASSISTANT_CONTAINER_H_
#define VOICE_ASSISTANT_CONTAINER_H_

#include "ui.h"
#include "panic.h"
#include "voice_ui.h"

/*============================================================================*
  Definitions
 *============================================================================*/

/*! \brief Macro that defines the maximum no of voice assistants supported in the system */
#ifdef ENABLE_AUDIO_TUNING_MODE
#define MAX_NO_VA_SUPPORTED                     (2)
#else
#define MAX_NO_VA_SUPPORTED                     (1)
#endif
/*============================================================================*
    Types & Defines for Voice Assistant
 *============================================================================*/

/*! \brief Voice Assistant states */
typedef enum
{
    VOICE_ASSISTANT_STATE_IDLE=0,             /*!< Voice Assistant is initialized, but currently not connected.      */
    VOICE_ASSISTANT_STATE_CONNECTED,        /*!< Voice Assistant has established a Bluetooth connection.           */
    VOICE_ASSISTANT_STATE_ACTIVE            /*!< Voice Assistant is currently capturing the local microphone data. */
} voice_assistant_state_t;

/*! \brief Voice Assistant provider names*/
typedef enum
{
voice_assistant_provider_audio_tuning = 0, 
voice_assistant_provider_ama,
voice_assistant_provider_gaa 
} voice_assistant_provider_t;

/*! \brief Voice Assistant Callback Routines */
typedef struct
{
    voice_assistant_provider_t (*GetProvider)(void);
    void (*EventHandler)(ui_input_t event_id);
    void (*Suspend)(void);
}voice_assistant_if;

/*! \brief Voice assistant handle descriptor */
typedef struct
{
   voice_assistant_state_t    voice_assistant_state;
   voice_assistant_if*        voice_assistant;
}voice_assistant_handle_t;

/*\{*/

/*! \brief Initialise the Voice Assistants Component.

    \param init_task Unused
 */
void VoiceAssistants_Init(void);

/*! \brief DeInitialise the Voice Assistants Component and free up allocated resources.

    \param none 
 */
void VoiceAssistants_UnRegister(voice_assistant_handle_t* va_handle);


/*! \brief Registration method to be called by Voice Assistant.

    \param va_table  voice assistant registered call back
 */
voice_assistant_handle_t* VoiceAssistants_Register(voice_assistant_if* va_table);

/*! \brief Function called by voice assistant to inform their current state.
    \param  
 */
void VoiceAssistants_SetVaState(voice_assistant_handle_t* va_handle,
                                                             voice_assistant_state_t va_state);

/*! \brief Get the active Voice Assistant
*/
voice_assistant_handle_t* VoiceAssistant_GetActiveVa(void);

/*! \brief Initialise the voice assistants service
 */
void VoiceAssistant_SetActiveVa(voice_assistant_handle_t* va_handle);

/*! \brief Function called by voice assistant to handle ui events.
    \param
 */
void VoiceAssistants_UiEventHandler(voice_assistant_handle_t* va_handle, ui_input_t event_id);

/*! \brief Function called by voice assistant to suspend current voice assitant.
    \param  
 */
void VoiceAssistants_Suspend(voice_assistant_handle_t* va_handle);

/*!  \brief Function called by VA's get handle corresponding to given provider_name
 *
 *    \param voice_assistant_provider name
*/
voice_assistant_handle_t* VoiceAssistants_GetHandleFromProvider(voice_assistant_provider_t provider_name);

#endif


