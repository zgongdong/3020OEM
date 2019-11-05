/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       earbud_ui_config.c
\brief      ui configuration table

    This file contains ui configuration table which maps different logical inputs to
    corresponding ui inputs based upon ui provider contexts.
*/

#include "earbud_ui_config.h"
#include "ui.h"

#if defined(HAVE_9_BUTTONS)
#include "9_buttons.h"
#elif defined(HAVE_7_BUTTONS)
#include "7_buttons.h"
#elif defined(HAVE_6_BUTTONS)
#include "6_buttons.h"
#elif defined(HAVE_4_BUTTONS)
#include "4_buttons.h"
#elif defined(HAVE_2_BUTTONS)
#include "2_button.h"
#elif defined(HAVE_1_BUTTON)
#include "1_button.h"
#endif

/* Needed for UI contexts - transitional; when table is code generated these can be anonymised
 * unsigned ints and these includes can be removed. */
#include "av.h"
#include "hfp_profile.h"
#include "bt_device.h"
#include "scofwd_profile.h"
#include "earbud_sm.h"
#include "power_manager.h"
#include "voice_ui.h"

#include <macros.h>

/*! \brief ui config table*/
const ui_config_table_content_t earbud_ui_config_table[] =
{
    {APP_MFB_BUTTON_SINGLE_CLICK,      ui_provider_hfp,             context_hfp_voice_call_sco_active,    ui_input_voice_call_hang_up                   },
    {APP_MFB_BUTTON_SINGLE_CLICK,      ui_provider_hfp,             context_hfp_voice_call_sco_inactive,  ui_input_voice_call_hang_up                   },
    {APP_MFB_BUTTON_SINGLE_CLICK,      ui_provider_hfp,             context_hfp_voice_call_outgoing,      ui_input_voice_call_hang_up                   },
    {APP_MFB_BUTTON_SINGLE_CLICK,      ui_provider_hfp,             context_hfp_voice_call_incoming,      ui_input_voice_call_accept                    },
    {APP_MFB_BUTTON_SINGLE_CLICK,      ui_provider_media_player,    context_av_is_streaming,              ui_input_toggle_play_pause                    },
    {APP_MFB_BUTTON_SINGLE_CLICK,      ui_provider_media_player,    context_av_connected,                 ui_input_toggle_play_pause                    },
    {APP_MFB_BUTTON_SINGLE_CLICK,      ui_provider_device,          context_handset_not_connected,        ui_input_connect_handset                      },

    {APP_MFB_BUTTON_HELD_RELEASE_1SEC, ui_provider_hfp,             context_hfp_voice_call_sco_active,    ui_input_hfp_transfer_to_ag                   },
    {APP_MFB_BUTTON_HELD_RELEASE_1SEC, ui_provider_hfp,             context_hfp_voice_call_sco_inactive,  ui_input_hfp_transfer_to_headset              },
    {APP_MFB_BUTTON_HELD_RELEASE_1SEC, ui_provider_hfp,             context_hfp_voice_call_incoming,      ui_input_voice_call_reject                    },
    {APP_MFB_BUTTON_HELD_RELEASE_1SEC, ui_provider_media_player,    context_av_is_streaming,              ui_input_stop_av_connection                   },

    {APP_MFB_BUTTON_HELD_1SEC,         ui_provider_sm,              context_sm_out_of_case,               ui_input_button_held_1                        },
    {APP_MFB_BUTTON_HELD_3SEC,         ui_provider_sm,              context_sm_out_of_case,               ui_input_button_held_2                        },
    {APP_MFB_BUTTON_HELD_6SEC,         ui_provider_sm,              context_sm_out_of_case,               ui_input_button_held_3                        },
#ifdef APP_MFB_BUTTON_HELD_8SEC
    {APP_MFB_BUTTON_HELD_8SEC,         ui_provider_sm,              context_sm_out_of_case,               ui_input_button_held_4                        },
#endif
    {APP_BUTTON_DFU,                   ui_provider_sm,              context_sm_out_of_case,               ui_input_dfu_active_when_in_case_request      },
    {APP_BUTTON_HELD_DFU,              ui_provider_sm,              context_sm_out_of_case,               ui_input_play_dfu_button_held_tone            },
    {APP_BUTTON_FACTORY_RESET,         ui_provider_sm,              context_sm_out_of_case,               ui_input_factory_reset_request                },
    {APP_BUTTON_HELD_FACTORY_RESET,    ui_provider_sm,              context_sm_out_of_case,               ui_input_play_factory_reset_tone              },

    {APP_MFB_BUTTON_HELD_RELEASE_6SEC, ui_provider_sm,              context_sm_out_of_case,               ui_input_sm_pair_handset                      },
    {APP_MFB_BUTTON_HELD_RELEASE_8SEC, ui_provider_sm,              context_sm_out_of_case,               ui_input_sm_delete_handsets                   },

    {APP_VA_BUTTON_DOWN,              ui_provider_voice_ui,        context_voice_ui_default,              ui_input_va_1                                 },
    {APP_VA_BUTTON_SINGLE_CLICK,      ui_provider_voice_ui,        context_voice_ui_default,              ui_input_va_3                                 },
    {APP_VA_BUTTON_DOUBLE_CLICK,      ui_provider_voice_ui,        context_voice_ui_default,              ui_input_va_4                                 },
    {APP_VA_BUTTON_HELD_1SEC,         ui_provider_voice_ui,        context_voice_ui_default,              ui_input_va_5                                 },
    {APP_VA_BUTTON_RELEASE,           ui_provider_voice_ui,        context_voice_ui_default,              ui_input_va_6                                 },
    
#if defined(HAVE_4_BUTTONS) || defined(HAVE_6_BUTTONS) || defined(HAVE_7_BUTTONS) || defined(HAVE_9_BUTTONS)
    {APP_BUTTON_VOLUME_UP_DOWN,       ui_provider_hfp,             context_hfp_voice_call_sco_active,    ui_input_hfp_volume_stop                      },
    {APP_BUTTON_VOLUME_UP_DOWN,       ui_provider_av,              context_av_is_streaming,              ui_input_av_volume_down_remote_stop           },
    {APP_BUTTON_VOLUME_UP_DOWN,       ui_provider_hfp,             context_hfp_connected,                ui_input_hfp_volume_stop                      },
    {APP_BUTTON_VOLUME_UP_DOWN,       ui_provider_hfp,             context_hfp_voice_call_sco_active,    ui_input_hfp_mute_toggle                      },

    {APP_BUTTON_VOLUME_DOWN,          ui_provider_hfp,             context_hfp_voice_call_sco_active,    ui_input_hfp_volume_down_start                },
    {APP_BUTTON_VOLUME_DOWN,          ui_provider_av,              context_av_is_streaming,              ui_input_av_volume_down_start                 },
    {APP_BUTTON_VOLUME_DOWN,          ui_provider_hfp,             context_hfp_connected,                ui_input_hfp_volume_down_start                },

    {APP_BUTTON_VOLUME_UP,            ui_provider_hfp,             context_hfp_voice_call_sco_active,    ui_input_hfp_volume_up_start                  },
    {APP_BUTTON_VOLUME_UP,            ui_provider_av,              context_av_is_streaming,              ui_input_av_volume_up_start                   },
    {APP_BUTTON_VOLUME_UP,            ui_provider_hfp,             context_hfp_connected,                ui_input_hfp_volume_up_start                  },

    {APP_BUTTON_VOLUME_DOWN_RELEASE,  ui_provider_hfp,             context_hfp_voice_call_sco_active,    ui_input_hfp_volume_stop                      },
    {APP_BUTTON_VOLUME_DOWN_RELEASE,  ui_provider_av,              context_av_is_streaming,              ui_input_av_volume_down_remote_stop           },
    {APP_BUTTON_VOLUME_DOWN_RELEASE,  ui_provider_hfp,             context_hfp_connected,                ui_input_hfp_volume_stop                      },

    {APP_BUTTON_VOLUME_UP_RELEASE,    ui_provider_hfp,             context_hfp_voice_call_sco_active,    ui_input_hfp_volume_stop                      },
    {APP_BUTTON_VOLUME_UP_RELEASE,    ui_provider_av,              context_av_is_streaming,              ui_input_av_volume_up_remote_stop             },
    {APP_BUTTON_VOLUME_UP_RELEASE,    ui_provider_hfp,             context_hfp_connected,                ui_input_hfp_volume_stop                      },
#endif /* HAVE_4_BUTTONS || HAVE_6_BUTTONS || HAVE_7_BUTTONS || HAVE_9_BUTTONS */

#if defined(HAVE_6_BUTTONS) || defined(HAVE_7_BUTTONS) || defined(HAVE_9_BUTTONS)
    {APP_BUTTON_FORWARD,              ui_provider_media_player,    context_av_is_streaming,              ui_input_av_forward                           },
    {APP_BUTTON_FORWARD_HELD,         ui_provider_media_player,    context_av_is_streaming,              ui_input_av_fast_forward_start                },
    {APP_BUTTON_FORWARD_HELD_RELEASE, ui_provider_media_player,    context_av_is_streaming,              ui_input_fast_forward_stop                    },
    {APP_BUTTON_BACKWARD,             ui_provider_media_player,    context_av_is_streaming,              ui_input_av_backward                          },
    {APP_BUTTON_BACKWARD_HELD,        ui_provider_media_player,    context_av_is_streaming,              ui_input_av_rewind_start                      },
    {APP_BUTTON_BACKWARD_HELD_RELEASE,ui_provider_media_player,    context_av_is_streaming,              ui_input_rewind_stop                          },
#endif /* HAVE_6_BUTTONS || HAVE_7_BUTTONS || HAVE_9_BUTTONS */
};

const ui_config_table_content_t* EarbudUi_GetConfigTable(unsigned* table_length)
{
    *table_length = ARRAY_DIM(earbud_ui_config_table);
    return earbud_ui_config_table;
}
