/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for the Earbud Application user interface LED indications.
*/
#ifndef EARBUD_LED_H
#define EARBUD_LED_H

#include "led_manager.h"

/*! \brief The colour filter for the led_state applicable when the battery is low.
    \param led_state The input state.
    \return The filtered led_state.
*/
extern uint16 app_led_filter_battery_low(uint16 led_state);

/*! \brief The colour filter for the led_state applicable when charging but
           the battery voltage is still low.
    \param led_state The input state.
    \return The filtered led_state.
*/
extern uint16 app_led_filter_charging_low(uint16 led_state);

/*! \brief The colour filter for the led_state applicable when charging and the
           battery voltage is ok.
    \param led_state The input state.
    \return The filtered led_state.
*/
extern uint16 app_led_filter_charging_ok(uint16 led_state);

/*! \brief The colour filter for the led_state applicable when charging is complete.
    \param led_state The input state.
    \return The filtered led_state.
*/
extern uint16 app_led_filter_charging_complete(uint16 led_state);

//!@{ \name LED pattern and ringtone note sequence arrays.
extern const ledPattern app_led_pattern_power_on[];
extern const ledPattern app_led_pattern_power_off[];
extern const ledPattern app_led_pattern_error[];
extern const ledPattern app_led_pattern_idle[];
extern const ledPattern app_led_pattern_idle_connected[];
extern const ledPattern app_led_pattern_pairing[];
extern const ledPattern app_led_pattern_pairing_deleted[];
extern const ledPattern app_led_pattern_sco[];
extern const ledPattern app_led_pattern_call_incoming[];
extern const ledPattern app_led_pattern_battery_empty[];
extern const ledPattern app_led_pattern_peer_pairing[];
extern const ledPattern app_led_pattern_flash_once[];

#ifdef INCLUDE_DFU
extern const ledPattern app_led_pattern_dfu[];
#endif

#ifdef INCLUDE_AV
extern const ledPattern app_led_pattern_streaming[];
extern const ledPattern app_led_pattern_streaming_aptx[];
#endif
//!@}

/*! \brief Show HFP incoming call LED pattern */
#define appUiHfpCallIncomingActive() \
    appLedSetPattern(app_led_pattern_call_incoming, LED_PRI_HIGH)

/*! \brief Cancel HFP incoming call LED pattern */
#define appUiHfpCallIncomingInactive() \
    appLedStopPattern(LED_PRI_HIGH)

/*! \brief Show HFP call active LED pattern */
#define appUiHfpCallActive() \
    appLedSetPattern(app_led_pattern_sco, LED_PRI_HIGH)

/*! \brief Show HFP call imactive LED pattern */
#define appUiHfpCallInactive() \
    appLedStopPattern(LED_PRI_HIGH)

/*! \brief Show AV streaming active LED pattern */
#define appUiAvStreamingActive() \
    (appLedSetPattern(app_led_pattern_streaming, LED_PRI_MED))

//jacob to be discussed
/*! \brief Show AV APIX streaming active LED pattern */
#define appUiAvStreamingActiveAptx() \
    ( appLedSetPattern(app_led_pattern_streaming_aptx, LED_PRI_MED))

/*! \brief Cancel AV SBC/MP3 streaming active LED pattern */
#define appUiAvStreamingInactive() \
    (appLedStopPattern(LED_PRI_MED))

/*! \brief Battery OK, cancel any battery filter */
#define appUiBatteryOk() \
    appLedCancelFilter(0)

/*! \brief Enable battery low filter */
#define appUiBatteryLow() \
    appLedSetFilter(app_led_filter_battery_low, 0)

/*! \brief Show LED pattern for idle headset */
#define appUiIdleActive() \
    appLedSetPattern(app_led_pattern_idle, LED_PRI_LOW)

/*! \brief Show LED pattern for connected headset */
#define appUiIdleConnectedActive() \
    appLedSetPattern(app_led_pattern_idle_connected, LED_PRI_LOW)

/*! \brief Cancel LED pattern for idle/connected headset */
#define appUiIdleInactive() \
    appLedStopPattern(LED_PRI_LOW)

/*! \brief Cancel pairing active LED pattern */
#define appUiPairingInactive(is_user_initiated) \
    appLedStopPattern(LED_PRI_MED)

/*! \brief Cancel inquiry active LED pattern */
#define appUiPeerPairingInactive() \
    { appLedStopPattern(LED_PRI_MED); \
      MessageCancelFirst(EarbudUi_GetTask(), APP_INTERNAL_UI_INQUIRY_TIMEOUT); }

#ifdef INCLUDE_CHARGER
/*! \brief Charger charging, enable charging filter */
#define appUiChargerChargingLow() \
    appLedSetFilter(app_led_filter_charging_low, 1)
#endif

#ifdef INCLUDE_CHARGER
/*! \brief Charger charging, enable charging filter */
#define appUiChargerChargingOk() \
    appLedSetFilter(app_led_filter_charging_ok, 1)
#endif

#ifdef INCLUDE_CHARGER
/*! \brief Charger charging complete, enable charging complete filter */
#define appUiChargerComplete() \
    appLedSetFilter(app_led_filter_charging_complete, 1)
#endif

#ifdef INCLUDE_CHARGER
/*! \brief Charger disconnected, cancel any charger filter */
#define appUiChargerDisconnected() \
    appLedCancelFilter(1)
#endif

/*! \brief flash once LED pattern  */			//jacob
#define appUiLedFlashOnce() \
    appLedSetPattern(app_led_pattern_flash_once, LED_PRI_HIGH)

#endif // EARBUD_LED_H
