/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       audio_manager.h
\brief      file for voice assistant audio manager interface
*/

#ifndef _AUDIO_MANAGER_
#define _AUDIO_MANAGER_

#include <csrtypes.h>
#include <stream.h>

/*@{*/

/*!
    \brief Defines the different codecs supported
 
 */
typedef enum
{
    audio_manager_codec_sbc = 0,
    audio_manager_codec_invalid,
    audio_manager_codec_max
}audio_manager_codec_t;

/*!
    \brief voice assistant audio manager configurations related to audio

    Defines the different audio configuration for voice assistant
*/
typedef struct
{
    void* codec_config;
    audio_manager_codec_t codec_type;
} audio_manager_audio_config_t;


typedef void (* voiceDataReceived)(Source mic_source);


/*! \brief Initialze the Audio Manager instance.
    \param none.
    \return none.
*/
void AudioManager_Init(void);


/*************************************************************************************
    \brief This is generic interfaces to inform start capturing mic data which needs to sent

    \param voice_data_received : Callback function pointer once the voice data which is captured by
                                               mic is available

    \param audio_config : configurations related to codec type, codec configuration, cvc type and 
                                    mic configuraiotn

    \return TRUE if able to start the capture chain else FALSE.
*/
bool AudioManager_StartCapture(voiceDataReceived voice_data_received, const audio_manager_audio_config_t *audio_config);

/*************************************************************************************
    \brief This interfaces informs to stop capturing mic data

    \param none

    \return TRUE if able to stop the capture chain else FALSE.
*/
bool AudioManager_StopCapture(void);


/*************************************************************************************
    \brief This interfaces deinitialize Audio manager

    \param none

    \return none
*/
void AudioManager_DeInit(void);

/*@}*/

#endif /* _AUDIO_MANAGER_ */