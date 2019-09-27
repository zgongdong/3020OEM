/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Source file contains ui prompts config table which maps different System Events to
            corresponding ui inputs.
*/

#include "ui_prompts.h"
#include "av.h"
#include "pairing.h"
#include "telephony_messages.h"
#include "domain_message.h"
#include "ui_prompts_config_table.h"

#include <hydra_macros.h>

/*! \brief ui_prompts config table*/
const ui_prompts_config_table_t ui_prompts_config_table[] =
{
     { PAIRING_ACTIVE,                 ui_input_prompt_pairing             },
     { PAIRING_ACTIVE_USER_INITIATED,  ui_input_prompt_pairing             },
     { PAIRING_COMPLETE,               ui_input_prompt_pairing_successful  },
     { PAIRING_FAILED,                 ui_input_prompt_pairing_failed      },
     { TELEPHONY_CONNECTED,            ui_input_prompt_connected           },
     { AV_CONNECTED,                   ui_input_prompt_connected           },
     { TELEPHONY_DISCONNECTED,         ui_input_prompt_disconnected        },
};

unsigned UiPrompts_GetConfigTableSize(void)
{
    return ARRAY_DIM(ui_prompts_config_table);
}
