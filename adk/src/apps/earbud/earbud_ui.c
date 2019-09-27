/*!
\copyright  Copyright (c) 2008 - 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       earbud_ui.c
\brief      Earbud application User Interface Indications
*/

#include "earbud_ui.h"

#include "av.h"
#include "charger_monitor.h"
#include <earbud_rules_config.h>
#include "earbud_sm.h"
#include "hfp_profile.h"
#include "pairing.h"
#include "power_manager.h"
#include "scofwd_profile.h"
#include "ui.h"
#include "telephony_messages.h"
#include "volume_service.h"

static TaskData task;

/*! \brief provides indicator task

 *  \param void

    \return indicator task pointer.
*/
Task EarbudUi_GetTask(void)
{
    return &task;
}

const message_group_t indication_interested_ui_inputs[] =
{
    UI_INPUTS_TONE_FEEDBACK_MESSAGE_GROUP,
    UI_INPUTS_DEVICE_STATE_MESSAGE_GROUP
};

const message_group_t indication_interested_msg_groups[] =
{
    SYSTEM_MESSAGE_GROUP,
    APP_HFP_MESSAGE_GROUP,
    AV_UI_MESSAGE_GROUP,
    POWER_UI_MESSAGE_GROUP,
    CHARGER_MESSAGE_GROUP,
    PAIRING_MESSAGE_GROUP,
    SFWD_MESSAGE_GROUP,
    VOLUME_SERVICE_MESSAGE_GROUP,
    TELEPHONY_MESSAGE_GROUP,
};

/*! \brief Report a generic error on LEDs and play tone */
void EarbudUi_Error(void)
{
    appUiPlayTone(app_tone_error);
    appLedSetPattern(app_led_pattern_error, LED_PRI_EVENT);
}

/*! \brief Play HFP error tone and set LED error pattern.
    \param silent If TRUE the error is not presented on the UI.
*/
void EarbudUi_HfpError(bool silent)
{
    if (!silent)
    {
        appUiPlayTone(app_tone_error);
        appLedSetPattern(app_led_pattern_error, LED_PRI_EVENT);
    }
}

/*! \brief Play AV error tone and set LED error pattern. */
void EarbudUi_AvError(void)
{
    appUiPlayTone(app_tone_error);
    appLedSetPattern(app_led_pattern_error, LED_PRI_EVENT);
}

/*! \brief Play power on prompt and LED pattern */
void EarbudUi_PowerOn(void)
{
    /* Enable LEDs */
    appLedEnable(TRUE);

    appLedSetPattern(app_led_pattern_power_on, LED_PRI_EVENT);
    appUiPlayPrompt(PROMPT_POWER_ON);
}

/*! \brief Play power off prompt and LED pattern.
    \param lock The caller's lock, may be NULL.
    \param lock_mask Set bits in lock_mask will be cleared in lock when the UI completes.
 */
void EarbudUi_PowerOff(uint16 *lock, uint16 lock_mask)
{
    appLedSetPattern(app_led_pattern_power_off, LED_PRI_EVENT);
    appUiPlayPromptClearLock(PROMPT_POWER_OFF, lock, lock_mask);

    /* Disable LEDs */
    appLedEnable(FALSE);
}

/*! \brief Prepare UI for sleep.
    \note If in future, this function is modified to play a tone, it should
    be modified to resemble #EarbudUi_PowerOff, so the caller's lock is cleared when
    the tone is completed. */
void EarbudUi_Sleep(void)
{
    appLedSetPattern(app_led_pattern_power_off, LED_PRI_EVENT);
    appLedEnable(FALSE);
}

static void earbudUi_PlayToneFeedbackOnButtonPress(MessageId ui_input)
{
    switch (ui_input)
    {
        case ui_input_button_held_1:
            appUiButton();
            break;
        case ui_input_button_held_2:
            appUiButton2();
            break;
        case ui_input_button_held_3:
            appUiButton3();
            break;
        case ui_input_button_held_4:
            appUiButton4();
            break;
        case ui_input_dfu_active_when_in_case_request:
            appUiDfuRequested();
            break;
        case ui_input_play_dfu_button_held_tone:
            appUiButtonDfu();
            break;
        case ui_input_play_factory_reset_tone:
            appUiButtonFactoryReset();
            break;
        default:
            EarbudUi_Error();
            break;
    }
}

static void earbudUi_IndicateSystemUiEvents(MessageId id, Message message)
{
    UNUSED(message);
    switch (id)
    {
        case PAGING_START:
            appUiPagingStart();
            break;
        case PAGING_STOP:
            appUiPagingStop();
            break;
        default:
            break;
    }
}

