/*!
\copyright  Copyright (c) 2005 - 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief	    Application state machine
*/

/* local includes */
#include "earbud.h"
#include "earbud_sm.h"
#include "earbud_sm_marshal_defs.h"
#include "earbud_anc_tuning.h"
#include "earbud_init.h"
#include "earbud_sm_private.h"
#include "earbud_ui.h"
#include "earbud_log.h"
#include "earbud_rules_config.h"
#include "earbud_primary_rules.h"
#include "earbud_secondary_rules.h"
#include "earbud_config.h"

/* framework includes */
#include <state_proxy.h>
#include <handset_signalling.h>
#include <pairing.h>
#include <power_manager.h>
#include <gaia_handler.h>
#include <device_upgrade.h>
#include <device_upgrade_peer.h>
#include <device_db_serialiser.h>
#include <gatt_handler.h>
#include <bredr_scan_manager.h>
#include <connection_manager.h>
#include <handset_service.h>
#include <hfp_profile.h>
#include <logical_input_switch.h>
#include <scofwd_profile.h>
#include <shadow_profile.h>
#include <ui.h>
#include <peer_signalling.h>
#include <key_sync.h>
#include <gatt_server_gatt.h>
#include <device_list.h>
#include<device_properties.h>
#include <profile_manager.h>
#include <tws_topology.h>
#include <hdma.h>

/* system includes */
#include <panic.h>
#include <connection.h>
#include <ps.h>
#include <boot.h>
#include <message.h>

#pragma unitsuppress Unused
/*!< Application state machine. */
smTaskData app_sm;

const message_group_t sm_ui_inputs[] =
{
    UI_INPUTS_HANDSET_MESSAGE_GROUP,
    UI_INPUTS_DEVICE_STATE_MESSAGE_GROUP
};

static void appSmHandleInternalDeleteHandsets(void);

#ifdef INCLUDE_DFU
static void appSmEnterDfuOnStartup(bool upgrade_reboot);
static void appSmNotifyUpgradeStarted(void);
static void appSmStartDfuTimer(void);
#endif /* INCLUDE_DFU */

/*****************************************************************************
 * SM utility functions
 *****************************************************************************/
static void earbudSm_SendCommandToPeer(earbud_sm_msg_type type)
{
    earbud_sm_msg_empty_payload_t* msg = PanicUnlessMalloc(sizeof(earbud_sm_msg_empty_payload_t));
    msg->type = type;
    appPeerSigMarshalledMsgChannelTx(SmGetTask(),
                                     PEER_SIG_MSG_CHANNEL_APPLICATION,
                                     msg, MARSHAL_TYPE_earbud_sm_msg_empty_payload_t);
}

/*! \brief Set event on active rule set */
static void appSmRulesSetEvent(rule_events_t event)
{
    switch (SmGetTaskData()->role)
    {
        case tws_topology_role_primary:
            PrimaryRules_SetEvent(SmGetTask(), event);
            break;
        case tws_topology_role_secondary:
            SecondaryRules_SetEvent(SmGetTask(), event);
            break;
        default:
            break;
    }
}

/*! \brief Reset event on active rule set */
static void appSmRulesResetEvent(rule_events_t event)
{
    switch (SmGetTaskData()->role)
    {
        case tws_topology_role_primary:
            PrimaryRules_ResetEvent(event);
            break;
        case tws_topology_role_secondary:
            SecondaryRules_ResetEvent(event);
            break;
        default:
            break;
    }
}

/*! \brief Mark rule action complete on active rule set */
static void appSmRulesSetRuleComplete(MessageId message)
{
    switch (SmGetTaskData()->role)
    {
        case tws_topology_role_primary:
            PrimaryRules_SetRuleComplete(message);
            break;
        case tws_topology_role_secondary:
            SecondaryRules_SetRuleComplete(message);
            break;
        default:
            break;
    }
}

static void appSmCancelDfuTimers(void)
{
    DEBUG_LOG("appSmCancelDfuTimers Cancelled all SM_INTERNAL_TIMEOUT_DFU_... timers");

    MessageCancelAll(SmGetTask(),SM_INTERNAL_TIMEOUT_DFU_ENTRY);
    MessageCancelAll(SmGetTask(),SM_INTERNAL_TIMEOUT_DFU_MODE_START);
    MessageCancelAll(SmGetTask(),SM_INTERNAL_TIMEOUT_DFU_AWAIT_DISCONNECT);
}

/*! \brief Delete all peer and handset pairing and reboot device. */
static void appSmDeletePairingAndReset(void)
{
    bdaddr bd_addr;

    DEBUG_LOG("appSmDeletePairingAndReset");

    /* cancel the link disconnection, may already be gone if it fired to get us here */
    MessageCancelFirst(SmGetTask(), SM_INTERNAL_TIMEOUT_LINK_DISCONNECTION);

    /* delete all paired handsets */
    appSmHandleInternalDeleteHandsets();

    /* delete paired peer earbud */
    if (appDeviceGetPeerBdAddr(&bd_addr))
        appDeviceDelete(&bd_addr);

    /* delete self device */
    if (appDeviceGetMyBdAddr(&bd_addr))
        appDeviceDelete(&bd_addr);

    /* Flood fill PS to force a defrag on reboot */
    PsFlood();

    appPowerReboot();
}

/*! \brief Force disconnection of the specified links.
    \return Bitfield of links that should be disconnected.
            If zero, all links are disconnected.
            If non-zero, caller needs to wait for link disconnection to complete
 * */
static smDisconnectBits appSmDisconnectLinks(smDisconnectBits links_to_disconnect)
{
    smDisconnectBits disconnecting_links = 0;

    if (links_to_disconnect & SM_DISCONNECT_PEER)
    {
        /* \todo just commented out for the time being, this is a work-in-progress */
//        if (appDeviceIsPeerConnected())
        {
//            DEBUG_LOG("appSmDisconnectLinks PEER IS CONNECTED");
            appPeerSigDisconnect(SmGetTask());
//            appAvDisconnectPeer();
            ScoFwdDisconnectPeer(SmGetTask());
            disconnecting_links |= SM_DISCONNECT_PEER;
        }
    }
    if (links_to_disconnect & SM_DISCONNECT_HANDSET)
    {
        /* \todo just commented out for the time being, this is a work-in-progress */
//        if (!appDeviceIsHandsetHfpDisconnected())
        {
//            DEBUG_LOG("appSmDisconnectLinks HANDSET HFP IS CONNECTED");
            appHfpDisconnectInternal();
            disconnecting_links |= SM_DISCONNECT_HANDSET;
        }
        /* \todo just commented out for the time being, this is a work-in-progress */
//        if (appDeviceIsHandsetA2dpConnected() || appDeviceIsHandsetAvrcpConnected())
        {
//            DEBUG_LOG("appSmDisconnectLinks HANDSET AV IS CONNECTED");
            appAvDisconnectHandset();
            disconnecting_links |= SM_DISCONNECT_HANDSET;
        }
    }
    return disconnecting_links;
}


/*! \brief Determine which state to transition to based on physical state.
    \return appState One of the states that correspond to a physical earbud state.
*/
static appState appSmCalcCoreState(void)
{
    bool busy = appAvIsStreaming() || appHfpIsScoActive();

    switch (appSmGetPhyState())
    {
        case PHY_STATE_IN_CASE:
            return APP_STATE_IN_CASE_IDLE;

        case PHY_STATE_OUT_OF_EAR:
        case PHY_STATE_OUT_OF_EAR_AT_REST:
            return busy ? APP_STATE_OUT_OF_CASE_BUSY :
                          APP_STATE_OUT_OF_CASE_IDLE;

        case PHY_STATE_IN_EAR:
            return busy ? APP_STATE_IN_EAR_BUSY :
                          APP_STATE_IN_EAR_IDLE;

        /* Physical state is unknown, default to in-case state */
        case PHY_STATE_UNKNOWN:
            return APP_STATE_IN_CASE_IDLE;

        default:
            Panic();
    }

    return APP_STATE_IN_EAR_IDLE;
}

static void appSmSetCoreState(void)
{
    appState state = appSmCalcCoreState();
    if (state != appGetState())
        appSetState(state);
}

/*! \brief Set the core app state for the first time. */
static void appSmSetInitialCoreState(void)
{
    smTaskData *sm = SmGetTaskData();

    /* Register with physical state manager to get notification of state
     * changes such as 'in-case', 'in-ear' etc */
    appPhyStateRegisterClient(&sm->task);

    /* Get latest physical state */
    sm->phy_state = appPhyStateGetState();

    /* Generate physical state events */
    switch (sm->phy_state)
    {
        case PHY_STATE_IN_CASE:
            appSmRulesSetEvent(RULE_EVENT_IN_CASE);
            break;

        case PHY_STATE_OUT_OF_EAR_AT_REST:
        case PHY_STATE_OUT_OF_EAR:
            appSmRulesSetEvent(RULE_EVENT_OUT_CASE);
            appSmRulesSetEvent(RULE_EVENT_OUT_EAR);
            break;

        case PHY_STATE_IN_EAR:
            appSmRulesSetEvent(RULE_EVENT_OUT_CASE);
            appSmRulesSetEvent(RULE_EVENT_IN_EAR);
            break;

        default:
            Panic();
    }

    /* Update core state */
    appSmSetCoreState();
}

/*! \brief Initiate disconnect of all links */
static void appSmInitiateLinkDisconnection(smDisconnectBits links_to_disconnect, uint16 timeout_ms,
                                           smPostDisconnectAction post_disconnect_action)
{
    smDisconnectBits disconnecting_links = appSmDisconnectLinks(links_to_disconnect);
    MESSAGE_MAKE(msg, SM_INTERNAL_LINK_DISCONNECTION_COMPLETE_T);
    if (!disconnecting_links)
    {
        appSmDisconnectLockClearLinks(SM_DISCONNECT_ALL);
    }
    else
    {
        appSmDisconnectLockSetLinks(disconnecting_links);
    }
    msg->post_disconnect_action = post_disconnect_action;
    MessageSendConditionally(SmGetTask(), SM_INTERNAL_LINK_DISCONNECTION_COMPLETE,
                             msg, &appSmDisconnectLockGet());

    /* Start a timer to force reset if we fail to complete disconnection */
    MessageSendLater(SmGetTask(), SM_INTERNAL_TIMEOUT_LINK_DISCONNECTION, NULL, timeout_ms);
}


static bool appSmStateNewBleConnectionChange(appState last,appState next)
{
    bool ble_was_allowed = appSmStateAreNewBleConnectionsAllowed(last);
    bool ble_now_allowed = appSmStateAreNewBleConnectionsAllowed(next);
    bool state_has_changed = last != next;

    /* If we are BLE connectable and state has changed may need to re-advertise */
    if (state_has_changed && ble_now_allowed)
        return TRUE;

    if (ble_was_allowed != ble_now_allowed)
        return TRUE;

    return FALSE;
}

/*! \brief Cancel A2DP and SCO out of ear timers. */
static void appSmCancelOutOfEarTimers(void)
{
    MessageCancelAll(SmGetTask(), SM_INTERNAL_TIMEOUT_OUT_OF_EAR_A2DP);
    MessageCancelAll(SmGetTask(), SM_INTERNAL_TIMEOUT_OUT_OF_EAR_SCO);

    PrimaryRules_SetRuleComplete(CONN_RULES_A2DP_TIMEOUT_CANCEL);
}

/*! \brief Determine if we have an A2DP restart timer pending. */
bool appSmIsA2dpRestartPending(void)
{
    /* try and cancel the A2DP auto restart timer, if there was one
     * then we're inside the time required to automatically restart
     * audio due to earbud being put back in the ear, so we need to
     * send a play request to the handset */
    return MessageCancelFirst(SmGetTask(), SM_INTERNAL_TIMEOUT_IN_EAR_A2DP_START);
}

/*****************************************************************************
 * SM state transition handler functions
 *****************************************************************************/
/*! \brief Enter initialising state

    This function is called whenever the state changes to
    APP_STATE_INITIALISING.
    It is reponsible for initialising global aspects of the application,
    i.e. non application module related state.
*/
static void appEnterInitialising(void)
{
    DEBUG_LOG("appEnterInitialising");
}

/*! \brief Exit initialising state.
 */
static void appExitInitialising(void)
{
    DEBUG_LOG("appExitInitialising");
}

/*! \brief Enter
 */
static void appEnterDfuCheck(void)
{
    DEBUG_LOG("appEnterDfuCheck");

    /* We only get into DFU check, if there wasn't a pending DFU
       So send a message to go to startup. */
    MessageSend(SmGetTask(), SM_INTERNAL_NO_DFU, NULL);
}

/*! \brief Exit
 */
static void appExitDfuCheck(void)
{
    DEBUG_LOG("appExitDfuCheck");
}

/*! \brief Enter actions when we enter the factory reset state.
    The application will drop all links, delete all pairing and reboot.
 */
static void appEnterFactoryReset(void)
{
    DEBUG_LOG("appEnterFactoryReset");
    appSmInitiateLinkDisconnection(SM_DISCONNECT_ALL, appConfigFactoryResetTimeoutMs(),
                                   POST_DISCONNECT_ACTION_NONE);
}

/*! \brief Exit factory reset. */
static void appExitFactoryReset(void)
{
    /* Should never happen */
    Panic();
}

/*! \brief Actions to take on entering startup state
 */
static void appEnterStartup(void)
{
    DEBUG_LOG("appEnterStartup");

    TwsTopology_Start(SmGetTask());

    /* Basically disable DFU for now */
#if 0
    /* As we have entered startup state we know there is no upgrade in
       progress. If needed block upgrades until we know we are in an
       appropriate state */
#ifdef INCLUDE_DFU
    if (   (   !appConfigDfuAllowBleUpgradeOutOfCase()
            && !appConfigDfuAllowBredrUpgradeOutOfCase())
        || appConfigDfuOnlyFromUiInCase())
    {
        appUpgradeAllowUpgrades(FALSE);
    }
#endif
#endif
}

static void appExitStartup(void)
{
    DEBUG_LOG("appExitStartup");
}

/*! \brief Start process to pair with peer earbud.
 */
static void appEnterPeerPairing(void)
{
    DEBUG_LOG("appEnterPeerPairing");
}

/*! \brief Exit actions when peer pairing completed.
 */
static void appExitPeerPairing(void)
{
    DEBUG_LOG("appExitPeerPairing");
}


/*! \brief Start process to pairing with handset.
 */
static void appEnterHandsetPairing(void)
{
    DEBUG_LOG("appEnterHandsetPairing");

    appSmInitiateLinkDisconnection(SM_DISCONNECT_HANDSET,
                                   appConfigLinkDisconnectionTimeoutTerminatingMs(),
                                   POST_DISCONNECT_ACTION_HANDSET_PAIRING);
}

/*! \brief Exit actions when handset pairing completed.
 */
static void appExitHandsetPairing(void)
{
    DEBUG_LOG("appExitHandsetPairing");

    appSmRulesSetRuleComplete(CONN_RULES_HANDSET_PAIR);
    Pairing_PairCancel();
}

/*! \brief Enter
 */
