/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Source file contains ui prompts config table which maps different System Events to
            corresponding ui inputs.
*/
#ifndef UI_PROMPTS_CONFIG_TABLE_H
#define UI_PROMPTS_CONFIG_TABLE_H

/*! \brief ui_prompts config table*/
extern const ui_prompts_config_table_t ui_prompts_config_table[];

/*! brief Initialise UI Prompts module */
unsigned UiPrompts_GetConfigTableSize(void);

#endif // UI_PROMPTS_CONFIG_TABLE_H
