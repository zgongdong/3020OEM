/*!
\copyright  Copyright (c) 2017 - 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Implementation of specifc application testing functions
*/

#include "earbud_test.h"

#include "earbud_init.h"
#include "earbud_log.h"
#include "pairing.h"
#include "power_manager.h"
#include "earbud_sm.h"
#include "scofwd_profile_config.h"
#include "battery_monitor_config.h"
#include "charger_monitor_config.h"
#include "kymera_config.h"
#include "temperature_config.h"
#include "peripherals/thermistor.h"
#include "ui.h"
#include "peer_link_keys.h"
#include "microphones.h"

#if defined(HAVE_9_BUTTONS)
#include "9_buttons.h"
#elif defined(HAVE_6_BUTTONS)
#include "6_buttons.h"
#elif defined(HAVE_2_BUTTONS)
#include "2_button.h"
#elif defined(HAVE_1_BUTTON)
#include "1_button.h"
#endif

#include <device_properties.h>
#include <device_list.h>
#include <device.h>
#include <hfp_profile.h>
#include <scofwd_profile.h>
#include <battery_monitor.h>
#include <av.h>
#include <av_config.h>
#include <logical_input_switch.h>
#include <profile_manager.h>
#include <peer_find_role_private.h>
#include <peer_signalling.h>


#include <cryptovm.h>
#include <ps.h>
#include <boot.h>
#include <feature.h>
#include <panic.h>
#include <stdio.h>
#include <bdaddr.h>

#ifdef GC_SECTIONS
/* Move all functions in KEEP_PM section to ensure they are not removed during
 * garbage collection */
#pragma unitcodesection KEEP_PM
#endif

static void testTaskHandler(Task task, MessageId id, Message message);
TaskData testTask = {testTaskHandler};

struct 
{
    uint16 testcase;
    uint16 iteration;
    uint16 last_marker;
} test_support = {0};

/*! \brief Handle ACL Mode Changed Event

    This message is received when the link mode of an ACL has changed from sniff
    to active or vice-versa.  Currently the application ignores this message.
*/
static void appTestHandleClDmModeChangeEvent(CL_DM_MODE_CHANGE_EVENT_T *evt)
{
    DEBUG_LOGF("appHandleClDmModeChangeEvent, event %d, %x,%x,%lx", evt->mode, evt->bd_addr.nap, evt->bd_addr.uap, evt->bd_addr.lap);

    if (appConfigScoForwardingEnabled())
    {
        CL_DM_MODE_CHANGE_EVENT_T *fwd = PanicUnlessNew(CL_DM_MODE_CHANGE_EVENT_T);

        /* Support for the Sco Forwarding test command, appTestScoFwdLinkStatus() */
        *fwd = *evt;

        MessageSend(&testTask,CL_DM_MODE_CHANGE_EVENT,fwd);
    }
}


bool appTestHandleConnectionLibraryMessages(MessageId id, Message message,
                                            bool already_handled)
{
    switch(id)
    {
        case CL_DM_MODE_CHANGE_EVENT:
            appTestHandleClDmModeChangeEvent((CL_DM_MODE_CHANGE_EVENT_T *)message);
            return TRUE;
    }

    return already_handled;
}




#ifndef FAKE_BATTERY_LEVEL
#define FAKE_BATTERY_LEVEL (0)
#endif
uint16 appTestBatteryVoltage = FAKE_BATTERY_LEVEL;

/*! \brief Returns the current battery voltage
 */
uint16 appTestGetBatteryVoltage(void)
{
    DEBUG_LOGF("appTestGetBatteryVoltage, %u", appBatteryGetVoltage());
    return appBatteryGetVoltage();
}

void appTestSetBatteryVoltage(uint16 new_level)
{
    DEBUG_LOGF("appTestSetBatteryVoltage, %u", new_level);
    appTestBatteryVoltage = new_level;
    MessageSend(&GetBattery()->task, MESSAGE_BATTERY_PROCESS_READING, NULL);
}

bool appTestBatteryStateIsOk(void)
{
    return (battery_level_ok == appBatteryGetState());
}

bool appTestBatteryStateIsLow(void)
{
    return (battery_level_low == appBatteryGetState());
}

bool appTestBatteryStateIsCritical(void)
{
    return (battery_level_critical == appBatteryGetState());
}

bool appTestBatteryStateIsTooLow(void)
{
    return (battery_level_too_low == appBatteryGetState());
}

uint32 appTestBatteryFilterResponseTimeMs(void)
{
    return BATTERY_FILTER_LEN * appConfigBatteryReadPeriodMs();
}

#ifdef HAVE_THERMISTOR
uint16 appTestThermistorDegreesCelsiusToMillivolts(int8 temperature)
{
    return appThermistorDegreesCelsiusToMillivolts(temperature);
}
#endif

/*! \brief Put Earbud into Handset Pairing mode
*/
void appTestPairHandset(void)
{
    DEBUG_LOG("appTestPairHandset");
    appSmPairHandset();
}

/*! \brief Delete all Handset pairing
*/
void appTestDeleteHandset(void)
{
    DEBUG_LOG("appTestDeleteHandset");
    appSmDeleteHandsets();
}

/*! \brief Put Earbud into Peer Pairing mode
*/
void appTestPairPeer(void)
{
    DEBUG_LOG("appTestPairPeer UNSUPPORTED");
//    appPairingPeerPair(NULL, FALSE);
}

/*! \brief Delete Earbud peer pairing
*/
bool appTestDeletePeer(void)
{
    bdaddr bd_addr;

    DEBUG_LOG("appTestDeletePeer");
    /* Check if we have previously paired with an earbud */
    if (appDeviceGetPeerBdAddr(&bd_addr))
    {
        return appDeviceDelete(&bd_addr);
    }
    else
    {
        DEBUG_LOG("appTestDeletePeer: NO PEER TO DELETE");
        return FALSE;
    }
}

bool appTestGetPeerAddr(bdaddr *peer_address)
{
    DEBUG_LOG("appTestGetPeerAddr");
    if (appDeviceGetPeerBdAddr(peer_address))
        return TRUE;
    else
        return FALSE;
}


/*! \brief Return if Earbud is in a Pairing mode
*/
bool appTestIsPairingInProgress(void)
{
    DEBUG_LOG("appTestIsPairingInProgress");
    return !appPairingIsIdle();
}

/*! \brief Initiate Earbud A2DP connection to the Handset
*/
bool appTestHandsetA2dpConnect(void)
{
    bdaddr bd_addr;

    DEBUG_LOG("appTestHandsetA2dpConnect");
    if (appDeviceGetHandsetBdAddr(&bd_addr))
    {
        return appAvA2dpConnectRequest(&bd_addr, A2DP_CONNECT_NOFLAGS);
    }
    else
    {
        return FALSE;
    }
}

/*! Used to block the rules for handset pairing */
bool appTestHandsetPairingBlocked = FALSE;

/*! \brief Stop the earbud pairing with a handset
 */
void appTestBlockAutoHandsetPairing(bool block)
{
    bool was_blocked = appTestHandsetPairingBlocked;
    bool paired_already = appTestIsHandsetPaired();

    appTestHandsetPairingBlocked = block;

    DEBUG_LOG("appTestBlockAutoHandsetPairing(%d) was %d (paired:%d)",
                    block, was_blocked, paired_already);
}

/*! \brief Return if Earbud has a handset pairing
 */
bool appTestIsHandsetPaired(void)
{
    bool paired = BtDevice_IsPairedWithHandset();

    DEBUG_LOG("appTestIsHandsetPaired:%d",paired);

    return paired;
}

/*! \brief Return if Earbud has an Handset A2DP connection
*/
bool appTestIsHandsetA2dpConnected(void)
{
    bdaddr bd_addr;

    DEBUG_LOG("appTestIsHandsetA2dpConnected");
    if (appDeviceGetHandsetBdAddr(&bd_addr))
    {
        /* Find handset AV instance */
        avInstanceTaskData *theInst = appAvInstanceFindFromBdAddr(&bd_addr);
        if (theInst)
            return appA2dpIsConnected(theInst);
    }

    /* If we get here then there's no A2DP connected for handset */
    return FALSE;
}

