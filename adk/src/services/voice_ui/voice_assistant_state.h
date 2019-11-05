
/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file

\defgroup   voice_assistant voice assistant 
\ingroup    services
\brief      A component responsible for controlling voice assistants
*/

#ifndef VOICE_ASSISTANT_STATE_H_
#define VOICE_ASSISTANT_STATE_H_

#include <task_list.h>
#include "voice_ui.h"
#include "voice_assistant_container.h"

/*\{*/

/*! \brief Get message client list
*/
task_list_t * VoiceAssistant_GetMessageClients(void);

/*! \brief Get the active Voice Assistant state
*/
voice_assistant_state_t VoiceAssistant_GetState(voice_assistant_handle_t * va_handle);

/*! \brief Create message clients task list
*/
bool VoiceAssistant_InitMessages(void);

/*! \brief handler for voice assistant state updates
*/
void VoiceAssistant_HandleState(voice_assistant_handle_t *handle, voice_assistant_state_t state);

/*\}*/

#endif /* VOICE_ASSISTANT_STATE_H_ */