static void earbudUi_IndicateTelephonyUiEvents(MessageId id, Message message)
{
    UNUSED(message);
    switch (id)
    {
    case TELEPHONY_CONNECTED:
        appUiHfpConnected(appGetHfp()->flags & HFP_CONNECT_NO_UI);
        break;
    case TELEPHONY_INCOMING_CALL:
        appUiHfpCallIncomingActive();
        break;
    case TELEPHONY_INCOMING_CALL_OUT_OF_BAND_RINGTONE:
        appUiHfpRing(appGetHfp()->caller_id_active);
        break;
    case TELEPHONY_INCOMING_CALL_ENDED:
        appUiHfpCallIncomingInactive();
        break;
    case TELEPHONY_CALL_ANSWERED:
        appUiHfpAnswer();
        break;
    case TELEPHONY_CALL_REJECTED:
        break;
    case TELEPHONY_CALL_ONGOING:
        appUiHfpCallActive();
        break;
    case TELEPHONY_CALL_ENDED:
        appUiHfpCallInactive();
        break;
    case TELEPHONY_CALL_HUNG_UP:
        appUiHfpHangup();
        break;
    case TELEPHONY_UNENCRYPTED_CALL_STARTED:
        appUiHfpScoUnencryptedReminder();
        break;
    case TELEPHONY_CALL_CONNECTION_FAILURE:
        EarbudUi_HfpError(appGetHfp()->flags & HFP_CONNECT_NO_ERROR_UI);
        break;
    case TELEPHONY_LINK_LOSS_OCCURRED:
        appUiHfpLinkLoss();
        break;
    case TELEPHONY_DISCONNECTED:
        appUiHfpDisconnected();
        break;
    case TELEPHONY_CALL_AUDIO_RENDERED_LOCAL:
        appUiHfpScoConnected();
        break;
    case TELEPHONY_CALL_AUDIO_RENDERED_REMOTE:
        if (appGetHfp()->sco_ui_indication)
        {
            appUiHfpScoDisconnected();
        }
        break;
    case TELEPHONY_ERROR:
        EarbudUi_HfpError(FALSE);
        break;
    case TELEPHONY_MUTE_ACTIVE:
        EarbudTones_MuteReminderIndication();
        break;
    case TELEPHONY_MUTE_INACTIVE:
        EarbudTones_CancelMuteReminder();
        break;
    case TELEPHONY_TRANSFERED:
        appUiHfpTransfer();
        break;
    default:
        break;
    }
}

static void earbudUi_IndicateAvUiEvents(MessageId id)
{
    switch (id)
    {
    case AV_STREAMING_ACTIVE:
        appUiAvStreamingActive();
        break;
    case AV_STREAMING_ACTIVE_APTX:
        appUiAvStreamingActiveAptx();
        break;
    case AV_STREAMING_INACTIVE:
        appUiAvStreamingInactive();
        break;
    case AV_CONNECTED:
        appUiAvConnected();
        break;
    case AV_CONNECTED_PEER:
        appUiAvPeerConnected();
        break;
    case AV_ERROR:
        EarbudUi_AvError();
        break;
    case AV_LINK_LOSS:
        appUiAvLinkLoss();
        break;
    case AV_DISCONNECTED:
        appUiAvDisconnected();
        break;
    case AV_REMOTE_CONTROL:
        appUiAvRemoteControl();
        break;
    case AV_VOLUME_LIMIT:
        appUiAvVolumeLimit();
        break;
    case AV_VOLUME_UP:
        appUiAvVolumeUp();
        break;
    case AV_VOLUME_DOWN:
        appUiAvVolumeDown();
        break;
    default:
        break;
    }
}

static void earbudUi_IndicatePairingUiEvents(MessageId id, Message message)
{
    UNUSED(message);
    switch (id)
    {
    case PAIRING_ACTIVE:
        appUiPairingActive(FALSE);
        break;
    case PAIRING_ACTIVE_USER_INITIATED:
        appUiPairingActive(TRUE);
        break;
    case PAIRING_COMPLETE:
        appUiPairingComplete();
        break;
    case PAIRING_FAILED:
        appUiPairingFailed();
        break;
    default:
        break;
    }
}

