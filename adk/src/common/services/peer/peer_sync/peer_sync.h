/*!
\copyright  Copyright (c) 2005 - 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       peer_sync.h
\brief	    Application peer synchronisation logic interface.
*/

#ifndef PEER_SYNC_H_
#define PEER_SYNC_H_

#include "peer_signalling.h"
#include "peer_sync_types.h"

/*! \brief The state of synchronisation with the peer.
    @startuml
    participant Earbud1 as EB1
    participant Earbud2 as EB2
    participant Handset as HS
    EB1 -> EB2: Peer Sync
    EB1 <- EB2: Peer Sync
    EB1 -> EB2: Connect A2DP/AVRCP
    EB1 -> HS: Connect A2DP/AVRCP
    note left: Peer Sync information of\nEarbud1 is now out of date
    EB1 -> EB2: Peer Sync
    ==  ==
    EB1 -> HS: Disconnect A2DP/AVRCP
    note left: Peer Sync information of\nEarbud1 is now out of date
    EB1 -> EB2: Peer Sync
    @enduml
*/

/*! Enumeration of messages which peer sync module can send. */
enum peerSyncMessages
{
    /*! Message sent to registered clients to notify of current peer sync state. */
    PEER_SYNC_STATUS = PEER_SYNC_MESSAGE_BASE,
};

/*! \brief Definition of #PEER_SYNC_STATUS message. */
typedef struct
{
    /*! Current state of peer sync completion.
        TRUE peer sync is complete, otherwise FALSE. */
    bool peer_sync_complete;
} PEER_SYNC_STATUS_T;

/*!< Task information for peer synchronisation. */
extern peerSyncTaskData    app_peer_sync;

/*! Get pointer to the peer sync data structure */
#define PeerSyncGetTaskData()    (&app_peer_sync)

/* PEER SYNC MODULE APIs
 ***********************/
/*! \brief Send a peer sync to peer earbud.

    \param response TRUE if this is a response peer sync.
 */
void appPeerSyncSend(bool response);

/*! \brief Determine if peer sync is complete.
    \return bool TRUE if peer sync is complete, FALSE otherwise.

    A complete peer sync is defined as both peers having sent their most
    up to date peer sync message and having received a peer sync from
    their peer.
 */
bool appPeerSyncIsComplete(void);

/*! \brief Clear peer sync for both sent and received status. */
void appPeerSyncResetSync(void);

/*! \brief Register a task to receive #PEER_SYNC_STATUS messages.
    \param task Task which #PEER_SYNC_STATUS messages will be sent to.
 */
void appPeerSyncStatusClientRegister(Task task);

/*! \brief Unregister a task to stop receiving #PEER_SYNC_STATUS messages.
    \param task Previously registered task, for which #PEER_SYNC_STATUS should no longer be sent. */
void appPeerSyncStatusClientUnregister(Task task);

/*! \brief Initialise the peer sync module. */
bool appPeerSyncInit(Task init_task);

/* FUNCTIONS TO QUERY INFORMATION SYNCHRONISED BY PEER SYNC
 **********************************************************/
/*! \brief Query if the peer's handset supports TWS+.
    \return TRUE if supported, otherwise FALSE. */
bool appPeerSyncIsPeerHandsetTws(void);

/*! \brief Query if the peer has an A2DP connection to its handset.
    \return TRUE if connected, otherwise FALSE. */
bool appPeerSyncIsPeerHandsetA2dpConnected(void);

/*! \brief Query if the peer is A2DP streaming from its handset.
    \return TRUE if streaming, otherwise FALSE. */
bool appPeerSyncIsPeerHandsetA2dpStreaming(void);

/*! \brief Query if the peer has an AVRCP connection to its handset.
    \return TRUE if connected, otherwise FALSE. */
bool appPeerSyncIsPeerHandsetAvrcpConnected(void);

/*! \brief Query if the peer has an HFP connection to its handset.
    \return TRUE if connected, otherwise FALSE. */
bool appPeerSyncIsPeerHandsetHfpConnected(void);

/*! \brief Query if the peer is in-case.
    \return TRUE if in-case, otherwise FALSE. */
bool appPeerSyncIsPeerInCase(void);

/*! \brief Query if the peer is in-ear.
    \return TRUE if in-ear, otherwise FALSE. */
bool appPeerSyncIsPeerInEar(void);

/*! \brief Query if the peer is pairing.
    \return TRUE if pairing, otherwise FALSE. */
bool appPeerSyncIsPeerPairing(void);

/*! \brief Query if the peer is paired with a handset.
    \return TRUE if paired, otherwise FALSE. */
bool appPeerSyncHasPeerHandsetPairing(void);


/*! \brief Query if the peer is BLE advertising.

    \return TRUE if advertising, otherwise FALSE. */
bool appPeerSyncIsPeerAdvertising(void);


/*! \brief Query if the peer is paired with a handset.

    \return TRUE if in a BLE connection, otherwise FALSE. */
bool appPeerSyncIsPeerBleConnected(void);


/*! \brief Query if peer is still processing rules.
    \return TRUE if peer still has rules in progress, otherwise FALSE. */
bool appPeerSyncPeerRulesInProgress(void);

/*! \brief Query if peer sync is in progress.
    \return TRUE if peer sync is in progress, otherwise FALSE. */
bool appPeerSyncIsInProgress(void);

/*! \brief Query synchronised battery levels.

    \param[out] battery_level       This earbud's battery_level (sent in last sync).
    \param[out] peer_battery_level  The peer earbud's battery level (received in the last sync).
*/
void appPeerSyncGetPeerBatteryLevel(uint16 *battery_level, uint16 *peer_battery_level);

/*! \brief Query the peer earbud SCO status.
    \return bool TRUE if peer earbud has an active SCO, FALSE otherwise.
*/
bool appPeerSyncIsPeerScoActive(void);

/*! \brief Query the peer handset's address.

    \param[out] peer_handset_addr The address.
*/
void appPeerSyncGetPeerHandsetAddr(bdaddr *peer_handset_addr);

/*! \brief Query if peer DFU is in progress.
    \return TRUE if peer DFU is in progress, otherwise FALSE. */
bool appPeerSyncPeerDfuInProgress(void);

#endif /* PEER_SYNC_H_ */