bool appTestIsHandsetA2dpMediaConnected(void)
{
    bdaddr bd_addr;

    DEBUG_LOG("appTestIsHandsetA2dpMediaConnected");
    if (appDeviceGetHandsetBdAddr(&bd_addr))
    {
        /* Find handset AV instance */
        avInstanceTaskData *theInst = appAvInstanceFindFromBdAddr(&bd_addr);
        if (theInst)
            return appA2dpIsConnectedMedia(theInst);
    }

    /* If we get here then there's no A2DP connected for handset */
    return FALSE;
}

/*! \brief Return if Earbud is in A2DP streaming mode with the handset
*/
bool appTestIsHandsetA2dpStreaming(void)
{
    bdaddr bd_addr;

    DEBUG_LOG("appTestIsHandsetA2dpStreaming");
    if (appDeviceGetHandsetBdAddr(&bd_addr))
    {
        /* Find handset AV instance */
        avInstanceTaskData *theInst = appAvInstanceFindFromBdAddr(&bd_addr);
        if (theInst)
            return appA2dpIsStreaming(theInst);
    }

    /* If we get here then there's no A2DP connected for handset */
    return FALSE;
}

bool appTestIsA2dpPlaying(void)
{
    DEBUG_LOGF("appTestIsA2dpPlaying, status %u", appAvPlayStatus());
    return appAvPlayStatus() == avrcp_play_status_playing;
}

/*! \brief Initiate Earbud AVRCP connection to the Handset
*/
bool appTestHandsetAvrcpConnect(void)
{
    bdaddr bd_addr;

    DEBUG_LOG("appTestHandsetAvrcpConnect");
    if (appDeviceGetHandsetBdAddr(&bd_addr))
        return  appAvAvrcpConnectRequest(NULL, &bd_addr);
    else
        return FALSE;
}

/*! \brief Return if Earbud has an Handset AVRCP connection
*/
bool appTestIsHandsetAvrcpConnected(void)
{
    bdaddr bd_addr;

    DEBUG_LOG("appTestIsHandsetAvrcpConnected");
    if (appDeviceGetHandsetBdAddr(&bd_addr))
    {
        /* Find handset AV instance */
        avInstanceTaskData *theInst = appAvInstanceFindFromBdAddr(&bd_addr);
        if (theInst)
            return appAvrcpIsConnected(theInst);
    }

    /* If we get here then there's no AVRCP connected for handset */
    return FALSE;
}

/*! \brief Initiate Earbud HFP connection to the Handset
*/
bool appTestHandsetHfpConnect(void)
{
    bdaddr bd_addr;

    DEBUG_LOG("appTestHandsetHfpConnect");
    if (appDeviceGetHandsetBdAddr(&bd_addr))
        return appHfpConnectWithBdAddr(&bd_addr, hfp_handsfree_107_profile);
    else
        return FALSE;
}

/*! \brief Return if Earbud has an Handset HFP connection
*/
bool appTestIsHandsetHfpConnected(void)
{
    DEBUG_LOG("appTestIsHandsetHfpConnected");
    return appHfpIsConnected();
}

/*! \brief Return if Earbud has an Handset HFP SCO connection
*/
bool appTestIsHandsetHfpScoActive(void)
{
    DEBUG_LOG("appTestIsHandsetHfpScoActive");
    return appHfpIsScoActive();
}

/*! \brief Initiate Earbud HFP Voice Dial request to the Handset
*/
bool appTestHandsetHfpVoiceDial(void)
{
    DEBUG_LOG("appTestHandsetHfpVoiceDial");
    if (appHfpIsConnected())
    {
        appHfpCallVoice();
        return TRUE;
    }
    else
        return FALSE;
}

/*! \brief Initiate Earbud HFP Voice Transfer request to the Handset
*/
bool appTestHandsetHfpVoiceTransfer(void)
{
    DEBUG_LOG("appTestHandsetHfpVoiceTransfer");
    if (appHfpIsCall())
    {
        if (appHfpIsScoActive())
            appHfpTransferToAg();
        else
            appHfpTransferToHeadset();
        return TRUE;
    }
    else
        return FALSE;
}

bool appTestHandsetHfpMute(void)
{
    DEBUG_LOG("appTestHandsetHfpMute");
    if (appHfpIsCall())
    {
        if (!appHfpIsMuted())
            appHfpMuteToggle();
        return TRUE;
    }
    else
        return FALSE;
}

bool appTestHandsetHfpUnMute(void)
{
    DEBUG_LOG("appTestHandsetHfpUnMute");
    if (appHfpIsCall())
    {
        if (appHfpIsMuted())
            appHfpMuteToggle();
        return TRUE;
    }
    else
        return FALSE;
}

bool appTestHandsetHfpVoiceTransferToAg(void)
{
    DEBUG_LOG("appTestHandsetHfpVoiceTransferToAg");
    if (appHfpIsCall() && appHfpIsScoActive())
    {
        appHfpTransferToAg();
        return TRUE;
    }
    else
        return FALSE;
}

bool appTestHandsetHfpVoiceTransferToHeadset(void)
{
    DEBUG_LOG("appTestHandsetHfpVoiceTransferToHeadset");
    if (appHfpIsCall())
    {
        appHfpTransferToHeadset();
        return TRUE;
    }
    else
        return FALSE;
}

bool appTestHandsetHfpCallAccept(void)
{
    DEBUG_LOG("appTestHandsetHfpCallAccept");
    if (appHfpIsCall())
    {
        appHfpCallAccept();
    }
    else if (ScoFwdIsCallIncoming())
    {
        ScoFwdCallAccept();
    }
    else
    {
        return FALSE;
    }
    return TRUE;
}

bool appTestHandsetHfpCallReject(void)
{
    DEBUG_LOG("appTestHandsetHfpCallReject");
    if (appHfpIsCall())
    {
        appHfpCallReject();
    }
    else if (ScoFwdIsCallIncoming())
    {
        ScoFwdCallReject();
    }
    else
    {
        return FALSE;
    }
    return TRUE;
}

bool appTestHandsetHfpCallHangup(void)
{
    DEBUG_LOG("appTestHandsetHfpCallHangup");
    if (appHfpIsCall())
    {
        appHfpCallHangup();
    }
    else if (ScoFwdIsReceiving() && !ScoFwdIsCallIncoming())
    {
        ScoFwdCallHangup();
    }
    else
    {
        return FALSE;
    }
    return TRUE;
}

bool appTestHandsetHfpCallLastDialed(void)
{
    DEBUG_LOG("appTestHandsetHfpCallLastDialed");
    if (appHfpIsConnected())
    {
        appHfpCallLastDialed();
        return TRUE;
    }
    else
        return FALSE;
}

bool appTestHandsetHfpSetScoVolume(uint8 volume)
{
    DEBUG_LOG("appTestHandsetHfpScoVolume");
    if (appHfpIsCall())
    {
        appKymeraScoSetVolume(volume);
        return TRUE;
    }
    else
        return FALSE;
}

bool appTestIsHandsetHfpMuted(void)
{
    DEBUG_LOG("appTestIsHandsetHfpMuted");
    return appHfpIsMuted();
}

bool appTestIsHandsetHfpCall(void)
{
    DEBUG_LOG("appTestIsHandsetHfpCall");
    return appHfpIsCall();
}

bool appTestIsHandsetHfpCallIncoming(void)
{
    DEBUG_LOG("appTestIsHandsetHfpCallIncoming");
    return appHfpIsCallIncoming();
}

bool appTestIsHandsetHfpCallOutgoing(void)
{
    DEBUG_LOG("appTestIsHandsetHfpCallOutgoing");
    return appHfpIsCallOutgoing();
}

/*! \brief Return if Earbud has a connection to the Handset
*/
bool appTestIsHandsetConnected(void)
{
    DEBUG_LOG("appTestIsHandsetConnected");
    return appTestIsHandsetA2dpConnected() ||
           appTestIsHandsetAvrcpConnected() ||
           appTestIsHandsetHfpConnected();
}

