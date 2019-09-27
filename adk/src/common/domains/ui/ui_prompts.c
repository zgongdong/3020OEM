/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Source file converts System Events to corresponding ui inputs using table.
            And injects the UI inputs to UI module.
*/

#include "earbud_log.h"
#include "ui_prompts.h"
#include "ui_prompts_config_table.h"
#include "pairing.h"
#include "ui.h"
#include "av.h"
#include "prompts.h"
#include "domain_message.h"

ui_promptsTaskData  ui_prompts;

#define ERROR_MESSAGE_ID_NOT_PRESENT 0xFF

static uint8 uiPrompts_GetMessageIdIndexFromList(MessageId id)
{
    uint8 index = ERROR_MESSAGE_ID_NOT_PRESENT;
    uint8 i = 0;
    for(i = 0 ; i < UiPrompts_GetConfigTableSize(); i++)
    {
        if(id == ui_prompts_config_table[i].sys_event)
        {
            return i;
        }
    }
    return index;
}

static void uiPrompts_UpdateMessageGroupsToSniff(void)
{
    uint8 index = 0;

    for(index = 0 ; index < UiPrompts_GetConfigTableSize(); index++)
    {
        MessageId sys_event = ui_prompts_config_table[index].sys_event;
        message_group_t group = ID_TO_MSG_GRP(sys_event);

        MessageBroker_RegisterInterestInMsgGroups(
            UiPrompts_GetUiPromptsTask(),
            &group,
            1);      
    }    
}
 
static void uiPrompts_HandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);
    
    uint8 index_in_list = uiPrompts_GetMessageIdIndexFromList(id);

    /* make sure System Event exists in the Table and UI inputs needs to be injected */
    if(index_in_list != ERROR_MESSAGE_ID_NOT_PRESENT && ui_prompts.process_events)
    {
        Ui_InjectUiInput(ui_prompts_config_table[index_in_list].ui_input);
    }    
}

Task UiPrompts_GetUiPromptsTask(void)
{
    return &ui_prompts.task;
}

void UiPrompts_ProcessEvents(bool process_events)
{
    ui_prompts.process_events = process_events;    
}

/*! brief Initialise Ui prompts module */
bool UiPrompts_Init(Task init_task)
{
    UNUSED(init_task);

    ui_promptsTaskData *theTaskData = uiPromptsGetTaskData();

    /* Set up task handler */
    theTaskData->task.handler = uiPrompts_HandleMessage;
    theTaskData->process_events = TRUE;
    
    /* Add the MessageGroup using the ui_prompts_config_table table*/
    uiPrompts_UpdateMessageGroupsToSniff();

   /* this module ultimately play the prompts, will be moved to earbud_init*/
    Prompts_Init();

    return TRUE;
}
