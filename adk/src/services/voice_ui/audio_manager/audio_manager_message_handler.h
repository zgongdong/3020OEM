/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       audio_manager_message_handler.h
\brief      file for audio manager handling messages from Audio DSP
*/

#ifndef _AUDIO_MANAGER_MESSAGE_HANDLER_
#define _AUDIO_MANAGER_MESSAGE_HANDLER_

#include <message.h>

/***************************************************************************
DESCRIPTION
    Data/Control message handler for VA audio manager
 
PARAMS
    task - task associated with the message
    id - message id which needs to be handled
    message - data payload which needs to be processed
 
RETURNS
    None
*/
void AudioManager_MsgHandler(Task task, MessageId id, Message message);

/*@}*/

#endif /* _AUDIO_MANAGER_MESSAGE_HANDLER_ */