/*! \brief Initiate Earbud A2DP connection to the the Peer
*/
bool appTestPeerA2dpConnect(void)
{
    bdaddr bd_addr;

    DEBUG_LOG("appTestPeerA2dpConnect");
    if (appDeviceGetPeerBdAddr(&bd_addr))
    {
        return appAvA2dpConnectRequest(&bd_addr, A2DP_CONNECT_NOFLAGS);
    }
    else
    {
        return FALSE;
    }
}

/*! \brief Return if Earbud has a Peer A2DP connection
*/
bool appTestIsPeerA2dpConnected(void)
{
    bdaddr bd_addr;

    DEBUG_LOG("appTestIsPeerA2dpConnected");
    if (appDeviceGetPeerBdAddr(&bd_addr))
    {
        /* Find peer AV instance */
        avInstanceTaskData *theInst = appAvInstanceFindFromBdAddr(&bd_addr);
        if (theInst)
            return appA2dpIsConnected(theInst);
    }

    /* If we get here then there's no A2DP connected for handset */
    return FALSE;
}


/*! \brief Check if Earbud is in A2DP streaming mode with peer Earbud
 */
bool appTestIsPeerA2dpStreaming(void)
{
    bdaddr bd_addr;

    DEBUG_LOG("appTestIsPeerA2dpStreaming");
    if (appDeviceGetPeerBdAddr(&bd_addr))
    {
        /* Find peer AV instance */
        avInstanceTaskData *theInst = appAvInstanceFindFromBdAddr(&bd_addr);
        if (theInst)
            return appA2dpIsStreaming(theInst);
    }

    /* If we get here then there's no A2DP connected for peer */
    return FALSE;
}


/*! \brief Initiate Earbud AVRCP connection to the the Peer
*/
bool appTestPeerAvrcpConnect(void)
{
    bdaddr bd_addr;

    DEBUG_LOG("appTestPeerAvrcpConnect");
    if (appDeviceGetPeerBdAddr(&bd_addr))
        return  appAvAvrcpConnectRequest(NULL, &bd_addr);
    else
        return FALSE;
}

/*! \brief Return if Earbud has a Peer AVRCP connection
*/
bool appTestIsPeerAvrcpConnected(void)
{
    bdaddr bd_addr;

    DEBUG_LOG("appTestIsPeerAvrcpConnected");
    if (appDeviceGetPeerBdAddr(&bd_addr))
    {
        /* Find handset AV instance */
        avInstanceTaskData *theInst = appAvInstanceFindFromBdAddr(&bd_addr);
        if (theInst)
            return appAvrcpIsConnected(theInst);
    }

    /* If we get here then there's no AVRCP connected for handset */
    return FALSE;
}

/*! \brief Send the Avrcp pause command to the Handset
*/
void appTestAvPause(void)
{
    DEBUG_LOG("appTestAvPause");
    Ui_InjectUiInput(ui_input_pause);
}

/*! \brief Send the Avrcp play command to the Handset
*/
void appTestAvPlay(void)
{
    DEBUG_LOG("appTestAvPlay");
    Ui_InjectUiInput(ui_input_play);
}

/*! \brief Send the Avrcp stop command to the Handset
*/
void appTestAvStop(void)
{
    DEBUG_LOG("appTestAvStop");
    Ui_InjectUiInput(ui_input_stop_av_connection);
}

/*! \brief Send the Avrcp forward command to the Handset
*/
void appTestAvForward(void)
{
    DEBUG_LOG("appTestAvForward");
    Ui_InjectUiInput(ui_input_av_forward);
}

/*! \brief Send the Avrcp backward command to the Handset
*/
void appTestAvBackward(void)
{
    DEBUG_LOG("appTestAvBackward");
    Ui_InjectUiInput(ui_input_av_backward);
}

/*! \brief Send the Avrcp fast forward state command to the Handset
*/
void appTestAvFastForwardStart(void)
{
    DEBUG_LOG("appTestAvFastForwardStart");
    Ui_InjectUiInput(ui_input_av_fast_forward_start);
}

/*! \brief Send the Avrcp fast forward stop command to the Handset
*/
void appTestAvFastForwardStop(void)
{
    DEBUG_LOG("appTestAvFastForwardStop");
    Ui_InjectUiInput(ui_input_fast_forward_stop);
}

/*! \brief Send the Avrcp rewind start command to the Handset
*/
void appTestAvRewindStart(void)
{
    DEBUG_LOG("appTestAvRewindStart");
    Ui_InjectUiInput(ui_input_av_rewind_start);
}

/*! \brief Send the Avrcp rewind stop command to the Handset
*/
void appTestAvRewindStop(void)
{
    DEBUG_LOG("appTestAvRewindStop");
    Ui_InjectUiInput(ui_input_rewind_stop);
}

/*! \brief Send the Avrcp volume change command to the Handset
*/
bool appTestAvVolumeChange(int8 step)
{
    DEBUG_LOGF("appTestAvVolumeChange %d", step);
    return appAvVolumeChange(step);
}

/*! \brief Send the Avrcp pause command to the Handset
*/
void appTestAvVolumeSet(uint8 volume)
{
    DEBUG_LOGF("appTestAvVolumeSet %d", volume);
   appAvVolumeSet(volume, NULL);
}

void appTestAvVolumeSetDb(int8 gain)
{
    /* Set default volume as set in earbud_config.h */
    const int rangeDb = appConfigMaxVolumedB() - appConfigMinVolumedB();

    DEBUG_LOGF("appTestAvVolumeSetDb %d", gain);
    if (    gain < appConfigMinVolumedB()
        ||  gain > appConfigMaxVolumedB())
    {
        DEBUG_LOGF("appTestAvVolumeSetDb. Gain %d outside range %d-%d",
                        gain,appConfigMinVolumedB(),appConfigMaxVolumedB());
    }

    appAvVolumeSet((gain - appConfigMinVolumedB()) * VOLUME_MAX / rangeDb, NULL);
}

void appTestPowerAllowDormant(bool enable)
{
    powerTaskData *thePower = PowerGetTaskData();

    DEBUG_LOGF("appTestPowerAllowDormant %d", enable);
    thePower->allow_dormant = enable;
}

/*! \brief Test the generation of link kets */
extern void TestLinkkeyGen(void)
{
    bdaddr bd_addr;
    uint16 lk[8];
    uint16 lk_out[8];

    bd_addr.nap = 0x0002;
    bd_addr.uap = 0x5B;
    bd_addr.lap = 0x00FF02;

    lk[0] = 0x9541;
    lk[1] = 0xe6b4;
    lk[2] = 0x6859;
    lk[3] = 0x0791;
    lk[4] = 0x9df9;
    lk[5] = 0x95cd;
    lk[6] = 0x9570;
    lk[7] = 0x814b;

    DEBUG_LOG("appTestPowerAllowDormant");
    PeerLinkKeys_GenerateKey(&bd_addr, lk, 0x74777332UL, lk_out);

#if 0
    bd_addr.nap = 0x0000;
    bd_addr.uap = 0x74;
    bd_addr.lap = 0x6D7031;

    lk[0] = 0xec02;
    lk[1] = 0x34a3;
    lk[2] = 0x57c8;
    lk[3] = 0xad05;
    lk[4] = 0x3410;
    lk[5] = 0x10a6;
    lk[6] = 0x0a39;
    lk[7] = 0x7d9b;
#endif

    PeerLinkKeys_GenerateKey(&bd_addr, lk, 0x6c656272UL, lk_out);

}

/*! \brief Test the cryptographic key conversion function, producing an H6 key */
extern void TestH6(void)
{
    uint8 key_h7[16] = {0xec,0x02,0x34,0xa3,0x57,0xc8,0xad,0x05,0x34,0x10,0x10,0xa6,0x0a,0x39,0x7d,0x9b};
    //uint32 key_id = 0x6c656272;
    uint32 key_id = 0x7262656c;
    uint8 key_h6[16];

    DEBUG_LOG("appTestPowerAllowDormant");
    CryptoVmH6(key_h7, key_id, key_h6);
    printf("H6: ");
    for (int h6_i = 0; h6_i < 16; h6_i++)
        printf("%02x ", key_h6[h6_i]);
    printf("\n");
}

