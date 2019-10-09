/*!
\copyright  Copyright (c) 2008 - 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       earbud_init.c
\brief      Initialisation module
*/

#ifndef AV_HEADSET_INIT_C
#define AV_HEADSET_INIT_C


#include "earbud.h"
#include "earbud_init.h"

#include "authentication.h"
#include "earbud_log.h"
#include "earbud_test.h"
#include "temperature.h"
#include "handset_signalling.h"
#include "handset_service.h"
#include "pairing.h"
#include "power_manager.h"
#include "earbud_ui.h"
#include "earbud_sm.h"
#include "earbud_handover.h"
#include "voice_ui.h"
#ifdef ENABLE_AUDIO_TUNING_MODE
#include "audio_tuning_mode.h"
#endif
#include "gaia_handler.h"
#include "gatt_handler.h"
#include "gatt_connect.h"
#include "gatt_server_battery.h"
#include "gatt_server_gatt.h"
#include "device_upgrade.h"
#ifdef INCLUDE_DFU_PEER
#include "device_upgrade_peer.h"
#endif
#include "gaia.h"
#include "connection_manager_config.h"
#include "bt_device_class.h"
#include "le_advertising_manager.h"
#include "le_scan_manager.h"
#include "link_policy.h"
#include "logical_input_switch.h"
#include "local_addr.h"
#include "local_name.h"
#include "connection_message_dispatcher.h"
#include "ui.h"
#include "volume_messages.h"
#include "volume_service.h"
#include "media_player.h"
#include "telephony_service.h"
#include "audio_sources.h"
#include "voice_sources.h"
#include "peer_link_keys.h"
#include "peer_ui.h"
#include "telephony_messages.h"
#include "anc_state_manager.h"
#include "audio_curation.h"
#if defined(HAVE_9_BUTTONS)
#include "9_buttons.h"
#elif defined(HAVE_6_BUTTONS)
#include "6_buttons.h"
#elif defined(HAVE_2_BUTTONS)
#include "2_button.h"
#elif defined(HAVE_1_BUTTON)
#include "1_button.h"
#endif

#include <init.h>
#include <bredr_scan_manager.h>
#include <connection_manager.h>
#include <hfp_profile.h>
#include <scofwd_profile.h>
#include <handover_profile.h>
#include <shadow_profile.h>
#include <charger_monitor.h>
#include <message_broker.h>
#include <profile_manager.h>
#include <state_proxy.h>
#include <peer_signalling.h>
#include <tws_topology.h>
#include <peer_find_role.h>
#include <peer_pair_le.h>
#include <key_sync.h>
#include <ui_prompts.h>

#include <panic.h>
#include <pio.h>
#include <stdio.h>
#include <feature.h>


/*!< Structure used while initialising */
initData    app_init;


static bool appPioInit(Task init_task)
{
#ifndef USE_BDADDR_FOR_LEFT_RIGHT
    /* Make PIO2 an input with pull-up */
    PioSetMapPins32Bank(0, 1UL << appConfigHandednessPio(), 1UL << appConfigHandednessPio());
    PioSetDir32Bank(0, 1UL << appConfigHandednessPio(), 0);
    PioSet32Bank(0, 1UL << appConfigHandednessPio(), 1UL << appConfigHandednessPio());
    DEBUG_LOGF("appPioInit, left %d, right %d", appConfigIsLeft(), appConfigIsRight());
#endif

    UNUSED(init_task);

    return TRUE;
}

static bool appLicenseCheck(Task init_task)
{
    if (FeatureVerifyLicense(APTX_CLASSIC))
        DEBUG_LOG("appLicenseCheck: aptX Classic is licensed, aptX A2DP CODEC is enabled");
    else
        DEBUG_LOG("appLicenseCheck: aptX Classic not licensed, aptX A2DP CODEC is disabled");

    if (FeatureVerifyLicense(APTX_CLASSIC_MONO))
        DEBUG_LOG("appLicenseCheck: aptX Classic Mono is licensed, aptX TWS+ A2DP CODEC is enabled");
    else
        DEBUG_LOG("appLicenseCheck: aptX Classic Mono not licensed, aptX TWS+ A2DP CODEC is disabled");

    if (FeatureVerifyLicense(CVC_RECV))
        DEBUG_LOG("appLicenseCheck: cVc Receive is licensed");
    else
        DEBUG_LOG("appLicenseCheck: cVc Receive not licensed");

    if (FeatureVerifyLicense(CVC_SEND_HS_1MIC))
        DEBUG_LOG("appLicenseCheck: cVc Send 1-MIC is licensed");
    else
        DEBUG_LOG("appLicenseCheck: cVc Send 1-MIC not licensed");

    if (FeatureVerifyLicense(CVC_SEND_HS_2MIC_MO))
        DEBUG_LOG("appLicenseCheck: cVc Send 2-MIC is licensed");
    else
        DEBUG_LOG("appLicenseCheck: cVc Send 2-MIC not licensed");

    UNUSED(init_task);
    return TRUE;
}

