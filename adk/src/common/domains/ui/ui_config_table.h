/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       ui_config_table.h
\brief     Contains definitions required by ui config table.


*/

#ifndef _UI_CONFIG_TABLE_H_
#define _UI_CONFIG_TABLE_H_

#include "domain_message.h"

#define FOREACH_UI_INPUT(UI_INPUT) \
    UI_INPUT(ui_input_voice_call_hang_up = UI_INPUTS_TELEPHONY_MESSAGE_BASE)   \
    UI_INPUT(ui_input_voice_call_accept)  \
    UI_INPUT(ui_input_hfp_transfer_to_ag)   \
    UI_INPUT(ui_input_hfp_transfer_to_headset)  \
    UI_INPUT(ui_input_voice_call_reject)  \
    UI_INPUT(ui_input_send_toggle = UI_INPUTS_MEDIA_PLAYER_MESSAGE_BASE)  \
    UI_INPUT(ui_input_play)  \
    UI_INPUT(ui_input_pause)  \
    UI_INPUT(ui_input_stop_av_connection)  \
    UI_INPUT(ui_input_av_forward)  \
    UI_INPUT(ui_input_av_backward)  \
    UI_INPUT(ui_input_av_fast_forward_start) \
    UI_INPUT(ui_input_fast_forward_stop) \
    UI_INPUT(ui_input_av_rewind_start) \
    UI_INPUT(ui_input_rewind_stop) \
    UI_INPUT(ui_input_sco_forward_call_hang_up = UI_INPUTS_PEER_MESSAGE_BASE) \
    UI_INPUT(ui_input_sco_forward_call_accept) \
    UI_INPUT(ui_input_sco_voice_call_reject) \
    UI_INPUT(ui_input_button_held_1  = UI_INPUTS_TONE_FEEDBACK_MESSAGE_BASE) \
    UI_INPUT(ui_input_button_held_2) \
    UI_INPUT(ui_input_button_held_3) \
    UI_INPUT(ui_input_button_held_4) \
    UI_INPUT(ui_input_power_off = UI_INPUTS_DEVICE_STATE_MESSAGE_BASE) \
    UI_INPUT(ui_input_factory_reset_request) \
    UI_INPUT(ui_input_dfu_active_when_in_case_request) \
    UI_INPUT(ui_input_play_dfu_button_held_tone) \
    UI_INPUT(ui_input_play_factory_reset_tone) \
    UI_INPUT(ui_input_hfp_volume_stop = UI_INPUTS_VOLUME_MESSAGE_BASE) \
    UI_INPUT(ui_input_av_volume_down_remote_stop) \
    UI_INPUT(ui_input_av_volume_up_remote_stop) \
    UI_INPUT(ui_input_hfp_mute_toggle) \
    UI_INPUT(ui_input_hfp_volume_down_start) \
    UI_INPUT(ui_input_hfp_volume_up_start) \
    UI_INPUT(ui_input_sco_fwd_volume_down_start) \
    UI_INPUT(ui_input_sco_fwd_volume_up_start) \
    UI_INPUT(ui_input_av_volume_down_start) \
    UI_INPUT(ui_input_av_volume_up_start) \
    UI_INPUT(ui_input_sco_fwd_volume_stop) \
    UI_INPUT(ui_input_sm_pair_handset = UI_INPUTS_HANDSET_MESSAGE_BASE) \
    UI_INPUT(ui_input_sm_delete_handsets) \
    UI_INPUT(ui_input_connect_handset) \
    UI_INPUT(ui_input_anc_on = UI_INPUTS_AUDIO_CURATION_MESSAGE_BASE) \
    UI_INPUT(ui_input_anc_off) \
    UI_INPUT(ui_input_anc_toggle_on_off) \
    UI_INPUT(ui_input_anc_set_mode_1) \
    UI_INPUT(ui_input_anc_set_mode_2) \
    UI_INPUT(ui_input_anc_set_mode_3) \
    UI_INPUT(ui_input_anc_set_mode_4) \
    UI_INPUT(ui_input_anc_set_mode_5) \
    UI_INPUT(ui_input_anc_set_mode_6) \
    UI_INPUT(ui_input_anc_set_mode_7) \
    UI_INPUT(ui_input_anc_set_mode_8) \
    UI_INPUT(ui_input_anc_set_mode_9) \
    UI_INPUT(ui_input_anc_set_mode_10) \
    UI_INPUT(ui_input_anc_set_next_mode) \
    UI_INPUT(ui_input_anc_enter_tuning_mode) \
    UI_INPUT(ui_input_anc_exit_tuning_mode) \
    UI_INPUT(ui_input_prompt_pairing = UI_INPUTS_PROMPT_MESSAGE_BASE) \
    UI_INPUT(ui_input_prompt_pairing_successful) \
    UI_INPUT(ui_input_prompt_pairing_failed) \
    UI_INPUT(ui_input_prompt_connected) \
    UI_INPUT(ui_input_prompt_disconnected) \
    UI_INPUT(ui_input_invalid = UI_INPUTS_BOUNDS_CHECK_MESSAGE_BASE) \

#define GENERATE_ENUM(ENUM) ENUM,
#define GENERATE_STRING(STRING) #STRING,

enum UI_INPUT_ENUM {
    FOREACH_UI_INPUT(GENERATE_ENUM)
};

/*! The first UI input, not in the enum, as that causes ui_input_string_debug
    test to fail */
#define ui_input_first UI_INPUTS_TELEPHONY_MESSAGE_BASE

/*! Macro to test if a message id is a UI input message */
#define isMessageUiInput(msg_id) (ui_input_first <= (msg_id) && (msg_id) < ui_input_invalid)

typedef enum UI_INPUT_ENUM ui_input_t;

#ifndef DISABLE_LOG
extern const char * const UI_INPUT_STRING[];
#endif

/*! \brief ui providers */
typedef enum
{
    ui_provider_hfp,
    ui_provider_scofwd,
    ui_provider_device,
    ui_provider_media_player,
    ui_provider_av,
    ui_provider_indicator,
    ui_provider_sm,
    ui_provider_power,
    ui_provider_audio_curation,
    ui_providers_max
} ui_providers_t;

unsigned Ui_GetConfigTableSize(void);

bool Ui_IsUiInputStringIndexValid(unsigned index);

#endif /* _UI_CONFIG_TABLE_H_ */






