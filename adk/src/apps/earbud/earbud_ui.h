/*!
\copyright  Copyright (c) 2008 - 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       earbud_ui.h
\brief	    Header file for the application User Interface
*/

#ifndef EARBUD_UI_H_
#define EARBUD_UI_H_

#include "domain_message.h"
#include "earbud_tones.h"
#include "earbud_led.h"

/*! \brief Time between inquiry in progress reminders (in seconds) */
#define APP_UI_INQUIRY_REMINDER_TIME            (5)

/*! \brief Time between connecting reminder tone (in seconds) */
#define APP_UI_CONNECTING_TIME                  (5)

/*! Internal messages */
enum
{
                                            /*!  Connecting reminder timeout */
    APP_INTERNAL_UI_CONNECTING_TIMEOUT= INTERNAL_MESSAGE_BASE,
    APP_INTERNAL_UI_INQUIRY_TIMEOUT        /*!< Inquiry reminder timeout */
};

/*! \brief provides indicator task

 *  \param void

    \return indicator task pointer.
*/
Task EarbudUi_GetTask(void);

#if INCLUDE_AV
/*! \brief Handle UI changes for AV state change */
#define appUiAvState(state) \
    /* Add any AV state indication here */
#endif

/*! \brief Play tone and show LED pattern for battery critical status */
#define appUiBatteryCritical() \
    { appLedSetPattern(app_led_pattern_battery_empty, LED_PRI_EVENT); \
      appUiPlayTone(app_tone_battery_empty); }

/*! \brief Start paging reminder */
#define appUiPagingStart() \
    MessageSendLater(EarbudUi_GetTask(), APP_INTERNAL_UI_CONNECTING_TIMEOUT, NULL, D_SEC(APP_UI_CONNECTING_TIME))

/*! \brief Stop paging reminder */
#define appUiPagingStop() \
    MessageCancelFirst(EarbudUi_GetTask(), APP_INTERNAL_UI_CONNECTING_TIMEOUT)

/*! \brief Play paging reminder tone */
#define appUiPagingReminder() \
    { appUiPlayTone(app_tone_paging_reminder); \
      MessageSendLater(EarbudUi_GetTask(), APP_INTERNAL_UI_CONNECTING_TIMEOUT, NULL, D_SEC(APP_UI_CONNECTING_TIME)); }

/*! \brief Show pairing active LED pattern only, prompt is played by prompts module*/
#define appUiPairingActive(is_user_initiated) \
do \
{  \
    appLedSetPattern(app_led_pattern_pairing, LED_PRI_MED); \
} while(0)

/*! \brief Play pairing deleted tone */
#define appUiPairingDeleted() \
    { appUiPlayTone(app_tone_pairing_deleted); \
      appLedSetPattern(app_led_pattern_pairing_deleted, LED_PRI_EVENT); }

/*! \brief Play inquiry active tone, show LED pattern */
#define appUiPeerPairingActive(is_user_initiated) \
    { if (is_user_initiated) \
        appUiPlayTone(app_tone_peer_pairing); \
      appLedSetPattern(app_led_pattern_peer_pairing, LED_PRI_MED); \
      MessageSendLater(EarbudUi_GetTask(), APP_INTERNAL_UI_INQUIRY_TIMEOUT, NULL, D_SEC(APP_UI_INQUIRY_REMINDER_TIME)); }

/*! \brief Play inquiry active reminder tone */
#define appUiPeerPairingReminder() \
    { appUiPlayTone(app_tone_peer_pairing_reminder); \
      MessageSendLater(EarbudUi_GetTask(), APP_INTERNAL_UI_INQUIRY_TIMEOUT, NULL, D_SEC(APP_UI_INQUIRY_REMINDER_TIME)); }

/*! \brief Play inquiry error tone */
#define appUiPeerPairingError() do { \
      appUiPlayTone(app_tone_peer_pairing_error); \
      appLedSetPattern(app_led_pattern_error, LED_PRI_EVENT); \
      } while (0)

#ifdef INCLUDE_DFU
/*! \brief Play DFU active tone, show LED pattern */
#define appUiDfuActive() \
    { appUiPlayTone(app_tone_dfu); \
      appLedEnable(TRUE); \
      appLedSetPattern(app_led_pattern_dfu, LED_PRI_LOW); }

/*! \brief Play DFU active tone, show LED pattern */
#define appUiDfuRequested() do { \
      appUiPlayTone(app_tone_dfu); \
      appLedSetPattern(app_led_pattern_dfu, LED_PRI_EVENT); \
      } while(0)
#endif

#ifdef INCLUDE_CHARGER
/*! \brief Charger connected */
#define appUiChargerConnected()
#endif

/*! brief Initialise indicator module */
bool EarbudUi_Init(Task init_task);

void EarbudUi_Error(void);
void EarbudUi_HfpError(bool silent);
void EarbudUi_AvError(void);

#endif /* EARBUD_UI_H_ */
