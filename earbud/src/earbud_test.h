/*!
\copyright  Copyright (c) 2017 - 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       earbud_test.h
\brief      Interface for specifc application testing functions
*/

#ifndef EARBUD_TEST_H
#define EARBUD_TEST_H

#include <bdaddr.h>
#include <earbud_sm.h>
#include <peer_find_role.h>

#include <anc.h>

#define MESSAGE_BATTERY_PROCESS_READING     0x40

/*! Test value for battery voltage. If non-zero, the function
    appBatteryGetVoltage() will return this value instead of
    a real reading.
 */
extern uint16 appTestBatteryVoltage;
extern bool appTestHandsetPairingBlocked;
extern TaskData testTask;

/*! \brief Returns the current battery voltage
 */
uint16 appTestGetBatteryVoltage(void);

/*! \brief Sets the battery voltage to be returned by
    appBatteryGetVoltage().

    This test function also causes the battery module to read
    this new voltage level - which may be subjected to hysteresis.
 */
void appTestSetBatteryVoltage(uint16 new_level);

/*! \brief Return TRUE if the current battery state is battery_level_ok. */
bool appTestBatteryStateIsOk(void);

/*! \brief Return TRUE if the current battery state is battery_level_low. */
bool appTestBatteryStateIsLow(void);

/*! \brief Return TRUE if the current battery state is battery_level_critical. */
bool appTestBatteryStateIsCritical(void);

/*! \brief Return TRUE if the current battery state is battery_level_too_low. */
bool appTestBatteryStateIsTooLow(void);

/*! \brief Return the number of milliseconds taken for the battery measurement
    filter to fully respond to a change in input battery voltage */
uint32 appTestBatteryFilterResponseTimeMs(void);

#ifdef HAVE_THERMISTOR
/*! \brief Returns the expected thermistor voltage at a specified temperature.
    \param temperature The specified temperature in degrees Celsius.
    \return The equivalent milli-voltage.
*/
uint16 appTestThermistorDegreesCelsiusToMillivolts(int8 temperature);
#endif

/*! \brief Put Earbud into Handset Pairing mode
*/
void appTestPairHandset(void);

/*! \brief Delete all Handset pairing
*/
void appTestDeleteHandset(void);

/*! \brief Delete Earbud peer pairing

    Attempts to delete pairing to earbud. Check the debug output if
    this test command fails, as it will report failure reason.

    \return TRUE if pairing was successfully removed.
            FALSE if there is no peer pairing, or still connected to the device.
*/
bool appTestDeletePeer(void);

/*! \brief Get the devices peer bluetooth address

    \param  peer_address    pointer to the peer paired address if found
    \return TRUE if the device is peer paired
            FALSE if device is not peer paired
*/
bool appTestGetPeerAddr(bdaddr *peer_address);

/*! \brief Return if Earbud is in a Pairing mode

    \return bool TRUE Earbud is in pairing mode
                 FALSE Earbud is not in pairing mode
*/
bool appTestIsPairingInProgress(void);

/*! \brief Initiate Earbud A2DP connection to the Handset

    \return bool TRUE if A2DP connection is initiated
                 FALSE if no handset is paired
*/
bool appTestHandsetA2dpConnect(void);

/*! \brief Stop the earbud automatically pairing with a handset

    Rules that permit pairing will be stopped while a block is in
    place.

    \param  block   Enable/Disable the block
*/
void appTestBlockAutoHandsetPairing(bool block);

/*! \brief Return if Earbud has a handset pairing

    \return TRUE Earbud is paired with at least one handset, 
            FALSE otherwise
*/
bool appTestIsHandsetPaired(void);

/*! \brief Return if Earbud has an Handset A2DP connection

    \return bool TRUE Earbud has A2DP Handset connection
                 FALSE Earbud does not have A2DP Handset connection
*/
bool appTestIsHandsetA2dpConnected(void);

/*! \brief Return if Earbud has an Handset A2DP media connection

    \return bool TRUE Earbud has A2DP Handset media connection
                 FALSE Earbud does not have A2DP Handset connection
*/
bool appTestIsHandsetA2dpMediaConnected(void);

/*! \brief Return if Earbud is in A2DP streaming mode with the handset

    \return bool TRUE Earbud is in A2DP streaming mode
                     FALSE Earbud is not in A2DP streaming mode
*/
bool appTestIsHandsetA2dpStreaming(void);