static void appEnterInCaseIdle(void)
{
    DEBUG_LOG("appEnterInCaseIdle");
}

/*! \brief Exit
 */
static void appExitInCaseIdle(void)
{
    DEBUG_LOG("appExitInCaseIdle");
}

/*! \brief Enter
 */
static void appEnterInCaseDfu(void)
{
    /* send peer sync to inform peer that DFU is in progress. */
    /*! \todo need event based mechanism to inform peer of DFU */
//    appPeerSyncSend(FALSE);

    SmGetTaskData()->enter_dfu_in_case = FALSE;

    if (TwsTopology_GetRole() != tws_topology_role_dfu)
    {
        DEBUG_LOG("appEnterInCaseDfu - switching to DFU topology");

        /*! We have entered DFU mode. Make sure that the topology catches up */
        TwsTopology_SwitchToDfuRole();
    }
    else
    {
        bool in_case = !appPhyStateIsOutOfCase();
        bool secondary = !BtDevice_IsMyAddressPrimary();
        DEBUG_LOG("appEnterInCaseDfu - In DFU topology already. Case:%d Primary:%d",
                    in_case, secondary);

        /*! \todo assuming that when we have entered case as secondary, 
            it is appropriate to cancel DFU timeouts 

            Exiting the case will leave DFU mode cleanly */
        if (in_case && secondary)
        {
            appSmCancelDfuTimers();
        }
    }
}

/*! \brief Exit
 */
static void appExitInCaseDfu(void)
{
    DEBUG_LOG("appExitInCaseDfu");
    appSmCancelDfuTimers();
    TwsTopology_EndDfuRole();
}

/*! \brief Enter
 */
static void appEnterOutOfCaseIdle(void)
{
    DEBUG_LOG("appEnterOutOfCaseIdle");

    /* Show idle on LEDs */
    appUiIdleActive();

    if (appConfigIdleTimeoutMs())
    {
        MessageSendLater(SmGetTask(), SM_INTERNAL_TIMEOUT_IDLE, NULL, appConfigIdleTimeoutMs());
    }
}

/*! \brief Exit
 */
static void appExitOutOfCaseIdle(void)
{
    DEBUG_LOG("appExitOutOfCaseIdle");

    /* Stop idle on LEDs */
    appUiIdleInactive();

    MessageCancelFirst(SmGetTask(), SM_INTERNAL_TIMEOUT_IDLE);
}

/*! \brief Enter
 */
static void appEnterOutOfCaseBusy(void)
{
    DEBUG_LOG("appEnterOutOfCaseBusy");
}

/*! \brief Cancel the audio pause timer.

    May have already fired, which is causing us to exit this state, but
    safe to do so.
 */
static void appExitOutOfCaseBusy(void)
{
    DEBUG_LOG("appExitOutOfCaseBusy");
}

/*! \brief Enter. */
static void appEnterOutOfCaseSoporific(void)
{
    DEBUG_LOG("appEnterOutOfCaseSoporific");
}

/*! \brief Exit. */
static void appExitOutOfCaseSoporific(void)
{
    DEBUG_LOG("appExitOutOfCaseSoporific");
}

/*! \brief Exit. */
static void appExitOutOfCaseSoporificTerminating(void)
{
    DEBUG_LOG("appExitOutOfCaseSoporificTerminating");
}

/*! \brief Enter. */
static void appEnterOutOfCaseSoporificTerminating(void)
{
    DEBUG_LOG("appEnterOutOfCaseSoporificTerminating");
}

/*! \brief Common actions when one of the terminating substates is entered.
 */
static void appEnterSubStateTerminating(void)
{
    DEBUG_LOG("appEnterSubStateTerminating");
    DeviceDbSerialiser_Serialise();

    appSmInitiateLinkDisconnection(SM_DISCONNECT_ALL, appConfigLinkDisconnectionTimeoutTerminatingMs(),
                                   POST_DISCONNECT_ACTION_NONE);
}

/*! \brief Common actions when one of the terminating substates is exited.
 */
static void appExitSubStateTerminating(void)
{
    DEBUG_LOG("appExitSubStateTerminating");
}

/*! \brief Enter
 */
static void appEnterInEarIdle(void)
{
    DEBUG_LOG("appEnterInEarIdle");
}

/*! \brief Exit
 */
static void appExitInEarIdle(void)
{
    DEBUG_LOG("appExitInEarIdle");
}

/*! \brief Enter
 */
static void appEnterInEarBusy(void)
{
    DEBUG_LOG("appEnterInEarBusy");
}

/*! \brief Exit
 */
static void appExitInEarBusy(void)
{
    DEBUG_LOG("appExitInEarBusy");
}

/*! \brief Exit APP_STATE_IN_CASE parent function actions. */
static void appExitInCase(void)
{
    DEBUG_LOG("appExitInCase");

    /* run rules for being taken out of the case */
    appSmRulesResetEvent(RULE_EVENT_IN_CASE);
    appSmRulesSetEvent(RULE_EVENT_OUT_CASE);
}

/*! \brief Exit APP_STATE_OUT_OF_CASE parent function actions. */
static void appExitOutOfCase(void)
{
    DEBUG_LOG("appExitOutOfCase");
}

/*! \brief Exit APP_STATE_IN_EAR parent function actions. */
static void appExitInEar(void)
{
    DEBUG_LOG("appExitInEar");

    /* Run rules for out of ear event */
    appSmRulesResetEvent(RULE_EVENT_IN_EAR);
    appSmRulesSetEvent(RULE_EVENT_OUT_EAR);
}

/*! \brief Enter APP_STATE_IN_CASE parent function actions. */
static void appEnterInCase(void)
{
    DEBUG_LOG("appEnterInCase");

    /* request handset signalling module send current state to handset. */
    appHandsetSigSendEarbudStateReq(PHY_STATE_IN_CASE);

    /* Run rules for in case event */
    appSmRulesResetEvent(RULE_EVENT_OUT_CASE);
    appSmRulesSetEvent(RULE_EVENT_IN_CASE);
}

/*! \brief Enter APP_STATE_OUT_OF_CASE parent function actions. */
static void appEnterOutOfCase(void)
{
    DEBUG_LOG("appEnterOutOfCase");

    /* request handset signalling module send current state to handset. */
    appHandsetSigSendEarbudStateReq(PHY_STATE_OUT_OF_EAR);
}

/*! \brief Enter APP_STATE_IN_EAR parent function actions. */
static void appEnterInEar(void)
{
    DEBUG_LOG("appEnterInEar");

    /* request handset signalling module send current state to handset. */
    appHandsetSigSendEarbudStateReq(PHY_STATE_IN_EAR);

    /* Run rules for in ear event */
    appSmRulesResetEvent(RULE_EVENT_OUT_EAR);
    appSmRulesSetEvent(RULE_EVENT_IN_EAR);

    /* \todo this should move to a rule at some point */
    if (appHfpIsCallIncoming())
    {
        DEBUG_LOG("appEnterInEar accepting call HFP");
        appHfpCallAccept();
    }
    else if (ScoFwdIsCallIncoming())
    {
        DEBUG_LOG("appEnterInEar accepting call SCOFWD");
        ScoFwdCallAccept();
    }
}

/*****************************************************************************
 * End of SM state transition handler functions
 *****************************************************************************/

/* This function is called to change the applications state, it automatically
   calls the entry and exit functions for the new and old states.
*/
void appSetState(appState new_state)
{
    appState previous_state = SmGetTaskData()->state;

    DEBUG_LOGF("appSetState, state 0x%02x to 0x%02x", previous_state, new_state);

    /* Handle state exit functions */
    switch (previous_state)
    {
        case APP_STATE_NULL:
            /* This can occur when DFU is entered during INIT. */
            break;

        case APP_STATE_INITIALISING:
            appExitInitialising();
            break;

        case APP_STATE_DFU_CHECK:
            appExitDfuCheck();
            break;

        case APP_STATE_FACTORY_RESET:
            appExitFactoryReset();
            break;

        case APP_STATE_STARTUP:
            appExitStartup();
            break;

        case APP_STATE_PEER_PAIRING:
            appExitPeerPairing();
            break;

        case APP_STATE_HANDSET_PAIRING:
            appExitHandsetPairing();
            break;

        case APP_STATE_IN_CASE_IDLE:
            appExitInCaseIdle();
            break;

        case APP_STATE_IN_CASE_DFU:
            appExitInCaseDfu();
            break;

        case APP_STATE_OUT_OF_CASE_IDLE:
            appExitOutOfCaseIdle();
            break;

        case APP_STATE_OUT_OF_CASE_BUSY:
            appExitOutOfCaseBusy();
            break;

        case APP_STATE_OUT_OF_CASE_SOPORIFIC:
            appExitOutOfCaseSoporific();
            break;

        case APP_STATE_OUT_OF_CASE_SOPORIFIC_TERMINATING:
            appExitOutOfCaseSoporificTerminating();
            break;

        case APP_STATE_TERMINATING:
            DEBUG_LOG("appExitTerminating");
            break;

        case APP_STATE_IN_CASE_TERMINATING:
            DEBUG_LOG("appExitInCaseTerminating");
            break;

        case APP_STATE_OUT_OF_CASE_TERMINATING:
            DEBUG_LOG("appExitOutOfCaseTerminating");
            break;

        case APP_STATE_IN_EAR_TERMINATING:
            DEBUG_LOG("appExitInEarTerminating");
            break;

        case APP_STATE_IN_CASE_DISCONNECTING:
            DEBUG_LOG("appExitInCaseDisconnecting");
            break;

        case APP_STATE_OUT_OF_CASE_DISCONNECTING:
            DEBUG_LOG("appExitOutOfCaseDisconnecting");
            break;

        case APP_STATE_IN_EAR_DISCONNECTING:
            DEBUG_LOG("appExitInEarDisconnecting");
            break;

        case APP_STATE_IN_EAR_IDLE:
            appExitInEarIdle();
            break;

        case APP_STATE_IN_EAR_BUSY:
            appExitInEarBusy();
            break;

        default:
            DEBUG_LOGF("Attempted to exit unsupported state 0x%02x", SmGetTaskData()->state);
            Panic();
            break;
    }

    /* if exiting a sleepy state */
    if (appSmStateIsSleepy(previous_state) && !appSmStateIsSleepy(new_state))
        appPowerClientProhibitSleep(SmGetTask());

    /* if exiting a terminating substate */
    if (appSmSubStateIsTerminating(previous_state) && !appSmSubStateIsTerminating(new_state))
        appExitSubStateTerminating();

    /* if exiting the APP_STATE_IN_CASE parent state */
    if (appSmStateInCase(previous_state) && !appSmStateInCase(new_state))
        appExitInCase();

    /* if exiting the APP_STATE_OUT_OF_CASE parent state */
    if (appSmStateOutOfCase(previous_state) && !appSmStateOutOfCase(new_state))
        appExitOutOfCase();

    /* if exiting the APP_STATE_IN_EAR parent state */
    if (appSmStateInEar(previous_state) && !appSmStateInEar(new_state))
        appExitInEar();

    /* Set new state */
    SmGetTaskData()->state = new_state;

    /* transitioning from connectable to not connectable, or vice versa */
    if (appSmStateIsConnectable(previous_state) != appSmStateIsConnectable(new_state))
    {
        /*! \todo move to topology */
        /* Kick the rules engine. The rule for RULE_EVENT_PAGE_SCAN_UPDATE is
           an always evaluated rule, so the event does not need to be reset */
        appSmRulesSetEvent(RULE_EVENT_PAGE_SCAN_UPDATE);
    }

    /* transitioning from BLE connectable to not connectable, or vice versa */
    if (appSmStateNewBleConnectionChange(previous_state,new_state))
    {
        /*! \todo move to topology */
        appSmRulesSetEvent(RULE_EVENT_BLE_CONNECTABLE_CHANGE);
    }

    /* if entering the APP_STATE_IN_CASE parent state */
    if (!appSmStateInCase(previous_state) && appSmStateInCase(new_state))
        appEnterInCase();

    /* if entering the APP_STATE_OUT_OF_CASE parent state */
    if (!appSmStateOutOfCase(previous_state) && appSmStateOutOfCase(new_state))
        appEnterOutOfCase();

    /* if entering the APP_STATE_IN_EAR parent state */
    if (!appSmStateInEar(previous_state) && appSmStateInEar(new_state))
        appEnterInEar();

    /* if entering a terminating substate */
    if (!appSmSubStateIsTerminating(previous_state) && appSmSubStateIsTerminating(new_state))
        appEnterSubStateTerminating();

    /* if entering a sleepy state */
    if (!appSmStateIsSleepy(previous_state) && appSmStateIsSleepy(new_state))
        appPowerClientAllowSleep(SmGetTask());

    /* Handle state entry functions */
    switch (new_state)
    {
        case APP_STATE_INITIALISING:
            appEnterInitialising();
            break;

        case APP_STATE_DFU_CHECK:
            appEnterDfuCheck();
            break;

        case APP_STATE_FACTORY_RESET:
            appEnterFactoryReset();
            break;

        case APP_STATE_STARTUP:
            appEnterStartup();
            break;

        case APP_STATE_PEER_PAIRING:
            appEnterPeerPairing();
            break;

        case APP_STATE_HANDSET_PAIRING:
            appEnterHandsetPairing();
            break;

        case APP_STATE_IN_CASE_IDLE:
            appEnterInCaseIdle();
            break;

        case APP_STATE_IN_CASE_DFU:
            appEnterInCaseDfu();
            break;

        case APP_STATE_OUT_OF_CASE_IDLE:
            appEnterOutOfCaseIdle();
            break;

        case APP_STATE_OUT_OF_CASE_BUSY:
            appEnterOutOfCaseBusy();
            break;

        case APP_STATE_OUT_OF_CASE_SOPORIFIC:
            appEnterOutOfCaseSoporific();
            break;

        case APP_STATE_OUT_OF_CASE_SOPORIFIC_TERMINATING:
            appEnterOutOfCaseSoporificTerminating();
            break;

        case APP_STATE_TERMINATING:
            DEBUG_LOG("appEnterTerminating");
            break;

        case APP_STATE_IN_CASE_TERMINATING:
            DEBUG_LOG("appEnterInCaseTerminating");
            break;

        case APP_STATE_OUT_OF_CASE_TERMINATING:
            DEBUG_LOG("appEnterOutOfCaseTerminating");
            break;

        case APP_STATE_IN_EAR_TERMINATING:
            DEBUG_LOG("appEnterInEarTerminating");
            break;

        case APP_STATE_IN_CASE_DISCONNECTING:
            DEBUG_LOG("appEnterInCaseDisconnecting");
            break;

        case APP_STATE_OUT_OF_CASE_DISCONNECTING:
            DEBUG_LOG("appEnterOutOfCaseDisconnecting");
            break;

        case APP_STATE_IN_EAR_DISCONNECTING:
            DEBUG_LOG("appEnterInEarDisconnecting");
            break;

        case APP_STATE_IN_EAR_IDLE:
            appEnterInEarIdle();
            break;

        case APP_STATE_IN_EAR_BUSY:
            appEnterInEarBusy();
            break;

        default:
            DEBUG_LOGF("Attempted to enter unsupported state 0x%02x", new_state);
            Panic();
            break;
    }
}

