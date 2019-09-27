/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       audio_curation.c
\brief      A component responsible for controlling audio curation services.
*/

#include "audio_curation.h"
#include "logging.h"
#include "ui.h"
#include "anc_state_manager.h"
#include "anc.h"
#include "power_manager.h"

static void audioCuration_HandleMessage(Task task, MessageId id, Message message);
static const TaskData ui_task = {audioCuration_HandleMessage};


static const message_group_t ui_inputs[] =
{
    UI_INPUTS_AUDIO_CURATION_MESSAGE_GROUP
};

static Task audioCuration_UiTask(void)
{
  return (Task)&ui_task;
}

static anc_mode_t getAncModeFromUiInput(MessageId ui_input)
{
    anc_mode_t mode;

    switch(ui_input)
    {
        case ui_input_anc_set_mode_1:
            mode = anc_mode_1;
            break;
        case ui_input_anc_set_mode_2:
            mode = anc_mode_2;
            break;
        case ui_input_anc_set_mode_3:
            mode = anc_mode_3;
            break;
        case ui_input_anc_set_mode_4:
            mode = anc_mode_4;
            break;
        case ui_input_anc_set_mode_5:
            mode = anc_mode_5;
            break;
        case ui_input_anc_set_mode_6:
            mode = anc_mode_6;
            break;
        case ui_input_anc_set_mode_7:
            mode = anc_mode_7;
            break;
        case ui_input_anc_set_mode_8:
            mode = anc_mode_8;
            break;
        case ui_input_anc_set_mode_9:
            mode = anc_mode_9;
            break;
        case ui_input_anc_set_mode_10:
            mode = anc_mode_10;
        break;
        default:
            mode = anc_mode_1;
            break;
    }
    return mode;
}

/*! \brief provides ANC state machine context to the User Interface module.

    \param[in]  void

    \return     context - current context of ANC state machine.
*/
static unsigned getAncCurrentContext(void)
{
    audio_curation_provider_context_t context = BAD_CONTEXT;

    if(AncStateManager_IsTuningModeActive())
    {
        context = context_anc_tuning_mode_active;
    }
    else if(AncStateManager_IsEnabled())
    {
        context = context_anc_enabled;
    }
    else
    {
        context = context_anc_disabled;
    }

    return (unsigned)context;
}

static void handlePowerClientEvents(MessageId id)
{
    switch(id)
    {
        /* Power indication */
        case APP_POWER_SHUTDOWN_PREPARE_IND:
            AncStateManager_PowerOff();
            appPowerShutdownPrepareResponse(audioCuration_UiTask());
            break;

        case APP_POWER_SLEEP_PREPARE_IND:
            AncStateManager_PowerOff();
            appPowerSleepPrepareResponse(audioCuration_UiTask());
            break;

        /*In case of Sleep/SHUTDOWN cancelled*/
        case APP_POWER_SHUTDOWN_CANCELLED_IND:
        case APP_POWER_SLEEP_CANCELLED_IND:
            AncStateManager_PowerOn();
            break;

        default:
            break;
    }
}

static void handleUiDomainInput(MessageId ui_input)
{
    switch(ui_input)
    {
        case ui_input_anc_on:
            DEBUG_LOG("handleUiDomainInput, anc on input");
            AncStateManager_Enable();
            break;

        case ui_input_anc_off:
            DEBUG_LOG("handleUiDomainInput, anc off input");
            AncStateManager_Disable();
            break;

        case ui_input_anc_toggle_on_off:
            DEBUG_LOG("handleUiDomainInput, anc toggle on/off input");
            if(AncStateManager_IsEnabled())
            {
                AncStateManager_Disable();
            }
            else
            {
                AncStateManager_Enable();
            }

            break;

        case ui_input_anc_set_mode_1:
        case ui_input_anc_set_mode_2:
        case ui_input_anc_set_mode_3:
        case ui_input_anc_set_mode_4:
        case ui_input_anc_set_mode_5:
        case ui_input_anc_set_mode_6:
        case ui_input_anc_set_mode_7:
        case ui_input_anc_set_mode_8:
        case ui_input_anc_set_mode_9:
        case ui_input_anc_set_mode_10:
            DEBUG_LOG("handleUiDomainInput, anc set mode input");
            AncStateManager_SetMode(getAncModeFromUiInput(ui_input));
            break;

        case ui_input_anc_set_next_mode:
            DEBUG_LOG("handleUiDomainInput, anc next mode input");
            AncStateManager_SetNextMode();
            break;

        case ui_input_anc_enter_tuning_mode:
            DEBUG_LOG("handleUiDomainInput, enter anc tuning input");
            AncStateManager_EnterTuningMode();
            break;

        case ui_input_anc_exit_tuning_mode:
            DEBUG_LOG("handleUiDomainInput, exit anc tuning input");
            AncStateManager_ExitTuningMode();
            break;

        default:
            DEBUG_LOG("handleUiDomainInput, unhandled input");
            break;
    }
}

static unsigned getCurrentContext(void)
{
    return getAncCurrentContext();
}

static void audioCuration_HandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);

    if (id >= APP_POWER_INIT_CFM && id <= APP_POWER_SHUTDOWN_CANCELLED_IND)
    {
        handlePowerClientEvents(id);
    }
    if (ID_TO_MSG_GRP(id) == UI_INPUTS_AUDIO_CURATION_MESSAGE_GROUP)
    {
        handleUiDomainInput(id);
    }
}


/*! \brief Initialise the audio curation service

    \param[in]  init_task - added to inline with app_init

    \return     bool - returns initalization status
*/
bool AudioCuration_Init(Task init_task)
{
    Ui_RegisterUiProvider(ui_provider_audio_curation, getCurrentContext);

    Ui_RegisterUiInputConsumer(audioCuration_UiTask(), ui_inputs, ARRAY_DIM(ui_inputs));

    DEBUG_LOG("AudioCuration_Init, called");

    /* register with power to receive shutdown messages. */
    appPowerClientRegister(audioCuration_UiTask());
    appPowerClientAllowSleep(audioCuration_UiTask());

    UNUSED(init_task);
    
    return TRUE;
}

