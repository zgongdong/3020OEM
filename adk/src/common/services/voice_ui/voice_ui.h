/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       voice_ui.h
\defgroup   voice_ui 
\ingroup    services
\brief      A component responsible for controlling voice assistants.

Responsibilities:
- Voice Ui control (like A way to start/stop voice capture as a result of some user action) 
  Notifications to be sent to the Application raised by the VA.

The Voice Ui uses  \ref audio_domain Audio domain and \ref bt_domain BT domain.

*/

#ifndef VOICE_UI_H_
#define VOICE_UI_H_

#include "domain_message.h"

/*! \brief Voice UI Provider contexts */
typedef enum
{
    context_voice_ui_idle = 0,
    context_voice_ui_available,
    context_voice_ui_capture_in_progress,
} voice_ui_context_t;


/*! \brief Messages sent by the voice ui service to interested clients. */
typedef enum
{
    VOICE_UI_IDLE   = VOICE_UI_SERVICE_MESSAGE_BASE,
    VOICE_UI_CONNECTED,
    VOICE_UI_ACTIVE,
    VOICE_UI_CAPTURE_START,
    VOICE_UI_CAPTURE_END
} voice_ui_msg_id_t;

/*! \brief voice ui service message. */
typedef struct
{
    void *vui_handle;
} voice_ui_msg_t;

/*\{*/

/*! \brief Initialise the voice ui service

    \param init_task Unused
 */
bool VoiceUi_Init(Task init_task);

/*\}*/

#endif /* VOICE_UI_H_ */