/*! \brief report the saved handset information */
extern void appTestHandsetInfo(void)
{
    bdaddr bd_addr;
    DEBUG_LOG("appTestHandsetInfo");
    if (appDeviceGetHandsetBdAddr(&bd_addr))
    {
        DEBUG_LOGF("appTestHandsetInfo, bdaddr %04x,%02x,%06lx",
                   bd_addr.nap, bd_addr.uap, bd_addr.lap);
//        DEBUG_LOGF("appTestHandsetInfo, supported %02x, connected %02x",
//                   attributes.supported_profiles, attributes.connected_profiles);
//        DEBUG_LOGF("appTestHandsetInfo, a2dp volume %u",
//                   attributes.a2dp_volume);
//        DEBUG_LOGF("appTestHandsetInfo, flags %02x",
//                   attributes.flags);
    }
}

#include "earbud_log.h"

/*! \brief Simple test function to make sure that the DEBUG_LOG macros
        work */
extern void TestDebug(void)
{
    DEBUG_LOGF("test %d %d", 1, 2);
    DEBUG_LOG("test");
}

/*! \brief Generate event that Earbud is now in the case. */
void appTestPhyStateInCaseEvent(void)
{
    DEBUG_LOG("appTestPhyStateInCaseEvent");
    appPhyStateInCaseEvent();
}

/*! \brief Generate event that Earbud is now out of the case. */
void appTestPhyStateOutOfCaseEvent(void)
{
    DEBUG_LOG("appTestPhyStateOutOfCaseEvent");
    appPhyStateOutOfCaseEvent();
}

/*! \brief Generate event that Earbud is now in ear. */
void appTestPhyStateInEarEvent(void)
{
    DEBUG_LOG("appTestPhyStateInEarEvent");
    appPhyStateInEarEvent();
}

/*! \brief Generate event that Earbud is now out of the ear. */
void appTestPhyStateOutOfEarEvent(void)
{
    DEBUG_LOG("appTestPhyStateOutOfEarEvent");
    appPhyStateOutOfEarEvent();
}

/*! \brief Generate event that Earbud is now moving */
void appTestPhyStateMotionEvent(void)
{
    DEBUG_LOG("appTestPhyStateMotionEvent");
    appPhyStateMotionEvent();
}

/*! \brief Generate event that Earbud is now not moving. */
void appTestPhyStateNotInMotionEvent(void)
{
    DEBUG_LOG("appTestPhyStateNotInMotionEvent");
    appPhyStateNotInMotionEvent();
}

bool appTestPhyStateIsInEar(void)
{
    return (PHY_STATE_IN_EAR == appPhyStateGetState());
}


bool appTestPhyStateOutOfEar(void)
{
    return     PHY_STATE_OUT_OF_EAR == appPhyStateGetState()
            || PHY_STATE_OUT_OF_EAR_AT_REST == appPhyStateGetState();
}


bool appTestPhyStateIsInCase(void)
{
    return (PHY_STATE_IN_CASE == appPhyStateGetState());
}

#define ATTRIBUTE_BASE_PSKEY_INDEX  100
#define TDL_BASE_PSKEY_INDEX        142
#define TDL_INDEX_PSKEY             141
#define TDL_SIZE                    8
/*! \brief Delete all state and reboot the earbud.
*/
void appTestResetAndReboot(void)
{
    DEBUG_LOG("appTestResetAndReboot");
    for (int i=0; i<TDL_SIZE; i++)
    {
        PsStore(ATTRIBUTE_BASE_PSKEY_INDEX+i, NULL, 0);
        PsStore(TDL_BASE_PSKEY_INDEX+i, NULL, 0);
        PsStore(TDL_INDEX_PSKEY, NULL, 0);
    }

    /* Clear out any in progress DFU status */
    appTestUpgradeResetState();

    /* Flood fill PS to force a defrag on reboot */
    PsFlood();

    /* call SM to action the reboot */
    appSmReboot();
}

/*! \brief Reset an Earbud to factory defaults.
    Will drop any connections, delete all pairing and reboot.
*/
void appTestFactoryReset(void)
{
    DEBUG_LOG("appTestFactoryReset");
    appTestResetAndReboot();
}

/*! \brief Determine if the earbud has a paired peer earbud.
*/
bool appTestIsPeerPaired(void)
{
    bool paired = BtDevice_IsPairedWithPeer();

    DEBUG_LOG("appTestIsPeerPaired:%d",paired);

    return paired;
}

void appTestConnectHandset(void)
{
    DEBUG_LOG("appTestConnectHandset");
    appSmConnectHandset();
}

bool appTestConnectHandsetA2dpMedia(void)
{
    DEBUG_LOG("appTestConnectHandsetA2dpMedia");
    return appAvConnectHandsetA2dpMedia();
}

bool appTestIsPeerSyncComplete(void)
{
    DEBUG_LOG("appTestIsPeerSyncComplete ** DEPRECATED **");
    /* test scripts may rely on this, for now just return TRUE so
     * they keep running. */
    return TRUE;
}

static void testTaskHandler(Task task, MessageId id, Message message)
{
    static lp_power_mode   powermode = 42;
    static uint16          interval = (uint16)-1;

    UNUSED(task);

    switch (id)
    {
        case CL_DM_ROLE_CFM:
            {
                const CL_DM_ROLE_CFM_T *cfm = (const CL_DM_ROLE_CFM_T *)message;
                bdaddr  peer;
                tp_bdaddr sink_addr;

                if (  !SinkGetBdAddr(cfm->sink, &sink_addr)
                   || !appDeviceGetPeerBdAddr(&peer))
                {
                    return;
                }

                if (   BdaddrIsSame(&peer, &sink_addr.taddr.addr)
                    && hci_success == cfm->status)
                {
                    if (hci_role_master == cfm->role)
                    {
                        DEBUG_LOG("SCO FORWARDING LINK IS: MASTER");
                    }
                    else if (hci_role_slave == cfm->role)
                    {
                        DEBUG_LOG("SCO FORWARDING LINK IS: SLAVE");
                    }
                    else
                    {
                        DEBUG_LOGF("SCO FORWARDING LINK STATE IS A MYSTERY: %d",cfm->role);
                    }
                    DEBUG_LOGF("SCO FORWARDING POWER MODE (cached) IS %d (sniff = %d)",powermode,lp_sniff);
                    DEBUG_LOGF("SCO FORWARDING INTERVAL (cached) IS %d",interval);
                }
            }
            break;

        case CL_DM_MODE_CHANGE_EVENT:
            {
                bdaddr peer;
                const CL_DM_MODE_CHANGE_EVENT_T *mode = (const CL_DM_MODE_CHANGE_EVENT_T *)message;

                if (!appDeviceGetPeerBdAddr(&peer))
                    return;

                if (BdaddrIsSame(&mode->bd_addr, &peer))
                {
                    powermode = mode->mode;
                    interval = mode->interval;
                }
            }
            break;
    }
}

void appTestScoFwdLinkStatus(void)
{
    scoFwdTaskData *theScoFwd = GetScoFwd();

    DEBUG_LOG("appTestScoFwdLinkStatus");

    ConnectionGetRole(&testTask,theScoFwd->link_sink);
}

bool appTestScoFwdConnect(void)
{
    bdaddr peer;
    DEBUG_LOG("appTestScoFwdConnect");

    if (!appDeviceGetPeerBdAddr(&peer) || !appConfigScoForwardingEnabled())
        return FALSE;

    ScoFwdConnectPeer(&testTask);
    return TRUE;
}

bool appTestScoFwdDisconnect(void)
{
    DEBUG_LOG("appTestScoFwdDisconnect");

    if (!appConfigScoForwardingEnabled())
        return FALSE;


    ScoFwdDisconnectPeer(&testTask);
    return TRUE;
}