/*! \brief Forward CL_INIT_CFM message to the init task handler. */
static void appInitFwdClInitCfm(const CL_INIT_CFM_T * cfm)
{
    CL_INIT_CFM_T *copy = PanicUnlessNew(CL_INIT_CFM_T);
    *copy = *cfm;

    MessageSend(appInitGetInitTask(), CL_INIT_CFM, copy);
}

/*! \brief Handle Connection library confirmation message */
static void appInitHandleClInitCfm(const CL_INIT_CFM_T *cfm)
{
    if (cfm->status != success)
        Panic();

    /* Set the class of device to indicate this is a headset */
    ConnectionWriteClassOfDevice(AUDIO_MAJOR_SERV_CLASS | RENDER_MAJOR_SERV_CLASS |
                                 AV_MAJOR_DEVICE_CLASS | HEADSET_MINOR_DEVICE_CLASS);

    /* Allow SDP without security, requires authorisation */
    ConnectionSmSetSecurityLevel(0, 1, ssp_secl4_l0, TRUE, TRUE, FALSE);

    /* Reset security mode config - always turn off debug keys on power on */
    ConnectionSmSecModeConfig(appGetAppTask(), cl_sm_wae_acl_owner_none, FALSE, TRUE);

    appInitFwdClInitCfm(cfm);
}

/*! \brief Connection library Message Handler

    This function is the main message handler for the main application task, every
    message is handled in it's own seperate handler function.  The switch
    statement is broken into seperate blocks to reduce code size, if execution
    reaches the end of the function then it is assumed that the message is
    unhandled.
*/
static void appHandleClMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);

    //DEBUG_LOG("appHandleClMessage called, message id = 0x%x", id);
    /* Handle Connection Library messages that are not sent directly to
       the requestor */
    if (CL_MESSAGE_BASE <= id && id < CL_MESSAGE_TOP)
    {
        bool handled = FALSE;

        if (id == CL_INIT_CFM)
        {
            appInitHandleClInitCfm((const CL_INIT_CFM_T *)message);
            return;
        }

        /* Pass connection library messages in turn to the modules that
           are interested in them.
         */
        handled |= LeScanManager_HandleConnectionLibraryMessages(id, message, handled);
        handled |= PeerPairLe_HandleConnectionLibraryMessages(id, message, handled);
        handled |= Pairing_HandleConnectionLibraryMessages(id, message, handled);
        handled |= ConManagerHandleConnectionLibraryMessages(id, message, handled);
        handled |= appLinkPolicyHandleConnectionLibraryMessages(id, message, handled);
        handled |= appAuthHandleConnectionLibraryMessages(id, message, handled);
        handled |= LeAdvertisingManager_HandleConnectionLibraryMessages(id, message, handled);
        handled |= appTestHandleConnectionLibraryMessages(id, message, handled);
        handled |= PeerFindRole_HandleConnectionLibraryMessages(id, message, handled);
        handled |= HandoverProfile_HandleConnectionLibraryMessages(id, message, handled);
        handled |= LocalAddr_HandleConnectionLibraryMessages(id, message, handled);
        handled |= ShadowProfile_HandleConnectionLibraryMessages(id, message, handled);

        if (handled)
        {
            return;
        }
    }

    DEBUG_LOG("appHandleClMessage called but unhandled, message id = %d", id);
    appHandleUnexpected(id);
}

/*! \brief Connection library initialisation */
static bool appConnectionInit(Task init_task)
{
    static const msg_filter filter = {msg_group_acl | msg_group_mode_change};

    ConnectionMessageDispatcher_Init();

    /* Initialise the Connection Manager */
#if defined(APP_SECURE_CONNECTIONS)
    ConnectionInitEx3(ConnectionMessageDispatcher_GetHandler(), &filter, appConfigMaxPairedDevices(), CONNLIB_OPTIONS_SC_ENABLE);
#else
    ConnectionInitEx3(ConnectionMessageDispatcher_GetHandler(), &filter, appConfigMaxPairedDevices(), CONNLIB_OPTIONS_NONE);
#endif

    appInitSetInitTask(init_task);

    ConnectionMessageDispatcher_RegisterInitClient(&app_init.task);

    return TRUE;
}

static bool appInitTransportManagerInitFixup(Task init_task)
{
    UNUSED(init_task);

    TransportMgrInit();

    return TRUE;
}