appState appGetState(void)
{
    return SmGetTaskData()->state;
}

/*! \brief Modify the substate of the current parent appState and return the new state. */
static appState appSetSubState(appSubState substate)
{
    appState state = appGetState();
    state &= ~APP_SUBSTATE_MASK;
    state |= substate;
    return state;
}

static appSubState appGetSubState(void)
{
    return appGetState() & APP_SUBSTATE_MASK;
}

/*! \brief Update the disconnecting links lock status */
static void appSmUpdateDisconnectingLinks(void)
{
    bdaddr remote_bdaddr;
    if (appSmDisconnectLockPeerIsDisconnecting())
    {
        DEBUG_LOG("appSmUpdateDisconnectingLinks disconnecting peer");
        if (!appDeviceGetPeerBdAddr(&remote_bdaddr) || !ConManagerIsConnected(&remote_bdaddr))
        {
            DEBUG_LOG("appSmUpdateDisconnectingLinks peer disconnected");
            appSmDisconnectLockClearLinks(SM_DISCONNECT_PEER);
        }
    }
    if (appSmDisconnectLockHandsetIsDisconnecting())
    {
        DEBUG_LOG("appSmUpdateDisconnectingLinks disconnecting handset");
        if (!appDeviceGetHandsetBdAddr(&remote_bdaddr) || !ConManagerIsConnected(&remote_bdaddr))
        {
            DEBUG_LOG("appSmUpdateDisconnectingLinks handset disconnected");
            appSmDisconnectLockClearLinks(SM_DISCONNECT_HANDSET);
        }
    }
}

/*! \brief Handle notification of (dis)connections. */
static void appSmHandleConManagerConnectionInd(CON_MANAGER_CONNECTION_IND_T* ind)
{
    DEBUG_LOGF("appSmHandleConManagerConnectionInd connected:%d", ind->connected);

    if (ind->ble)
    {
        /*! Although we get this, indicating that the BLE link has changed
            state, our booleans that track this are not yet updated, so
            there is no point kicking the peer synchronisation. */
    }

    if (ind->connected)
    {
        if (!ind->ble)
        {
            /* A BREDR connection won't neccessarily evaluate rules.
               And if we evaluate them now, the conditions needed for Upgrade control
               will not have changed.  Send a message instead. */
            MessageSend(SmGetTask(), SM_INTERNAL_BREDR_CONNECTED, NULL);
        }
        return;
    }

    appSmUpdateDisconnectingLinks();

    if (ind->ble)
    {
        DEBUG_LOGF("appSmHandleConManagerConnectionInd BLE link");
        return;
    }

    switch (appGetState())
    {
        default:
            /*! \todo unless we only get this message on peer disconnect,
             * how is this telling us weve disconnect from the peer, and
             * what use is the print other than debug? */
            /* Check if we've disconnected to peer */
            if (appDeviceIsPeer(&ind->bd_addr))
            {
                DEBUG_LOG("appSmHandleConManagerConnectionInd, disconnected from peer");
            }
            break;
    }

    /* Check if message was sent due to link-loss */
    if (ind->reason == hci_error_conn_timeout)
    {
        /* Generate link-loss events */
        if (appDeviceIsPeer(&ind->bd_addr))
        {
            DEBUG_LOG("appSmHandleConManagerConnectionInd, peer link loss");
            appSmRulesSetEvent(RULE_EVENT_PEER_LINK_LOSS);
        }
        else if (appDeviceIsHandset(&ind->bd_addr))
        {
            DEBUG_LOG("appSmHandleConManagerConnectionInd, handset link loss");
            appSmRulesSetEvent(RULE_EVENT_HANDSET_LINK_LOSS);
        }
    }

}

/*! \brief Handle completion of application module initialisation. */
static void appSmHandleInitConfirm(void)
{
    DEBUG_LOG("appSmHandleInitConfirm");

    /* Now we are fully initialised, enable messages from the always on
       event for DFU checking */
    appSmRulesSetEvent(RULE_EVENT_CHECK_DFU);

    switch (appGetState())
    {
        case APP_STATE_INITIALISING:
            appSetState(APP_STATE_DFU_CHECK);
            appPowerOn();
            break;

        case APP_STATE_IN_CASE_DFU:
            appPowerOn();
            break;

        default:
            Panic();
    }
}

/*! \brief Handle completion of peer pairing. 
*/
void appSmHandlePairingPeerPairConfirm(void);
void appSmHandlePairingPeerPairConfirm(void)
{
}

/*! \brief Handle completion of handset pairing. */
static void appSmHandlePairingPairConfirm(PAIRING_PAIR_CFM_T *cfm)
{
    DEBUG_LOGF("appSmHandlePairingPairConfirm, status %d", cfm->status);

    switch (appGetState())
    {
        case APP_STATE_HANDSET_PAIRING:
            appSmSetCoreState();
            break;

        case APP_STATE_FACTORY_RESET:
            /* Nothing to do, even if pairing with handset succeeded, the final
            act of factory reset is to delete handset pairing */
            break;

        default:
            /* Ignore, paired with handset with known address as requested by peer */
            break;
    }
}

/*! \brief Handle state machine transitions when earbud removed from the ear,
           or transitioning motionless out of the ear to just out of the ear. */
static void appSmHandlePhyStateOutOfEarEvent(void)
{
    DEBUG_LOGF("appSmHandlePhyStateOutOfEarEvent state=0x%x", appGetState());

    if (appSmIsCoreState())
        appSmSetCoreState();
}

/*! \brief Handle state machine transitions when earbud motionless out of the ear. */
static void appSmHandlePhyStateOutOfEarAtRestEvent(void)
{
    DEBUG_LOGF("appSmHandlePhyStateOutOfEarAtRestEvent state=0x%x", appGetState());

    if (appSmIsCoreState())
        appSmSetCoreState();
}

/*! \brief Handle state machine transitions when earbud is put in the ear. */
static void appSmHandlePhyStateInEarEvent(void)
{
    DEBUG_LOGF("appSmHandlePhyStateInEarEvent state=0x%x", appGetState());
    if (appSmIsCoreState())
        appSmSetCoreState();
}

/*! \brief Handle state machine transitions when earbud is put in the case. */
static void appSmHandlePhyStateInCaseEvent(void)
{
    DEBUG_LOGF("appSmHandlePhyStateInCaseEvent state=0x%x", appGetState());

    /*! \todo Need to add other non-core states to this conditional from which we'll
     * permit a transition back to a core state, such as...peer pairing? sleeping? */
    if (appSmIsCoreState() ||
        (appGetState() == APP_STATE_HANDSET_PAIRING))
    {
        appSmSetCoreState();
    }
}

/*! \brief Handle changes in physical state of the earbud. */
static void appSmHandlePhyStateChangedInd(smTaskData* sm, PHY_STATE_CHANGED_IND_T *ind)
{
    UNUSED(sm);

    DEBUG_LOGF("appSmHandlePhyStateChangedInd new phy state %d", ind->new_state);

    sm->phy_state = ind->new_state;

    switch (ind->new_state)
    {
        case PHY_STATE_IN_CASE:
            appSmHandlePhyStateInCaseEvent();
            return;
        case PHY_STATE_OUT_OF_EAR:
            appSmHandlePhyStateOutOfEarEvent();
            return;
        case PHY_STATE_OUT_OF_EAR_AT_REST:
            appSmHandlePhyStateOutOfEarAtRestEvent();
            return;
        case PHY_STATE_IN_EAR:
            appSmHandlePhyStateInEarEvent();
            return;
        default:
            break;
    }
}

/*! \brief Take action following power's indication of imminent sleep.
    SM only permits sleep when soporific. */
static void appSmHandlePowerSleepPrepareInd(void)
{
    DEBUG_LOG("appSmHandlePowerSleepPrepareInd");

    PanicFalse(APP_STATE_OUT_OF_CASE_SOPORIFIC == appGetState());

    appSetState(APP_STATE_OUT_OF_CASE_SOPORIFIC_TERMINATING);
}

/*! \brief Handle sleep cancellation. */
static void appSmHandlePowerSleepCancelledInd(void)
{
    DEBUG_LOG("appSmHandlePowerSleepCancelledInd");
    /* Sleep can only be entered from a core state, so return to the current core state */
    appSmSetCoreState();
}

/*! \brief Take action following power's indication of imminent shutdown.
    Can be received in any state. */
static void appSmHandlePowerShutdownPrepareInd(void)
{
    DEBUG_LOG("appSmHandlePowerShutdownPrepareInd");

    appSetState(appSetSubState(APP_SUBSTATE_TERMINATING));
}

/*! \brief Handle shutdown cancelled. */
static void appSmHandlePowerShutdownCancelledInd(void)
{
    DEBUG_LOG("appSmHandlePowerShutdownCancelledInd");
    /* Shutdown can be entered from any state (including non core states, e.g.
     peer pairing etc), so startup again, to determine the correct state to enter
     */
    appSetState(APP_STATE_STARTUP);
}


/*! \brief Start pairing with peer earbud. */
static void appSmHandleConnRulesPeerPair(void)
{
    DEBUG_LOG("appSmHandleConnRulesPeerPair");

    switch (appGetState())
    {
        case APP_STATE_STARTUP:
            DEBUG_LOG("appSmHandleConnRulesPeerPair, rule said pair with peer");
            appSetState(APP_STATE_PEER_PAIRING);
            break;

    default:
            break;
    }
}

static void appSmHandleConnRulesHandsetPair(void)
{
    DEBUG_LOG("appSmHandleConnRulesHandsetPair");

    switch (appGetState())
    {
        case APP_STATE_OUT_OF_CASE_IDLE:
        case APP_STATE_IN_EAR_IDLE:
            DEBUG_LOG("appSmHandleConnRulesHandsetPair, rule said pair with handset");
            appSmClearUserPairing();
            appSetState(APP_STATE_HANDSET_PAIRING);
            break;
        default:
            break;
    }
}

static void appSmHandleConnRulesEnterDfu(void)
{
    DEBUG_LOG("appSmHandleConnRulesEnterDfu");

    switch (appGetState())
    {
        case APP_STATE_IN_CASE_IDLE:
        case APP_STATE_IN_CASE_DFU:
            DEBUG_LOG("appSmHandleConnRulesEnterDfu, rule said enter DFU");
#ifdef INCLUDE_DFU
            appSmEnterDfuMode();
#endif
            break;

        default:
            break;
    }
    appSmRulesSetRuleComplete( CONN_RULES_ENTER_DFU);
}

static void appSmHandleConnRulesAllowHandsetConnect(void)
{
    DEBUG_LOG("appSmHandleConnRulesAllowHandsetConnect");

    ConManagerAllowHandsetConnect(TRUE);
    appSmRulesSetRuleComplete(CONN_RULES_ALLOW_HANDSET_CONNECT);
}

static void appSmHandleConnRulesRejectHandsetConnect(void)
{
    DEBUG_LOG("appSmHandleConnRulesRejectHandsetConnect");

    ConManagerAllowHandsetConnect(FALSE);
    appSmRulesSetRuleComplete(CONN_RULES_REJECT_HANDSET_CONNECT);
}

static void EarbudSm_HandleDfuNotifyPrimary(void)
{
    DEBUG_LOG("EarbudSm_HandleDfuNotifyPrimary");

    earbudSm_SendCommandToPeer(earbud_sm_ind_dfu_ready);
}

static void appSmHandleConnRulesForwardLinkKeys(void)
{
    bdaddr peer_addr;

    DEBUG_LOG("appSmHandleConnRulesForwardLinkKeys");

    /* Can only send this if we have a peer earbud */
    if (appDeviceGetPeerBdAddr(&peer_addr))
    {
        /* Attempt to send handset link keys to peer device */
//        appPairingTransmitHandsetLinkKeysReq();
        KeySync_Sync();
    }

    /* In all cases mark rule as done */
    appSmRulesSetRuleComplete(CONN_RULES_PEER_SEND_LINK_KEYS);
}

extern uint8 profile_list[4];

/*! \brief Connect HFP, A2DP and AVRCP to last connected handset. */
static void appSmHandleConnRulesConnectHandset(CONN_RULES_CONNECT_HANDSET_T* crch)
{
    DEBUG_LOGF("appSmHandleConnRulesConnectHandset profiles %u", crch->profiles);

    bool is_mru_handset = TRUE;
    device_t handset_device = DeviceList_GetFirstDeviceWithPropertyValue(device_property_mru, &is_mru_handset, sizeof(uint8));
    if (handset_device)
    {
        Device_SetProperty(handset_device, device_property_profiles_connect_order, profile_list, sizeof(profile_list));
        ProfileManager_ConnectProfilesRequest(&SmGetTaskData()->task, handset_device);
    }
    else
    {
        bdaddr handset_address = {0,0,0};
        if (appDeviceGetHandsetBdAddr(&handset_address))
        {
            handset_device = BtDevice_GetDeviceForBdAddr(&handset_address);
            Device_SetProperty(handset_device, device_property_profiles_connect_order, profile_list, sizeof(profile_list));
            ProfileManager_ConnectProfilesRequest(&SmGetTaskData()->task, handset_device);
        }
    }
}


/*! \brief Connect Peer Signalling to peer. */
static void appSmHandleConnRulesConnectPeerSignalling(void)
{
    bdaddr peer_addr = {0};

    if (appDeviceGetPeerBdAddr(&peer_addr))
    {
        DEBUG_LOG("appSmHandleConnRulesConnectPeerSignalling - connecting peer");
        appPeerSigConnect(SmGetTask(), &peer_addr);
    }
    else
    {
        DEBUG_LOG("appSmHandleConnRulesConnectPeerSignalling - no peer_addr");
    }
	
    /* Mark rule as done */
    appSmRulesSetRuleComplete(CONN_RULES_CONNECT_PEER_SIGNALLING);
}

/*! \brief Connect A2DP and AVRCP to peer. */
static void appSmHandleConnRulesConnectPeer(const CONN_RULES_CONNECT_PEER_T* crcp)
{
    DEBUG_LOGF("appSmHandleConnRulesConnectPeer profiles %u", crcp->profiles);

    if ((crcp->profiles & DEVICE_PROFILE_PEERSIG))
    {
        bdaddr peer_addr;
        appDeviceGetPeerBdAddr(&peer_addr);
        appPeerSigConnect(SmGetTask(), &peer_addr);
    }
#ifdef INCLUDE_SCOFWD
    if ((crcp->profiles & DEVICE_PROFILE_SCOFWD))
    {
        /* Connect SCO Forwarding link to peer */
        ScoFwdConnectPeer(SmGetTask());
    }
#endif /* INCLUDE_SCOFWD */

    if (crcp->profiles & DEVICE_PROFILE_A2DP)
    {
        /* Connect AVRCP and A2DP to peer */
//        appAvConnectPeer();
    }

    /* Only connect shadow profile if there is an active HFP call in progress.
       When shadow_profile supports A2DP also start if A2DP is streaming. */
    if (appHfpIsCall())
    {
        ShadowProfile_Connect();
    }

    /* Mark rule as done */
    appSmRulesSetRuleComplete(CONN_RULES_CONNECT_PEER);
}