bool appTestPowerOff(void)
{
    DEBUG_LOG("appTestPowerOff");
    return appPowerOffRequest();
}

/* Remove these once feature_if.h has been updated */
#define APTX_ADAPTIVE (40)
#define APTX_ADAPTIVE_MONO (41)

bool appTestLicenseCheck(void)
{
    const uint8 license_table[7] =
    {
        APTX_CLASSIC, APTX_CLASSIC_MONO,
        CVC_RECV, CVC_SEND_HS_1MIC, CVC_SEND_HS_2MIC_MO,
        APTX_ADAPTIVE_MONO
    };
    const bool license_enabled[7] =
    {
        appConfigAptxEnabled(), appConfigAptxEnabled(),
        TRUE, TRUE, TRUE,
        appConfigAptxAdaptiveEnabled()
    };

    bool licenses_ok = TRUE;

    DEBUG_LOG("appTestLicenseCheck");
    for (int i = 0; i < ARRAY_DIM(license_table); i++)
    {
        if (license_enabled[i])
        {
            if (!FeatureVerifyLicense(license_table[i]))
            {
                DEBUG_LOGF("appTestLicenseCheck: License for feature %d not valid", license_table[i]);
                licenses_ok = FALSE;
            }
            else
                DEBUG_LOGF("appTestLicenseCheck: License for feature %d valid", license_table[i]);
        }
    }

    return licenses_ok;
}

void appTestConnectAfterPairing(bool enable)
{
    UNUSED(enable);
    DEBUG_LOG("appTestConnectAfterPairing ** DEPRECATED **");
//    connRulesTaskData *conn_rules = ConnRulesGetTaskData();
//    conn_rules->allow_connect_after_pairing = enable;
}

bool appTestScoFwdForceDroppedPackets(unsigned percentage_to_drop, int multiple_packets)
{
#ifdef INCLUDE_SCOFWD_TEST_MODE
    scoFwdTaskData *theScoFwd = GetScoFwd();

    if (!(0 <= percentage_to_drop && percentage_to_drop <= 100))
    {
        return FALSE;
    }
    if (multiple_packets > 100)
    {
        return FALSE;
    }

    theScoFwd->percentage_to_drop = percentage_to_drop;
    theScoFwd->drop_multiple_packets = multiple_packets;
    return TRUE;
#else
    UNUSED(percentage_to_drop);
    UNUSED(multiple_packets);
    return FALSE;
#endif
}

bool appTestMicFwdLocalMic(void)
{
    if (appConfigScoForwardingEnabled() && appConfigMicForwardingEnabled())
    {
        if (ScoFwdIsSending())
        {
            appKymeraScoUseLocalMic();
            return TRUE;
        }
    }
    return FALSE;
}

bool appTestMicFwdRemoteMic(void)
{
    if (appConfigScoForwardingEnabled() && appConfigMicForwardingEnabled())
    {
        if (ScoFwdIsSending())
        {
            appKymeraScoUseRemoteMic();
            return TRUE;
        }
    }
    return FALSE;
}

static bool appTestPtsModeEnabled;

bool appTestIsPtsMode(void)
{
    return appTestPtsModeEnabled;
}

void appTestSetPtsMode(bool Enabled)
{
    appTestPtsModeEnabled = Enabled;
}

bool appTestPtsSetAsPeer(void)
{
#if 0 // Break PTS Tester pending updates to bt_device VMCSA-1006
    bdaddr addr;
    if (appDeviceGetHandsetBdAddr(&addr))
    {
        appDeviceAttributes attributes;
        if (appDeviceGetHandsetAttributes(&addr, &attributes, NULL))
        {
            bdaddr peer_addr;
            /* Delete peer pairing if it exists */
            if (appDeviceGetPeerBdAddr(&peer_addr))
            {
                ConnectionAuthSetPriorityDevice(&peer_addr, FALSE);
                ConnectionSmDeleteAuthDevice(&peer_addr);
            }

            attributes.type = DEVICE_TYPE_EARBUD;
            attributes.tws_version = DEVICE_TWS_VERSION;
            attributes.supported_profiles = DEVICE_PROFILE_A2DP;
            attributes.flags |= DEVICE_FLAGS_IS_PTS;

            appDeviceSetAttributes(&addr, &attributes);
            DEBUG_LOGF("appTestPtsSetAsPeer, converting device %x,%x,%lx into Peer/PTS pairing",
                       addr.nap, addr.uap, addr.lap);
            DEBUG_LOG("appTestPtsSetAsPeer, rebooting");
            appSmReboot();
            return TRUE;
        }
    }
#endif

    DEBUG_LOG("appTestPtsSetAsPeer, failed");
    return FALSE;
}

/*! \brief Determine if appConfigScoForwardingEnabled is TRUE
*/
bool appTestIsScoFwdIncluded(void)
{
    return appConfigScoForwardingEnabled();
}

/*! \brief Determine if appConfigMicForwardingEnabled is TRUE.
*/
bool appTestIsMicFwdIncluded(void)
{
    return appConfigMicForwardingEnabled();
}

/*! \brief Initiate earbud handset handover */
void appTestInitiateHandover(void)
{
    appSmInitiateHandover();
}

/*! \brief Macro to create function declaration/definition that returns the value
    of the specified configuration.
    \param returntype The type returned by the function.
    \param name Given a configuration named appConfigXYZ(), XYZ should be passed
    to the name argument and a function will be created called appTestConfigXYZ().*/
#define MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(returntype, name) \
returntype appTestConfig##name(void);                   \
returntype appTestConfig##name(void)                    \
{                                                       \
    return appConfig##name();                           \
}

/*! \name Config Accessor functions.
    \note These don't have a public declaration as they are not expected to be
    called within the earbud code - the appConfig functions/macros should be used
    instead.
*/
//!@{
MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(uint16, BatteryFullyCharged)
MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(uint16, BatteryVoltageOk)
MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(uint16, BatteryVoltageLow)
MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(uint16, BatteryVoltageCritical)
MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(int8, BatteryChargingTemperatureMax)
MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(int8, BatteryChargingTemperatureMin)
MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(int8, BatteryDischargingTemperatureMax)
MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(int8, BatteryDischargingTemperatureMin)
MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(uint32, BatteryReadPeriodMs)
MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(uint16, ChargerTrickleCurrent)
MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(uint16, ChargerPreCurrent)
MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(uint16, ChargerPreFastThresholdVoltage)
MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(uint16, ChargerFastCurrent)
MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(uint16, ChargerTerminationCurrent)
MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(uint16, ChargerTerminationVoltage)
MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(uint32, ChargerPreChargeTimeoutMs)
MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(uint32, ChargerFastChargeTimeoutMs)
#ifdef INCLUDE_TEMPERATURE
MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(uint32, TemperatureMeasurementIntervalMs)
#endif
//!@}


uint16 FFA_coefficients[15] = {0xFD66, 0x00C4, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0101, 0xFFE6, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000};
uint16 FFB_coefficients[15] = {0xFE66, 0xFF5F, 0xFFC8, 0x0028, 0x0071, 0x001C, 0xFFC9, 0x00E1, 0xFFEC, 0xFF18, 0xFFF2, 0x0028, 0x0015, 0x0027, 0xFFE6};
uint16 FB_coefficients[15] =  {0x004C, 0xFCE0, 0xFF5E, 0x0118, 0x0094, 0x0016, 0xFFCB, 0x00C6, 0x0100, 0xFDD4, 0xFF4C, 0x0161, 0xFF64, 0xFFFD, 0x0057};
uint16 ANC0_FLP_A_SHIFT_1 = 8; //LPF
uint16 ANC0_FLP_A_SHIFT_2 = 9;
uint16 ANC0_FLP_B_SHIFT_1 = 8;
uint16 ANC0_FLP_B_SHIFT_2 = 8;
uint16 ANC1_FLP_A_SHIFT_1 = 5;
uint16 ANC1_FLP_A_SHIFT_2 = 5;
uint16 ANC1_FLP_B_SHIFT_1 = 5;
uint16 ANC1_FLP_B_SHIFT_2 = 5;
uint16 DC_FILTER_SHIFT = 7;

