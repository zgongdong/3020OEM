/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of audio interface of voice assistant.
*/
#include "voice_assistant_audio_if.h"
#include "audio_manager.h"

/* Utility function to convert the codec_type from voice assistant container to audio manager codec type */
static audio_manager_codec_t voiceAssistants_TranslateCodec(voice_assistant_codec_t codec)
{
#define CASE(from, to) case (from): if((from) == (to)) goto coerce; else return (to);

    switch(codec)
    {
        CASE(voice_assistant_codec_sbc, audio_manager_codec_sbc);
        coerce: return (audio_manager_codec_t) codec;
        default: return audio_manager_codec_invalid;
    }
#undef CASE
}

/*******************************************************************************/
void VoiceAssistants_StartVoiceCapture(dataReceived data_received,
                                                             const voice_assistant_audio_param_t *audio_param)
{
    audio_manager_audio_config_t audio_config;

    audio_config.codec_config = &audio_param->config.sbc;
    audio_config.codec_type = voiceAssistants_TranslateCodec(audio_param->codec);

    AudioManager_StartCapture((voiceDataReceived) data_received, &audio_config);
}

/*******************************************************************************/
void VoiceAssistants_StopVoiceCapture(void)
{
    AudioManager_StopCapture();
}

