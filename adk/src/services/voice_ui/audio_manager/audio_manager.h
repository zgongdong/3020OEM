/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Voice UI audio interface
*/

#ifndef _AUDIO_MANAGER_
#define _AUDIO_MANAGER_

#include "va_audio_types.h"
#include <csrtypes.h>
#include <stream.h>

/*@{*/

typedef void (* voiceDataReceived)(Source mic_source);


/*! \brief Initialze the Audio Manager instance.
*/
void AudioManager_Init(void);


/*! \brief Start capturing and sending mic data to the application
    \param voice_data_received Callback function to call once mic data becomes available
    \param audio_config Configuration related to encoder type/parameters, cvc type and mic parameters
    \return TRUE if able to start the capture chain else FALSE.
*/
bool AudioManager_StartCapture(voiceDataReceived voice_data_received, const va_audio_voice_capture_params_t *audio_config);

/*! \brief Stop capturing and sending mic data to the application.
           After this call the mic Source becomes invalid.
    \return TRUE if able to stop the capture chain else FALSE.
*/
bool AudioManager_StopCapture(void);


/*! \brief This interfaces deinitialize Audio manager
*/
void AudioManager_DeInit(void);

/*@}*/

#endif /* _AUDIO_MANAGER_ */