/*! \brief Connect HFP, A2DP and AVRCP to peer's connected handset. */
static void appSmHandleConnRulesConnectPeerHandset(CONN_RULES_CONNECT_PEER_HANDSET_T* crcph)
{
    bdaddr peer_handset_addr;

    DEBUG_LOGF("appSmHandleConnRulesConnectPeerHandset profiles:%u", crcph->profiles);

    if (crcph->profiles & DEVICE_PROFILE_A2DP)
    {
        /* Connect to peer's handset for A2DP (and AVRCP) */
        StateProxy_GetPeerHandsetAddr(&peer_handset_addr);
        appAvConnectWithBdAddr(&peer_handset_addr);
    }
    if (crcph->profiles & DEVICE_PROFILE_HFP)
    {
        /* connecto to peer's handset for HFP */
        StateProxy_GetPeerHandsetAddr(&peer_handset_addr);
        appHfpConnectWithBdAddr(&peer_handset_addr, hfp_handsfree_107_profile);
    }

    /* Mark rule as done */
    appSmRulesSetRuleComplete(CONN_RULES_CONNECT_PEER_HANDSET);
}

/*! \brief Connect HFP, A2DP and AVRCP to peer's connected handset. */
static void appSmHandleConnRulesUpdateMruPeerHandset(void)
{
    bdaddr peer_handset_addr;
    DEBUG_LOG("appSmHandleConnRulesUpdateMruPeerHandset");

    /* Update most recent connected device */
    StateProxy_GetPeerHandsetAddr(&peer_handset_addr);
    appDeviceUpdateMruDevice(&peer_handset_addr);

    /* Mark rule as done */
    appSmRulesSetRuleComplete(CONN_RULES_UPDATE_MRU_PEER_HANDSET);

}

/*! \brief Send Earbud state and role messages to handset. */
static void appSmHandleConnRulesSendStatusToHandset(void)
{
    DEBUG_LOG("appSmHandleConnRulesSendStatusToHandset");

    /* request handset signalling module send current state to handset. */
    appHandsetSigSendEarbudStateReq(appSmGetPhyState());

    /* request handset signalling module sends current role to handset. */
    appHandsetSigSendEarbudRoleReq(appConfigIsLeft() ? EARBUD_ROLE_LEFT : EARBUD_ROLE_RIGHT);

    /* Mark rule as done */
    appSmRulesSetRuleComplete(CONN_RULES_SEND_STATE_TO_HANDSET);
}

/*! \brief Start timer to stop A2DP if earbud stays out of the ear. */
static void appSmHandleConnRulesA2dpTimeout(void)
{
    DEBUG_LOG("appSmHandleConnRulesA2dpTimeout");

    /* only take action if timeout is configured */
    if (appConfigOutOfEarA2dpTimeoutSecs())
    {
        /* either earbud is out of ear and case */
        if ((appSmIsOutOfCase()) ||
            (StateProxy_IsPeerOutOfCase() && StateProxy_IsPeerOutOfEar())) 
        {
            DEBUG_LOGF("appSmHandleConnRulesA2dpTimeout, out of case/ear, so starting %u second timer", appConfigOutOfEarA2dpTimeoutSecs());
            MessageSendLater(SmGetTask(), SM_INTERNAL_TIMEOUT_OUT_OF_EAR_A2DP,
                             NULL, D_SEC(appConfigOutOfEarA2dpTimeoutSecs()));
        }
    }

    appSmRulesSetRuleComplete(CONN_RULES_A2DP_TIMEOUT);
}

/*! \brief Handle decision to start media. */
static void appSmHandleConnRulesMediaPlay(void)
{
    DEBUG_LOG("appSmHandleConnRulesMediaPlay media play");

    /* request the handset plays the media player */
    Ui_InjectUiInput(ui_input_play);

    appSmRulesSetRuleComplete(CONN_RULES_MEDIA_PLAY);
}

/*! \brief Start timer to transfer SCO to AG if earbud stays out of the ear. */
static void appSmHandleConnRulesScoTimeout(void)
{
    DEBUG_LOG("appSmHandleConnRulesA2dpTimeout");

    if (appSmIsOutOfCase() && appConfigOutOfEarScoTimeoutSecs())
    {
        DEBUG_LOGF("appSmHandleConnRulesScoTimeout, out of case/ear, so starting %u second timer", appConfigOutOfEarScoTimeoutSecs());
        MessageSendLater(SmGetTask(), SM_INTERNAL_TIMEOUT_OUT_OF_EAR_SCO,
                         NULL, D_SEC(appConfigOutOfEarScoTimeoutSecs()));
    }

    appSmRulesSetRuleComplete(CONN_RULES_SCO_TIMEOUT);
}

/*! \brief Transfer SCO from AG as earbud is in the ear. */
static void appSmHandleConnRulesScoTransferToEarbud(void)
{
    DEBUG_LOGF("appSmHandleConnRulesScoTransferToEarbud, in ear transferring audio this %u peer %u", appSmIsInEar(), StateProxy_IsPeerInEar());
    appHfpTransferToHeadset();
    appSmRulesSetRuleComplete(CONN_RULES_SCO_TRANSFER_TO_EARBUD);
}

static void appSmHandleConnRulesScoTransferToHandset(void)
{
    DEBUG_LOG("appSmHandleConnRulesScoTransferToHandset");
    appHfpTransferToAg();
    appSmRulesSetRuleComplete(CONN_RULES_SCO_TRANSFER_TO_HANDSET);
}

/*! \brief Turns LEDs on */
static void appSmHandleConnRulesLedEnable(void)
{
    DEBUG_LOG("appSmHandleConnRulesLedEnable");

    appLedEnable(TRUE);
    appSmRulesSetRuleComplete(CONN_RULES_LED_ENABLE);
}

/*! \brief Turns LEDs off */
static void appSmHandleConnRulesLedDisable(void)
{
    DEBUG_LOG("appSmHandleConnRulesLedDisable");

    appLedEnable(FALSE);
    appSmRulesSetRuleComplete(CONN_RULES_LED_DISABLE);
}

/*! \brief Pause A2DP as an earbud is out of the ear for too long. */
static void appSmHandleInternalTimeoutOutOfEarA2dp(void)
{
    DEBUG_LOG("appSmHandleInternalTimeoutOutOfEarA2dp out of ear pausing audio");

    /* Double check we're still out of case when the timeout occurs */
    if (appSmIsOutOfCase() ||
        ((StateProxy_IsPeerOutOfEar()) && (StateProxy_IsPeerOutOfCase())))
    {
        /* request the handset pauses the media player */
        Ui_InjectUiInput(ui_input_pause);

        /* start the timer to autorestart the audio if earbud is placed
         * back in the ear. */
        if (appConfigInEarA2dpStartTimeoutSecs())
        {
            MessageSendLater(SmGetTask(), SM_INTERNAL_TIMEOUT_IN_EAR_A2DP_START,
                             NULL, D_SEC(appConfigInEarA2dpStartTimeoutSecs()));
        }
    }
}

/*! \brief Nothing to do. */
static void appSmHandleInternalTimeoutInEarA2dpStart(void)
{
    DEBUG_LOG("appSmHandleInternalTimeoutInEarA2dpStart");

    /* no action needed, we're just using the presence/absence of an active
     * timer when going into InEar to determine if we need to restart audio */
}

/*! \brief Transfer SCO to AG as earbud is out of the ear for too long. */
static void appSmHandleInternalTimeoutOutOfEarSco(void)
{
    DEBUG_LOG("appSmHandleInternalTimeoutOutOfEarSco out of ear transferring audio");

    /* If we're forwarding SCO, check peer state as transferring SCO on master will
     * also remove SCO on slave */
    if (ScoFwdIsSending())
    {
        /* Double check we're still out of case and peer hasn't gone back in the
         * ear when the timeout occurs */
        if (appSmIsOutOfCase() && !StateProxy_IsPeerInEar())
        {
            /* Transfer SCO to handset */
            appHfpTransferToAg();
        }
    }
    else
    {
        /* Double check we're still out of case */
        if (appSmIsOutOfCase())
        {
            /* Transfer SCO to handset */
            appHfpTransferToAg();
        }
    }
}


/*! \brief Earbud put in case disconnect link to handset */
static void appSmHandleConnRulesHandsetDisconnect(CONN_RULES_DISCONNECT_HANDSET_T *action)
{
    DEBUG_LOG("appSmHandleConnRulesHandsetDisconnect");

    smPostDisconnectAction post_disconnect_action = POST_DISCONNECT_ACTION_NONE;
    if (action && action->handover)
    {
        bool play_media;
        switch (appAvPlayStatus())
        {
            case avrcp_play_status_playing:
            case avrcp_play_status_fwd_seek:
            case avrcp_play_status_rev_seek:
                play_media = TRUE;
                break;
            default:
                play_media = FALSE;
                break;
        }
        post_disconnect_action = play_media ? POST_DISCONNECT_ACTION_HANDOVER_AND_PLAY_MEDIA
                                            : POST_DISCONNECT_ACTION_HANDOVER;
    }
    appSmInitiateLinkDisconnection(SM_DISCONNECT_HANDSET,
                                   appConfigLinkDisconnectionTimeoutTerminatingMs(),
                                   post_disconnect_action);
    appSetState(appSetSubState(APP_SUBSTATE_DISCONNECTING));
    appSmRulesSetRuleComplete(CONN_RULES_DISCONNECT_HANDSET);
}


/*! \brief Earbud put in case disconnect link to peer */
static void appSmHandleConnRulesPeerDisconnect(void)
{
    DEBUG_LOG("appSmHandleConnRulesPeerDisconnect");

#ifdef INCLUDE_SCOFWD
    ScoFwdDisconnectPeer(SmGetTask());
#endif /* INCLUDE_SCOFWD */
//    appAvDisconnectPeer();
    appPeerSigDisconnect(SmGetTask());

    ShadowProfile_Disconnect();

    appSmRulesSetRuleComplete(CONN_RULES_DISCONNECT_PEER);
}

#ifdef INCLUDE_SCOFWD
/*! \brief Let the SCO forwarding module know that we want to bring
        up the SCO forwarding link. */
static void appSmHandleConnRulesSendPeerScofwdConnect(void)
{
    bdaddr peer;

    DEBUG_LOG("appSmHandleConnRulesSendPeerScofwdConnect");

    if (appDeviceGetPeerBdAddr(&peer) && appDeviceIsHandsetHfpConnected() &&
        !appDeviceIsTwsPlusHandset(appHfpGetAgBdAddr()))
    {
        ScoFwdConnectPeer(SmGetTask());
        /*! \todo Decide whether to mark the rule complete when we
            don't know if the link is up yet.
            Yes and move completion out of the else like other rule action handlers */
    }
    else
    {
        appSmRulesSetRuleComplete(CONN_RULES_SEND_PEER_SCOFWD_CONNECT);
    }
}
#endif

/*! \brief Switch which microphone is used during MIC forwarding. */
static void appSmHandleConnRulesSelectMic(CONN_RULES_SELECT_MIC_T* crsm)
{
    DEBUG_LOGF("appSmHandleConnRulesSelectMic selected mic %u", crsm->selected_mic);

    switch (crsm->selected_mic)
    {
        case MIC_SELECTION_LOCAL:
            appKymeraScoUseLocalMic();
            break;
        case MIC_SELECTION_REMOTE:
            appKymeraScoUseRemoteMic();
            break;
        default:
            break;
    }

    appSmRulesSetRuleComplete(CONN_RULES_SELECT_MIC);
}

static void appSmHandleConnRulesScoForwardingControl(CONN_RULES_SCO_FORWARDING_CONTROL_T* crsfc)
{
    DEBUG_LOGF("appSmHandleConnRulesScoForwardingControl forwarding %u", crsfc->forwarding_control);

    if (crsfc->forwarding_control)
    {
        /* enabled forwarding */
        ScoFwdEnableForwarding();
    }
    else
    {
        /* disable forwarding */
        ScoFwdDisableForwarding();
    }

    appSmRulesSetRuleComplete(CONN_RULES_SCO_FORWARDING_CONTROL);
}

/*! \brief Handle confirmation of SM marhsalled msg TX, free the message memory. */
static void appSm_HandleMarshalledMsgChannelTxCfm(const PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM_T* cfm)
{
//    DEBUG_LOG("appSm_HandleMarshalledMsgChannelTxCfm channel %u status %u", cfm->channel, cfm->status);
    if (cfm->msg_ptr)
    {
        earbud_sm_msg_empty_payload_t *req = (earbud_sm_msg_empty_payload_t *)cfm->msg_ptr;
        if (req->type == earbud_sm_req_factory_reset)
        {
            if (cfm->status != peerSigStatusSuccess)
            {
                DEBUG_LOG("Failed to send earbud_sm_cmd_factory_reset_request to peer, applying locally.");
            }
            appSmFactoryReset();
        }
    }

    /* free the memory for the marshalled message now confirmed sent */
    free(cfm->msg_ptr);
}

static void appSm_DisconnectAndSwitchRole(bool become_primary)
{
    DEBUG_LOG("appSm_DisconnectAndSwitchRole");

    smPostDisconnectAction post_disconnect_action = become_primary ? POST_DISCONNECT_ACTION_SWITCH_TO_PRIMARY :
                                                                 POST_DISCONNECT_ACTION_SWITCH_TO_SECONDARY;

    appSmInitiateLinkDisconnection(SM_DISCONNECT_ALL,
                                   appConfigLinkDisconnectionTimeoutTerminatingMs(),
                                   post_disconnect_action);
    appSetState(appSetSubState(APP_SUBSTATE_DISCONNECTING));
}

#if 0
static void appSm_SwitchAddress(bool primary)
{
    if (!TwsTopology_SwitchAddress(primary))
    {
        int delay = 500;
        MESSAGE_MAKE(message, SM_INTERNAL_ADDR_SWITCH_DELAY_T);
        message->primary = primary;
        DEBUG_LOG("appSm_SwitchAddress failed to override bdaddr delaying for %ums", delay);
        MessageSendLater(SmGetTask(), SM_INTERNAL_TIMEOUT_ADDR_SWITCH_RETRY, message, delay); 
    }
    else
    {
        DEBUG_LOG("appSm_SwitchAddress success");

        if (primary)
        {
            /* record new role */
            SmGetTaskData()->role = tws_topology_role_primary;

            /* \todo remove this once LE role selection is available and the state proxy
             * will be able to listen to role selection events instead */
            StateProxy_SetRole(TRUE);

#ifdef INCLUDE_SHADOWING
            ShadowProfile_SetRole(TRUE);
#endif

            /*! \todo TEMP until connection rules migrate to TWS topology */
//            TwsTopology_RoleSwitchCompleted();

            /* kick the rules engines with a role switch event */
            PrimaryRules_SetEvent(SmGetTask(), RULE_EVENT_ROLE_SWITCH);
            if (appPhyStateGetState() != PHY_STATE_IN_CASE)
            {
                PrimaryRules_SetEvent(SmGetTask(), RULE_EVENT_OUT_CASE);
            }
        }
        else
        {
            /* record new role */
            SmGetTaskData()->role = tws_topology_role_secondary;

            /* \todo remove this once LE role selection is available and the state proxy
             * will be able to listen to role selection events instead */
            StateProxy_SetRole(FALSE);

#ifdef INCLUDE_SHADOWING
            ShadowProfile_SetRole(FALSE);
#endif

            /*! \todo TEMP eventually TWS topology will have the secondary paging the
             * primary */
            //        ScanManagerEnablePageScan(SCAN_MAN_USER_SM, SCAN_MAN_PARAMS_TYPE_FAST);

            /*! \todo TEMP until connection rules migrate to TWS topology */
//            TwsTopology_RoleSwitchCompleted();

            /* kick the rules engines with a role switch event */
            SecondaryRules_SetEvent(SmGetTask(), RULE_EVENT_ROLE_SWITCH);
            if (appPhyStateGetState() != PHY_STATE_IN_CASE)
            {
                SecondaryRules_SetEvent(SmGetTask(), RULE_EVENT_OUT_CASE);
            }

        }
    }
}
#endif

