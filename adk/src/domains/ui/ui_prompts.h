/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file converts System Events to corresponding ui inputs using table.
            And injects the UI inputs to UI module.
*/
#ifndef UI_PROMPTS_H
#define UI_PROMPTS_H

#include "ui_inputs.h"

/*! \brief ui_prompt task structure */
typedef struct
{
    /*! The  task. */
    TaskData task;

    /*! inject UI input prompts messages*/
    bool process_events;
} ui_promptsTaskData;

typedef struct
{
    /*! System Event that module is interseted in */
    MessageId sys_event;

    /*! Corresponding UI Input for System Event */
    ui_input_t ui_input;
}ui_prompts_config_table_t;

/*!< Indicator data structure */
extern ui_promptsTaskData  ui_prompts;

/*! Get pointer to ui_prompts data structure */
#define uiPromptsGetTaskData()          (&ui_prompts)

/*! Get the pointer to UI Prompts Task */
Task UiPrompts_GetUiPromptsTask(void);

/*! brief Initialise UI Prompts module */
bool UiPrompts_Init(Task init_task);

/*! Set the flag whether to inject UI Input prompts messages when System Events received */
void UiPrompts_ProcessEvents(bool process_events);

#endif // UI_PROMPTS_H