static void earbudUi_IndicateChargerUiEvents(MessageId id, Message message)
{
    UNUSED(message);
    switch (id)
    {
    case CHARGER_MESSAGE_DETACHED:
        appUiChargerDisconnected();
        break;
    case CHARGER_MESSAGE_ATTACHED:
        appUiChargerConnected();
        break;
    case CHARGER_MESSAGE_COMPLETED:
        appUiChargerComplete();
        break;
    case CHARGER_MESSAGE_CHARGING_OK:
        appUiChargerChargingOk();
        break;
    case CHARGER_MESSAGE_CHARGING_LOW:
        appUiChargerChargingLow();
        break;
    default:
        break;
    }
}

static void earbudUi_IndicatePowerUiEvents(MessageId id, Message message)
{
    switch (id)
    {
    case POWER_ON:
        EarbudUi_PowerOn();
        break;
    case POWER_OFF_MSG:
        {
            POWER_OFF_MSG_T * msg = (POWER_OFF_MSG_T *)message;
            EarbudUi_PowerOff(msg->lock, msg->mask);
        }
        break;
    default:
        break;
    }
}

static void earbudUI_IndicateVolumeServiceUIEvents(MessageId id)
{
    switch(id)
    {
        case VOLUME_SERVICE_MIN_VOLUME:
        case VOLUME_SERVICE_MAX_VOLUME:
            appUiAvVolumeLimit();
            break;
        default:
            break;
    }
}

static void earbudUi_IndicateUiEvent(MessageId id, Message message)
{
    switch (ID_TO_MSG_GRP(id))
    {
    case SYSTEM_MESSAGE_GROUP:
        earbudUi_IndicateSystemUiEvents(id, message);
        break;
    case TELEPHONY_MESSAGE_GROUP:
        earbudUi_IndicateTelephonyUiEvents(id, message);
        break;
    case AV_UI_MESSAGE_GROUP:
        earbudUi_IndicateAvUiEvents(id);
        break;
    case SFWD_MESSAGE_GROUP:
        if (id==SCOFWD_RINGING)
            appUiHfpRing();
        break;
    case PAIRING_MESSAGE_GROUP:
        earbudUi_IndicatePairingUiEvents(id, message);
        break;
    case CHARGER_MESSAGE_GROUP:
        earbudUi_IndicateChargerUiEvents(id, message);
        break;
    case POWER_APP_MESSAGE_GROUP:
        earbudUi_IndicatePowerUiEvents(id, message);
        break;
    case VOLUME_SERVICE_MESSAGE_GROUP:
        earbudUI_IndicateVolumeServiceUIEvents(id);
        break;
    default:
        break;
    }
}

/*! brief Display state based context information on the UI */
static void earbudUi_ConsumeContext(MessageId id, Message message)
{
    UNUSED(id);
    UI_PROVIDER_CONTEXT_UPDATED_T * msg = (UI_PROVIDER_CONTEXT_UPDATED_T*) message;
    switch (msg->provider)
    {
    case ui_provider_hfp:
        appUiHfpState(msg->context);
        break;
    case ui_provider_media_player:
        appUiAvState(msg->context);
        break;
    default:
        break;
    }
}

/*! \brief Message Handler

    This function is the main message handler for the indicator instance.It recieves
    messages from ui module and invokes further handler which basically calls respective
    functions for different ui inputs.
*/
static void earbudUi_HandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);

    if (id == UI_PROVIDER_CONTEXT_UPDATED)
    {
        earbudUi_ConsumeContext(id, message);
    }
    else if (id >= UI_INPUTS_TONE_FEEDBACK_MESSAGE_BASE && id < ui_input_invalid)
    {
        earbudUi_PlayToneFeedbackOnButtonPress(id);
    }
    else
    {
        earbudUi_IndicateUiEvent(id, message);
    }
}

/*! brief Initialise indicator module */
bool EarbudUi_Init(Task init_task)
{
    UNUSED(init_task);

    task.handler = earbudUi_HandleMessage;

    EarbudPrompts_Init();
    EarbudTones_Init();

    Ui_RegisterUiInputConsumer(
                EarbudUi_GetTask(),
                indication_interested_ui_inputs,
                ARRAY_DIM(indication_interested_ui_inputs));

    MessageBroker_RegisterInterestInMsgGroups(
                EarbudUi_GetTask(),
                indication_interested_msg_groups,
                ARRAY_DIM(indication_interested_msg_groups));

    Ui_RegisterContextConsumers(
                ui_provider_hfp,
                EarbudUi_GetTask());

    Ui_RegisterContextConsumers(
                ui_provider_media_player,
                EarbudUi_GetTask());

    return TRUE;
}