#if 0
/* Actually perform the role switch.
 * Will migrate to TWS topology
 * Assumption here is that BTSS is now idle or address swap will fail
 * If you're seeing failures the most likely things are:-
 *   - still have a link up, most likely due to peer signalling
 *   - BREDR scanning is active
 */
static void appSm_CompleteSwitchRole(bool become_primary)
{
    DEBUG_LOG("appSm_CompleteSwitchRole %u", become_primary);

    if (become_primary)
    {
        /* handover current Secondary rule set events to Primary rule set */
        rule_events_t current_events = SecondaryRules_GetEvents();
        DEBUG_LOG("appSm_CompleteSwitchRole switch rule sets to PRIMARY %lx", current_events);
        PrimaryRules_SetEvent(SmGetTask(), current_events);
        SecondaryRules_ResetEvent(RULE_EVENT_ALL_EVENTS_MASK);

        /* Setup Primary BT address */
        appSm_SwitchAddress(TRUE);
    }
    else
    {
        /* handover current Primary rule set event to Secondary rule set */
        rule_events_t current_events = PrimaryRules_GetEvents();
        DEBUG_LOG("appSm_CompleteSwitchRole switch rule sets to SECONDARY %lx", current_events);
        SecondaryRules_SetEvent(SmGetTask(), current_events);
        PrimaryRules_ResetEvent(RULE_EVENT_ALL_EVENTS_MASK);

        /* Setup Secondary BT address */
        appSm_SwitchAddress(FALSE);
    }

    BredrScanManager_ScanResume();
}

/*! \brief Switch Primary/Secondary Role.
    
    May not be required if already in the correct role, the recipient of
    an earbud_sm_role_decision_req_t message can have already decided before
    an earbud_sm_role_decision_cfm_t is received.

    Temporary contents in here (and perhaps the whole function) this is a
    coarse BREDR-based role switch that only decides which Primary/Secondary
    rule set to use. Most logic will move into TWS topology.
*/
static void appSm_SwitchRole(bool become_primary)
{
    if (become_primary)
    {
        /* want primary role and currently a secondary */
        if (SmGetTaskData()->role != tws_topology_role_primary)
        {
            /* \todo
             * At the moment a page scan is the only thing going on,
             * we're the secondary so it shouldn't be, fix that! */
            StateProxy_Stop();
            ConManagerAllowConnection(cm_transport_ble, FALSE);
            BredrScanManager_ScanPause(SmGetTask());
            appSm_DisconnectAndSwitchRole(become_primary);
        }
        /*! \todo this should probably move with the delayed switch... */
        LogicalInputSwitch_SetRerouteToPeer(FALSE);
    }
    else
    {
        if (SmGetTaskData()->role != tws_topology_role_secondary)
        {
            /* want secondary role, must always do the switch as may be using
             * incorrect address for secondary role */
            StateProxy_Stop();
            ConManagerAllowConnection(cm_transport_ble, FALSE);
            BredrScanManager_ScanPause(SmGetTask());
            appSm_DisconnectAndSwitchRole(become_primary);
        }

        /*! \todo this should probably move with the delayed switch... */
        LogicalInputSwitch_SetRerouteToPeer(TRUE);
    }
}
#endif

static void earbudSm_HandleMarshalledEmptyPayloadMsg(const earbud_sm_msg_empty_payload_t* msg)
{
    PanicNull((void *)msg);
    switch(msg->type)
    {
        case earbud_sm_req_dfu_active_when_in_case:
            appSmEnterDfuModeInCase(TRUE);
            break;

        case earbud_sm_req_factory_reset:
            DEBUG_LOG("earbud_sm_cmd_factory_reset_request received from Peer");
            appSmFactoryReset();
            break;

        case earbud_sm_ind_dfu_ready:
            DEBUG_LOG("earbud_sm_ind_dfu_ready_indication received from Peer");
            break;

        default:
            break;
    }
}

/*! \brief Handle SM->SM marshalling messages.
    
    Currently the only handling is to make the Primary/Secondary role switch
    decision.
*/
static void appSm_HandleMarshalledMsgChannelRxInd(PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND_T* ind)
{
    switch (ind->type)
    {
        /* State Proxy internal messages */
        case MARSHAL_TYPE_earbud_sm_msg_empty_payload_t:
            earbudSm_HandleMarshalledEmptyPayloadMsg((const earbud_sm_msg_empty_payload_t*)ind->msg);
            break;

        default:
            break;
    }

    /* free unmarshalled msg */
    free(ind->msg);
}

/*! \brief Use pairing activity to generate rule event to check for link keys that may need forwarding. */
static void appSm_HandlePairingActivity(const PAIRING_ACTIVITY_T* pha)
{
    if (pha->status == pairingCompleteVersionChanged)
    {
        appSmRulesSetEvent(RULE_EVENT_PEER_UPDATE_LINKKEYS);
    }
    else if (pha->status == pairingLinkKeyReceived)
    {
        appSmRulesSetEvent(RULE_EVENT_RX_HANDSET_LINKKEY);
    }
}

/*! \brief Handle rule action to update page scan settings. */
static void appSmHandleBleConnectionUpdate(const CONN_RULES_BLE_CONNECTION_UPDATE_T* update)
{
    ConManagerAllowConnection(cm_transport_ble, update->enable);

    DEBUG_LOGF("appSmHandleBleConnectionUpdate enable:%u Kicking peer sync", update->enable);
    /*! \todo need event based mechanism for notifying peer of BLE changes */
//  appPeerSyncSend(FALSE);
}


/*! \brief Idle timeout */
static void appSmHandleInternalTimeoutIdle(void)
{
    DEBUG_LOG("appSmHandleInternalTimeoutIdle");
    PanicFalse(APP_STATE_OUT_OF_CASE_IDLE == appGetState());
    appSetState(APP_STATE_OUT_OF_CASE_SOPORIFIC);
}

/*! \brief Handle rule action to update page scan settings. */
static void appSmHandleConnRulesPageScanUpdate(const CONN_RULES_PAGE_SCAN_UPDATE_T* crpsu)
{
    DEBUG_LOGF("appSmHandleConnRulesPageScanUpdate enable:%u", crpsu->enable);

    if (crpsu->enable)
        BredrScanManager_PageScanRequest(SmGetTask(), SCAN_MAN_PARAMS_TYPE_SLOW);
    else
        BredrScanManager_PageScanRelease(SmGetTask());
}

/*! \brief Handle command to pair with a handset. */
/*! \todo This trigger to do standard handset pairing from peer message could go direct
 *        to pairing component.
 *        Or if we want to keep TWS knowledge out of pairing then TWS topology could
 *        register to get it from peer signalling (or use alternate marshalling msg)
 *        and the topology calls a generic pairing API to kick pairing to known address */
static void appSmHandlePeerSigPairHandsetIndication(PEER_SIG_PAIR_HANDSET_IND_T *ind)
{
    smTaskData *sm = SmGetTaskData();

    DEBUG_LOGF("appSmHandlePeerSigPairHandsetIndication, bdaddr %04x,%02x,%06lx",
               ind->handset_addr.nap, ind->handset_addr.uap, ind->handset_addr.lap);

    /* Check if we already know about this handset */
    if (BtDevice_isKnownBdAddr(&ind->handset_addr))
    {
        /* Attempt to delete it, this will fail if we're connected */
        if (appDeviceDelete(&ind->handset_addr))
        {
            DEBUG_LOG("appSmHandlePeerSigPairHandsetIndication, known handset deleting and re-pairing");
            Pairing_PairAddress(&sm->task, &ind->handset_addr);
        }
        else
            DEBUG_LOG("appSmHandlePeerSigPairHandsetIndication, known handset, current connected so ignore pairing request");
    }
    else
    {
        DEBUG_LOG("appSmHandlePeerSigPairHandsetIndication, pairing with handset");
        Pairing_PairAddress(&sm->task, &ind->handset_addr);
    }
}

/*! \brief Handle command to connect to handset. */
static void appSmHandlePeerSigConnectHandsetIndication(PEER_SIG_CONNECT_HANDSET_IND_T *ind)
{
    /* Generate event that will run rules to connect to handset */
    rule_events_t event = ind->play_media ? RULE_EVENT_HANDOVER_RECONNECT_AND_PLAY
                                            : RULE_EVENT_HANDOVER_RECONNECT;
    appSmRulesSetEvent(event);
}

static void appSmHandleAvA2dpConnectedInd(const AV_A2DP_CONNECTED_IND_T *ind)
{
    DEBUG_LOG("appSmHandleAvA2dpConnectedInd %04x,%02x,%06lx", ind->bd_addr.nap, ind->bd_addr.uap, ind->bd_addr.lap);

    if (appDeviceIsHandset(&ind->bd_addr))
    {
        appSmRulesSetRuleComplete(CONN_RULES_CONNECT_HANDSET);
        appSmRulesResetEvent(RULE_EVENT_HANDSET_A2DP_DISCONNECTED);
        appSmRulesSetEvent(RULE_EVENT_HANDSET_A2DP_CONNECTED);
    }
}

static void appSmHandleAvA2dpDisconnectedInd(const AV_A2DP_DISCONNECTED_IND_T *ind)
{
    DEBUG_LOGF("appSmHandleAvA2dpDisconnectedInd, reason %u inst %x", ind->reason, ind->av_instance);

    switch (appGetState())
    {
        default:
        {
            if (appDeviceIsHandset(&ind->bd_addr))
            {
                /* Clear connected and set disconnected events */
                appSmRulesResetEvent(RULE_EVENT_HANDSET_A2DP_CONNECTED);
                appSmRulesSetEvent(RULE_EVENT_HANDSET_A2DP_DISCONNECTED);
            }
        }
        break;
    }
}

static void appSmHandleAvAvrcpConnectedInd(const AV_AVRCP_CONNECTED_IND_T *ind)
{
    DEBUG_LOG("appSmHandleAvAvrcpConnectedInd %04x,%02x,%06lx", ind->bd_addr.nap, ind->bd_addr.uap, ind->bd_addr.lap);

    if (appDeviceIsHandset(&ind->bd_addr))
    {
        appSmRulesResetEvent(RULE_EVENT_HANDSET_AVRCP_DISCONNECTED);
        appSmRulesSetEvent(RULE_EVENT_HANDSET_AVRCP_CONNECTED);
    }

    /* Check if connected to TWS+ device (handset or earbud) */
    if (appDeviceIsTwsPlusHandset(&ind->bd_addr) || appDeviceIsPeer(&ind->bd_addr))
    {
        DEBUG_LOG("appSmHandleAvAvrcpConnectedInd, using wallclock for UI synchronisation");

        /* Use wallclock on this connection to synchronise LEDs */
        appLedSetWallclock(ind->sink);
    }

    if (appDeviceIsPeer(&ind->bd_addr))
    {
#ifdef INCLUDE_SCOFWD
        /* Use wallclock on this connection to synchronise RINGs */
        ScoFwdSetWallclock(ind->sink);
#endif /* INCLUDE_SCOFWD */
        appSmRulesResetEvent(RULE_EVENT_PEER_DISCONNECTED);
        appSmRulesSetEvent(RULE_EVENT_PEER_CONNECTED);
    }
}

static void appSmHandleAvAvrcpDisconnectedInd(const AV_AVRCP_DISCONNECTED_IND_T *ind)
{
    DEBUG_LOG("appSmHandleAvAvrcpDisconnectedInd");

    switch (appGetState())
    {
        default:
        {
            if (appDeviceIsHandset(&ind->bd_addr))
            {
                appSmRulesResetEvent(RULE_EVENT_HANDSET_AVRCP_CONNECTED);
                appSmRulesSetEvent(RULE_EVENT_HANDSET_AVRCP_DISCONNECTED);
            }

            /* Check if disconnected from TWS+ device (handset or earbud) */
            if (appDeviceIsTwsPlusHandset(&ind->bd_addr) || appDeviceIsPeer(&ind->bd_addr))
            {
                /* Don't use wallclock */
                appLedSetWallclock(0);

            }

            if (appDeviceIsPeer(&ind->bd_addr))
            {
#ifdef INCLUDE_SCOFWD
                ScoFwdSetWallclock(0);
#endif /* INCLUDE_SCOFWD */
                appSmRulesResetEvent(RULE_EVENT_PEER_CONNECTED);
                appSmRulesSetEvent(RULE_EVENT_PEER_DISCONNECTED);
            }
        }
        break;
    }
}

static void appSmHandleAvStreamingActiveInd(void)
{
    DEBUG_LOGF("appSmHandleAvStreamingActiveInd state=0x%x", appGetState());

    /* We only care about this event if we're in a core state,
     * and it could be dangerous if we're not */
    if (appSmIsCoreState())
        appSmSetCoreState();
}

static void appSmHandleAvStreamingInactiveInd(void)
{
    DEBUG_LOGF("appSmHandleAvStreamingInactiveInd state=0x%x", appGetState());

    /* We only care about this event if we're in a core state,
     * and it could be dangerous if we're not */
    if (appSmIsCoreState())
        appSmSetCoreState();
}

static void appSmHandleHfpConnectedInd(APP_HFP_CONNECTED_IND_T *ind)
{
    DEBUG_LOG("appSmHandleHfpConnectedInd %04x,%02x,%06lx", ind->bd_addr.nap, ind->bd_addr.uap, ind->bd_addr.lap);

    if (appDeviceIsHandset(&ind->bd_addr))
    {
        appSmRulesSetRuleComplete( CONN_RULES_CONNECT_HANDSET);
        appSmRulesResetEvent(RULE_EVENT_HANDSET_HFP_DISCONNECTED);
        appSmRulesSetEvent(RULE_EVENT_HANDSET_HFP_CONNECTED);
    }
}

static void appSmHandleHfpDisconnectedInd(APP_HFP_DISCONNECTED_IND_T *ind)
{
    DEBUG_LOGF("appSmHandleHfpDisconnectedInd, reason %u", ind->reason);

    switch (appGetState())
    {
        default:
        {
            if (appDeviceIsHandset(&ind->bd_addr))
            {
                appSmRulesResetEvent(RULE_EVENT_HANDSET_HFP_CONNECTED);
                appSmRulesSetEvent(RULE_EVENT_HANDSET_HFP_DISCONNECTED);
            }
        }
        break;
    }
}

