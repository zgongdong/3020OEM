/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   ui_domain UI
\ingroup    domains
\brief      UI domain/component

    These apis are used by application as well as different ui providers, ui input 
    consumers as well as ui provider context consumers to register/unregister themselves
    with the ui module as well as handling incoming logical input.This module sends mapped
    ui input for the logical input to interested ui input consumers.
*/

#ifndef UI_H_
#define UI_H_

#include <message.h>

#include "domain_message.h"
#include "ui_inputs.h"

#define BAD_CONTEXT 0xFF

/*\{*/

/*! External messages */
typedef enum
{
    UI_PROVIDER_CONTEXT_UPDATED=UI_MESSAGE_BASE,
} ui_message_t;

/*! \brief config table struct*/
typedef struct{

    unsigned logical_input;
    ui_providers_t ui_provider_id;
    unsigned ui_provider_context;
    ui_input_t ui_input;
}ui_config_table_content_t;

/*! \brief ui providers context callback */
typedef unsigned (*ui_provider_context_callback_t)(void);

/*! \brief ui provider context consumer callback */
typedef Task (*ui_provider_ctxt_consumer_task_t)(void);

/*! \brief ui input consumer callback */
typedef Task (*ui_input_consumer_task_t)(void);

/*! \brief ui input inject callback */
typedef void (*inject_ui_input)(ui_input_t ui_input, uint32 delay);

typedef struct
{
    ui_providers_t provider;
    unsigned context;
} UI_PROVIDER_CONTEXT_UPDATED_T;


/*! \brief provides ui task to other components

    \param[in]  void

    \return     Task - ui task.
*/
Task Ui_GetUiTask(void);

/*! \brief Registers ui provider context callback.

    This api is called by different ui providers to register their context callbacks
    with ui module.These callbacks are used by the ui module to query ui providers
    current context so that it may check in the ui config table if there is any ui event
    mapped for this context upon arrival of any logical input and can send that ui event
    to interested ui input consumers if there is any.

    \param ui_provider - ui provider which wants to register its context callback.
    \param ui_provider_context_callback - function pointer as context callback.

    \return void
*/
void Ui_RegisterUiProvider(ui_providers_t ui_provider, ui_provider_context_callback_t ui_provider_context_callback);

/*! \brief Unregister ui provider context callback.

    This api is called by application to unregister ui providers.This will
    free the memory acquired during ui providers registration.

    \param void

    \return void
*/
void Ui_UnregisterUiProviders(void);

/*! \brief Registeres individual module as ui input consumers

     This api is called by those modules who want to register themself with ui as ui
     input consumers.Each module provides its task handler as well as ui inputs in a
     list in which that module is interested.If any of those ui inputs are generated
     in the system then those ui input consumers who have registed for that ui input
     will be informed via registered task handler callback.

    \param ui_input_consumer_task - Task handler address which will be invoked to inform ui
                                    input arrival.
    \param msg_groups_of_interest - list of the message groups in which the given module is interested.
    \param num_msg_groups - size of the interested message groups list.

    \return void
*/
void Ui_RegisterUiInputConsumer(Task ui_input_consumer_task,
                                const message_group_t * msg_groups_of_interest,
                                unsigned num_msg_groups);

/*! \brief Registeres individual module as ui providers' context consumers

     This api is called by those modules who are interested in the change of context
     of provided ui provider.These interested module provide the module task handler
     as well as ui provider name whose context change information is to be notified to
     the interested module ( whose task handler is being registered).When any ui input
     arrival will change the context of that module in which other consumer module is
     interested then that module (who is interested in the context of other module)
     will recieve this information via message.

     \param ui_provider -  name of the ui provider whose context change information is being
                           requested.
     \param ui_provider_ctxt_consumer_task - Task handler address which will be invoked to
                                             inform context change.

     \return void
*/
void Ui_RegisterContextConsumers(ui_providers_t ui_provider,
                                 Task ui_provider_ctxt_consumer_task);

/*! \brief Unregister ui providers' context consumers.

    This api is called by application to unregister ui providers' context consumers.
    This will free the memory acquired during the registration of context consumers.

    \param void

    \return void
*/
void Ui_UnregisterContextConsumers(void);

/*! \brief Informs changed context to ui.

    This api is called any ui provider to inform ui that its context has changed due to arrival
    of an ui input.

    \param ui_provider - name of the ui provider who is sending this context change information
                         to ui module.
    \param latest_ctxt - latest context of that module.
*/
void Ui_InformContextChange(ui_providers_t ui_provider, unsigned latest_ctxt);

/*! \brief Injects an ui input directly to ui module.

    This api can be called by any module to inject any ui input in to the system.

    \param ui_input - ui input to be injected in to the system

    \return void
*/
void Ui_InjectUiInput(ui_input_t ui_input);

/*! \brief Injects an ui input directly to ui module.

    This api can be called by any module to inject any ui input in to the system.
    The delay is required so that ui inputs can be injected after a delay -
    this is currently required for use cases where an even needs to be sent to the
    secondary earbud and handled at the same time on both earbuds.
    The delay means the event is delivered at this same time.

    \param ui_input - ui input to be injected in to the system
    \param delay - the delay in ms before the message will be sent.
                   use D_IMMEDIATE to send immediately not 0.

    \return void
*/
void Ui_InjectUiInputWithDelay(ui_input_t ui_input, uint32 delay);

/*! \brief Initialise ui module task

    Called at start up to initialise ui module.

    \return TRUE indicating success
*/
bool Ui_Init(Task init_task);

/*! \brief Configure ui with table mapping logical inputs to ui inputs.

    This must be called before any logical inputs can be processed by the ui
    moodule.

    \param config_table - table used to generate ui inputs.
    \param config_size - no of rows in config_table.
*/
void Ui_SetConfigurationTable(const ui_config_table_content_t* config_table,
                              unsigned config_size);

/*! \brief Register a task to receive one of the UI input message group's messages.
    \param task The task that will receive the group messages.
    \param group The UI input message group of interest.
*/
void Ui_RegisterUiInputsMessageGroup(Task task, message_group_t group);

/*! \brief Registers the hook/interceptor function to receive UI input events
    \param ui_intercept_func - interceptor/hook function to receive UI inputs.
    \return The current UI interceptor function.
*/
inject_ui_input Ui_RegisterUiInputsInterceptor(inject_ui_input ui_intercept_func);

/*\}*/

#endif /* UI_H_ */
