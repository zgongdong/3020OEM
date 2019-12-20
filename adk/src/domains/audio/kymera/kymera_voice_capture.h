/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Private header of voice captuer kymera module

*/

#ifndef KYMERA_VOICE_CAPTURE_H
#define KYMERA_VOICE_CAPTURE_H

#include "domain_message.h"
#include "va_audio_types.h"

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
    bool   status;
    Source capture_source;
} KYMERA_START_VOICE_CAPTURE_CFM_T;

/*! \brief Contents of KYMERA_STOP_VOICE_CAPTURE_CFM message
*/
typedef struct
{
    /*! TRUE on success, FALSE otherwise */
    bool status;
} KYMERA_STOP_VOICE_CAPTURE_CFM_T;

/*! \brief Start voice capture.
    \param client Task to send the CFM response to.
    \param params Parameters based on which the audio capture will be configured.
    \return A KYMERA_START_VOICE_CAPTURE_CFM message will be send to the client task.
*/
void Kymera_StartVoiceCapture(Task client, const va_audio_voice_capture_params_t *params);

/*! \brief Stop voice capture.
    \param client Task to send the CFM response to.
    \return A KYMERA_STOP_VOICE_CAPTURE_CFM message will be send to the client task.
*/
void Kymera_StopVoiceCapture(Task client);

/*! \brief Check if voice capture chain is up
*/
bool Kymera_IsVoiceCaptureActive(void);

#endif /* KYMERA_VOICE_CAPTURE_H */