#include <operator.h>
#include <vmal.h>
#include <micbias.h>
#include <audio_anc.h>
#include <cap_id_prim.h>
#include <opmsg_prim.h>
#include "kymera.h"

extern void appKymeraExternalAmpSetup(void);
extern void appKymeraExternalAmpControl(bool enable);

extern void appTestAncSetFilters(void);
void appTestAncSetFilters(void)
{
    //Source ADC_A = (Source)PanicFalse(StreamAudioSource(AUDIO_HARDWARE_CODEC, AUDIO_INSTANCE_0, AUDIO_CHANNEL_A));
    Source ADC_A = (Source)PanicFalse(StreamAudioSource(AUDIO_HARDWARE_DIGITAL_MIC, AUDIO_INSTANCE_0, AUDIO_CHANNEL_A));

    /* lLad IIRs for both ANC0 and ANC1 */
    PanicFalse(AudioAncFilterIirSet(AUDIO_ANC_INSTANCE_0, AUDIO_ANC_PATH_ID_FFA, 15, FFA_coefficients));
    PanicFalse(AudioAncFilterIirSet(AUDIO_ANC_INSTANCE_0, AUDIO_ANC_PATH_ID_FFB, 15, FFB_coefficients));
    PanicFalse(AudioAncFilterIirSet(AUDIO_ANC_INSTANCE_0, AUDIO_ANC_PATH_ID_FB, 15, FB_coefficients));
    //PanicFalse(AudioAncFilterIirSet(AUDIO_ANC_INSTANCE_1, AUDIO_ANC_PATH_ID_FFA, 15, FFA_coefficients));
    //PanicFalse(AudioAncFilterIirSet(AUDIO_ANC_INSTANCE_1, AUDIO_ANC_PATH_ID_FFB, 15, FFB_coefficients));
    //PanicFalse(AudioAncFilterIirSet(AUDIO_ANC_INSTANCE_1, AUDIO_ANC_PATH_ID_FB, 15, FB_coefficients));

    /* Set LPF filter shifts */
    PanicFalse(AudioAncFilterLpfSet(AUDIO_ANC_INSTANCE_0, AUDIO_ANC_PATH_ID_FFA, ANC0_FLP_A_SHIFT_1, ANC0_FLP_A_SHIFT_2));
    //PanicFalse(AudioAncFilterLpfSet(AUDIO_ANC_INSTANCE_1, AUDIO_ANC_PATH_ID_FFA, ANC1_FLP_A_SHIFT_1, ANC1_FLP_A_SHIFT_2));

    /* Set DC filters */
    PanicFalse(SourceConfigure(ADC_A, STREAM_ANC_FFA_DC_FILTER_ENABLE, 1));
    PanicFalse(SourceConfigure(ADC_A, STREAM_ANC_FFA_DC_FILTER_SHIFT, DC_FILTER_SHIFT));
    //PanicFalse(SourceConfigure(ADC_B, STREAM_ANC_FFA_DC_FILTER_ENABLE, 1));
    //PanicFalse(SourceConfigure(ADC_B, STREAM_ANC_FFA_DC_FILTER_SHIFT, DC_FILTER_SHIFT));

    /* Set LPF gains */
    PanicFalse(SourceConfigure(ADC_A, STREAM_ANC_FFA_GAIN, 128));
    PanicFalse(SourceConfigure(ADC_A, STREAM_ANC_FFA_GAIN_SHIFT, 0));
    //PanicFalse(SourceConfigure(ADC_B, STREAM_ANC_FFA_GAIN, 128));
    //PanicFalse(SourceConfigure(ADC_B, STREAM_ANC_FFA_GAIN_SHIFT, 0));
}

extern void appTestAncSetup(void);
void appTestAncSetup(void)
{
    const uint16 sample_rate = 48000;

    OperatorFrameworkEnable(1);

    /* Set up MICs */
    Source ADC_A = Microphones_TurnOnMicrophone(appConfigAncFeedForwardMic(), sample_rate, high_priority_user);
    Source ADC_B = Microphones_TurnOnMicrophone(appConfigAncFeedBackMic(), sample_rate, high_priority_user);
    SourceSynchronise(ADC_A, ADC_B);

    /* Get the DAC output sinks */
    Sink DAC_L = (Sink)PanicFalse(StreamAudioSink(AUDIO_HARDWARE_CODEC, AUDIO_INSTANCE_0, AUDIO_CHANNEL_A));
    PanicFalse(SinkConfigure(DAC_L, STREAM_CODEC_OUTPUT_RATE, sample_rate));
    PanicFalse(SinkConfigure(DAC_L, STREAM_CODEC_OUTPUT_GAIN, 12));
    Sink DAC_R = (Sink)StreamAudioSink(AUDIO_HARDWARE_CODEC, AUDIO_INSTANCE_0, AUDIO_CHANNEL_B);
    if (DAC_R)
    {
        PanicFalse(SinkConfigure(DAC_R, STREAM_CODEC_OUTPUT_RATE, sample_rate));
        PanicFalse(SinkConfigure(DAC_R, STREAM_CODEC_OUTPUT_GAIN, 12));
    }

    /* Set DAC gains */
    PanicFalse(SinkConfigure(DAC_L, STREAM_CODEC_RAW_OUTPUT_GAIN, 7));
    if (DAC_R)
        PanicFalse(SinkConfigure(DAC_R, STREAM_CODEC_RAW_OUTPUT_GAIN, 7));

    /* feedforward or feedback with analog mics */
    PanicFalse(SourceConfigure(ADC_A, STREAM_ANC_INSTANCE, AUDIO_ANC_INSTANCE_0));
    if (DAC_R && ADC_B)
        PanicFalse(SourceConfigure(ADC_B, STREAM_ANC_INSTANCE, AUDIO_ANC_INSTANCE_1));
    PanicFalse(SourceConfigure(ADC_A, STREAM_ANC_INPUT, AUDIO_ANC_PATH_ID_FFA));
    if (DAC_R && ADC_B)
        PanicFalse(SourceConfigure(ADC_B, STREAM_ANC_INPUT, AUDIO_ANC_PATH_ID_FFA));

    /* Associate DACS */
    PanicFalse(SinkConfigure(DAC_L, STREAM_ANC_INSTANCE, AUDIO_ANC_INSTANCE_0));
    if (DAC_R && ADC_B)
        PanicFalse(SinkConfigure(DAC_R, STREAM_ANC_INSTANCE, AUDIO_ANC_INSTANCE_1));

    /* Setup ANC filters */
    appTestAncSetFilters();

    /* Turn on ANC */
    PanicFalse(AudioAncStreamEnable(0x9, DAC_R && ADC_B ? 0x09 : 0x0));

    appKymeraExternalAmpSetup();
    appKymeraExternalAmpControl(TRUE);
}

#define GAIN_DB_TO_Q6N_SF (11146541)
#define GAIN_DB(x)      ((int32)(GAIN_DB_TO_Q6N_SF * (x)))

