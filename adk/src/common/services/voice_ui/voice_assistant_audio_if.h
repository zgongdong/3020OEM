/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   voice_assistant audio if
\ingroup    Service
\brief      A component responsible for managing voice assistant audio interface.

Responsibilities:
- Service component which acts as interface for audio.
*/

#ifndef VOICE_ASSISTANT_AUDIO_IF_H_
#define VOICE_ASSISTANT_AUDIO_IF_H_

#include <stream.h>
#include <audio_sbc_encoder_params.h>

/*============================================================================*
  Definitions
 *============================================================================*/

/*! \brief Different supported codec */
typedef enum
{
    voice_assistant_codec_sbc = 0,
    voice_assistant_codec_max
}voice_assistant_codec_t;


/* \brief Structure that defines parameters required for configuring codec */
typedef struct
{
    union
    {
        sbc_encoder_params_t sbc;
    }config;
    voice_assistant_codec_t codec;
}voice_assistant_audio_param_t;


typedef void (* dataReceived)(Source mic_source);


/*! \brief Function to start the capture of voice 
    \param data_received - Function pointer to callback when voice data is received
    \param audio_param - Pointer to audio configuration
 */
void VoiceAssistants_StartVoiceCapture(dataReceived data_received,
                                                             const voice_assistant_audio_param_t *audio_param);

/*! \brief Function to stop the capture of voice 
 */
void VoiceAssistants_StopVoiceCapture(void);

#endif /* VOICE_ASSISTANT_AUDIO_IF_H_ */