/*! \brief Initiate Earbud AVRCP connection to the Handset

    \return bool TRUE if AVRCP connection is initiated
                 FALSE if no handset is paired
*/


bool appTestIsA2dpPlaying(void);

bool appTestHandsetAvrcpConnect(void);

/*! \brief Return if Earbud has an Handset AVRCP connection

    \return bool TRUE Earbud has AVRCP Handset connection
                 FALSE Earbud does not have AVRCP Handset connection
*/
bool appTestIsHandsetAvrcpConnected(void);

/*! \brief Initiate Earbud HFP connection to the Handset

    \return bool TRUE if HFP connection is initiated
                 FALSE if no handset is paired
*/
bool appTestHandsetHfpConnect(void);

/*! \brief Return if Earbud has an Handset HFP connection

    \return bool TRUE Earbud has HFP Handset connection
                 FALSE Earbud does not have HFP Handset connection
*/
bool appTestIsHandsetHfpConnected(void);

/*! \brief Return if Earbud has an Handset HFP SCO connection

    \return bool TRUE Earbud has HFP SCO Handset connection
                 FALSE Earbud does not have HFP SCO Handset connection
*/
bool appTestIsHandsetHfpScoActive(void);

/*! \brief Initiate Earbud HFP Voice Dial request to the Handset
*/
void appTestHandsetHfpVoiceDial(void);

/*! \brief Toggle Microphone mute state on HFP SCO conenction to handset
*/
void appTestHandsetHfpMuteToggle(void);

/*! \brief Transfer HFP voice to the Handset
*/
void appTestHandsetHfpVoiceTransferToAg(void);

/*! \brief Transfer HFP voice to the Earbud
*/
void appTestHandsetHfpVoiceTransferToHeadset(void);

/*! \brief Accept incoming call, either local or forwarded (SCO forwarding)
*/
void appTestHandsetHfpCallAccept(void);

/*! \brief Reject incoming call, either local or forwarded (SCO forwarding)
*/
void appTestHandsetHfpCallReject(void);

/*! \brief End the current call, either local or forwarded (SCO forwarding)
*/
void appTestHandsetHfpCallHangup(void);

/*! \brief Initiated last number redial

    \return bool TRUE if last number redial was initiated
                 FALSE if HFP is not connected
*/
bool appTestHandsetHfpCallLastDialed(void);

/*! \brief Start decreasing the HFP volume
*/
void appTestHandsetHfpVolumeDownStart(void);

/*! \brief Start increasing the HFP volume
*/
void appTestHandsetHfpVolumeUpStart(void);

/*! \brief Stop increasing or decreasing HFP volume
*/
void appTestHandsetHfpVolumeStop(void);

/*! \brief Set the Hfp Sco volume

    \return bool TRUE if the volume set request was initiated
                 FALSE if HFP is not in a call
*/
bool appTestHandsetHfpSetScoVolume(uint8 volume);

/*! \brief Get microphone mute status

    \return bool TRUE if microphone muted,
                 FALSE if not muted
*/
bool appTestIsHandsetHfpMuted(void);

/*! \brief Check if call is in progress

    \return bool TRUE if call in progress,
                 FALSE if no call, or not connected
*/
bool appTestIsHandsetHfpCall(void);

/*! \brief Check if incoming call

    \return bool TRUE if call incoming,
                 FALSE if no call, or not connected
*/
bool appTestIsHandsetHfpCallIncoming(void);

/*! \brief Check if outgoing call

    \return bool TRUE if call outgoing,
                 FALSE if no call, or not connected
*/
bool appTestIsHandsetHfpCallOutgoing(void);

/*! \brief Return if Earbud has an ACL connection to the Handset

    It does not indicate if the handset is usable, with profiles
    connected. Use appTestIsHandsetConnected or 
    appTestIsHandsetFullyConnected.

    \return bool TRUE Earbud has an ACL connection
                 FALSE Earbud does not have an ACL connection to the Handset
*/
bool appTestIsHandsetAclConnected(void);