extern void appTestAudioPassthrough(void);
void appTestAudioPassthrough(void)
{
    const uint16 sample_rate = 48000;
    const int32 initial_gain = GAIN_DB(0);

    OperatorFrameworkEnable(1);

    /* Set up MICs */
    Source ADC_A = Microphones_TurnOnMicrophone(appConfigAncFeedForwardMic(), sample_rate, high_priority_user);
    Source ADC_B = Microphones_TurnOnMicrophone(appConfigAncFeedBackMic(), sample_rate, high_priority_user);
    SourceSynchronise(ADC_A, ADC_B);

    /* Get the DAC output sinks */
    Sink DAC_L = (Sink)PanicFalse(StreamAudioSink(AUDIO_HARDWARE_CODEC, AUDIO_INSTANCE_0, AUDIO_CHANNEL_A));
    PanicFalse(SinkConfigure(DAC_L, STREAM_CODEC_OUTPUT_RATE, sample_rate));
    PanicFalse(SinkConfigure(DAC_L, STREAM_CODEC_OUTPUT_GAIN, 12));
    Sink DAC_R = (Sink)StreamAudioSink(AUDIO_HARDWARE_CODEC, AUDIO_INSTANCE_0, AUDIO_CHANNEL_B);
    if (DAC_R)
    {
        PanicFalse(SinkConfigure(DAC_R, STREAM_CODEC_OUTPUT_RATE, sample_rate));
        PanicFalse(SinkConfigure(DAC_R, STREAM_CODEC_OUTPUT_GAIN, 12));
    }

    /* Set DAC gains */
    PanicFalse(SinkConfigure(DAC_L, STREAM_CODEC_RAW_OUTPUT_GAIN, 7));
    if (DAC_R)
        PanicFalse(SinkConfigure(DAC_R, STREAM_CODEC_RAW_OUTPUT_GAIN, 7));

    /* Now create the operator for routing the audio */
    Operator passthrough = (Operator)PanicFalse(VmalOperatorCreate(CAP_ID_BASIC_PASS));

    /* Configure the operators
     * Using the information from CS-312572-UG, Kymera Capability
     * Library User Guide
     */
    uint16 set_gain[] = { OPMSG_COMMON_ID_SET_PARAMS, 1, 1, 1, UINT32_MSW(initial_gain), UINT32_LSW(initial_gain) };
    PanicFalse(VmalOperatorMessage(passthrough, set_gain, 6, NULL, 0));

    /* And connect everything */
    /* ...line_in to the passthrough */
    PanicFalse(StreamConnect(ADC_A, StreamSinkFromOperatorTerminal(passthrough, 0)));
    if (ADC_B && DAC_R)
        PanicFalse(StreamConnect(ADC_B, StreamSinkFromOperatorTerminal(passthrough, 1)));

    /* ...and passthrough to line out */
    PanicFalse(StreamConnect(StreamSourceFromOperatorTerminal(passthrough, 0), DAC_L));
    if (DAC_R)
        PanicFalse(StreamConnect(StreamSourceFromOperatorTerminal(passthrough, 1), DAC_R));

    /* Finally start the operator */
    PanicFalse(OperatorStartMultiple(1, &passthrough, NULL));

    appKymeraExternalAmpSetup();
    appKymeraExternalAmpControl(TRUE);
}



#include <usb_hub.h>

extern void appTestUsbAudioPassthrough(void);
void appTestUsbAudioPassthrough(void)
{
    const uint16 sample_rate = 48000;

    OperatorFrameworkEnable(1);

    /* Attach USB */
    //UsbHubAttach();

    /* Get the DAC output sinks */
    Sink DAC_L = (Sink)PanicFalse(StreamAudioSink(AUDIO_HARDWARE_CODEC, AUDIO_INSTANCE_0, AUDIO_CHANNEL_A));
    PanicFalse(SinkConfigure(DAC_L, STREAM_CODEC_OUTPUT_RATE, sample_rate));
    PanicFalse(SinkConfigure(DAC_L, STREAM_CODEC_OUTPUT_GAIN, 12));
    Sink DAC_R = (Sink)StreamAudioSink(AUDIO_HARDWARE_CODEC, AUDIO_INSTANCE_0, AUDIO_CHANNEL_B);
    if (DAC_R)
    {
        PanicFalse(SinkConfigure(DAC_R, STREAM_CODEC_OUTPUT_RATE, sample_rate));
        PanicFalse(SinkConfigure(DAC_R, STREAM_CODEC_OUTPUT_GAIN, 12));
    }

    /* Set DAC gains */
    PanicFalse(SinkConfigure(DAC_L, STREAM_CODEC_RAW_OUTPUT_GAIN, 7));
    if (DAC_R)
        PanicFalse(SinkConfigure(DAC_R, STREAM_CODEC_RAW_OUTPUT_GAIN, 7));

    /* Now create the operator for routing the audio */
    Operator usb_rx = (Operator)PanicFalse(VmalOperatorCreate(CAP_ID_USB_AUDIO_RX));

    /* Configure the operators
     * Using the information from CS-312572-UG, Kymera Capability
     * Library User Guide
     */
    uint16 set_config[] =
    {
        OPMSG_USB_AUDIO_ID_SET_CONNECTION_CONFIG,
        0,                  // data_format
        sample_rate / 25,   // sample_rate
        1,                  // number_of_channels
        2 * 8,              // subframe_size
        2 * 8               // subframe_resolution
    };

    PanicFalse(VmalOperatorMessage(usb_rx, set_config, 6, NULL, 0));

    /* And connect everything */
    /* ...USB sournce to USB Rx operator */
    PanicFalse(StreamConnect(StreamUsbEndPointSource(end_point_iso_in), StreamSinkFromOperatorTerminal(usb_rx, 0)));

    /* ...and USB Rx operator to line out */
    PanicFalse(StreamConnect(StreamSourceFromOperatorTerminal(usb_rx, 0), DAC_L));
    if (DAC_R)
        PanicFalse(StreamConnect(StreamSourceFromOperatorTerminal(usb_rx, 1), DAC_R));

    /* Finally start the operator */
    PanicFalse(OperatorStartMultiple(1, &usb_rx, NULL));

    appKymeraExternalAmpSetup();
    appKymeraExternalAmpControl(TRUE);
}

#define ANC_TUNING_SINK_USB_LEFT      0 /*can be any other backend device. PCM used in this tuning graph*/
#define ANC_TUNING_SINK_MIC1_LEFT     4 /* must be connected to internal ADC. Analog or digital */

extern void appTestAncTuningSetSource(void);
void appTestAncTuningSetSource(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    uint16 set_source_control[8] =
    {
        0x2002, 2,
        6, 0, ANC_TUNING_SINK_MIC1_LEFT,
        8, 0, ANC_TUNING_SINK_USB_LEFT,
    };
    PanicFalse(VmalOperatorMessage(theKymera->anc_tuning,
                                   &set_source_control, SIZEOF_OPERATOR_MESSAGE(set_source_control),
                                   NULL, 0));
}


void appTestEnterDfuWhenEnteringCase(bool unused)
{
    bool enter_dfu_logical_input_sent = FALSE;

    DEBUG_LOG("appTestEnterDfuWhenEnteringCase");

    UNUSED(unused);

    if (appPeerSigIsConnected() && appSmIsOutOfCase())
    {
        MessageSend(LogicalInputSwitch_GetTask(), APP_BUTTON_DFU, NULL);
        enter_dfu_logical_input_sent = TRUE;
    }

    DEBUG_LOG("appTestEnterDfuWhenEnteringCase cmd_sent=%d", enter_dfu_logical_input_sent);
}

bool appTestIsInitialisationCompleted(void)
{
    DEBUG_LOG("appTestIsInitialisationCompleted");

    return appInitCompleted();
}

bool appTestIsPrimary(void)
{
    bool prim = appSmIsPrimary();

    DEBUG_LOG("appTestIsPrimary: %d",prim);

    return prim;
}


bool appTestIsTopologyRole(app_test_topology_role_enum_t checked_role)
{
    bool role_matches = FALSE;
    tws_topology_role role = TwsTopology_GetRole();

    switch (checked_role)
    {
        case app_test_topology_role_none:
            role_matches = (role == tws_topology_role_none);
            DEBUG_LOG("appTestIsTopologyRole. No role:%d", role_matches);
            break;

        case app_test_topology_role_dfu:
            role_matches = (role == tws_topology_role_dfu);
            DEBUG_LOG("appTestIsTopologyRole. DFU:%d", role_matches);
            break;

        case app_test_topology_role_any_primary:
            role_matches = (role == tws_topology_role_primary);
            DEBUG_LOG("appTestIsTopologyRole. Primary:%d (Acting:%d)", 
                            role_matches, TwsTopology_IsActingPrimary());
            break;

        case app_test_topology_role_primary:
            role_matches = TwsTopology_IsFullPrimary();
            DEBUG_LOG("appTestIsTopologyRole. Primary(Full):%d", role_matches);
            break;

        case app_test_topology_role_acting_primary:
            role_matches = TwsTopology_IsActingPrimary();
            DEBUG_LOG("appTestIsTopologyRole. Acting Primary:%d", role_matches);
            break;

        case app_test_topology_role_secondary:
            role_matches = (role == tws_topology_role_secondary);
            DEBUG_LOG("appTestIsTopologyRole. Secondary:%d", role_matches);
            break;

        default:
            DEBUG_LOG("appTestIsTopologyRole. Unsupported role:%d",checked_role);
            Panic();
            break;
    }

    return role_matches;
}


