/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file     audio_tuning_mode.c
\defgroup   audio_tuning_mode
\brief      The file contains the implementation details of all the interface and helper functions to VA audio tuning functionality
*/

#ifdef ENABLE_AUDIO_TUNING_MODE
#ifndef UNUSED
#define UNUSED(x) ((void)x)
#endif
#include "ui.h"
#include "audio_tuning_mode.h"
#include <audio_sbc_encoder_params.h>
#include <voice_assistant_container.h>
#include <voice_assistant_audio_if.h>
/* Private #defines for configuring sbc codec*/
#define AUDIO_TUNING_SBC_NUM_SUBBANDS                  8
#define AUDIO_TUNING_SBC_BLOCK_SIZE                    16
#define AUDIO_TUNING_SBC_SAMPLE_RATE                   16000
#define AUDIO_TUNING_SBC_CHANNEL_MODE_MONO             0
#define AUDIO_TUNING_SBC_ALLOC_METHOD_LOUDNESS         0
/* function declaration*/
static voice_assistant_provider_t audioTuningMode_GetProviderName(void);
static bool audio_tuning_mode_enabled = FALSE;
static void audioTuningMode_EventHandler(ui_input_t event_id);
static void audioTuningMode_AudioInputDataReceived(Source source);
static void audioTuningMode_Toggle(void);
static void audioTuningMode_PrepareCodecConfiguration(voice_assistant_audio_param_t *audio_cfg, unsigned bit_pool);

/*voice assistant interface function mapping */
static voice_assistant_if audio_tuning_interface =
{
    audioTuningMode_GetProviderName,
    audioTuningMode_EventHandler,
    NULL
};
/*! \brief function to return the provider name
    \param none.
    \return return the audio_tuning_mode as name of voice_assistant_provider.
*/
static voice_assistant_provider_t audioTuningMode_GetProviderName(void)
{
    return voice_assistant_provider_audio_tuning ;
}
/*! \brief function to handle audio input data
    \param source.
    \return none.
*/
static void audioTuningMode_AudioInputDataReceived(Source source)
{
    UNUSED(source);
}
/*! \brief  Event handler for audio tuning mode
    \param event_id.
    \return none.
*/
static void audioTuningMode_EventHandler(ui_input_t event_id)
{
    switch(event_id)
    {
        case ui_input_audio_tuning_mode_toggle:
            audioTuningMode_Toggle();
            break;
        default:
            break;
    }
}
/*! \brief  The function to toggle audio tuning mode from on to off and vice versa
    \param none
    \return none.
*/
void audioTuningMode_Toggle(void)
{
    if(!audio_tuning_mode_enabled)
    {
        voice_assistant_audio_param_t audio_cfg;
        audioTuningMode_PrepareCodecConfiguration(&audio_cfg, 24);
        VoiceAssistants_StartVoiceCapture(audioTuningMode_AudioInputDataReceived, &audio_cfg);
        audio_tuning_mode_enabled = TRUE ;
    }
    else
    {
        VoiceAssistants_StopVoiceCapture();
        audio_tuning_mode_enabled = FALSE ;
    }
}
/*! \brief  The function to initialize the audio tuning mode
    \param Task
    \return  true if initializatioin sucessfull.
*/
bool AudioTuningMode_Init(Task init_task)
{
    UNUSED(init_task);
    voice_assistant_handle_t *audio_tuning_handle = VoiceAssistants_Register(&audio_tuning_interface);
    VoiceAssistants_SetVaState(audio_tuning_handle, VOICE_ASSISTANT_STATE_IDLE);
    return TRUE;
}
/*! \brief  The function to prepare the codec configuration and populate the audio_cfg
    \param  audio_cfg, bit_pool
    \return   none
*/
static void audioTuningMode_PrepareCodecConfiguration(voice_assistant_audio_param_t *audio_cfg, unsigned bit_pool)
{
    /* Should only happen when switching from Classic (24) to LE (12) */
    audio_cfg->config.sbc.bitpool_size = bit_pool;
    audio_cfg->config.sbc.number_of_subbands = AUDIO_TUNING_SBC_NUM_SUBBANDS;
    audio_cfg->config.sbc.number_of_blocks = AUDIO_TUNING_SBC_BLOCK_SIZE;
    audio_cfg->config.sbc.sample_rate = AUDIO_TUNING_SBC_SAMPLE_RATE;
    audio_cfg->config.sbc.channel_mode = AUDIO_TUNING_SBC_CHANNEL_MODE_MONO;
    audio_cfg->config.sbc.allocation_method = AUDIO_TUNING_SBC_ALLOC_METHOD_LOUDNESS;
    audio_cfg->codec = voice_assistant_codec_sbc;
}
#endif /*ENABLE_AUDIO_TUNING_MODE */