static void appSmHandleHfpScoConnectedInd(void)
{
    DEBUG_LOGF("appSmHandleHfpScoConnectedInd state=0x%x", appGetState());

    /* We only care about this event if we're in a core state,
     * and it could be dangerous if we're not */
    if (appSmIsCoreState())
        appSmSetCoreState();

    /* this was getting set by scofwd...
       evaluates whether to turn on SCO forwarding
       \todo decide if this is the correct course or if scofwd should
             have an event listened to by SM that sets this event. */
    appSmRulesSetEvent(RULE_EVENT_SCO_ACTIVE);
}

static void appSmHandleHfpScoDisconnectedInd(void)
{
    DEBUG_LOGF("appSmHandleHfpScoDisconnectedInd state=0x%x", appGetState());

    /* We only care about this event if we're in a core state,
     * and it could be dangerous if we're not */
    if (appSmIsCoreState())
        appSmSetCoreState();
}

static void appSmHandleShadowProfileConnectedInd(void)
{
    DEBUG_LOGF("appSmHandleShadowProfileConnectedInd state=0x%x", appGetState());

    /*  Shadow ACL connection established, Invoke Hdma_Init  */
    if (appSmIsPrimary())
        Hdma_Init();
 
}

static void appSmHandleShadowProfileDisconnectedInd(void)
{
    DEBUG_LOGF("appSmHandleShadowProfileDisconnectedInd state=0x%x", appGetState());

    /*  Shadow ACL connection disconnected, Invoke HDMA_Destroy  */
    if (appSmIsPrimary())
        Hdma_Destroy();


}

static void appSmHandleInternalPairHandset(void)
{
    if (appSmStateIsIdle(appGetState()))
    {
        appSmSetUserPairing();
        appSetState(APP_STATE_HANDSET_PAIRING);
    }
    else
        DEBUG_LOG("appSmHandleInternalPairHandset can only pair in IDLE state");
}

/*! \brief Delete pairing for all handsets.
    \note There must be no connections to a handset for this to succeed. */
static void appSmHandleInternalDeleteHandsets(void)
{
    DEBUG_LOG("appSmHandleInternalDeleteHandsets");

    switch (appGetState())
    {
        case APP_STATE_IN_CASE_IDLE:
        case APP_STATE_OUT_OF_CASE_IDLE:
        case APP_STATE_IN_EAR_IDLE:
        case APP_STATE_FACTORY_RESET:
        {
            BtDevice_DeleteAllHandsetDevices();
            break;
        }
        default:
            DEBUG_LOGF("appSmHandleInternalDeleteHandsets bad state %u",
                                                        appGetState());
            break;
    }
}

/*! \brief Handle request to start factory reset. */
static void appSmHandleInternalFactoryReset(void)
{
    if (appSmStateIsIdle(appGetState()))
        appSetState(APP_STATE_FACTORY_RESET);
    else
        DEBUG_LOG("appSmHandleInternalFactoryReset can only factory reset in IDLE state");
}

/*! \brief Handle failure to successfully disconnect links within timeout.
    Manually tear down the ACL and wait for #SM_INTERNAL_LINK_DISCONNECTION_COMPLETE.
*/
static void appSmHandleInternalLinkDisconnectionTimeout(void)
{
    bdaddr addr;

    DEBUG_LOG("appSmHandleInternalLinkDisconnectionTimeout");
    if (appSmDisconnectLockPeerIsDisconnecting())
    {
    if (appDeviceGetPeerBdAddr(&addr) && ConManagerIsConnected(&addr))
        {
            DEBUG_LOG("appSmHandleInternalLinkDisconnectionTimeout PEER IS STILL CONNECTED");
            ConManagerSendCloseAclRequest(&addr, TRUE);
        }
        else
        {
            appSmDisconnectLockClearLinks(SM_DISCONNECT_PEER);
        }
    }
    if (appSmDisconnectLockHandsetIsDisconnecting())
    {
        if (appDeviceGetHandsetBdAddr(&addr) && ConManagerIsConnected(&addr))
        {
            DEBUG_LOG("appSmHandleInternalLinkDisconnectionTimeout HANDSET IS STILL CONNECTED");
            ConManagerSendCloseAclRequest(&addr, TRUE);
        }
        else
        {
            appSmDisconnectLockClearLinks(SM_DISCONNECT_HANDSET);
        }
    }
}

#ifdef INCLUDE_DFU
/*! Change the state to DFU mode with an optional timeout

    \param timeoutMs Timeout (in ms) after which the DFU state
    may be exited
 */
static void appSmHandleEnterDfuWithTimeout(uint32 timeoutMs)
{
    MessageCancelAll(SmGetTask(), SM_INTERNAL_TIMEOUT_DFU_ENTRY);
    if (timeoutMs)
    {
        MessageSendLater(SmGetTask(), SM_INTERNAL_TIMEOUT_DFU_ENTRY, NULL, timeoutMs);
    }

    if (APP_STATE_IN_CASE_DFU != appGetState())
    {
        DEBUG_LOGF("appSmHandleEnterDfuWithTimeout. restarted SM_INTERNAL_TIMEOUT_DFU_ENTRY - %dms",timeoutMs);

        appSetState(APP_STATE_IN_CASE_DFU);
    }
    else
    {
        DEBUG_LOGF("appSmHandleEnterDfuWithTimeout. restarted SM_INTERNAL_TIMEOUT_DFU_ENTRY - %dms. Already in APP_STATE_IN_CASE_DFU",timeoutMs);
    }

    appSmRulesSetEvent(RULE_EVENT_DFU_CONNECT);
}

/*! \todo (very) temporary variable. This is required as 
        \li goals are not queued at present
        \li setting the event below raises a goal, while an existing one is running
 */
bool dfu_has_been_restarted = FALSE;

static void appSmHandleEnterDfuUpgraded(void)
{
    appSmHandleEnterDfuWithTimeout(appConfigDfuTimeoutToStartAfterRestartMs());

    dfu_has_been_restarted = TRUE;
        /*! \todo this should be handled in the device_upgrade files,
                or at least not call an Upgrade API */
    //TwsTopology_SelectPrimaryAddress(UpgradePeerIsPrimaryDevice());
}

static void appSmHandleEnterDfuStartup(void)
{
    appSmHandleEnterDfuWithTimeout(appConfigDfuTimeoutToStartAfterRebootMs());

    dfu_has_been_restarted = FALSE;
}

static void appSmHandleDfuEnded(bool error)
{
    DEBUG_LOGF("appSmHandleDfuEnded(%d)",error);

    ConManagerAllowHandsetConnect(FALSE);
    if (appGetState() == APP_STATE_IN_CASE_DFU)
    {
        appSetState(APP_STATE_STARTUP);
        if (error)
        {
            appGaiaDisconnect();
        }
    }
}


static void appSmHandleUgradePeerStarted(void)
{
    DEBUG_LOG("appSmHandleUgradePeerStarted");
    
    appSmNotifyUpgradeStarted();
}


static void appSmHandleUpgradeConnected(void)
{
    switch (appGetState())
    {
        case APP_STATE_IN_CASE_DFU:
            /* If we are in the special DFU case mode then start a fresh time
               for dfu mode as well as a timer to trap for an application that
               opens then closes the upgrade connection. */
            DEBUG_LOG("appSmHandleStartUpgrade, valid state to enter DFU");
            appSmStartDfuTimer();

            DEBUG_LOG("appSmHandleUpgradeConnected Start SM_INTERNAL_TIMEOUT_DFU_AWAIT_DISCONNECT %dms",
                                appConfigDfuTimeoutCheckForSupportMs());
            MessageSendLater(SmGetTask(), SM_INTERNAL_TIMEOUT_DFU_AWAIT_DISCONNECT,
                                NULL, appConfigDfuTimeoutCheckForSupportMs());
            break;

        default:
            /* In all other states and modes we don't need to do anything.
               Start of an actual upgrade will be blocked if needed,
               see appSmHandleDfuAllow() */
            break;
    }
}


static void appSmHandleUpgradeDisconnected(void)
{
    DEBUG_LOG("appSmHandleUpgradeDisconnected");

    if (MessageCancelAll(SmGetTask(), SM_INTERNAL_TIMEOUT_DFU_AWAIT_DISCONNECT))
    {
        DEBUG_LOG("appSmHandleUpgradeDisconnected Cancelled SM_INTERNAL_TIMEOUT_DFU_AWAIT_DISCONNECT timer");

        appSmStartDfuTimer();
    }

}

#endif


static void appSmHandleNoDfu(void)
{

    if (appGetState() != APP_STATE_IN_CASE_DFU)
    {
        DEBUG_LOG("appSmHandleNoDfu. Not gone into DFU, so safe to assume we can continue");

        appSetState(APP_STATE_STARTUP);
    }
    else
    {
        DEBUG_LOG("appSmHandleNoDfu. We are in DFU mode !");
    }
}

/*! \brief Reboot the earbud, no questions asked. */
static void appSmHandleInternalReboot(void)
{
    BootSetMode(BootGetMode());
}

/*! \brief Handle indication all requested links are now disconnected. */
static void appSmHandleInternalAllRequestedLinksDisconnected(SM_INTERNAL_LINK_DISCONNECTION_COMPLETE_T *msg)
{
    DEBUG_LOGF("appSmHandleInternalAllRequestedLinksDisconnected 0x%x", appGetState());
    MessageCancelFirst(SmGetTask(), SM_INTERNAL_TIMEOUT_LINK_DISCONNECTION);

    switch (msg->post_disconnect_action)
    {
        case POST_DISCONNECT_ACTION_HANDOVER_AND_PLAY_MEDIA:
        case POST_DISCONNECT_ACTION_HANDOVER:
        break;
        case POST_DISCONNECT_ACTION_HANDSET_PAIRING:
            Pairing_Pair(SmGetTask(), appSmIsUserPairing());
        break;

#if 0
        case POST_DISCONNECT_ACTION_SWITCH_TO_PRIMARY:
            appSm_CompleteSwitchRole(TRUE);
        break;
        case POST_DISCONNECT_ACTION_SWITCH_TO_SECONDARY:
            appSm_CompleteSwitchRole(FALSE);
        break;
#endif

        case POST_DISCONNECT_ACTION_NONE:
        default:
        break;
    }

    switch (appGetState())
    {
        case APP_STATE_OUT_OF_CASE_SOPORIFIC_TERMINATING:
            appPowerSleepPrepareResponse(SmGetTask());
        break;
        case APP_STATE_FACTORY_RESET:
            appSmDeletePairingAndReset();
        break;
        default:
            switch (appGetSubState())
            {
                case APP_SUBSTATE_TERMINATING:
                    appPowerShutdownPrepareResponse(SmGetTask());
                    break;
                case APP_SUBSTATE_DISCONNECTING:
                    appSmSetCoreState();
                    break;
                default:
                    break;
            }
        break;
    }
}

static void appSm_HandleTwsTopologyRoleChange(tws_topology_role new_role)
{
    switch (new_role)
    {
        case tws_topology_role_none:
            DEBUG_LOG("appSm_HandleTwsTopologyRoleChange NONE");
            SmGetTaskData()->role = tws_topology_role_none;
            SecondaryRules_ResetEvent(RULE_EVENT_ALL_EVENTS_MASK);
            PrimaryRules_ResetEvent(RULE_EVENT_ALL_EVENTS_MASK);
            break;

        case tws_topology_role_primary:
            DEBUG_LOG("appSm_HandleTwsTopologyRoleChange PRIMARY");
//            appSm_SwitchRole(TRUE);
            SmGetTaskData()->role = tws_topology_role_primary;
            LogicalInputSwitch_SetRerouteToPeer(FALSE);
            StateProxy_SetRole(TRUE);

            PrimaryRules_SetEvent(SmGetTask(), RULE_EVENT_ROLE_SWITCH);
            if (appPhyStateGetState() != PHY_STATE_IN_CASE)
            {
                PrimaryRules_SetEvent(SmGetTask(), RULE_EVENT_OUT_CASE);
            }
            break;

        case tws_topology_role_secondary:
            DEBUG_LOG("appSm_HandleTwsTopologyRoleChange SECONDARY");
//            appSm_SwitchRole(FALSE);
            SmGetTaskData()->role = tws_topology_role_secondary;
            LogicalInputSwitch_SetRerouteToPeer(TRUE);
            StateProxy_SetRole(FALSE);

            SecondaryRules_SetEvent(SmGetTask(), RULE_EVENT_ROLE_SWITCH);
            if (appPhyStateGetState() != PHY_STATE_IN_CASE)
            {
                SecondaryRules_SetEvent(SmGetTask(), RULE_EVENT_OUT_CASE);
            }
            break;

        case tws_topology_role_dfu:
            DEBUG_LOG("appSm_HandleTwsTopologyRoleChange DFU. Make no changes");
            if (dfu_has_been_restarted)
            {
                TwsTopology_SelectPrimaryAddress(UpgradePeerIsPrimaryDevice());
            }
            break;

        default:
            break;
    }
}

static void appSm_HandleTwsTopologyStartCfm(TWS_TOPOLOGY_START_CFM_T* cfm)
{
    DEBUG_LOG("appSm_HandleTwsTopologyStartCfm ********");
    DEBUG_LOGF("appSm_HandleTwsTopologyStartCfm sts %u role %u", cfm->status, cfm->role);
    DEBUG_LOG("appSm_HandleTwsTopologyStartCfm ********");

    if (appGetState() == APP_STATE_STARTUP)
    {
        /* topology up and running, SM can move on from STARTUP state */
        appSmSetInitialCoreState();

        if (cfm->status == tws_topology_status_success)
        {
            appSm_HandleTwsTopologyRoleChange(cfm->role);
        }
    }
    else
    {
        DEBUG_LOGF("appSm_HandleTwsTopologyStartCfm unexpected state %u", appGetState());
    }
}

static void appSm_HandleTwsTopologyRoleChangedInd(TWS_TOPOLOGY_ROLE_CHANGED_IND_T* ind)
{
    DEBUG_LOG("appSm_HandleTwsTopologyRoleChangedInd ********");
    DEBUG_LOGF("appSm_HandleTwsTopologyRoleChangedInd role %u", ind->role);
    DEBUG_LOG("appSm_HandleTwsTopologyRoleChangedInd ********");
    appSm_HandleTwsTopologyRoleChange(ind->role);
}


static void appSmHandleDfuAllow(const CONN_RULES_DFU_ALLOW_T *dfu)
{
#ifdef INCLUDE_DFU
    appUpgradeAllowUpgrades(dfu->enable);
#else
    UNUSED(dfu);
#endif
}


static void appSmHandleInternalBredrConnected(void)
{
    DEBUG_LOG("appSmHandleInternalBredrConnected");

    /* Kick the state machine to see if this changes DFU state.
       A BREDR connection for upgrade can try an upgrade before we
       allow it. Always evaluate events do not need resetting. */
    appSmRulesSetEvent(RULE_EVENT_CHECK_DFU);
}


