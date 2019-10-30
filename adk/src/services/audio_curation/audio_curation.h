/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       audio_curation.h
\defgroup   audio_curation Audio Curation
\ingroup    services
\brief      A component responsible for controlling audio curation services.

Responsibilities:
- Handles ANC UI inputs: ANC Power On, Off, ANC On, Off, Toggle etc which in-turn calls audio anc domain interfaces.

The Audio Curation uses \ref audio_anc_domain Audio Anc domain and \ref ui_domain UI domain.

*/

#ifndef AUDIO_CURATION_H_
#define AUDIO_CURATION_H_

/*@{*/

/*! \brief Audio Curation UI Provider contexts */
typedef enum
{
    context_anc_disabled,
    context_anc_enabled,
    context_anc_tuning_mode_active,
} audio_curation_provider_context_t;


/*! \brief Initialise the audio curation service

    \param init_task Unused
 */
bool AudioCuration_Init(Task init_task);

/*@}*/

#endif /* AUDIO_CURATION_H_ */
