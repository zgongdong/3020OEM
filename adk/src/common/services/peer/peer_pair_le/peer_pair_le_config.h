/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Configuration related definitions for the LE pairing service .

\todo       Defines came from BREDR pairing and may not all be relevant, used, described
            correctly.
*/

#ifndef PEER_PAIR_LE_CONFIG_H_
#define PEER_PAIR_LE_CONFIG_H_

#include <message.h>

#ifdef CF133_BATT
/*! The minimum RSSI to pair with a device. This limits pairing to devices 
    that are in close proximity. */
#define appConfigPeerPairLeMinRssi() (-60)
#else
/*! The minimum RSSI to pair with a device. This limits pairing to devices 
    that are in close proximity. */
#define appConfigPeerPairLeMinRssi() (-50)
#endif

/*! Minimum difference in RSSI if multiple devices discovered
    during peer pairing discovery. This is used if there are 
    at least two devices above the RSSI threshold. 

    If the RSSI of the top two devices is within this threshold, 
    then peer pairing discovery will restart. */
#define appConfigPeerPairLeMinRssiDelta() (10)

/*! Timeout in seconds for user initiated peer pairing */
#define appConfigPeerPairLeTimeout()       (120)

/*! The time to delay before connecting to a discovered device.
    This applies from the first advert seen for any device and
    allows additional devices to be discovered. The first device
    might not be the one wanted. */
#define appConfigPeerPairLeTimeoutPeerSelect()  D_SEC(2)


/*! The milliseconds to delay from server completing pairing to initiating
    disconnection. Normally, the client will disconnect from the server
    before this timeout expires. */
#define appConfigPeerPairLeTimeoutServerCompleteDisconnect() 500


/*! \name Connection parameters

    These should be selected to have
    \li A reasonably low duty cycle for scanning
    \li A short connection interval and latency so message exchanges are fast
    \li A short supervision timeout so that poor/failed connections drop
    \li A range for parameters where applicable. 
        This allows the controllers some flexibility in negotiating the best 
        settings to fit around other activities

    \todo These need review and need to be similar for peer find role
 */
/* @{ */

/*! Interval at which the scanning to create a connection repeats 

    This is in units of 0.625 ms. The allowed range is between
    0x0004 (2.5 ms) and 0x4000 (10240 ms). */
#define PeerPairLeConfigConnectionScanInterval()          320

/*! The amount of the scan interval used for scanning

    The duty cycle can be calculated as 
    PeerPairLeConfigConnectionScanWindow()/PeerPairLeConfigConnectionScanInterval()

    Scan window in units of 0.625 ms. The allowed range is between
    0x0004 (2.5 ms) and 0x4000 (10.240 s). */
#define PeerPairLeConfigConnectionScanWindow()            80

/*! The supervision timeout for the link when created

    The supervision timeout is in units of 10 ms. The allowed 
    range is between 0x000a (100 ms) and 0x0c80 (32 s). */
#define PeerPairLeConfigConnectionSupervisionTimeout()    2000

/*! The minimum supervision timeout we will accept if the peer attempts
    to change it */
#define PeerPairLeConfigConnectionMinSupervisionTimeout() 1000

/*! The maximum supervision timeout we will accept if the peer attempts
    to change it */
#define PeerPairLeConfigConnectionMaxSupervisionTimeout() PeerPairLeConfigConnectionSupervisionTimeout()

/*! The maximum time that we will attempt to create a timeout for

    This is in units of 100ms
    \todo Check the units. Documentation suggests slots?
*/
#define PeerPairLeConfigConnectionAttemptsTimeout()       40

/*! The initial latency of the connection

    This is the number of events that the slave is allowed to intentionally 
    miss
 */
#define PeerPairLeConfigConnectionLatency()               0

/*! The maximum latency that we will allow if the slave requests an 
    update to the latency setting */
#define PeerPairLeConfigConnectionMaxLatency()            64


/*! The minimum size of the connection interval that the controller can select

    The controller may select a value between this and 
    PeerPairLeConfigConnectionIntervalMaximum() to fit around any
    other activities on the radio.

    The connection interval is in units of 1.25 ms. The allowed range is between
    0x0006 (7.5 ms) and 0x0c80 (4 s). */
#define PeerPairLeConfigConnectionIntervalMinimum()       24

/*! The maximum size of the connection interval that the controller can select

    The controller may select a value between 
    PeerPairLeConfigConnectionIntervalMinimum() and this to 
    fit around any other activities on the radio.

    The connection interval is in units of 1.25 ms. The allowed range is between
    0x0006 (7.5 ms) and 0x0c80 (4 s). */
#define PeerPairLeConfigConnectionIntervalMaximum()       40

/* @} */


/*! \name Advertising parameters

    Parameters used for advertising when connecting to peer

    The parameters should be selected to be reasonably fast as no other
    activties are taking place.
 */
/* @{ */

/*! The minimum advertising interval.

    This is specified in units of 0.625ms. Allowed values are
    32 (0x20, 20ms) to 16,384 (0x4000, 10 seconds)

    The controller may select an interval between this and 
    PeerPairLeConfigMaximumAdvertisingInterval() to fit advertising
    around any other radio activity
    */
#define PeerPairLeConfigMinimumAdvertisingInterval()  0xA0

/*! The maximum advertising interval.

    This is specified in units of 0.625ms. Allowed values are
    32 (0x20, 20ms) to 16,384 (0x4000, 10 seconds)

    The controller may select an interval between 
    PeerPairLeConfigMinimumAdvertisingInterval() and this to 
    fit advertising around any other radio activity
    */
#define PeerPairLeConfigMaximumAdvertisingInterval()  0xC0

/*! @} */


#endif /* PEER_PAIR_LE_CONFIG_H_ */