/*! \brief provides Earbud Application state machine context to the User Interface module.

    \param[in]  void

    \return     current_sm_ctxt - current context of sm module.
*/
static unsigned earbudSm_GetCurrentContext(void)
{
    sm_provider_context_t context = BAD_CONTEXT;

    if (appSmIsOutOfCase())
    {
        context = context_sm_out_of_case;
    }
    else
    {
        context = context_sm_in_case;
    }

    return (unsigned)context;
}

/*! \brief handles sm module specific ui inputs

    Invokes routines based on ui input received from ui module.

    \param[in] id - ui input

    \returns void
 */
static void appSmHandleUiInput(MessageId  ui_input)
{
    switch (ui_input)
    {
        case ui_input_connect_handset:
            appSmConnectHandset();
            break;
        case ui_input_sm_pair_handset:
            appSmPairHandset();
            break;
        case ui_input_sm_delete_handsets:
            appSmDeleteHandsets();
            break;
        case ui_input_factory_reset_request:
            earbudSm_SendCommandToPeer(earbud_sm_req_factory_reset);
            /* The factory reset shall be applied locally once the peer has been commanded to reset.
             * N.b. if transimission of this command fails (e.g. single Earbud in use), the reset
             * shall still be applied locally. */
            break;
        case ui_input_dfu_active_when_in_case_request:
            appSmEnterDfuModeInCase(TRUE);
            earbudSm_SendCommandToPeer(earbud_sm_req_dfu_active_when_in_case);
            break;
        default:
            break;
    }
}

static void appSmGeneratePhyStateRulesEvents(phy_state_event event)
{
    switch(event)
    {
        case phy_state_event_in_case:
            appSmRulesSetEvent(RULE_EVENT_PEER_IN_CASE);
            break;
        case phy_state_event_out_of_case:
            appSmRulesSetEvent(RULE_EVENT_PEER_OUT_CASE);
            break;
        case phy_state_event_in_ear:
            appSmRulesSetEvent(RULE_EVENT_PEER_IN_EAR);
            break;
        case phy_state_event_out_of_ear:
            appSmRulesSetEvent(RULE_EVENT_PEER_OUT_EAR);
            break;
        default:
            DEBUG_LOG("appSmGeneratePhyStateRulesEvents unhandled event %u", event);
            break;
    }
}

static void appSmGenerateA2dpRulesEvents(bool connected)
{
    if (!StateProxy_IsPeerHandsetAvrcpConnected() &&
        !StateProxy_IsPeerHandsetHfpConnected())
    {
        appSmRulesSetEvent(connected ? RULE_EVENT_PEER_HANDSET_CONNECTED :
                                         RULE_EVENT_PEER_HANDSET_DISCONNECTED);
    }
}

static void appSmGenerateAvrcpRulesEvents(bool connected)
{
    if (!StateProxy_IsPeerHandsetA2dpConnected() &&
        !StateProxy_IsPeerHandsetHfpConnected())
    {
        appSmRulesSetEvent(connected ? RULE_EVENT_PEER_HANDSET_CONNECTED :
                                         RULE_EVENT_PEER_HANDSET_DISCONNECTED);
    }
}

static void appSmGenerateHfpRulesEvents(bool connected)
{
    if (!StateProxy_IsPeerHandsetA2dpConnected() &&
        !StateProxy_IsPeerHandsetAvrcpConnected())
    {
        appSmRulesSetEvent(connected ? RULE_EVENT_PEER_HANDSET_CONNECTED :
                                         RULE_EVENT_PEER_HANDSET_DISCONNECTED);
    }
}

static void appSmGeneratePairingRulesEvents(PAIRING_ACTIVITY_T* pha)
{
    if (pha->status == pairingInProgress)
    {
        appSmRulesSetEvent(RULE_EVENT_PEER_PAIRING);
    }
    else
    {
        appSmRulesResetEvent(RULE_EVENT_PEER_PAIRING);
    }
}

static void appSmHandleStateProxyEvent(const STATE_PROXY_EVENT_T* sp_event)
{
    DEBUG_LOG("appSmHandleStateProxyEvent source %u type %u", sp_event->source, sp_event->type);
    switch(sp_event->type)
    {
        case state_proxy_event_type_phystate:
        {
            if (sp_event->source == state_proxy_source_remote)
            {
                DEBUG_LOG("appSmHandleStateProxyEvent phystate %u", sp_event->event.phystate);
                appSmGeneratePhyStateRulesEvents(sp_event->event.phystate.event);
            }
        }
        break;
        case state_proxy_event_type_a2dp_conn: 
            appSmGenerateA2dpRulesEvents(TRUE);
            break;
        case state_proxy_event_type_a2dp_discon: 
            appSmGenerateA2dpRulesEvents(FALSE);
            break;
        case state_proxy_event_type_avrcp_conn: 
            appSmGenerateAvrcpRulesEvents(TRUE);
            break;
        case state_proxy_event_type_avrcp_discon: 
            appSmGenerateAvrcpRulesEvents(FALSE);
            break;
        case state_proxy_event_type_hfp_conn: 
            appSmGenerateHfpRulesEvents(TRUE);
            break;
        case state_proxy_event_type_hfp_discon: 
            appSmGenerateHfpRulesEvents(FALSE);
            break;
        case state_proxy_event_type_a2dp_supported:
            appSmRulesSetEvent(RULE_EVENT_PEER_A2DP_SUPPORTED);
            break;
        case state_proxy_event_type_avrcp_supported:
            appSmRulesSetEvent(RULE_EVENT_PEER_AVRCP_SUPPORTED);
            break;
        case state_proxy_event_type_hfp_supported:
            appSmRulesSetEvent(RULE_EVENT_PEER_HFP_SUPPORTED);
            break;
        case state_proxy_event_type_handset_linkloss:
            appSmRulesSetEvent(RULE_EVENT_PEER_HANDSET_LINK_LOSS);
            break;
        case state_proxy_event_type_pairing:
            if (sp_event->source == state_proxy_source_remote)
            {
                appSmGeneratePairingRulesEvents(&sp_event->event.handset_activity);
            }
            break;
        default:
            break;
    }
}

/*! \brief Enable ANC hardware module */
static void appSmHandleConnRulesAncEnable(void)
{
    DEBUG_LOG("appSmHandleConnRulesAncEnable ANC Enable");

    /* Inject UI input ANC Enable */
    Ui_InjectUiInput(ui_input_anc_on);

    appSmRulesSetRuleComplete(CONN_RULES_ANC_ENABLE);
}

/*! \brief Disable ANC hardware module */
static void appSmHandleConnRulesAncDisable(void)
{
    DEBUG_LOG("appSmHandleConnRulesAncDisable ANC Disable");

    /* Inject UI input ANC Disable */
    Ui_InjectUiInput(ui_input_anc_off);

    appSmRulesSetRuleComplete(CONN_RULES_ANC_DISABLE);
}

/*! \brief Enter ANC tuning mode */
static void appSmHandleConnRulesAncEnterTuning(void)
{
    DEBUG_LOG("appSmHandleConnRulesAncEnterTuning ANC Enter Tuning Mode");

    /* Inject UI input ANC enter tuning */
    Ui_InjectUiInput(ui_input_anc_enter_tuning_mode);

    appSmRulesSetRuleComplete(CONN_RULES_ANC_TUNING_START);
}

/*! \brief Exit ANC tuning mode */
static void appSmHandleConnRulesAncExitTuning(void)
{
    DEBUG_LOG("appSmHandleConnRulesAncExitTuning ANC Exit Tuning Mode");

    /* Inject UI input ANC exit tuning */
    Ui_InjectUiInput(ui_input_anc_exit_tuning_mode);

    appSmRulesSetRuleComplete(CONN_RULES_ANC_TUNING_STOP);
}

/*! \brief Application state machine message handler. */
void appSmHandleMessage(Task task, MessageId id, Message message)
{
    smTaskData* sm = (smTaskData*)task;

    if (isMessageUiInput(id))
    {
        appSmHandleUiInput(id);
        return;
    }

    switch (id)
    {
        case APPS_COMMON_INIT_CFM:
            appSmHandleInitConfirm();
            break;

        /* Pairing completion confirmations */
        case PAIRING_PAIR_CFM:
            appSmHandlePairingPairConfirm((PAIRING_PAIR_CFM_T *)message);
            break;

        /* Connection manager status indications */
        case CON_MANAGER_CONNECTION_IND:
            appSmHandleConManagerConnectionInd((CON_MANAGER_CONNECTION_IND_T*)message);
            break;

        /* AV status change indications */
        case AV_A2DP_CONNECTED_IND:
            appSmHandleAvA2dpConnectedInd((AV_A2DP_CONNECTED_IND_T *)message);
            break;
        case AV_A2DP_DISCONNECTED_IND:
            appSmHandleAvA2dpDisconnectedInd((AV_A2DP_DISCONNECTED_IND_T *)message);
            break;
        case AV_AVRCP_CONNECTED_IND:
            appSmHandleAvAvrcpConnectedInd((AV_AVRCP_CONNECTED_IND_T *)message);
            break;
        case AV_AVRCP_DISCONNECTED_IND:
            appSmHandleAvAvrcpDisconnectedInd((AV_AVRCP_DISCONNECTED_IND_T *)message);
            break;
        case AV_STREAMING_ACTIVE_IND:
            appSmHandleAvStreamingActiveInd();
            break;
        case AV_STREAMING_INACTIVE_IND:
            appSmHandleAvStreamingInactiveInd();
            break;

        /* HFP status change indications */
        case APP_HFP_CONNECTED_IND:
            appSmHandleHfpConnectedInd((APP_HFP_CONNECTED_IND_T *)message);
            break;
        case APP_HFP_DISCONNECTED_IND:
            appSmHandleHfpDisconnectedInd((APP_HFP_DISCONNECTED_IND_T *)message);
            break;
        case APP_HFP_SCO_CONNECTED_IND:
            appSmHandleHfpScoConnectedInd();
            break;
        case APP_HFP_SCO_DISCONNECTED_IND:
            appSmHandleHfpScoDisconnectedInd();
            break;
        case SHADOW_PROFILE_CONNECT_IND:
            appSmHandleShadowProfileConnectedInd();
            break;
        case SHADOW_PROFILE_DISCONNECT_IND:
            appSmHandleShadowProfileDisconnectedInd();
            break;

        /* Physical state changes */
        case PHY_STATE_CHANGED_IND:
            appSmHandlePhyStateChangedInd(sm, (PHY_STATE_CHANGED_IND_T*)message);
            break;

        /* Power indications */
        case APP_POWER_SLEEP_PREPARE_IND:
            appSmHandlePowerSleepPrepareInd();
            break;
        case APP_POWER_SLEEP_CANCELLED_IND:
            appSmHandlePowerSleepCancelledInd();
            break;
        case APP_POWER_SHUTDOWN_PREPARE_IND:
            appSmHandlePowerShutdownPrepareInd();
            break;
        case APP_POWER_SHUTDOWN_CANCELLED_IND:
            appSmHandlePowerShutdownCancelledInd();
            break;

        /* Connection rules */
        case CONN_RULES_PEER_PAIR:
            appSmHandleConnRulesPeerPair();
            break;
        case CONN_RULES_PEER_SEND_LINK_KEYS:
            appSmHandleConnRulesForwardLinkKeys();
            break;
        case CONN_RULES_CONNECT_HANDSET:
            appSmHandleConnRulesConnectHandset((CONN_RULES_CONNECT_HANDSET_T*)message);
            break;
        case CONN_RULES_CONNECT_PEER_SIGNALLING:
            appSmHandleConnRulesConnectPeerSignalling();
            break;
        case CONN_RULES_CONNECT_PEER:
            appSmHandleConnRulesConnectPeer((CONN_RULES_CONNECT_PEER_T*)message);
            break;
        case CONN_RULES_CONNECT_PEER_HANDSET:
            appSmHandleConnRulesConnectPeerHandset((CONN_RULES_CONNECT_PEER_HANDSET_T*)message);
            break;
        case CONN_RULES_UPDATE_MRU_PEER_HANDSET:
            appSmHandleConnRulesUpdateMruPeerHandset();
            break;
        case CONN_RULES_SEND_STATE_TO_HANDSET:
            appSmHandleConnRulesSendStatusToHandset();
            break;
        case CONN_RULES_A2DP_TIMEOUT:
            appSmHandleConnRulesA2dpTimeout();
            break;
        case CONN_RULES_A2DP_TIMEOUT_CANCEL:
            appSmCancelOutOfEarTimers();
            break;
        case CONN_RULES_MEDIA_PLAY:
            appSmHandleConnRulesMediaPlay();
            break;
        case CONN_RULES_SCO_TIMEOUT:
            appSmHandleConnRulesScoTimeout();
            break;
        case CONN_RULES_SCO_TRANSFER_TO_EARBUD:
            appSmHandleConnRulesScoTransferToEarbud();
            break;
        case CONN_RULES_SCO_TRANSFER_TO_HANDSET:
            appSmHandleConnRulesScoTransferToHandset();
            break;
        case CONN_RULES_LED_ENABLE:
            appSmHandleConnRulesLedEnable();
            break;
        case CONN_RULES_LED_DISABLE:
            appSmHandleConnRulesLedDisable();
            break;
        case CONN_RULES_DISCONNECT_HANDSET:
            appSmHandleConnRulesHandsetDisconnect((CONN_RULES_DISCONNECT_HANDSET_T*)message);
            break;
        case CONN_RULES_DISCONNECT_PEER:
            appSmHandleConnRulesPeerDisconnect();
            break;
        case CONN_RULES_HANDSET_PAIR:
            appSmHandleConnRulesHandsetPair();
            break;
        case CONN_RULES_ENTER_DFU:
            appSmHandleConnRulesEnterDfu();
            break;
        case CONN_RULES_ALLOW_HANDSET_CONNECT:
            appSmHandleConnRulesAllowHandsetConnect();
            break;
        case CONN_RULES_REJECT_HANDSET_CONNECT:
            appSmHandleConnRulesRejectHandsetConnect();
            break;
        case CONN_RULES_PAGE_SCAN_UPDATE:
            appSmHandleConnRulesPageScanUpdate((CONN_RULES_PAGE_SCAN_UPDATE_T*)message);
            break;
        case CONN_RULES_SEND_PEER_SCOFWD_CONNECT:
#ifdef INCLUDE_SCOFWD
            appSmHandleConnRulesSendPeerScofwdConnect();
#endif
            break;
        case CONN_RULES_SELECT_MIC:
            appSmHandleConnRulesSelectMic((CONN_RULES_SELECT_MIC_T*)message);
            break;
        case CONN_RULES_SCO_FORWARDING_CONTROL:
            appSmHandleConnRulesScoForwardingControl((CONN_RULES_SCO_FORWARDING_CONTROL_T*)message);
            break;
        case CONN_RULES_ANC_ENABLE:
            appSmHandleConnRulesAncEnable();
            break;
        case CONN_RULES_ANC_DISABLE:
            appSmHandleConnRulesAncDisable();
            break;
        case CONN_RULES_ANC_TUNING_START:
            appSmHandleConnRulesAncEnterTuning();
            break;
        case CONN_RULES_ANC_TUNING_STOP:
            appSmHandleConnRulesAncExitTuning();
            break;

        case CONN_RULES_BLE_CONNECTION_UPDATE:
            appSmHandleBleConnectionUpdate((const CONN_RULES_BLE_CONNECTION_UPDATE_T *)message);
            break;

        case CONN_RULES_DFU_ALLOW:
            appSmHandleDfuAllow((const CONN_RULES_DFU_ALLOW_T*)message);
            break;

        case SEC_CONN_RULES_ENTER_DFU:
            appSmHandleConnRulesEnterDfu();
            EarbudSm_HandleDfuNotifyPrimary();
            break;

        /* Peer signalling messages */
        case PEER_SIG_PAIR_HANDSET_IND:
            appSmHandlePeerSigPairHandsetIndication((PEER_SIG_PAIR_HANDSET_IND_T*)message);
            break;
        case PEER_SIG_CONNECT_HANDSET_IND:
            appSmHandlePeerSigConnectHandsetIndication((PEER_SIG_CONNECT_HANDSET_IND_T*)message);
            break;
        case PEER_SIG_CONNECT_HANDSET_CFM:
            break;

        /* TWS Topology messages */
        case TWS_TOPOLOGY_START_CFM:
            appSm_HandleTwsTopologyStartCfm((TWS_TOPOLOGY_START_CFM_T*)message);
            break;

        case TWS_TOPOLOGY_ROLE_CHANGED_IND:
            appSm_HandleTwsTopologyRoleChangedInd((TWS_TOPOLOGY_ROLE_CHANGED_IND_T*)message);
            break;

#ifdef INCLUDE_DFU
        /* Messages from GAIA */
        case APP_GAIA_UPGRADE_ACTIVITY:
            /* We do not monitor activity at present.
               Might be a use to detect long upgrade stalls without a disconnect */
            break;

        case APP_GAIA_DISCONNECTED:
            appSmHandleDfuEnded(FALSE);
            break;

        case APP_GAIA_UPGRADE_CONNECTED:
            appSmHandleUpgradeConnected();
            break;

        case APP_GAIA_UPGRADE_DISCONNECTED:
            appSmHandleUpgradeDisconnected();
            break;

        /* Messages from UPGRADE */
        case APP_UPGRADE_REQUESTED_TO_CONFIRM:
            appSmEnterDfuOnStartup(TRUE);
            break;

        case APP_UPGRADE_REQUESTED_IN_PROGRESS:
            appSmEnterDfuOnStartup(TRUE);
            break;

        case APP_UPGRADE_ACTIVITY:
            /* We do not monitor activity at present.
               Might be a use to detect long upgrade stalls without a disconnect */
            break;

        case APP_UPGRADE_STARTED:
            appSmNotifyUpgradeStarted();
            break;

        case APP_UPGRADE_COMPLETED:
            appSmHandleDfuEnded(FALSE);
            GattServerGatt_SetGattDbChanged();
            break;

        case DEVICE_UPGRADE_PEER_STARTED:
            appSmNotifyUpgradeStarted();
            break;
#endif /* INCLUDE_DFU */

        case STATE_PROXY_EVENT:
            appSmHandleStateProxyEvent((const STATE_PROXY_EVENT_T*)message);
            break;

        /* SM internal messages */
        case SM_INTERNAL_PAIR_HANDSET:
            appSmHandleInternalPairHandset();
            break;
        case SM_INTERNAL_DELETE_HANDSETS:
            appSmHandleInternalDeleteHandsets();
            break;
        case SM_INTERNAL_FACTORY_RESET:
            appSmHandleInternalFactoryReset();
            break;
        case SM_INTERNAL_TIMEOUT_LINK_DISCONNECTION:
            appSmHandleInternalLinkDisconnectionTimeout();
            break;

#ifdef INCLUDE_DFU
        case SM_INTERNAL_ENTER_DFU_UI:
            appSmHandleEnterDfuWithTimeout(appConfigDfuTimeoutAfterEnteringCaseMs());
            break;

        case SM_INTERNAL_ENTER_DFU_UPGRADED:
            appSmHandleEnterDfuUpgraded();
            break;

        case SM_INTERNAL_ENTER_DFU_STARTUP:
            appSmHandleEnterDfuStartup();
            break;

        case SM_INTERNAL_TIMEOUT_DFU_ENTRY:
            DEBUG_LOG("appSmHandleMessage SM_INTERNAL_TIMEOUT_DFU_ENTRY");

            appSmHandleDfuEnded(TRUE);
            break;

        case SM_INTERNAL_TIMEOUT_DFU_MODE_START:
            DEBUG_LOG("appSmHandleMessage SM_INTERNAL_TIMEOUT_DFU_MODE_START");

            appSmEnterDfuModeInCase(FALSE);
            break;

        case SM_INTERNAL_TIMEOUT_DFU_AWAIT_DISCONNECT:
            /* No action needed for this timer */
            DEBUG_LOG("appSmHandleMessage SM_INTERNAL_TIMEOUT_DFU_AWAIT_DISCONNECT");
            break;
#endif

        case SM_INTERNAL_NO_DFU:
            appSmHandleNoDfu();
            break;

        case SM_INTERNAL_TIMEOUT_OUT_OF_EAR_A2DP:
            appSmHandleInternalTimeoutOutOfEarA2dp();
            break;

        case SM_INTERNAL_TIMEOUT_IN_EAR_A2DP_START:
            appSmHandleInternalTimeoutInEarA2dpStart();
            break;

        case SM_INTERNAL_TIMEOUT_OUT_OF_EAR_SCO:
            appSmHandleInternalTimeoutOutOfEarSco();
            break;

        case SM_INTERNAL_TIMEOUT_IDLE:
            appSmHandleInternalTimeoutIdle();
            break;

        case SM_INTERNAL_REBOOT:
            appSmHandleInternalReboot();
            break;

        case SM_INTERNAL_LINK_DISCONNECTION_COMPLETE:
            appSmHandleInternalAllRequestedLinksDisconnected((SM_INTERNAL_LINK_DISCONNECTION_COMPLETE_T *)message);
            break;

        case SM_INTERNAL_BREDR_CONNECTED:
            appSmHandleInternalBredrConnected();
            break;

#if 0
        case SM_INTERNAL_TIMEOUT_ADDR_SWITCH_RETRY:
            appSm_SwitchAddress(((SM_INTERNAL_ADDR_SWITCH_DELAY_T*)message)->primary);
            break;
#endif

            /* marshalled messaging */
        case PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND:
            appSm_HandleMarshalledMsgChannelRxInd((PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND_T*)message);
            break;
        case PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM:
            appSm_HandleMarshalledMsgChannelTxCfm((PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM_T*)message);
            break;

        case PAIRING_ACTIVITY:
            appSm_HandlePairingActivity((PAIRING_ACTIVITY_T*)message);
            break;

        default:
            appHandleUnexpected(id);
            break;
    }
}

