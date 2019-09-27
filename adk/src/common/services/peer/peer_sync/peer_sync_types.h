/*!
\copyright  Copyright (c) 2018 - 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       peer_sync_types.h
\brief      Header file defining types for peer synchronisation module
*/

#ifndef PEER_SYNC_TYPES_H_
#define PEER_SYNC_TYPES_H_

#include <csrtypes.h>
#include <task_list.h>

typedef enum sm_peer_sync_states
{
    PEER_SYNC_NONE       = 0x00,       /*!< No peer sync messages sent or received. */

    PEER_SYNC_SENDING    = 0x01,       /*!< Peer sync has been transmitted. */
    PEER_SYNC_SENT       = 0x02,       /*!< Peer sync has been transmitted and delivery confirmed. */
    PEER_SYNC_RECEIVED   = 0x04,       /*!< Peer sync has been received. */

    /*!< Peer sync sent and received and peer state is valid. */
    PEER_SYNC_COMPLETE   = PEER_SYNC_RECEIVED | PEER_SYNC_SENT
} peerSyncState;

/*! Peer Sync module data. */
typedef struct
{
    /* -- State to run the peer sync module -- */
    TaskData task;                      /*!< Peer sync module Task. */

    peerSyncState peer_sync_state;      /*!< The state of synchronisation with the peer */

    uint8 peer_sync_tx_seqnum;          /*!< Peer sync transmit sequence number. */
    uint8 peer_sync_rx_seqnum;          /*!< Peer sync receive sequence number. */

    /* -- Event notification -- */
    task_list_t* peer_sync_status_tasks;   /*!< List of tasks registered to receive PEER_SYNC_STATUS
                                             messages. */

    /* -- Information synchronised via peer sync -- */
    uint16 sync_battery_level;          /*!< Battery level that was sent in sync message */
    uint16 peer_battery_level;          /*!< Battery level of peer that was received in sync message */
    uint16 peer_handset_tws;            /*!< The peer's handset's TWS version */
    bdaddr peer_handset_addr;           /*!< The peer's handset's address */

    bool peer_a2dp_connected:1;         /*!< The peer has A2DP connected */
    bool peer_a2dp_streaming:1;         /*!< The peer has A2DP streaming */
    bool peer_avrcp_connected:1;        /*!< The peer has AVRCP connected */
    bool peer_hfp_connected:1;          /*!< The peer has HFP connected */
    bool peer_in_case:1;                /*!< The peer is in the case */
    bool peer_in_ear:1;                 /*!< The peer is in the ear */
    bool peer_is_pairing:1;             /*!< The peer is pairing */
    bool peer_has_handset_pairing:1;    /*!< The peer is paired with a handset */
    bool peer_rules_in_progress:1;      /*!< Peer still has rules in progress. */
    bool sent_in_progress:1;            /*!< Have sent a peer sync with rules_in_progress set */
    bool peer_sco_active:1;             /*!< The peer has an active SCO */
    bool peer_dfu_in_progress:1;        /*!< The peer earbud is performing DFU. */
    bool peer_advertising:1;            /*!< The peer earbud is BLE advertising */
    bool peer_ble_connected:1;          /*!< The peer earbud has a BLE connection */
} peerSyncTaskData;

#endif /* PEER_SYNC_TYPES_H_ */