/*! \brief Return if Earbud has a profile connection to the Handset

    This can be HFP, A2DP or AVRCP. It does not indicate if there
    is an ACL connection.

    \return bool TRUE Earbud has a connection to the Handset
                 FALSE Earbud does not have a connection to the Handset
*/
bool appTestIsHandsetConnected(void);

/*! \brief Is the handset completely connected (all profiles)

    This function checks whether the handset device is connected
    completely. Unlike appTestIsHandsetConnected() this function
    checks that all the handset profiles required are connected.

    \return TRUE if there is a handset, all required profiles
        are connected, and we have not started disconnecting. 
        FALSE in all other cases.
 */
bool appTestIsHandsetFullyConnected(void);

/*! \brief Initiate Earbud A2DP connection to the the Peer

    \return bool TRUE if A2DP connection is initiated
                 FALSE if no Peer is paired
*/
bool appTestPeerA2dpConnect(void);

/*! \brief Return if Earbud has a Peer A2DP connection

    \return bool TRUE Earbud has A2DP Peer connection
                 FALSE Earbud does not have A2DP Peer connection
*/
bool appTestIsPeerA2dpConnected(void);

/*! \brief Check if Earbud is in A2DP streaming mode with peer Earbud

    \return TRUE if A2DP streaming to peer device
*/
bool appTestIsPeerA2dpStreaming(void);

/*! \brief Initiate Earbud AVRCP connection to the the Peer

    \return bool TRUE if AVRCP connection is initiated
                 FALSE if no Peer is paired
*/
bool appTestPeerAvrcpConnect(void);

/*! \brief Return if Earbud has a Peer AVRCP connection

    \return bool TRUE Earbud has AVRCP Peer connection
                 FALSE Earbud does not have AVRCP Peer connection
*/
bool appTestIsPeerAvrcpConnected(void);

/*! \brief Send the AV toggle play/pause command
*/
void appTestAvTogglePlayPause(void);

/*! \brief Start dynamic handover procedure.
    \return TRUE if handover was initiated, otherwise FALSE.
*/
bool earbudTest_DynamicHandover(void);

/*! \brief Send the Avrcp pause command to the Handset
*/
void appTestAvPause(void);

/*! \brief Send the Avrcp play command to the Handset
*/
void appTestAvPlay(void);

/*! \brief Send the Avrcp stop command to the Handset
*/
void appTestAvStop(void);

/*! \brief Send the Avrcp forward command to the Handset
*/
void appTestAvForward(void);

/*! \brief Send the Avrcp backward command to the Handset
*/
void appTestAvBackward(void);

/*! \brief Send the Avrcp fast forward state command to the Handset
*/
void appTestAvFastForwardStart(void);

/*! \brief Send the Avrcp fast forward stop command to the Handset
*/
void appTestAvFastForwardStop(void);

/*! \brief Send the Avrcp rewind start command to the Handset
*/
void appTestAvRewindStart(void);

/*! \brief Send the Avrcp rewind stop command to the Handset
*/
void appTestAvRewindStop(void);

/*! \brief Send the Avrcp volume change command to the Handset

    \param step Step change to apply to volume

    \return bool TRUE volume step change sent
                 FALSE volume step change was not sent
*/
bool appTestAvVolumeChange(int8 step);

/*! \brief Send the Avrcp pause command to the Handset

    \param volume   New volume level to set (0-127).
*/
void appTestAvVolumeSet(uint8 volume);

/*! \brief Allow tests to control whether the earbud will enter dormant.

    If an earbud enters dormant, cannot be woken by a test.

    \note Even if dormant mode is enabled, the application may not
        enter dormant. Dormant won't be used if the application is
        busy, or connected to a charger - both of which are quite
        likely for an application under test.

    \param  enable  Use FALSE to disable dormant, or TRUE to enable.
*/
void appTestPowerAllowDormant(bool enable);

/*! \brief Generate event that Earbud is now in the case. */
void appTestPhyStateInCaseEvent(void);

/*! \brief Generate event that Earbud is now out of the case. */
void appTestPhyStateOutOfCaseEvent(void);

/*! \brief Generate event that Earbud is now in ear. */
void appTestPhyStateInEarEvent(void);

/*! \brief Generate event that Earbud is now out of the ear. */
void appTestPhyStateOutOfEarEvent(void);

/*! \brief Generate event that Earbud is now moving */
void appTestPhyStateMotionEvent(void);

