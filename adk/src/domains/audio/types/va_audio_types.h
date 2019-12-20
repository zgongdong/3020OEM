/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Data types used by APIs related to Voice Assistants audio

*/

#ifndef VA_AUDIO_TYPES_H_
#define VA_AUDIO_TYPES_H_

#include <audio_sbc_encoder_params.h>

/*! \brief Defines the different codecs used for voice assistants audio */
typedef enum
{
    va_audio_codec_sbc
} va_audio_codec_t;

/*! \brief Union of the parameters of each encoder */
typedef union
{
   sbc_encoder_params_t sbc;
} va_audio_encoder_params_t;

/*! \brief Voice capture parameters */
typedef struct
{
    va_audio_codec_t          encoder;
    va_audio_encoder_params_t encoder_params;
} va_audio_voice_capture_params_t;

#endif /* VA_AUDIO_TYPES_H_ */