bool appTestIsTopologyIdle(void)
{
    bool idle = (TwsTopology_GetRole() == app_test_topology_role_none);

    DEBUG_LOG("appTestIsTopologyIdle:%d", idle);
    return idle;
}

static void earbudTest_ReportProperty(device_t device, device_property_t property)
{
    if (Device_IsPropertySet(device,property))
    {
        uint8 value = 0;
        uint16 u16_value = 0;
        switch (property)
        {
        case device_property_bdaddr:
            {
                bdaddr bd_addr = DeviceProperties_GetBdAddr(device);
                DEBUG_LOG("\tbdaddr nap %x uap %x lap %x", bd_addr.nap, bd_addr.uap, bd_addr.lap);
            }
            break;
        case device_property_a2dp_volume:
            Device_GetPropertyU8(device, property, &value);
            DEBUG_LOG("\ta2dp_volume %02x", value);
            break;
        case device_property_hfp_profile:
            Device_GetPropertyU8(device, property, &value);
            DEBUG_LOG("\thfp_profile %02x", value);
            break;
        case device_property_type:
            Device_GetPropertyU8(device, property, &value);
            DEBUG_LOG("\ttype %02x", value);
            break;
        case device_property_link_mode:
            Device_GetPropertyU8(device, property, &value);
            DEBUG_LOG("\tlink_mode %02x", value);
            break;
        case device_property_supported_profiles:
            Device_GetPropertyU8(device, property, &value);
            DEBUG_LOG("\tsupported_profiles %02x", value);
            break;
        case device_property_last_connected_profiles:
            Device_GetPropertyU8(device, property, &value);
            DEBUG_LOG("\tlast_connected_profiles %02x", value);
            break;
        case device_property_connected_profiles:
            Device_GetPropertyU8(device, property, &value);
            DEBUG_LOG("\tcurrently connected_profiles %02x", value);
            break;
        case device_property_tws_version:
            Device_GetPropertyU16(device, property, &u16_value);
            DEBUG_LOG("\ttws_version %04x", u16_value);
            break;
        case device_property_flags:
            Device_GetPropertyU16(device, property, &u16_value);
            DEBUG_LOG("\tflags %04x", u16_value);
            break;
        case device_property_sco_fwd_features:
            Device_GetPropertyU16(device, property, &u16_value);
            DEBUG_LOG("\tsco_fwd_features %04x", u16_value);
            break;
        case device_property_mru:
            Device_GetPropertyU8(device, property, &value);
            DEBUG_LOG("\tmru %1x", value);
            break;
        case device_property_profile_request_index:
            Device_GetPropertyU8(device, property, &value);
            DEBUG_LOG("\tprofile_request_index %1x", value);
            break;
        case device_property_profiles_connect_order:
            {
                profile_t *ptr_profile_order = NULL;
                size_t size;
                PanicFalse(Device_GetProperty(device, property, (void *)&ptr_profile_order, &size));
                profile_t array[5] = {profile_manager_bad_profile};
                memcpy(array, ptr_profile_order, MIN(size,5));
                DEBUG_LOG("\tprofiles_connect_order %d,%d,%d,%d,%d", array[0], array[1], array[2], array[3], array[4]);
            }
            break;
        default:
            break;
        }
    }
}

static void earbudTest_ReportDeviceData(device_t device, void * data)
{
    UNUSED(data);

    DEBUG_LOG("Device %x", device);
    for (int property=0; property < device_property_max_num; property++)
    {
        if (Device_IsPropertySet(device, property))
        {
            earbudTest_ReportProperty(device, property);
        }
    }
}

void EarbudTest_DeviceDatabaseReport(void)
{
    DEBUG_LOG("DeviceDatabase");
    DeviceList_Iterate(earbudTest_ReportDeviceData, NULL);
}

extern uint8 profile_list[4];

void EarbudTest_ConnectHandset(void)
{
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

void EarbudTest_DisconnectHandset(void)
{
    bool is_mru_handset = TRUE;
    device_t handset_device = DeviceList_GetFirstDeviceWithPropertyValue(device_property_mru, &is_mru_handset, sizeof(uint8));
    if (handset_device)
    {
        Device_SetProperty(handset_device, device_property_profiles_disconnect_order, profile_list, sizeof(profile_list));
        ProfileManager_DisconnectProfilesRequest(&SmGetTaskData()->task, handset_device);
    }
    else
    {
        bdaddr handset_address = {0,0,0};
        if (appDeviceGetHandsetBdAddr(&handset_address))
        {
            handset_device = BtDevice_GetDeviceForBdAddr(&handset_address);
            Device_SetProperty(handset_device, device_property_profiles_disconnect_order, profile_list, sizeof(profile_list));
            ProfileManager_DisconnectProfilesRequest(&SmGetTaskData()->task, handset_device);
        }
    }
}

bool appTestPrimaryAddressIsFromThisBoard(void)
{
    bdaddr self;
    uint16 flags;
    bool is_primary = FALSE;

    HYDRA_LOG_STRING(mine, "MINE");
    HYDRA_LOG_STRING(peers, "PEERS");


    appDeviceGetMyBdAddr(&self);

    if (appDeviceGetFlags(&self, &flags))
    {
        if (flags & DEVICE_FLAGS_SHADOWING_C_ROLE)
        {
            is_primary = TRUE;
        }
    }

    DEBUG_LOG("appTestPrimaryAddressIsFromThisBoard %04x%02x%06x %s", 
            self.nap, self.uap, self.lap,
            is_primary ? mine : peers);

    return is_primary;
}

void EarbudTest_PeerFindRoleOverrideScore(uint16 score)
{
    DEBUG_LOG("EarbudTest_PeerFindRoleOverrideScore 0x%x", score);
    PeerFindRole_OverrideScore((grss_figure_of_merit_t)score);
}

/* private API added to upgrade library */
extern bool UpgradePSClearStore(void);
bool appTestUpgradeResetState(void)
{
    DEBUG_LOG("appTestUpgradeResetState");

    return UpgradePSClearStore();
}

void EarbudTest_EnterInCaseDfu(void)
{
    DEBUG_LOG("EarbudTest_EnterInCaseDfu");

    appSmEnterDfuModeInCase(TRUE);
    appTestPhyStateInCaseEvent();
}

uint16 appTestSetTestNumber(uint16 test_number)
{
    test_support.testcase = test_number;

    return test_number;
}

uint16 appTestSetTestIteration(uint16 test_iteration)
{
    test_support.iteration = test_iteration;

    return test_iteration;
}

uint16 appTestWriteMarker(uint16 marker)
{
    static unsigned testcase = 0;
    test_support.last_marker = marker;

    if (   test_support.testcase 
        && (  test_support.testcase != testcase
           || marker == 0))
    {
        testcase = test_support.testcase;

        if (test_support.iteration)
        {
            DEBUG_LOG("@@@Testcase:%d  Iteration:%d -------------------------------");
        }
        else
        {
            DEBUG_LOG("@@@Testcase:%d  ------------------------------------------");
        }
    }

    if (marker)
    {
        if (test_support.testcase && test_support.iteration)
        {
            DEBUG_LOG("@@@Testcase marker: TC%d Iteration:%d Step:%d *************************",
                    testcase, test_support.iteration, marker);
        }
        else if (test_support.testcase)
        {
            DEBUG_LOG("@@@Testcase marker: TC%d Step:%d *************************",
                    testcase, marker);
        }
        else
        {
            DEBUG_LOG("@@@Testcase marker: Iteration:%d Step:%d *************************",
                    test_support.iteration, marker);
        }
    }

    return marker;
}

