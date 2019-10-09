/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       kymera_voice_capture.h
\brief      Private header of voice captuer kymera module

*/

#ifndef KYMERA_VOICE_CAPTURE_H
#define KYMERA_VOICE_CAPTURE_H

#include "domain_message.h"
#include <audio_sbc_encoder_params.h>

/*! \brief Messages send by kymera module. */
typedef enum
{
    KYMERA_START_VOICE_CAPTURE_CFM = KYMERA_MESSAGE_BASE,
    KYMERA_STOP_VOICE_CAPTURE_CFM,
    KYMERA_TOP
} kymera_msg_t;

/*! \brief Contents of KYMERA_START_VOICE_CAPTURE_CFM message
*/
typedef struct
{
    /*! TRUE on success, FALSE otherwise */
    bool status;
    Source capture_source;
} KYMERA_START_VOICE_CAPTURE_CFM_T;

/*! \brief Contents of KYMERA_STOP_VOICE_CAPTURE_CFM message
*/
typedef struct
{
    /*! TRUE on success, FALSE otherwise */
    bool status;
} KYMERA_STOP_VOICE_CAPTURE_CFM_T;

/*! \brief Voice capture parameters */
typedef struct
{
    union
    {
        sbc_encoder_params_t sbc;
    } encoder_params;
} voice_capture_params_t;

#ifdef INCLUDE_KYMERA_VOICE_CAPTURE
/*! \brief Start voice capture.
    \param client Task to send the CFM response to.
    \param params Parameters based on which the audio capture will be configured.
    \return A KYMERA_START_VOICE_CAPTURE_CFM message will be send to the client task.
*/
void AppKymera_StartVoiceCapture(Task client, const voice_capture_params_t *params);

/*! \brief Stop voice capture.
    \param client Task to send the CFM response to.
    \return A KYMERA_STOP_VOICE_CAPTURE_CFM message will be send to the client task.
*/
void AppKymera_StopVoiceCapture(Task client);

/*! \brief Check if voice capture chain is up
*/
bool appKymera_IsVoiceCaptureActive(void);
#else
#define AppKymera_StartVoiceCapture(client, params) ((void)(0))
#define AppKymera_StopVoiceCapture(client) ((void)(0))
#define appKymera_IsVoiceCaptureActive() FALSE
#endif /* INCLUDE_KYMERA_VOICE_CAPTURE */

#endif /* KYMERA_VOICE_CAPTURE_H */