static bool appMessageDispatcherRegister(Task init_task)
{
    Task client = &app_init.task;

    UNUSED(init_task);

    ConnectionMessageDispatcher_RegisterInquiryClient(client);
    ConnectionMessageDispatcher_RegisterCryptoClient(client);
    ConnectionMessageDispatcher_RegisterCsbClient(client);
    ConnectionMessageDispatcher_RegisterLeClient(client);
    ConnectionMessageDispatcher_RegisterTdlClient(client);
    ConnectionMessageDispatcher_RegisterL2capClient(client);
    ConnectionMessageDispatcher_RegisterLocalDeviceClient(client);
    ConnectionMessageDispatcher_RegisterPairingClient(client);
    ConnectionMessageDispatcher_RegisterLinkPolicyClient(client);
    ConnectionMessageDispatcher_RegisterTestClient(client);
    ConnectionMessageDispatcher_RegisterRemoteConnectionClient(client);
    ConnectionMessageDispatcher_RegisterRfcommClient(client);
    ConnectionMessageDispatcher_RegisterScoClient(client);
    ConnectionMessageDispatcher_RegisterSdpClient(client);

    return TRUE;
}


#ifdef USE_BDADDR_FOR_LEFT_RIGHT
static bool appConfigInit(Task init_task)
{
    /* Get local device address */
    ConnectionReadLocalAddr(init_task);

    return TRUE;
}

static bool appInitHandleClDmLocalBdAddrCfm(Message message)
{
    CL_DM_LOCAL_BD_ADDR_CFM_T *cfm = (CL_DM_LOCAL_BD_ADDR_CFM_T *)message;
    if (cfm->status != success)
        Panic();

    InitGetTaskData()->appInitIsLeft = cfm->bd_addr.lap & 0x01;

    DEBUG_LOGF("appInit, bdaddr %04x:%02x:%06x",
                    cfm->bd_addr.nap, cfm->bd_addr.uap, cfm->bd_addr.lap);
    DEBUG_LOGF("appInit, left %d, right %d", appConfigIsLeft(), appConfigIsRight());

    return TRUE;
}
#endif

#ifdef INIT_DEBUG
/*! Debug function blocks execution until appInitDebugWait is cleared:
    apps1.fw.env.vars['appInitDebugWait'].set_value(0) */
static bool appInitDebug(Task init_task)
{
    volatile static bool appInitDebugWait = TRUE;
    while(appInitDebugWait);

    UNUSED(init_task);
    return TRUE;
}
#endif