/*! \brief Generate event that Earbud is now not moving. */
void appTestPhyStateNotInMotionEvent(void);

/*! \brief Generate event that Earbud is now (going) off. */
void appTestPhyStateOffEvent(void);

/*! \brief Return TRUE if the earbud is in the ear. */
bool appTestPhyStateIsInEar(void);

/*! \brief Return TRUE if the earbud is out of the ear, but not in the case. */
bool appTestPhyStateOutOfEar(void);

/*! \brief Return TRUE if the earbud is in the case. */
bool appTestPhyStateIsInCase(void);

/*! \brief Reset an Earbud to factory defaults.
    Will drop any connections, delete all pairing and reboot.
*/
void appTestFactoryReset(void);

/*! \brief Determine if a reset has happened
    Will return TRUE until cleared by appTestResetHappenedClear()
*/
bool appTestResetHappened(void);

/*! \brief Connect to default handset. */
void appTestConnectHandset(void);

/*! \brief Connect the A2DP media channel to the handset
    \return True is request sent, else false
 */
bool appTestConnectHandsetA2dpMedia(void);

/*! \brief  Check if peer synchronisation was successful

    \returns TRUE if we are synchronised with the peer.
*/
bool appTestIsPeerSyncComplete(void);

/*! \brief Power off.
    \return TRUE if the device can power off - the device will drop connections then power off.
            FALSE if the device cannot currently power off.
*/
bool appTestPowerOff(void);

/*! \brief Determine if the earbud has a paired peer earbud.
    \return bool TRUE the earbud has a paired peer, FALSE the earbud has no paired peer.
*/
bool appTestIsPeerPaired(void);

/*! \brief Determine if the all licenses are correct.
    \return bool TRUE the licenses are correct, FALSE if not.
*/
bool appTestLicenseCheck(void);

/*! \brief Control 2nd earbud connecting to handset after TWS+ pairing.
    \param bool TRUE enable 2nd earbud connect to handset.
                FALSE disable 2nd earbud connect to handset, handset must connect.
    \note This API is deprecated, the feature is no longer supported.
 */
void appTestConnectAfterPairing(bool enable);

/*! \brief Asks the connection library about the sco forwarding link.

    The result is reported as debug.
 */
void appTestScoFwdLinkStatus(void);

/* \brief Enable or disable randomised dropping of SCO forwarding packets

    This function uses random numbers to stop transmissio of SCO forwarding
    packets, so causing error mechanisms to be used on the receiving side.
    Packet Loss Concealment (PLC) and possibly audio muting or disconnection.

    There are two modes.
    - set a percentage chance of a packet being dropped,
        if the previous packet was dropped
    - set the number of consecutive packets to drop every time.
        set this using a negative value for multiple_packets

    \param percentage        The random percentage of packets to not transmit
    \param multiple_packets  A negative number indicates the number of consecutive
                             packets to drop. \br 0, or a positive number indicates the
                             percentage chance a packet should drop after the last
                             packet was dropped. */
bool appTestScoFwdForceDroppedPackets(unsigned percentage_to_drop, int multiple_packets);

/*! \brief Requests that the L2CAP link used for SCO forwarding is connected.

    \returns FALSE if the device is not paired to another earbud, TRUE otherwise
 */
bool appTestScoFwdConnect(void);

/*! \brief Requests that the L2CAP link used for SCO forwarding is disconnected.

    \returns TRUE
 */
bool appTestScoFwdDisconnect(void);

/*! \brief Selects the local microphone for MIC forwarding

    Preconditions
    - in an HFP call
    - MIC forwarding enabled in the build
    - function called on the device connected to the handset

    \returns TRUE if the preconditions are met, FALSE otherwise

    \note will return TRUE even if the local MIC is currently selected
 */
bool appTestMicFwdLocalMic(void);

/*! \brief Selects the remote (forwarded) microphone for MIC forwarding

    Preconditions
    - in an HFP call
    - MIC forwarding enabled in the build
    - function called on the device connected to the handset

    \returns TRUE if the preconditions are met, FALSE otherwise

    \note will return TRUE even if the remote MIC is currently selected
 */
bool appTestMicFwdRemoteMic(void);

/*! \brief configure the Pts device as a peer device */
bool appTestPtsSetAsPeer(void);

