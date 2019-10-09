/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       kymera_aec.h
\brief      Private header to connect/manage to AEC chain
*/

#ifndef KYMERA_AEC_H_
#define KYMERA_AEC_H_

#ifdef INCLUDE_KYMERA_VOICE_CAPTURE
#define INCLUDE_KYMERA_AEC
#endif

#ifdef INCLUDE_KYMERA_AEC

typedef struct
{
    Source input_1;
    Sink   speaker_output_1;
} aec_connect_audio_output_t;

typedef struct
{
    uint32 mic_sample_rate;
    Sink   reference_output;
    Sink   mic_output_1;
    Sink   mic_output_2;
} aec_connect_audio_input_t;

/*! \brief Connect audio output source to AEC.
    \param params All parameters required to configure/connect to the AEC chain.
    \note Handles the creation of the AEC chain.
*/
void appKymera_ConnectAudioOutputToAec(const aec_connect_audio_output_t *params);

/*! \brief Disconnect audio output source from AEC.
    \note Handles the destruction of the AEC chain.
*/
void appKymera_DisconnectAudioOutputFromAec(void);

/*! \brief Connect audio input source to AEC.
    \param params All parameters required to configure/connect to the AEC chain.
    \note Handles the creation of the AEC chain.
*/
void appKymera_ConnectAudioInputToAec(const aec_connect_audio_input_t *params);

/*! \brief Disconnect audio input source from AEC.
    \note Handles the destruction of the AEC chain.
*/
void appKymera_DisconnectAudioInputFromAec(void);

#endif /* INCLUDE_KYMERA_AEC */

#endif /* KYMERA_AEC_H_ */