/*! \brief Table of initialisation functions */
static const init_table_entry_t appInitTable[] =
{
#ifdef INIT_DEBUG
    {appInitDebug,          0, NULL},
#endif
    {appPioInit,            0, NULL},
    {Ui_Init,               0, NULL},
    {appLicenseCheck,       0, NULL},
#ifdef INCLUDE_TEMPERATURE
    {appTemperatureInit,    TEMPERATURE_INIT_CFM, NULL},
#endif
    {appBatteryInit,        MESSAGE_BATTERY_INIT_CFM, NULL},
#ifdef INCLUDE_CHARGER
    {appChargerInit,        0, NULL},
#endif
    {appLedInit,            0, NULL},
    {appPowerInit,          APP_POWER_INIT_CFM, NULL},
    {appPhyStateInit,       PHY_STATE_INIT_CFM, NULL},
    {appConnectionInit,     CL_INIT_CFM, NULL},
    {appMessageDispatcherRegister, 0, NULL},
#ifdef USE_BDADDR_FOR_LEFT_RIGHT
    {appConfigInit,         CL_DM_LOCAL_BD_ADDR_CFM, appInitHandleClDmLocalBdAddrCfm},
#endif
    {appLinkPolicyInit,     0, NULL},
    {LocalAddr_Init,        0, NULL},
    {ConManagerInit,        0, NULL},
    {PrimaryRules_Init,     0, NULL},
    {SecondaryRules_Init,   0, NULL},
    {appDeviceInit,         CL_DM_LOCAL_BD_ADDR_CFM, appDeviceHandleClDmLocalBdAddrCfm},
    {BredrScanManager_Init, BREDR_SCAN_MANAGER_INIT_CFM, NULL},
    {LocalName_Init, LOCAL_NAME_INIT_CFM, NULL},
    {LeAdvertisingManager_Init,     0, NULL},
    {LeScanManager_Init,     0, NULL},
    {AudioSources_Init,      0, NULL},
    {VoiceSources_Init,      0, NULL},
    {Volume_InitMessages,   0, NULL},
    {VolumeService_Init,    0, NULL},
    {appAvInit,             AV_INIT_CFM, NULL},
    {appPeerSigInit,        PEER_SIG_INIT_CFM, NULL},
    {LogicalInputSwitch_Init,     0, NULL},
    {Pairing_Init,          PAIRING_INIT_CFM, NULL},
    {Telephony_InitMessages, 0, NULL},
    {appHfpInit,            APP_HFP_INIT_CFM, NULL},
    {appHandsetSigInit,     0, NULL},
    {appKymeraInit,         0, NULL},
#ifdef INCLUDE_SCOFWD
    {ScoFwdInit,            SFWD_INIT_CFM, NULL},
#endif
    {StateProxy_Init,       0, NULL},
    {MediaPlayer_Init,       0, NULL},
    {appInitTransportManagerInitFixup, 0, NULL},        //! \todo TransportManager does not meet expected init interface
#ifdef INCLUDE_DFU
    {appGaiaInit,           APP_GAIA_INIT_CFM, NULL},   // Gatt needs GAIA
#endif
    {GattConnect_Init,      0, NULL},   // GATT functionality is initialised by calling GattConnect_Init then GattConnect_ServerInitComplete.
    // All GATT Servers MUST be initialised after GattConnect_Init and before GattConnect_ServerInitComplete.
    {GattHandlerInit,      0, NULL}, 
    {PeerPairLe_Init,       0, NULL},
    {KeySync_Init,          0, NULL},
    {ProfileManager_Init,   0, NULL},
    {HandsetService_Init,   0, NULL},
    {PeerFindRole_Init,     0, PEER_FIND_ROLE_INIT_CFM},
    {TwsTopology_Init,      TWS_TOPOLOGY_INIT_CFM, NULL},
    {PeerLinkKeys_Init,     0, NULL},
#ifdef INCLUDE_GATT_BATTERY_SERVER
    {GattServerBattery_Init,0, NULL},
#endif
#ifdef INCLUDE_GATT_GAIA_SERVER
    {appGaiaGattServerInit, 0, NULL},
#endif
    {GattServerGatt_Init,   0, NULL},
    // All GATT Servers MUST be initialised before GATT initialisation is complete.
    {GattConnect_ServerInitComplete, GATT_CONNECT_SERVER_INIT_COMPLETE_CFM, NULL},
    {appSmInit,             0, NULL},
#ifdef INCLUDE_DFU
    {appUpgradeInit,        UPGRADE_INIT_CFM, NULL},    // Upgrade wants to start a connection (can be gatt)
#endif
#ifdef INCLUDE_DFU_PEER
    {appDeviceUpgradePeerInit,  DEVICE_UPGRADE_PEER_INIT_CFM, NULL},
#endif
    {VoiceUi_Init,   0, NULL},
#ifdef ENABLE_AUDIO_TUNING_MODE
    {AudioTuningMode_Init, 0, NULL}, 
#endif
    {EarbudUi_Init,      0, NULL},
    {TelephonyService_Init, 0, NULL},
#ifdef ENABLE_ANC
    {AncStateManager_Init, 0, NULL},
#endif
    {AudioCuration_Init, 0, NULL},
    {UiPrompts_Init,     0, NULL},
    {PeerUi_Init,        0, NULL},
};

void appInit(void)
{
    unsigned registrations_array_dim;

    app_init.task.handler = appHandleClMessage;

    registrations_array_dim = (unsigned)message_broker_group_registrations_end -
                              (unsigned)message_broker_group_registrations_begin;
    PanicFalse((registrations_array_dim % sizeof(message_broker_group_registration_t)) == 0);
    registrations_array_dim /= sizeof(message_broker_group_registration_t);

    MessageBroker_Init(message_broker_group_registrations_begin,
                       registrations_array_dim);

    AppsCommon_StartInit(appGetAppTask(), appInitTable, ARRAY_DIM(appInitTable));
}

void appInitSetInitialised(void)
{
#if defined(HAVE_6_BUTTONS) || defined(HAVE_9_BUTTONS)
    LogicalInputSwitch_SetLogicalInputIdRange(APP_MFB_BUTTON_SINGLE_CLICK,
                                              APP_BUTTON_LAST);
#else
    LogicalInputSwitch_SetLogicalInputIdRange(APP_MFB_BUTTON_SINGLE_CLICK,
                                              APP_BUTTON_FACTORY_RESET);
#endif


    /* Initialise input event manager with auto-generated tables for
     * the target platform. Connect to the logical Input Switch component */
    InputEventManagerInit(LogicalInputSwitch_GetTask(), InputEventActions,
                          sizeof(InputEventActions), &InputEventConfig);

    InitGetTaskData()->initialised = APP_INIT_COMPLETED_MAGIC;
}


void appInitSetInitTask(Task init_task)
{
    InitGetTaskData()->init_task = init_task;
}


Task appInitGetInitTask(void)
{
    return InitGetTaskData()->init_task;
}


#endif // AV_HEADSET_INIT_C