/*! \brief Are we running in PTS test mode */
bool appTestIsPtsMode(void);

/*! \brief Set or clear PTS test mode */
void appTestSetPtsMode(bool);

/*! \brief Determine if appConfigScoForwardingEnabled is TRUE
    \return bool Return value of appConfigScoForwardingEnabled.
*/
bool appTestIsScoFwdIncluded(void);

/*! \brief Determine if appConfigMicForwardingEnabled is TRUE.
    \return bool Return value of appConfigMicForwardingEnabled.
*/
bool appTestIsMicFwdIncluded(void);

/*! \brief Initiate earbud handset handover */
void appTestInitiateHandover(void);


/*! Handler for connection library messages
    This function is called to handle connection library messages used for testing.
    If a message is processed then the function returns TRUE.

    \param  id              Identifier of the connection library message
    \param  message         The message content (if any)
    \param  already_handled Indication whether this message has been processed by
                            another module. The handler may choose to ignore certain
                            messages if they have already been handled.

    \returns TRUE if the message has been processed, otherwise returns the
        value in already_handled
 */
extern bool appTestHandleConnectionLibraryMessages(MessageId id, Message message,
                                                   bool already_handled);

/*! Set flag so that DFU mode is entered when the device next goes "in case"

    This test function injects the Logical Input (i.e. mimics the UI button press event)
    corresponding to "enter DFU mode when the Earbuds are next placed in the case".

    \param unused - kept for backwards compatibility
*/
void appTestEnterDfuWhenEnteringCase(bool unused);

/*! Has the application infrastructure initialised

    When starting, the application initialises the modules it uses.
    This function checks if the sequence of module initialisation has completed

    \note When this function returns TRUE it does not indicate that the application
    is fully initialised. That would depend on the application state, and the
    status of the device.

    \returns TRUE if initialisation completed
 */
bool appTestIsInitialisationCompleted(void);

/*! Determine if the Earbud is currently primary.

    \return TRUE Earbud is Primary or Acting Primary. FALSE in all
             other cases

    \note A return value of FALSE for one Earbud DOES NOT imply it is
        secondary. See \ref appTestIsSecondary
*/
bool appTestIsPrimary(void);

/*! Determine if the Earbud is the Right Earbud.

    \return TRUE Earbud is the Right Earbud. FALSE Earbud is the Left Earbud
*/
bool appTestIsRight(void);

typedef enum
{
    app_test_topology_role_none,
    app_test_topology_role_dfu,
    app_test_topology_role_any_primary,
    app_test_topology_role_primary,
    app_test_topology_role_acting_primary,
    app_test_topology_role_secondary,
} app_test_topology_role_enum_t;

/*! Check if the earbud topology is in the specified role

    The roles are specified in the test API. If toplogy 
    is modified, the test code may need to change but uses in 
    tests should not.

    \return TRUE The topology is in the specified role
*/
bool appTestIsTopologyRole(app_test_topology_role_enum_t role);


/*! Test function to report if the Topology is in a stable state.

    The intention is to use this API between tests. Other than
    the topology being in the No Role state, the implementation 
    is not defined here. */
bool appTestIsTopologyIdle(void);

/*! Check if the application state machine is in the specified role

    The states are defined in earbud_sm.h, and can be accessed from 
    python by name - example
    \code
      apps1.fw.env.enums["appState"]["APP_STATE_IN_CASE_DFU"])
    \endcode

    \return TRUE The application state machine is in the specified state
*/
bool appTestIsApplicationState(appState checked_state);


/*! Report the contents of the Device Database. */
void EarbudTest_DeviceDatabaseReport(void);

void EarbudTest_ConnectHandset(void);
void EarbudTest_DisconnectHandset(void);

/*! Report if the primary bluetooth address originated with this board

    When devices are paired one of the two addresses is selected as the
    primary address. This process is effectively random. This function 
    indicates if the address chosen is that programmed into this device.

    The log will contain the actual primary bluetooth address.

    To find out if the device is using the primary address, use the 
    test command appTestIsPrimary().

    The Bluetooth Address can be retrieved by non test commands.
    Depending on the test automation it may not be possible to use that 
    technique (shown here)

    \code
        >>> prim_addr = apps1.fw.call.new("bdaddr")
        >>> apps1.fw.call.appDeviceGetPrimaryBdAddr(prim_addr.address)
        True
        >>> prim_addr
         |-bdaddr  : struct
         |   |-uint32 lap : 0x0000ff0d
         |   |-uint8 uap : 0x5b
         |   |-uint16 nap : 0x0002
        >>>
    \endcode

    \returns TRUE if the primary address is that originally assigned to 
        this board, FALSE otherwise
        FALSE
*/
bool appTestPrimaryAddressIsFromThisBoard(void);

