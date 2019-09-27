/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Configuration items for the find role service.
*/

#ifndef PEER_FIND_ROLE_CONFIG_H_
#define PEER_FIND_ROLE_CONFIG_H_

/*! Time after which we will generate and use a new resolvable address.

    This is in seconds. There is a minimum of 30 seconds.

    \todo We may want a global setting for this
*/
#define PeerFindRoleConfigResolvableAddressTimeoutSecs()    300


/*! Time after which advertising will be started when starting find role

    A backoff is applied when a connection for find role fails. This
    is effectively the initial backoff.

    This is specified in milliseconds
*/
#define PeerFindRoleConfigInitialAdvertisingBackoffMs()     0


/*! Time after which to disconnect the find role link from the server side

    A timeout is needed here to reduce the risk of one device determining
    a role, while the other side does not get the opposite role.

    A long timeout will block the server device from changing its 
    Bluetooth address as it is required there are no active links.
 */
#define PeerFindRoleConfigGattDisconnectTimeout()           100


/*! Time delay to allow scanning after an activity finishes.

    Scanning is blocked by activities that use Bluetooth bandwidth, although
    advertising will still happen.

    When the activity stops, a short delay is recommended as another
    activity could start - for instance a call disconnecting and music
    restarting.
 */
#define PeerFindRoleConfigAllowScanAfterActivityDelayMs()   D_SEC(2)


/*! \name Connection parameters

    These should be selected to have
    \li A reasonably low duty cycle for scanning
    \li A short connection interval and latency so message exchanges are fast
    \li A short supervision timeout so that poor/failed connections drop
    \li A range for parameters where applicable. 
        This allows the controllers some flexibility in negotiating the best 
        settings to fit around other activities

    \todo These need review and need to be similar for peer pairing
 */
 /* @{ */

/*! Interval at which the scanning to create a connection repeats 

    This is in units of 0.625 ms. The allowed range is between
    0x0004 (2.5 ms) and 0x4000 (10240 ms). */
#define PeerFindRoleConfigConnectionScanInterval()          320

/*! The amount of the scan interval used for scanning

    The duty cycle can be calculated as 
    PeerFindRoleConfigConnectionScanWindow()/PeerFindRoleConfigConnectionScanInterval()

    Scan window in units of 0.625 ms. The allowed range is between
    0x0004 (2.5 ms) and 0x4000 (10.240 s). */
#define PeerFindRoleConfigConnectionScanWindow()            80

/*! The supervision timeout for the link when created

    The supervision timeout is in units of 10 ms. The allowed 
    range is between 0x000a (100 ms) and 0x0c80 (32 s). */
#define PeerFindRoleConfigConnectionSupervisionTimeout()    100

/*! The minimum supervision timeout we will accept if the peer attempts
    to change it */
#define PeerFindRoleConfigConnectionMinSupervisionTimeout() PeerFindRoleConfigConnectionSupervisionTimeout()

/*! The maximum supervision timeout we will accept if the peer attempts
    to change it */
#define PeerFindRoleConfigConnectionMaxSupervisionTimeout() 150

/*! The maximum time that we will attempt to create a timeout for

    This is in units of 100ms
    \todo Check the units. Documentation suggests slots?

    \note that this is the default setting used if PeerFindRole_FindRole()
    is called with 0 as its high_speed_timeout parameter. Scanning
    will be restarted after this timeout */
#define PeerFindRoleConfigConnectionAttemptsTimeout()       100 

/*! The initial latency of the connection

    This is the number of events that the slave is allowed to intentionally 
    miss
 */
#define PeerFindRoleConfigConnectionLatency()               0

/*! The maximum latency that we will allow if the slave requests an 
    update to the latency setting */
#define PeerFindRoleConfigConnectionMaxLatency()            4


/*! The minimum size of the connection interval that the controller can select

    The controller may select a value between this and 
    PeerFindRoleConfigConnectionIntervalMaximum() to fit around any
    other activities on the radio.

    The connection interval is in units of 1.25 ms. The allowed range is between
    0x0006 (7.5 ms) and 0x0c80 (4 s). */
#define PeerFindRoleConfigConnectionIntervalMinimum()       12

/*! The maximum size of the connection interval that the controller can select

    The controller may select a value between 
    PeerFindRoleConfigConnectionIntervalMinimum() and this to 
    fit around any other activities on the radio.

    The connection interval is in units of 1.25 ms. The allowed range is between
    0x0006 (7.5 ms) and 0x0c80 (4 s). */
#define PeerFindRoleConfigConnectionIntervalMaximum()       24

/* @} */


/*! \name Advertising parameters

    Parameters used for advertising when connecting to peer

    The parameters should be selected to be reasonably slow so as to fit
    around other activities. When other activities may be taking place and
    the peer needs to be found, the intention is that the active peer 
    will only advertise - fairly slowly - and relies upon the other peer
    scanning a higher percentage of the time.

    PeerFindRoleConfigMinimumAdvertisingInterval() and 
    PeerFindRoleConfigMaximumAdvertisingInterval() should be different so 
    that the controller has the flexibility to select a value.
 */
/* @{ */

/*! The minimum advertising interval.

    This is specified in units of 0.625ms. Allowed values are
    32 (0x20, 20ms) to 16,384 (0x4000, 10 seconds)

    The controller may select an interval between this and 
    PeerFindRoleConfigMaximumAdvertisingInterval() to fit advertising
    around any other radio activity
    */
#define PeerFindRoleConfigMinimumAdvertisingInterval()  0xA0

/*! The maximum advertising interval.

    This is specified in units of 0.625ms. Allowed values are
    32 (0x20, 20ms) to 16,384 (0x4000, 10 seconds)

    The controller may select an interval between 
    PeerFindRoleConfigMinimumAdvertisingInterval() and this to 
    fit advertising around any other radio activity
    */
#define PeerFindRoleConfigMaximumAdvertisingInterval()  0xC0
/*! @} */


#endif /* PEER_FIND_ROLE_CONFIG_H_ */