bool appSmIsInEar(void)
{
    smTaskData *sm = SmGetTaskData();
    return sm->phy_state == PHY_STATE_IN_EAR;
}

bool appSmIsOutOfEar(void)
{
    smTaskData *sm = SmGetTaskData();
    return (sm->phy_state >= PHY_STATE_IN_CASE) && (sm->phy_state <= PHY_STATE_OUT_OF_EAR_AT_REST);
}

bool appSmIsInCase(void)
{
    smTaskData *sm = SmGetTaskData();
    return (sm->phy_state == PHY_STATE_IN_CASE) || (sm->phy_state == PHY_STATE_UNKNOWN);
}

bool appSmIsOutOfCase(void)
{
    smTaskData *sm = SmGetTaskData();
    return (sm->phy_state >= PHY_STATE_OUT_OF_EAR) && (sm->phy_state <= PHY_STATE_IN_EAR);
}

bool appSmIsConnectable(void)
{
    return appSmStateIsConnectable(appGetState());
}

/*! \todo Need a mechanism to know that an upgrade is in progress. Initial check suggests that this is harder than you'd think. */
bool appSmIsDfuPending(void)
{
#ifdef INCLUDE_DFU
    return (   SmGetTaskData()->enter_dfu_in_case
           || APP_STATE_IN_CASE_DFU == appGetState());
#else
    return FALSE;
#endif
}


void appSmPairHandset(void)
{
    smTaskData *sm = SmGetTaskData();
    MessageSend(&sm->task, SM_INTERNAL_PAIR_HANDSET, NULL);
}

void appSmDeleteHandsets(void)
{
    smTaskData *sm = SmGetTaskData();
    MessageSend(&sm->task, SM_INTERNAL_DELETE_HANDSETS, NULL);
}

#ifdef INCLUDE_DFU
void appSmEnterDfuMode(void)
{
    MessageSend(SmGetTask(),SM_INTERNAL_ENTER_DFU_UI, NULL);
}

static void appSmStartDfuTimer(void)
{
    PrimaryRules_SetEvent(SmGetTask(), RULE_EVENT_PAGE_SCAN_UPDATE);
    ConManagerAllowHandsetConnect(TRUE);
    appSmHandleEnterDfuWithTimeout(appConfigDfuTimeoutAfterGaiaConnectionMs());
}

void appSmEnterDfuModeInCase(bool enable)
{
    MessageCancelAll(SmGetTask(), SM_INTERNAL_TIMEOUT_DFU_MODE_START);
    SmGetTaskData()->enter_dfu_in_case = enable;
    ConManagerAllowHandsetConnect(TRUE);

    if (enable)
    {
        DEBUG_LOG("appSmEnterDfuModeInCase (re)start SM_INTERNAL_TIMEOUT_DFU_MODE_START %dms",
                            appConfigDfuTimeoutToPlaceInCaseMs());
        MessageSendLater(SmGetTask(), SM_INTERNAL_TIMEOUT_DFU_MODE_START,
                            NULL, appConfigDfuTimeoutToPlaceInCaseMs());

        /*! We will be entering DFU mode. Make sure that the topology is aware */
        TwsTopology_SwitchToDfuRole();
    }
}


/*! \brief Tell the state machine to enter DFU mode following a reboot.
    \param upgrade_reboot If TRUE, indicates that DFU triggered the reboot.
                          If FALSE, indicates the device was rebooted whilst
                          an upgrade was in progress.
*/
static void appSmEnterDfuOnStartup(bool upgrade_reboot)
{
    MessageSend(SmGetTask(),
                upgrade_reboot ? SM_INTERNAL_ENTER_DFU_UPGRADED
                               : SM_INTERNAL_ENTER_DFU_STARTUP,
                NULL);
}


/*! \brief Notification to the state machine of Upgrade start */
static void appSmNotifyUpgradeStarted(void)
{
    appSmCancelDfuTimers();
}
#endif /* INCLUDE_DFU */

/*! \brief provides state manager(sm) task to other components

    \param[in]  void

    \return     Task - sm task.
*/
Task SmGetTask(void)
{
  return &app_sm.task;
}

/*! \brief Initialise the main application state machine.
 */
bool appSmInit(Task init_task)
{
    smTaskData* sm = SmGetTaskData();

    memset(sm, 0, sizeof(*sm));
    sm->task.handler = appSmHandleMessage;
    sm->state = APP_STATE_NULL;
    sm->phy_state = appPhyStateGetState();

    /* configure rule sets */
    sm->primary_rules = PrimaryRules_GetRuleSet();
    sm->secondary_rules = SecondaryRules_GetRuleSet();
    sm->role = tws_topology_role_none;

    /* register with connection manager to get notification of (dis)connections */
    ConManagerRegisterConnectionsClient(&sm->task);

    /* register with HFP for changes in state */
    appHfpStatusClientRegister(&sm->task);

    /* register with AV to receive notifications of A2DP and AVRCP activity */
    appAvStatusClientRegister(&sm->task);

    /* register with peer signalling to receive handset commands */
    appPeerSigHandsetCommandsTaskRegister(&sm->task);

    /* register with power to receive sleep/shutdown messages. */
    appPowerClientRegister(&sm->task);

    /* set 'always run' events.
       setting the event isn't required, but it sets the task of the SM
       as the one to send the rule action message to. */
    PrimaryRules_SetEvent(SmGetTask(), RULE_EVENT_PAGE_SCAN_UPDATE);

    /* get remote phy state events */
    StateProxy_EventRegisterClient(&sm->task, appConfigStateProxyRegisteredEventsDefault() |
                                              appConfigStateProxyRegisteredEventsOptional());

    /* Register with peer signalling to use the State Proxy msg channel */
    appPeerSigMarshalledMsgChannelTaskRegister(SmGetTask(), 
                                               PEER_SIG_MSG_CHANNEL_APPLICATION,
                                               earbud_sm_marshal_type_descriptors,
                                               NUMBER_OF_MARSHAL_OBJECT_TYPES);

    /* Register to get pairing activity reports */
    Pairing_ActivityClientRegister(&sm->task);

    /* Register for role changed indications from TWS Topology */
    TwsTopology_RoleChangedRegisterClient(SmGetTask());

    /* Register for connect / disconnect events from shadow profile */
    ShadowProfile_ClientRegister(SmGetTask());
    
    appSetState(APP_STATE_INITIALISING);

    /* Register sm as ui provider*/
    Ui_RegisterUiProvider(ui_provider_sm,earbudSm_GetCurrentContext);

    Ui_RegisterUiInputConsumer(SmGetTask(), sm_ui_inputs, ARRAY_DIM(sm_ui_inputs));

    UNUSED(init_task);
    return TRUE;
}

void appSmConnectHandset(void)
{
    bdaddr handset_addr = {0};
    if (appDeviceGetHandsetBdAddr(&handset_addr))
    {
        uint8 profiles_to_connect = BtDevice_GetLastConnectedProfiles(&handset_addr);

        if (!profiles_to_connect)
        {
            device_t handset_device = BtDevice_GetDeviceForBdAddr(&handset_addr);
            PanicNull(handset_device);
            Device_GetPropertyU8(handset_device,device_property_supported_profiles, &profiles_to_connect);
        }

        DEBUG_LOG("appSmConnectHandset lap=%6x profiles=%02x", handset_addr.lap, profiles_to_connect);

        HandsetService_ConnectAddressRequest(SmGetTask(), &handset_addr, profiles_to_connect);
    }
}

/*! \brief Request a factory reset. */
void appSmFactoryReset(void)
{
    MessageSend(SmGetTask(), SM_INTERNAL_FACTORY_RESET, NULL);
}

/*! \brief Reboot the earbud. */
extern void appSmReboot(void)
{
    /* Post internal message to action the reboot, this breaks the call
     * chain and ensures the test API will return and not break. */
    MessageSend(SmGetTask(), SM_INTERNAL_REBOOT, NULL);
}

/*! \brief Start earbud handset handover */
extern void appSmInitiateHandover(void)
{
    appSmRulesSetEvent(RULE_EVENT_HANDOVER_DISCONNECT);
}

/*! \brief Determine if this Earbud is Primary or Secondary.
    \todo this will move to topology.
*/
bool appSmIsPrimary(void)
{
    return SmGetTaskData()->role == tws_topology_role_primary;
}