/*! \brief Override the score used in role selection.                      
                                                                       
    Setting a non-zero value will use that value rather than the    
    calculated one for future role selection decisions.             
   
    Setting a value of 0, will cancel the override and revert to    
    using the calculated score.

    If roles have already been selected, this will cause a re-selection.
    
    \param score Score to use for role selection decisions.         
*/                                                                  
void EarbudTest_PeerFindRoleOverrideScore(uint16 score);

/*! \brief Check to determine whether peer signalling is connected.

    Used to determine whether a peer device is connected

    \returns TRUE if peer signalling is connected, else FALSE.
*/
bool EarbudTest_IsPeerSignallingConnected(void);

/*! \brief Check to determine whether peer signalling is disconnected.

    Used to determine whether a peer device is disconnected, note this is not necessarily the same as
    !EarbudTest_IsPeerSignallingConnected() due to intermediate peer signalling states

    \returns TRUE if peer signalling is disconnected, else FALSE.
*/
bool EarbudTest_IsPeerSignallingDisconnected(void);

/*! Resets the PSKEY used to track the state of an upgrade 

    \return TRUE if the store was cleared
*/
bool appTestUpgradeResetState(void);

/*! Puts the Earbud into the In Case DFU state ready to test a DFU session
*/
void EarbudTest_EnterInCaseDfu(void);


/*! Set a test number to be displayed through appTestWriteMarker

    This is intended for use in test output. Set test_number to 
    zero to not display.

    \param  test_number The testcase number to use in output

    \return The test number. This can give output on the pydbg console 
        as well as the output.
*/
uint16 appTestSetTestNumber(uint16 test_number);


/*! Set a test iteration to be displayed through appTestWriteMarker

    This is intended for use in test output. Set test_iteration to 
    zero to not display.

    \param  test_iteration The test iteration number to use in output

    \return The test iteration. This can give output on the pydbg console 
        as well as the output.
*/
uint16 appTestSetTestIteration(uint16 test_iteration);


/*! Write a marker to output

    This is intended for use in test output

    \param  marker The value to include in the marker. A marker of 0 will
        write details of the testcase and iteration set through
        appTestSetTestNumber() and appTestSetTestIteration().

    \return The marker value. This can give output on the pydbg console 
        as well as the output.
*/
uint16 appTestWriteMarker(uint16 marker);


/*! \brief Send the VA Tap command
*/
void appTestVaTap(void);

/*! \brief Send the VA Double Tap command
*/
void appTestVaDoubleTap(void);

/*! \brief Send the VA Press and Hold command
*/
void appTestVaPressAndHold(void);

/*! \brief Send the VA Release command
*/
void appTestVaRelease(void);

/*! \brief  Set the ANC Enable state in the Earbud. */
void EarbudTest_SetAncEnable(void);

/*! \brief  Set the ANC Disable state in the Earbud. */
void EarbudTest_SetAncDisable(void);

/*! \brief  Set the ANC Enable/Disable state in both Earbuds. */
void EarbudTest_SetAncToggleOnOff(void);

/*! \brief  Set the ANC mode in both Earbuds.
    \param  mode Mode need to be given by user to set the dedicated mode.
*/
void EarbudTest_SetAncMode(anc_mode_t mode);

/*! \brief  To get the current ANC State in both Earbuds.
    \return bool TRUE if ANC enabled
                 else FALSE
*/
bool EarbudTest_GetAncstate(void);

/*! \brief  To get the current ANC mode in both Earbuds.
    \return Return the current ANC mode.
*/
anc_mode_t EarbudTest_GetAncMode(void);

/*! \brief Set the device to have a fixed role
*/
void appTestSetFixedRole(peer_find_role_fixed_role_t role);

#endif /* EARBUD_TEST_H */
