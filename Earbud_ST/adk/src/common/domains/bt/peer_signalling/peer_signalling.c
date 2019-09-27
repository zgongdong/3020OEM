/*!
\copyright  Copyright (c) 2015 - 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       peer_signalling.c
\brief	    Implementation of module providing signalling to headset peer device.
*/

#include "sdp.h"
#include "peer_signalling.h"
#include "peer_signalling_config.h"
#include "earbud_init.h"
#include "bt_device.h"
#include "earbud_log.h"

#include <service.h>
#include <connection_manager.h>
#ifdef L2CA_DISCONNECT_LINK_TRANSFERRED
#undef L2CA_DISCONNECT_LINK_TRANSFERRED
#endif
#include <bluestack/l2cap_prim.h>

#ifdef L2CA_DISCONNECT_LINK_TRANSFERRED
#undef L2CA_DISCONNECT_LINK_TRANSFERRED
#endif

#define L2CA_DISCONNECT_LINK_TRANSFERRED 0xFF

#include <source.h>
#include <panic.h>
#include <message.h>
#include <bdaddr.h>
#include <stdio.h>
#include <stdlib.h>
#include <sink.h>
#include <stream.h>
#include <marshal.h>

//#define DUMP_MARSHALL_DATA
#ifdef DUMP_MARSHALL_DATA
static void dump_buffer(const uint8* bufptr, size_t size)
{
    for (int i=0; i<size; i++)
        DEBUG_LOG("%x", bufptr[i]);
}
#endif

//#define AUTOCONNECT_WORKAROUND

/* A temporary workaround for when a peer_signaling API is called before
   peer_signalling has been connected. This replicates the previous auto
   connect behaviour and may be needed while the new topology is under
   development. */
#ifdef AUTOCONNECT_WORKAROUND
bool peer_sig_enable_auto_connect = FALSE;
bool peer_sig_panic_if_not_connected = FALSE;

#define checkPeerSigConnected(peer_sig, function_name) do { \
    if (!SinkIsValid(peer_sig->link_sink)) \
    { \
        DEBUG_LOG(#function_name " WARNING - peer_sig not connected (yet) state 0x%x", peer_sig->state); \
        \
        if (peer_sig_panic_if_not_connected) \
            Panic(); \
        \
        if (peer_sig_enable_auto_connect) \
        { \
            bdaddr peer_addr; \
            PanicFalse(appDeviceGetSecondaryBdAddr(&peer_addr)); \
            appPeerSigConnect(&peer_sig->task, &peer_addr); \
        } \
    } \
} while(0);
#else
#define checkPeerSigConnected(peer_sig, function_name)
#endif

/*! \brief Data held per client task for marshalled message channels. */
typedef struct
{
    marshaller_t    marshaller;
    unmarshaller_t  unmarshaller;
    peerSigMsgChannel msg_channel_id;
} marshal_msg_channel_data_t;

/*! \brief Types of lock used to control receipt of messages by the peer sig task. */
typedef enum
{
    /*! Unlocked. */
    peer_sig_lock_none = 0x00,

    /*! Lock for FSM transition states. */
    peer_sig_lock_fsm = 0x01,
    
    /*! Lock for busy handling a traditional message. */
    peer_sig_lock_op = 0x02,

    /*! Lock for busy handling a marshalled message. */
    peer_sig_lock_marshal = 0x04,
} peer_sig_lock;

/*! Peer signalling module state. */
typedef struct
{
    /* State for managing this peer signalling application module */
    TaskData task;                  /*!< Peer Signalling module task */
    appPeerSigState state:5;        /*!< Current state */
    uint16 lock;                    /*!< State machine lock */
    task_list_t *peer_sig_client_tasks;/*!< List of tasks registered for notifications
                                         of peer signalling channel availability */
    bool link_loss_occurred;        /*!< TRUE if link-loss has occurred */

    /* State related to maintaining signalling channel with peer */
    bdaddr peer_addr;               /*!< Bluetooth address of the peer we are signalling */

    /* Tasks registered to receive asynchronous incoming messages */
    /* \todo These could move to use the generic msg channel mechanism and simplify
     * the code. */
    Task rx_link_key_task;          /*!< Task to send handset link key to when received from peer */
    Task rx_handset_commands_task;  /*!< Task to send handset commands received from peer */

    /* State required to service various signalling requests */
    Task client_task;               /*!< Task to respond with result of current peer signalling operation */
    uint16 current_op;              /*!< Type of in progress operation. */
    bdaddr handset_addr;            /*!< Address of the handset for current operation */

    /* State related to msg channel facility. */
    task_list_t* msg_channel_tasks;         /*!< List of tasks and associated signalling channel. */
    peerSigMsgChannel current_msg_channel; /*!< Remember msg channel in use for TX confirmation msgs. */

    /* State related to L2CAP peer signalling channel */
    uint16 local_psm;               /*!< L2CAP PSM registered */
    uint16 remote_psm;              /*!< L2CAP PSM registered by peer device */
    uint8 sdp_search_attempts;         /*!< Count of failed SDP searches */
    uint16 pending_connects;
    Sink link_sink;                 /*!< The sink of the L2CAP link */
    Source link_source;             /*!< The source of the L2CAP link */

    /* State related to marshaling messages between peers */
    task_list_t* marshal_msg_channel_tasks;
    PEER_SIG_INTERNAL_MARSHALLED_MSG_CHANNEL_TX_REQ_T* retx;

    /* Record the Task which first requested a connect or disconnect */
    task_list_t connect_tasks;
    task_list_t disconnect_tasks;

} peerSigTaskData;

/******************************************************************************
 * General Definitions
 ******************************************************************************/

/*! Macro to make a message. */
#define MAKE_MESSAGE(TYPE) TYPE##_T *message = PanicUnlessNew(TYPE##_T);
/*! Macro to make message with variable length for array fields. */
#define MAKE_PEER_SIG_MESSAGE_WITH_LEN(TYPE, LEN) \
    TYPE##_T *message = (TYPE##_T *) PanicUnlessMalloc(sizeof(TYPE##_T) + LEN);

/******************************************************************************
 * Peer Signalling Message Definitions
 ******************************************************************************/
/*
 * PEER_SIG_CMD_CONNECT_HANDSET
 */
/*! OPID for requesting peer connect to the handset. */
#define PEER_SIG_CMD_CONNECT_HANDSET                     0x35
/*! Size of AVRCP_PEER_CMD_CONNECT_HANDSET message in bytes. */
#define PEER_SIG_CMD_CONNECT_HANDSET_SIZE                1
/*! Definition of bits in payload */
enum PEER_SIG_CMD_CONNECT_HANDSET_FLAGS
{
    PEER_SIG_CMD_CONNECT_HANDSET_FLAG_PLAY_MEDIA = 1<<0,
};

/*
 * PEER_SIG_CMD_ADD_LINK_KEY
 */
/*! OPID for sending handset link key to peer headset. */
#define PEER_SIG_CMD_ADD_LINK_KEY                     0x34
/*! Size of message to send link key to peer in bytes. */
#define PEER_SIG_CMD_ADD_LINK_KEY_SIZE                24
/*! Byte offset to address type field. */
#define PEER_SIG_CMD_ADD_LINK_KEY_ADDR_TYPE_OFFSET    0
/*! Byte offset to address. */
#define PEER_SIG_CMD_ADD_LINK_KEY_ADDR_OFFSET         1
/*! Byte offset to key type field. */
#define PEER_SIG_CMD_ADD_LINK_KEY_KEY_TYPE_OFFSET     7
/*! Byte offset to link key field. */
#define PEER_SIG_CMD_ADD_LINK_KEY_KEY_OFFSET          8

/*! BR/EDR address type. */
#define PEER_SIG_CMD_ADD_LINK_KEY_ADDR_TYPE_BREDR     0
/*! BR/EDR link key, generated by key H6/H7 deriviation process. */
#define PEER_SIG_CMD_ADD_LINK_KEY_KEY_TYPE_0          0

/*! Link key size of 16-bit words */
#define SIZE_LINK_KEY_16BIT_WORDS   8

/*
 * PEER_SIG_CMD_PAIR_HANDSET_ADDRESS
 */
/*! OPID for sending pair handset address msg to peer. */
#define PEER_SIG_CMD_PAIR_HANDSET_ADDRESS                     0x33
/*! Size of PEER_SIG_CMD_PAIR_HANDSET_ADDRESS message. */
#define PEER_SIG_CMD_PAIR_HANDSET_ADDRESS_SIZE                7
/*! Byte offset to address type field. */
#define PEER_SIG_CMD_PAIR_HANDSET_ADDRESS_ADDR_TYPE_OFFSET    0
/*! Byte offset to address field. */
#define PEER_SIG_CMD_PAIR_HANDSET_ADDRESS_ADDR_OFFSET         1

/*! Definitions for Peer Signalling message channel packet format. */
/*!@{ */
#define PEER_SIG_CMD_MSG_CHANNEL_MSG                  0x41
#define PEER_SIG_CMD_MSG_CHANNEL_HEADER_SIZE          6
#define PEER_SIG_CMD_MSG_CHANNEL_ID_OFFSET            0       /*! Msg Channel IDs are 32 bit */
#define PEER_SIG_CMD_MSG_CHANNEL_DATA_LENGTH_OFFSET   4       /*! Length limited to 16-bit */
#define PEER_SIG_CMD_MSG_CHANNEL_DATA_OFFSET          6
/*!@} */

static void appPeerSigMsgConnectionInd(peerSigStatus status);
static void appPeerSigStartInactivityTimer(void);
static void appPeerSigCancelInactivityTimer(void);
static void appPeerSigCancelInProgressOperation(void);
static void appPeerSigHandleInternalMarshalledMsgChannelTxRequest(const PEER_SIG_INTERNAL_MARSHALLED_MSG_CHANNEL_TX_REQ_T* req, bool retx);
static void appPeerSigMsgConnectCfm(Task task, peerSigStatus status);
static void appPeerSigMsgDisconnectCfm(Task task, peerSigStatus status);
static void appPeerSigSendConnectConfirmation(peerSigStatus status);
static void appPeerSigSendDisconnectConfirmation(peerSigStatus status);

/*!< Peer earbud signalling */
peerSigTaskData app_peer_sig;

/*! Get pointer to the peer signalling modules data structure */
#define PeerSigGetTaskData() (&app_peer_sig)


static void appPeerSigEnterConnectingAcl(void)
{
    DEBUG_LOG("appPeerSigEnterConnectingAcl");
}

static void appPeerSigExitConnectingAcl(void)
{
    DEBUG_LOG("appPeerSigExitConnectingAcl");
}

static void appPeerSigEnterConnectingLocal(void)
{
    peerSigTaskData *peer_sig = PeerSigGetTaskData();
    DEBUG_LOG("appPeerSigEnterConnectingLocal");

    /* Reset the sdp retry count */
    peer_sig->sdp_search_attempts = 0;
}

static void appPeerSigExitConnectingLocal(void)
{
    peerSigTaskData *peer_sig = PeerSigGetTaskData();
    DEBUG_LOG("appPeerSigExitConnectingLocal");

    /* We have finished (successfully or not) attempting to connect, so
     * we can relinquish our lock on the ACL.  Bluestack will then close
     * the ACL when there are no more L2CAP connections */
    ConManagerReleaseAcl(&peer_sig->peer_addr);
}

static void appPeerSigEnterConnectingRemote(void)
{
    DEBUG_LOG("appPeerSigEnterConnectingRemote");
}

static void appPeerSigExitConnectingRemote(void)
{
    DEBUG_LOG("appPeerSigExitConnectingRemote");
}

static void appPeerSigEnterConnected(void)
{
    DEBUG_LOG("appPeerSigEnterConnected");

    /* If we have any clients inform them of peer signalling connection */
    appPeerSigMsgConnectionInd(peerSigStatusConnected);

    /* If the connect was because of a client request, send a CFM to
       the client. */
    appPeerSigSendConnectConfirmation(peerSigStatusSuccess);

    appPeerSigStartInactivityTimer();
}

static void appPeerSigExitConnected(void)
{
    peerSigTaskData *peer_sig = PeerSigGetTaskData();
    DEBUG_LOG("appPeerSigExitConnected");

    appPeerSigCancelInactivityTimer();

    /* If we have any clients inform them of peer signalling disconnection */
    appPeerSigMsgConnectionInd(peer_sig->link_loss_occurred ? peerSigStatusLinkLoss : peerSigStatusDisconnected);
}

static void appPeerSigEnterDisconnected(void)
{
    peerSigTaskData *peer_sig = PeerSigGetTaskData();
    DEBUG_LOG("appPeerSigEnterDisconnected");

    /* Cancel any operation in progress */
    appPeerSigCancelInProgressOperation();

    /* Clear peer address, as we use that to detect if we've previously reject a peer connection */
    BdaddrSetZero(&peer_sig->peer_addr);

    /* If the disconnect was because of a client request, send a CFM to
       the client. */
    appPeerSigSendDisconnectConfirmation(peerSigStatusSuccess);

    /* If a client requested a connect, send a CFM to the client. */
    appPeerSigSendConnectConfirmation(peerSigStatusFail);

    /* Set the sink in the marshal_common module */
    MarshalCommon_SetSink(NULL);

    /* Reset the sdp retry count */
    peer_sig->sdp_search_attempts = 0;
}

static void appPeerSigExitDisconnected(void)
{
    DEBUG_LOG("appPeerSigExitDisconnected");
}

static void appPeerSigEnterInitialising(void)
{
    peerSigTaskData *peer_sig = PeerSigGetTaskData();

    DEBUG_LOG("appPeerSigEnterInitialising");

    /* Register a Protocol/Service Multiplexor (PSM) that will be
       used for this application. The same PSM is used at both
       ends. */
    ConnectionL2capRegisterRequest(&peer_sig->task, L2CA_PSM_INVALID, 0);
}

static void appPeerSigExitInitialising(void)
{
    DEBUG_LOG("appPeerSigExitInitialising");

    MessageSend(appInitGetInitTask(), PEER_SIG_INIT_CFM, NULL);
}

static void appPeerSigEnterSdpSearch(void)
{
    peerSigTaskData *peer_sig = PeerSigGetTaskData();

    DEBUG_LOG("appPeerSigEnterSdpSearch");

    /* Perform SDP search */
    ConnectionSdpServiceSearchAttributeRequest(&peer_sig->task, &peer_sig->peer_addr, 0x32,
                                               appSdpGetPeerSigServiceSearchRequestSize(), appSdpGetPeerSigServiceSearchRequest(),
                                               appSdpGetPeerSigAttributeSearchRequestSize(), appSdpGetPeerSigAttributeSearchRequest());

    peer_sig->sdp_search_attempts++;
}

static void appPeerSigExitSdpSearch(void)
{
    DEBUG_LOG("appPeerSigExitSdpSearch");
}

static void appPeerSigEnterDisconnecting(void)
{
    peerSigTaskData *peer_sig = PeerSigGetTaskData();

    DEBUG_LOG("appPeerSigEnterDisconnecting");

    if (SinkIsValid(peer_sig->link_sink))
    {
        ConnectionL2capDisconnectRequest(&peer_sig->task, peer_sig->link_sink);
    }
}

static void appPeerSigExitDisconnecting(void)
{
    DEBUG_LOG("appPeerSigExitDisconnecting");
}

static appPeerSigState appPeerSigGetState(void)
{
    peerSigTaskData *peer_sig = PeerSigGetTaskData();
    return peer_sig->state;
}

static void appPeerSigSetState(appPeerSigState state)
{
    peerSigTaskData *peer_sig = PeerSigGetTaskData();

    appPeerSigState old_state = appPeerSigGetState();
    DEBUG_LOGF("appPeerSigSetState, state %x", state);

    /* Handle state exit functions */
    switch (old_state)
    {
        case PEER_SIG_STATE_INITIALISING:
            appPeerSigExitInitialising();
            break;
        case PEER_SIG_STATE_DISCONNECTED:
            appPeerSigExitDisconnected();
            break;
        case PEER_SIG_STATE_CONNECTING_ACL:
            appPeerSigExitConnectingAcl();
            break;
        case PEER_SIG_STATE_CONNECTING_SDP_SEARCH:
            appPeerSigExitSdpSearch();
            break;
        case PEER_SIG_STATE_CONNECTING_LOCAL:
            appPeerSigExitConnectingLocal();
            break;
        case PEER_SIG_STATE_CONNECTING_REMOTE:
            appPeerSigExitConnectingRemote();
            break;
        case PEER_SIG_STATE_CONNECTED:
            appPeerSigExitConnected();
            break;
        case PEER_SIG_STATE_DISCONNECTING:
            appPeerSigExitDisconnecting();
            break;
        default:
            break;
    }

    /* Set new state */
    peer_sig->state = state;

    /* Update lock according to state */
    if (state & PEER_SIG_STATE_LOCK)
        peer_sig->lock |= peer_sig_lock_fsm;
    else
        peer_sig->lock &= ~peer_sig_lock_fsm;

    /* Handle state entry functions */
    switch (state)
    {
        case PEER_SIG_STATE_INITIALISING:
            appPeerSigEnterInitialising();
            break;
        case PEER_SIG_STATE_DISCONNECTED:
            appPeerSigEnterDisconnected();
            break;
        case PEER_SIG_STATE_CONNECTING_ACL:
            appPeerSigEnterConnectingAcl();
            break;
        case PEER_SIG_STATE_CONNECTING_SDP_SEARCH:
            appPeerSigEnterSdpSearch();
            break;
        case PEER_SIG_STATE_CONNECTING_LOCAL:
            appPeerSigEnterConnectingLocal();
            break;
        case PEER_SIG_STATE_CONNECTING_REMOTE:
            appPeerSigEnterConnectingRemote();
            break;
        case PEER_SIG_STATE_CONNECTED:
            appPeerSigEnterConnected();
            break;
        case PEER_SIG_STATE_DISCONNECTING:
            appPeerSigEnterDisconnecting();
            break;
        default:
            break;
    }
}

static void appPeerSigError(MessageId id)
{
    UNUSED(id);
    DEBUG_LOGF("appPeerSigError, state %u, id %u", appPeerSigGetState(), id);
    Panic();
}

/******************************************************************************
 * Messages sent to API clients
 ******************************************************************************/
/*! \brief Send PEER_SIG_LINK_KEY_TX_CFM message. */
static void appPeerSigMsgLinkKeyConfirmation(Task task, peerSigStatus status, const bdaddr* addr)
{
    MAKE_MESSAGE(PEER_SIG_LINK_KEY_TX_CFM);
    message->status = status;
    message->handset_addr = *addr;
    MessageSend(task, PEER_SIG_LINK_KEY_TX_CFM, message);
}

/*! \brief Send PEER_SIG_PAIR_HANDSET_CFM message. */
static void appPeerSigMsgPairHandsetConfirmation(Task task, peerSigStatus status, const bdaddr* addr)
{
    MAKE_MESSAGE(PEER_SIG_PAIR_HANDSET_CFM);
    message->status = status;
    message->handset_addr = *addr;
    MessageSend(task, PEER_SIG_PAIR_HANDSET_CFM, message);
}

/*! \brief Send indication of connection state to registered clients. */
static void appPeerSigMsgConnectionInd(peerSigStatus status)
{
    peerSigTaskData *peer_sig = PeerSigGetTaskData();
    Task next_client = 0;

    while (TaskList_Iterate(peer_sig->peer_sig_client_tasks, &next_client))
    {
        MAKE_MESSAGE(PEER_SIG_CONNECTION_IND);
        message->status = status;
        MessageSend(next_client, PEER_SIG_CONNECTION_IND, message);
    }
}

/*! \brief Send confirmation of result of message channel transmission to client. */
static void appPeerSigMsgChannelTxConfirmation(Task task, peerSigStatus status,
                                               peerSigMsgChannel channel)
{
    MAKE_MESSAGE(PEER_SIG_MSG_CHANNEL_TX_CFM);
    message->status = status;
    message->channel = channel;
    MessageSend(task, PEER_SIG_MSG_CHANNEL_TX_CFM, message);
}

/*! \brief Send confirmation of result of marshalled message transmission to client. */
static void appPeerSigMarshalledMsgChannelTxCfm(Task task, uint8* msg_ptr,
                                                peerSigStatus status,
                                                peerSigMsgChannel channel)
{
    MAKE_MESSAGE(PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM);
    message->status = status;
    message->channel = channel;
    message->msg_ptr = msg_ptr;
    MessageSend(task, PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM, message);
}

static void appPeerSigMsgConnectCfm(Task task, peerSigStatus status)
{
    MAKE_MESSAGE(PEER_SIG_CONNECT_CFM);
    message->status = status;
    MessageSend(task, PEER_SIG_CONNECT_CFM, message);
}

/*! \brief Send confirmation of a connection request to registered clients. */
static void appPeerSigSendConnectConfirmation(peerSigStatus status)
{
    peerSigTaskData *peer_sig = PeerSigGetTaskData();
    Task next_client = 0;

    /* Send PEER_SIG_CONNECT_CFM to all clients who made a connect request,
       then remove them from the list. */
    while (TaskList_Iterate(&peer_sig->connect_tasks, &next_client))
    {
        appPeerSigMsgConnectCfm(next_client, status);
        TaskList_RemoveTask(&peer_sig->connect_tasks, next_client);
        next_client = 0;
    }
}

static void appPeerSigMsgDisconnectCfm(Task task, peerSigStatus status)
{
    MAKE_MESSAGE(PEER_SIG_DISCONNECT_CFM);
    message->status = status;
    MessageSend(task, PEER_SIG_DISCONNECT_CFM, message);
}

/*! \brief Send confirmation of a disconnect request to registered clients. */
static void appPeerSigSendDisconnectConfirmation(peerSigStatus status)
{
    peerSigTaskData *peer_sig = PeerSigGetTaskData();
    Task next_client = 0;

    /* Send PEER_SIG_DISCONNECT_CFM to all clients who made a disconnect
       request, then remove them from the list. */
    while (TaskList_Iterate(&peer_sig->disconnect_tasks, &next_client))
    {
        appPeerSigMsgDisconnectCfm(next_client, status);
        TaskList_RemoveTask(&peer_sig->disconnect_tasks, next_client);
        next_client = 0;
    }
}

/* Cancel any pending internal startup requests and send a
   PEER_SIG_CONNECT_CFM(failed) to the client(s). */
static void appPeerSigCancelStartup(void)
{
    peerSigTaskData *peer_sig = PeerSigGetTaskData();
    MessageCancelAll(&peer_sig->task, PEER_SIG_INTERNAL_STARTUP_REQ);
    appPeerSigSendConnectConfirmation(peerSigStatusFail);
}

/* Cancel any pending internal shutdown requests and send a
   PEER_SIG_DISCONNECT_CFM(failed) to the client(s). */
static void appPeerSigCancelShutdown(void)
{
    peerSigTaskData *peer_sig = PeerSigGetTaskData();
    MessageCancelAll(&peer_sig->task, PEER_SIG_INTERNAL_SHUTDOWN_REQ);
    appPeerSigSendDisconnectConfirmation(peerSigStatusFail);
}

/******************************************************************************
 * Internal Peer Signalling management functions
 ******************************************************************************/

/*! \brief Cancel any already in progress operations that were waiting for responses from peer.
 */
static void appPeerSigCancelInProgressOperation(void)
{
    peerSigTaskData *peer_sig = PeerSigGetTaskData();

    /* Work out what operation is in progress and inform client of failure */
    switch (peer_sig->current_op)
    {
        case PEER_SIG_CMD_ADD_LINK_KEY:
            appPeerSigMsgLinkKeyConfirmation(peer_sig->client_task, peerSigStatusLinkKeyTxFail,
                                             &peer_sig->handset_addr);
            break;
        case PEER_SIG_CMD_PAIR_HANDSET_ADDRESS:
            appPeerSigMsgPairHandsetConfirmation(peer_sig->client_task, peerSigStatusPairHandsetTxFail,
                                                 &peer_sig->handset_addr);
            break;

        case PEER_SIG_CMD_MSG_CHANNEL_MSG:
            appPeerSigMsgChannelTxConfirmation(peer_sig->client_task, peerSigStatusMsgChannelTxFail,
                                               peer_sig->current_msg_channel);
            break;

        default:
            break;
    }

    /* Clear up, no operation in progress now */
    peer_sig->client_task = NULL;
    peer_sig->current_op = 0;

    /* Clear lock, this may result in the next message being delivered */
    peer_sig->lock &= ~peer_sig_lock_op;
}

/*! \brief Start (or connect) the peer signalling channel to a peer. */
static void appPeerSigStartup(Task task, const bdaddr *peer_addr)
{
    peerSigTaskData *peer_sig = PeerSigGetTaskData();

    DEBUG_LOGF("appPeerSigStartup peer_addr [ %04x,%02x,%06lx ]",
                  peer_addr->nap, peer_addr->uap, peer_addr->lap);

    /* Cancel any pending disconnects first. */
    appPeerSigCancelShutdown();

    MAKE_MESSAGE(PEER_SIG_INTERNAL_STARTUP_REQ);
    message->peer_addr = *peer_addr;
    MessageSendConditionally(&peer_sig->task, PEER_SIG_INTERNAL_STARTUP_REQ, message, &peer_sig->lock);

    TaskList_AddTask(&peer_sig->connect_tasks, task);
}

/*! \brief Shutdown (or disconnect) the peer signalling channel. */
static void appPeerSigShutdown(Task task)
{
    peerSigTaskData *peer_sig = PeerSigGetTaskData();

    /* Cancel any queued startup requests first. */
    appPeerSigCancelStartup();

    MessageSend(&peer_sig->task, PEER_SIG_INTERNAL_SHUTDOWN_REQ, NULL);

    TaskList_AddTask(&peer_sig->disconnect_tasks, task);
}

/*! \brief Set the inactivity timer.
 */
static void appPeerSigStartInactivityTimer(void)
{
    peerSigTaskData *peer_sig = PeerSigGetTaskData();
    DEBUG_LOG("appPeerSigStartInactivityTimer");

    if (appConfigPeerSignallingChannelTimeoutSecs())
    {
        MessageCancelAll(&peer_sig->task, PEER_SIG_INTERNAL_INACTIVITY_TIMER);
        MessageSendLater(&peer_sig->task, PEER_SIG_INTERNAL_INACTIVITY_TIMER, NULL,
                         appConfigPeerSignallingChannelTimeoutSecs() * 1000);
    }
}

/*! \brief Stop the inactivity timer.
 */
static void appPeerSigCancelInactivityTimer(void)
{
    peerSigTaskData *peer_sig = PeerSigGetTaskData();
    DEBUG_LOG("appPeerSigCancelInactivityTimer");

    MessageCancelAll(&peer_sig->task, PEER_SIG_INTERNAL_INACTIVITY_TIMER);
}

/*! \brief Handle inactivity timer, teardown signalling channel.
 */
static void appPeerSigInactivityTimeout(void)
{
    DEBUG_LOGF("appPeerSigInactivityTimeout, state %u", appPeerSigGetState());

    /* Both earbuds have an inactivity timeout, protect against race where
     * the peer signalling link may have just been disconnected by the other earbud */
    switch (appPeerSigGetState())
    {
        case PEER_SIG_STATE_CONNECTED:
            appPeerSigSetState(PEER_SIG_STATE_DISCONNECTING);
            break;

        default:
            break;
    }
}

static uint16 appPeerSigReadUint16(const uint8 *data)
{
    return data[0] + ((uint16)data[1] << 8);
}

//static uint24 appPeerSigReadUint24(const uint8 *data)
//{
//    return data[0] + ((uint16)data[1] << 8) + ((uint32)data[2] << 16);
//}

static uint32 appPeerSigReadUint32(const uint8 *data)
{
    return data[0] + (data[1] << 8) + ((uint32)data[2] << 16) + ((uint32)data[3] << 24);
}

static void appPeerSigWriteUint16(uint8 *data, uint16 val)
{
    data[0] = val & 0xFF;
    data[1] = (val >> 8) & 0xFF;
}

//static void appPeerSigWriteUint24(uint8 *data, uint24 val)
//{
//    data[0] = val & 0xFF;
//    data[1] = (val >> 8) & 0xFF;
//    data[2] = (val >> 16) & 0xFF;
//}

static void appPeerSigWriteUint32(uint8 *data, uint32 val)
{
    data[0] = val & 0xFF;
    data[1] = (val >> 8) & 0xFF;
    data[2] = (val >> 16) & 0xFF;
    data[3] = (val >> 24) & 0xFF;
}


static void appPeerSigHandleInternalStartupRequest(PEER_SIG_INTERNAL_STARTUP_REQ_T *req)
{
    peerSigTaskData *peer_sig = PeerSigGetTaskData();
    DEBUG_LOGF("appPeerSigHandleInternalStartupRequest, state %u, bdaddr %04x,%02x,%06lx",
               appPeerSigGetState(),
               req->peer_addr.nap, req->peer_addr.uap, req->peer_addr.lap);

    switch (appPeerSigGetState())
    {
        case PEER_SIG_STATE_CONNECTING_ACL:
        case PEER_SIG_STATE_DISCONNECTED:
        {
            /* Check if ACL is now up */
            if (ConManagerIsConnected(&req->peer_addr))
            {
                DEBUG_LOG("appPeerSigHandleInternalStartupRequest, ACL connected");

                /* Initiate if we created the ACL, or was previously rejected (peer_addr is not zero) */
                /*! \todo this mechanism was to avoid poor AVRCP handling of crossovers, we've
                 * since moved to L2CAP for the transport and we know only the primary will be
                 * creating the peer signalling profile, so this could be simplified */
                if (!ConManagerIsAclLocal(&req->peer_addr) || BdaddrIsSame(&peer_sig->peer_addr, &req->peer_addr))
                {
                    DEBUG_LOG("appPeerSigHandleInternalStartupRequest, ACL locally initiated");

                    /* Store address of peer */
                    peer_sig->peer_addr = req->peer_addr;

                    /* Begin the search for the peer signalling SDP record */
                    appPeerSigSetState(PEER_SIG_STATE_CONNECTING_SDP_SEARCH);

                }
                else
                {
                    DEBUG_LOG("appPeerSigHandleInternalStartupRequest, ACL remotely initiated");

                    /* Not locally initiated ACL, move to 'Disconnected' state */
                    appPeerSigSetState(PEER_SIG_STATE_DISCONNECTED);
                }
            }
            else
            {
                if (appPeerSigGetState() == PEER_SIG_STATE_DISCONNECTED)
                {
                    DEBUG_LOG("appPeerSigHandleInternalStartupRequest, ACL not connected, attempt to open ACL");

                    /* Post message back to ourselves, blocked on creating ACL */
                    MAKE_MESSAGE(PEER_SIG_INTERNAL_STARTUP_REQ);
                    message->peer_addr = req->peer_addr;
                    MessageSendConditionally(&peer_sig->task, PEER_SIG_INTERNAL_STARTUP_REQ, message, ConManagerCreateAcl(&req->peer_addr));

                    /* Wait in 'Connecting ACL' state for ACL to open */
                    appPeerSigSetState(PEER_SIG_STATE_CONNECTING_ACL);
                    return;
                }
                else
                {
                    DEBUG_LOG("appPeerSigHandleInternalStartupRequest, ACL failed to open, giving up");

                    /* ACL failed to open, move to 'Disconnected' state */
                    appPeerSigSetState(PEER_SIG_STATE_DISCONNECTED);
                }
            }
        }
        break;

        case PEER_SIG_STATE_CONNECTED:
            /* Already connected, send cfm back to client */
            appPeerSigSendConnectConfirmation(peerSigStatusConnected);
            break;

        default:
            appPeerSigError(PEER_SIG_INTERNAL_STARTUP_REQ);
            break;
    }
}

static void appPeerSigHandleInternalShutdownReq(void)
{
    peerSigTaskData* peer_sig = PeerSigGetTaskData();
    DEBUG_LOG("appPeerSigHandleInternalShutdownReq, state %u", appPeerSigGetState());

    switch (appPeerSigGetState())
    {
        case PEER_SIG_STATE_CONNECTING_SDP_SEARCH:
            {
                /* Cancel SDP search */
                ConnectionSdpTerminatePrimitiveRequest(&peer_sig->task);
            }
            /* Intentional fall-through to go to DISCONNECTING state */
        case PEER_SIG_STATE_CONNECTING_LOCAL:
            /* Intentional fall-through to go to DISCONNECTING state */
        case PEER_SIG_STATE_CONNECTED:
            {
                appPeerSigSetState(PEER_SIG_STATE_DISCONNECTING);
            }
            break;

        case PEER_SIG_STATE_DISCONNECTED:
            {
                appPeerSigSendDisconnectConfirmation(peerSigStatusSuccess);
            }
            break;

        default:
            break;
    }
}

/******************************************************************************
 * Handlers for peer messaging from peer signalling link
 ******************************************************************************/
/*! \brief Receive handset link key for peer.

    External interface for link keys is 16 bytes packed into 8 16-bit words,
    do the conversion from 16 8-bit words here.
 */
static bool appPeerSigHandleRxLinkKey(const uint8 *payload, uint16 payload_size)
{
    peerSigTaskData* peer_sig = PeerSigGetTaskData();
    uint16 key_len_words = SIZE_LINK_KEY_16BIT_WORDS * sizeof(uint16);

    DEBUG_LOGF("appPeerSigHandleRxLinkKey size:%d", payload_size);

    /* validate message:
     * message length not correct OR
     * we don't have a task registered to receive it OR
     * handset address type not supported OR
     * link key type not support */
    if (   (payload_size != PEER_SIG_CMD_ADD_LINK_KEY_SIZE)
        || (peer_sig->rx_link_key_task == NULL)
        || (payload[PEER_SIG_CMD_ADD_LINK_KEY_ADDR_TYPE_OFFSET] !=
                   PEER_SIG_CMD_ADD_LINK_KEY_ADDR_TYPE_BREDR)
        || (payload[PEER_SIG_CMD_ADD_LINK_KEY_KEY_TYPE_OFFSET] !=
                    PEER_SIG_CMD_ADD_LINK_KEY_KEY_TYPE_0)
        || !peer_sig->rx_link_key_task)
    {
        /* we don't like this message format, indicate failure to sender */
        return FALSE;
    }
    else
    {
        MAKE_PEER_SIG_MESSAGE_WITH_LEN(PEER_SIG_LINK_KEY_RX_IND, key_len_words);
        int index = PEER_SIG_CMD_ADD_LINK_KEY_ADDR_OFFSET;

        message->status = peerSigStatusSuccess;
        message->handset_addr.lap = (uint32)(((uint32)payload[index]) |
                                             ((uint32)payload[index+1]) << 8 |
                                             ((uint32)payload[index+2]) << 16);
        message->handset_addr.uap = payload[index+3];
        message->handset_addr.nap = (uint16)(((uint16)payload[index+4]) |
                                             ((uint16)payload[index+5]) << 8);
        index = PEER_SIG_CMD_ADD_LINK_KEY_KEY_OFFSET;
        memcpy(message->key, &payload[index], key_len_words);
        message->key_len = SIZE_LINK_KEY_16BIT_WORDS;

        /* send to registered client */
        MessageSend(peer_sig->rx_link_key_task, PEER_SIG_LINK_KEY_RX_IND, message);

        /* indicate we succeeded */
        return TRUE;
    }
}

/*! \brief Receive pair handset command. */
static bool appPeerSigHandlePairHandsetCommand(const uint8 *payload, uint16 payload_size)
{
    peerSigTaskData* peer_sig = PeerSigGetTaskData();

    /* validate message */
    if (   (payload_size != PEER_SIG_CMD_PAIR_HANDSET_ADDRESS_SIZE)
        || (payload[PEER_SIG_CMD_PAIR_HANDSET_ADDRESS_ADDR_TYPE_OFFSET] !=
                    PEER_SIG_CMD_ADD_LINK_KEY_ADDR_TYPE_BREDR)
        || !peer_sig->rx_handset_commands_task)
    {
        return FALSE;
    }
    else
    {
        /* tell pairing module to pair with specific handset */
        MAKE_MESSAGE(PEER_SIG_PAIR_HANDSET_IND);
        int index = PEER_SIG_CMD_PAIR_HANDSET_ADDRESS_ADDR_OFFSET;
        message->handset_addr.lap = (uint32)(((uint32)payload[index]) |
                                             ((uint32)payload[index+1]) << 8 |
                                             ((uint32)payload[index+2]) << 16);
        message->handset_addr.uap = payload[index+3];
        message->handset_addr.nap = (uint16)(((uint16)payload[index+4]) |
                                             ((uint16)payload[index+5]) << 8);
        MessageSend(peer_sig->rx_handset_commands_task, PEER_SIG_PAIR_HANDSET_IND, message);
        DEBUG_LOGF("appPeerSigHandlePairHandsetCommand %lx %x %x", message->handset_addr.lap, message->handset_addr.uap, message->handset_addr.nap);
        return TRUE;
    }
}

/*! \brief Handle incoming message channel transmission.
 */
static bool appPeerSigHandleMsgChannelRx(const uint8 *payload, uint16 payload_size)
{
    peerSigTaskData* peer_sig = PeerSigGetTaskData();

    /* validate message
     *  - must be at least size of a header+1 */
    if (payload_size > PEER_SIG_CMD_MSG_CHANNEL_HEADER_SIZE)
    {
        peerSigMsgChannel channel;
        uint16 msg_size;

        channel = appPeerSigReadUint32(&payload[PEER_SIG_CMD_MSG_CHANNEL_ID_OFFSET]);
        msg_size = appPeerSigReadUint16(&payload[PEER_SIG_CMD_MSG_CHANNEL_DATA_LENGTH_OFFSET]);

        /* make sure the message contains the same amount of data that the length
         * field in the header specifies */
        if (msg_size == (payload_size - PEER_SIG_CMD_MSG_CHANNEL_HEADER_SIZE))
        {
            Task task = 0;
            task_list_data_t data;
            bool handled = FALSE;

            /* look for a task registered to receive messages on this channel and
             * send it the message */
            while (TaskList_IterateWithData(peer_sig->msg_channel_tasks, &task, &data))
            {
                if ((data.u32 & channel) == channel)
                {
                    MAKE_PEER_SIG_MESSAGE_WITH_LEN(PEER_SIG_MSG_CHANNEL_RX_IND, msg_size-1);
                    message->channel = channel;
                    message->msg_size = msg_size;
                    memcpy(message->msg, &payload[PEER_SIG_CMD_MSG_CHANNEL_DATA_OFFSET], msg_size);
                    MessageSend(task, PEER_SIG_MSG_CHANNEL_RX_IND, message);
                    handled = TRUE;
                }
            }
            return handled;
        }
    }

    return FALSE;
}


/*! \brief Receive connect handset command. */
static bool appPeerSigHandleConnectHandsetCommand(const uint8 *payload, uint16 payload_size)
{
    peerSigTaskData* peer_sig = PeerSigGetTaskData();

    /* Do we have anybody interested ? */
    if (   !peer_sig->rx_handset_commands_task
        || (payload_size <= PEER_SIG_CMD_MSG_CHANNEL_HEADER_SIZE))
    {
        return FALSE;
    }

    MAKE_MESSAGE(PEER_SIG_CONNECT_HANDSET_IND);
    message->play_media = !!(payload[0] & PEER_SIG_CMD_CONNECT_HANDSET_FLAG_PLAY_MEDIA);

    /* tell client to connect to handset */
    MessageSend(peer_sig->rx_handset_commands_task, PEER_SIG_CONNECT_HANDSET_IND, message);
    DEBUG_LOG("appPeerSigHandleConnectHandsetCommand %d", message->play_media);
    return TRUE;
}


/******************************************************************************
 * Handlers for peer signalling channel L2CAP messages
 ******************************************************************************/


/*! \brief Handle result of L2CAP PSM registration request. */
static void appPeerSigHandleClL2capRegisterCfm(const CL_L2CAP_REGISTER_CFM_T *cfm)
{
    DEBUG_LOG("appPeerSigHandleClL2capRegisterCfm, status %u, psm %u", cfm->status, cfm->psm);
    PanicFalse(appPeerSigGetState() == PEER_SIG_STATE_INITIALISING);

    /* We have registered the PSM used for peer signalling links with
       connection manager, now need to wait for requests to process
       an incoming connection or make an outgoing connection. */
    if (success == cfm->status)
    {
        peerSigTaskData *peer_sig = PeerSigGetTaskData();

        /* Keep a copy of the registered L2CAP PSM, maybe useful later */
        peer_sig->local_psm = cfm->psm;

        /* Copy and update SDP record */
        uint8 *record = PanicUnlessMalloc(appSdpGetPeerSigServiceRecordSize());
        memcpy(record, appSdpGetPeerSigServiceRecord(), appSdpGetPeerSigServiceRecordSize());

        /* Write L2CAP PSM into service record */
        appSdpSetPeerSigPsm(record, cfm->psm);

        /* Register service record */
        ConnectionRegisterServiceRecord(&peer_sig->task, appSdpGetPeerSigServiceRecordSize(), record);
    }
    else
    {
        DEBUG_LOG("appPeerSigHandleClL2capRegisterCfm, failed to register L2CAP PSM");
        Panic();
    }
}

/*! \brief Handle result of the SDP service record registration request. */
static void appPeerSigHandleClSdpRegisterCfm(const CL_SDP_REGISTER_CFM_T *cfm)
{
    DEBUG_LOGF("appPeerSigHandleClSdpRegisterCfm, status %d", cfm->status);
    PanicFalse(appPeerSigGetState() == PEER_SIG_STATE_INITIALISING);

    if (cfm->status == sds_status_success)
    {
        /* Move to 'disconnected' state */
        appPeerSigSetState(PEER_SIG_STATE_DISCONNECTED);
    }
    else
        Panic();
}

/*! \brief Extract the remote PSM value from a service record returned by a SDP service search. */
static bool PeerSigGetL2capPSM(const uint8 *begin, const uint8 *end, uint16 *psm, uint16 id)
{
    ServiceDataType type;
    Region record, protocols, protocol, value;
    record.begin = begin;
    record.end   = end;

    while (ServiceFindAttribute(&record, id, &type, &protocols))
        if (type == sdtSequence)
            while (ServiceGetValue(&protocols, &type, &protocol))
            if (type == sdtSequence
               && ServiceGetValue(&protocol, &type, &value)
               && type == sdtUUID
               && RegionMatchesUUID32(&value, (uint32)UUID16_L2CAP)
               && ServiceGetValue(&protocol, &type, &value)
               && type == sdtUnsignedInteger)
            {
                *psm = (uint16)RegionReadUnsigned(&value);
                return TRUE;
            }

    return FALSE;
}

/*! \brief Initiate an L2CAP connection request to the peer. */
static void appPeerSigConnectL2cap(const bdaddr *bd_addr)
{
    peerSigTaskData *peer_sig = PeerSigGetTaskData();

    static const uint16 l2cap_conftab[] =
    {
        /* Configuration Table must start with a separator. */
        L2CAP_AUTOPT_SEPARATOR,
        /* Flow & Error Control Mode. */
        L2CAP_AUTOPT_FLOW_MODE,
        /* Set to Basic mode with no fallback mode */
            BKV_16_FLOW_MODE( FLOW_MODE_BASIC, 0 ),
        /* Local MTU exact value (incoming). */
        L2CAP_AUTOPT_MTU_IN,
        /*  Exact MTU for this L2CAP connection - 672. */
            672,
        /* Remote MTU Minumum value (outgoing). */
        L2CAP_AUTOPT_MTU_OUT,
        /*  Minimum MTU accepted from the Remote device. */
            48,
        /* Local Flush Timeout  - Accept Non-default Timeout*/
        L2CAP_AUTOPT_FLUSH_OUT,
            BKV_UINT32R(DEFAULT_L2CAP_FLUSH_TIMEOUT,0),
        /* Configuration Table must end with a terminator. */
        L2CAP_AUTOPT_TERMINATOR
    };

    DEBUG_LOG("appPeerSigConnectL2cap");

    ConnectionL2capConnectRequest(&peer_sig->task,
                                  bd_addr,
                                  peer_sig->local_psm, peer_sig->remote_psm,
                                  CONFTAB_LEN(l2cap_conftab),
                                  l2cap_conftab);

    peer_sig->pending_connects++;
}

/*! \brief Handle the result of a SDP service attribute search.

    The returned attributes are checked to make sure they match the expected
    format of a peer signalling service record.
 */
static void appPeerSigHandleClSdpServiceSearchAttributeCfm(const CL_SDP_SERVICE_SEARCH_ATTRIBUTE_CFM_T *cfm)
{
    peerSigTaskData *peer_sig = PeerSigGetTaskData();

    DEBUG_LOG("appPeerSigHandleClSdpServiceSearchAttributeCfm, status %d", cfm->status);

    switch (appPeerSigGetState())
    {
        case PEER_SIG_STATE_CONNECTING_SDP_SEARCH:
        {
            /* Find the PSM in the returned attributes */
            if (cfm->status == sdp_response_success)
            {
                if (PeerSigGetL2capPSM(cfm->attributes, cfm->attributes + cfm->size_attributes,
                                         &peer_sig->remote_psm, saProtocolDescriptorList))
                {
                    DEBUG_LOG("appPeerSigHandleClSdpServiceSearchAttributeCfm, peer psm 0x%x", peer_sig->remote_psm);

                    /* Initate outgoing peer L2CAP connection */
                    appPeerSigConnectL2cap(&peer_sig->peer_addr);
                    appPeerSigSetState(PEER_SIG_STATE_CONNECTING_LOCAL);
                }
                else
                {
                    /* No PSM found, malformed SDP record on peer? */
                    DEBUG_LOG("appPeerSigHandleClSdpServiceSearchAttributeCfm, malformed SDP record");
                    appPeerSigSetState(PEER_SIG_STATE_DISCONNECTED);
                }
            }
            else if (cfm->status == sdp_no_response_data)
            {
                /* Peer Earbud doesn't support Peer Signalling service */
                DEBUG_LOG("appPeerSigHandleClSdpServiceSearchAttributeCfm, unsupported");
                appPeerSigSetState(PEER_SIG_STATE_DISCONNECTED);
            }
            else
            {
                /* SDP seach failed, retry? */
                if (peer_sig->sdp_search_attempts < appConfigPeerSigSdpSearchTryLimit())
                {
                    DEBUG_LOG("appPeerSigHandleClSdpServiceSearchAttributeCfm, retry status 0x%x attempts %d",
                                    cfm->status, peer_sig->sdp_search_attempts);
                    appPeerSigSetState(PEER_SIG_STATE_CONNECTING_SDP_SEARCH);
                }
                else
                {
                    appPeerSigSetState(PEER_SIG_STATE_DISCONNECTED);
                }
            }
        }
        break;

        case PEER_SIG_STATE_DISCONNECTING:
        {
            /* Search cancelled by a client. */
            DEBUG_LOG("appPeerSigHandleClSdpServiceSearchAttributeCfm, cancelled");
            appPeerSigSetState(PEER_SIG_STATE_DISCONNECTED);
        }
        break;

        default:
        {
            DEBUG_LOG("appPeerSigHandleClSdpServiceSearchAttributeCfm, unexpected state 0x%x status", appPeerSigGetState(), cfm->status);
            Panic();
        }
        break;
    }
}

/*! \brief Handle a L2CAP connection request that was initiated by the remote peer device. */
static void appPeerSigHandleL2capConnectInd(const CL_L2CAP_CONNECT_IND_T *ind)
{
    peerSigTaskData *peer_sig = PeerSigGetTaskData();
    DEBUG_LOGF("appPeerSigHandleL2capConnectInd, state %u, psm %u", appPeerSigGetState(), ind->psm);
    PanicFalse(ind->psm == peer_sig->local_psm);
    bool accept = FALSE;

    static const uint16 l2cap_conftab[] =
    {
        /* Configuration Table must start with a separator. */
        L2CAP_AUTOPT_SEPARATOR,
        /* Local Flush Timeout  - Accept Non-default Timeout*/
        L2CAP_AUTOPT_FLUSH_OUT,
            BKV_UINT32R(DEFAULT_L2CAP_FLUSH_TIMEOUT,0),
        L2CAP_AUTOPT_TERMINATOR
    };

    switch (appPeerSigGetState())
    {
        case PEER_SIG_STATE_DISCONNECTED:
        {
            /* only accept Peer Signalling connections from paired peer devices. */
            if (appDeviceIsPeer(&ind->bd_addr))
            {
                DEBUG_LOG("appPeerSigHandleL2capConnectInd, accepted");

                /* Move to 'connecting local' state */
                appPeerSigSetState(PEER_SIG_STATE_CONNECTING_REMOTE);

                /* Accept connection */
                accept = TRUE;
            }
            else
            {
                DEBUG_LOG("appPeerSigHandleL2capConnectInd, rejected, unknown peer");

                /* Not a known peer, remember it just in case we're in the middle of pairing */
                peer_sig->peer_addr = ind->bd_addr;
            }
        }
        break;

        default:
        {
            DEBUG_LOG("appPeerSigHandleL2capConnectInd, rejected, state %u", appPeerSigGetState());
        }
        break;
    }

    /* Keep track of this connection */
    peer_sig->pending_connects++;

    /* Send a response accepting or rejcting the connection. */
    ConnectionL2capConnectResponse(&peer_sig->task,     /* The client task. */
                                   accept,                 /* Accept/reject the connection. */
                                   ind->psm,               /* The local PSM. */
                                   ind->connection_id,     /* The L2CAP connection ID.*/
                                   ind->identifier,        /* The L2CAP signal identifier. */
                                   CONFTAB_LEN(l2cap_conftab),
                                   l2cap_conftab);          /* The configuration table. */
}

/*! \brief Handle the result of a L2CAP connection request.

    This is called for both local and remote initiated L2CAP requests.
*/
static void appPeerSigHandleL2capConnectCfm(const CL_L2CAP_CONNECT_CFM_T *cfm)
{
    peerSigTaskData *peer_sig = PeerSigGetTaskData();
    DEBUG_LOGF("appPeerSigHandleL2capConnectCfm, status %u, pending %u", cfm->status, peer_sig->pending_connects);

    /* Pending connection, return, will get another message in a bit */
    if (l2cap_connect_pending == cfm->status)
    {
        DEBUG_LOG("appPeerSigHandleL2capConnectCfm, connect pending, wait");
        return;
    }

    /* Decrement number of pending connect confirms, panic if 0 */
    PanicFalse(peer_sig->pending_connects > 0);
    peer_sig->pending_connects--;

    switch (appPeerSigGetState())
    {
        case PEER_SIG_STATE_CONNECTING_LOCAL:
        case PEER_SIG_STATE_CONNECTING_REMOTE:
        {
            /* If connection was succesful, get sink, attempt to enable wallclock and move
             * to connected state */
            if (l2cap_connect_success == cfm->status)
            {
                DEBUG_LOGF("appPeerSigHandleL2capConnectCfm, connected, conn ID %u, flush remote %u", cfm->connection_id, cfm->flush_timeout_remote);

                PanicNull(cfm->sink);
                peer_sig->link_sink = cfm->sink;
                peer_sig->link_source = StreamSourceFromSink(cfm->sink);

                /* Set the sink in the marshal_common module */
                MarshalCommon_SetSink(peer_sig->link_sink);

                /* Configure the tx (sink) & rx (source) */
                appLinkPolicyUpdateRoleFromSink(peer_sig->link_sink);

                MessageStreamTaskFromSink(peer_sig->link_sink, &peer_sig->task);
                MessageStreamTaskFromSource(peer_sig->link_source, &peer_sig->task);

                PanicFalse(SinkConfigure(peer_sig->link_sink, VM_SINK_MESSAGES, VM_MESSAGES_ALL));
                PanicFalse(SourceConfigure(peer_sig->link_source, VM_SOURCE_MESSAGES, VM_MESSAGES_ALL));

                /* Connection successful; go to connected state */
                appPeerSigSetState(PEER_SIG_STATE_CONNECTED);
            }
            else
            {
                /* Connection failed, if no more pending connections, return to disconnected state */
                if (peer_sig->pending_connects == 0)
                {
                    DEBUG_LOG("appPeerSigHandleL2capConnectCfm, failed, go to disconnected state");

                    appPeerSigSendConnectConfirmation(peerSigStatusFail);

                    appPeerSigSetState(PEER_SIG_STATE_DISCONNECTED);
                }
                else
                {
                    DEBUG_LOG("appPeerSigHandleL2capConnectCfm, failed, wait");
                }
            }
        }
        break;

        case PEER_SIG_STATE_DISCONNECTING:
        {
            /* The L2CAP connect request was cancelled by a SHUTDOWN_REQ
               before we received the L2CAP_CONNECT_CFM. */
            DEBUG_LOG("appPeerSigHandleL2capConnectCfm, cancelled, pending %u", peer_sig->pending_connects);

            if (l2cap_connect_success == cfm->status)
            {
                peer_sig->link_sink = cfm->sink;

                /* Set the sink in the marshal_common module */
                MarshalCommon_SetSink(peer_sig->link_sink);

                /* Re-enter the DISCONNECTING state - this time the L2CAP
                   disconnect request will be sent because link_sink is valid. */

                appPeerSigSetState(PEER_SIG_STATE_DISCONNECTING);
            }
            else
            {
                /* There is no valid L2CAP link to disconnect so go straight
                   to DISCONNECTED. */
                appPeerSigSetState(PEER_SIG_STATE_DISCONNECTED);
            }
        }
        break;

        default:
        {
            /* Connect confirm receive not in connecting state, connection must have failed */
            PanicFalse(l2cap_connect_success != cfm->status);
            DEBUG_LOGF("appPeerSigHandleL2capConnectCfm, failed, pending %u", peer_sig->pending_connects);
        }
        break;
    }
}

/*! \brief Handle a L2CAP disconnect initiated by the remote peer. */
static void appPeerSigHandleL2capDisconnectInd(const CL_L2CAP_DISCONNECT_IND_T *ind)
{
    peerSigTaskData *peer_sig = PeerSigGetTaskData();
    DEBUG_LOGF("appPeerSigHandleL2capDisconnectInd, status %u", ind->status);

    /* Always send reponse */
    ConnectionL2capDisconnectResponse(ind->identifier, ind->sink);

    /* Only change state if sink matches */
    if (ind->sink == peer_sig->link_sink)
    {
        /* Inform clients if link loss and we initiated the original connection */
        if (ind->status == l2cap_disconnect_link_loss && !BdaddrIsZero(&peer_sig->peer_addr))
        {
            DEBUG_LOG("appPeerSigHandleL2capDisconnectInd, link-loss");

            /* Set link-loss flag */
            peer_sig->link_loss_occurred = TRUE;
        }
        else
        {
            /* Clear link-loss flag */
            peer_sig->link_loss_occurred = FALSE;
        }

        appPeerSigSetState(PEER_SIG_STATE_DISCONNECTED);
    }
}

/*! \brief Handle a L2CAP disconnect confirmation.

    This is called for both local and remote initiated disconnects.
 */
static void appPeerSigHandleL2capDisconnectCfm(const CL_L2CAP_DISCONNECT_CFM_T *cfm)
{
    UNUSED(cfm);
    DEBUG_LOGF("appPeerSigHandleL2capDisconnectCfm, status %u", cfm->status);

    /* Move to disconnected state if we're in the disconnecting state */
    if (appPeerSigGetState() == PEER_SIG_STATE_DISCONNECTING)
    {
        appPeerSigSetState(PEER_SIG_STATE_DISCONNECTED);
    }
}

/* Peer Signalling L2CAP message format

octet
0       |   Header
1       |   opid
2       |   opid
3       |
4       |
5       |
6+n     |

*/
#define PEER_SIG_HEADER_OFFSET 0
#define PEER_SIG_OPID_OFFSET 1

#define PEER_SIG_TYPE_MASK      0x3
#define PEER_SIG_TYPE_REQUEST   0x1
#define PEER_SIG_TYPE_RESPONSE  0x2
/*! Marshalled peer signalling message type.
 *  Peer signaling makes no assumptions about the contents, including
 *  not requiring that there be a response message in return. */
#define PEER_SIG_TYPE_MARSHAL   0x3

#define PEER_SIG_GET_HEADER_TYPE(hdr) ((hdr) & PEER_SIG_TYPE_MASK)
#define PEER_SIG_SET_HEADER_TYPE(hdr, type) ((hdr) = (((hdr) & ~PEER_SIG_TYPE_MASK) | type))

/* Request type */
#define PEER_SIG_PAYLOAD_SIZE_OFFSET 3
#define PEER_SIG_PAYLOAD_OFFSET 5
#define PEER_SIG_REQUEST_SIZE_MIN 5

/* Response type */
#define PEER_SIG_RESPONSE_STATUS_OFFSET 3
#define PEER_SIG_RESPONSE_SIZE  5

/* Marshal type */
#define PEER_SIG_MARSHAL_CHANNELID_OFFSET   1
#define PEER_SIG_MARSHAL_PAYLOAD_OFFSET     5
#define PEER_SIG_MARSHAL_HEADER_SIZE        5

enum peer_sig_reponse
{
    peer_sig_success = 0,
    peer_sig_fail,
};

/*! \brief Claim the requested number of octets from a sink. */
static uint8 *appPeerSigClaimSink(Sink sink, uint16 size)
{
    uint8 *dest = SinkMap(sink);
    uint16 available = SinkSlack(sink);
    uint16 claim_size = 0;
    uint16 already_claimed = SinkClaim(sink, 0);
    uint16 offset = 0;
    
    if (size > available)
    {
        DEBUG_LOGF("appPeerSigMarshalClaimSink attempt to claim too much %u", size);
        return NULL;
    }

    /* We currently have to claim an extra margin in the sink for bytes the
     * marshaller will add, describing the marshalled byte stream. This can
     * accumulate in the sink, so use them up first. */
    if (already_claimed < size)
    {
        claim_size = size - already_claimed;
        offset = SinkClaim(sink, claim_size);
    }
    else
    {
        offset = already_claimed;
    }

//    DEBUG_LOG("appPeerSigClaimSink sz_req %u sz_avail %u dest %p already_claimed %u offset %u",
//                                    size, available, dest, already_claimed, offset);

    if ((NULL == dest) || (offset == 0xffff))
    {
        DEBUG_LOG("appPeerSigClaimSink SinkClaim returned Invalid Offset");
        return NULL;
    }

    return (dest + offset - already_claimed);
}

/*! \brief Write a peer signalling request packet to the outgoing peer signalling sink.

    \param opid The opid type of the packet.
    \param payload Pointer to the data payload. NULL if there is no payload.
    \param payload_size Number of octets in the data payload. 0 if there is no payload.
 */
static void appPeerSigL2capSendRequest(uint16 opid, const uint8 *payload, uint16 payload_size)
{
    uint8 *data = NULL;
    uint16 size = payload_size + PEER_SIG_REQUEST_SIZE_MIN;
    peerSigTaskData *peer_sig = PeerSigGetTaskData();
    Sink sink = peer_sig->link_sink;

    data = appPeerSigClaimSink(sink, size);
    if (data)
    {
        uint8 hdr = 0;
        uint8 *msg = data;

        msg[PEER_SIG_HEADER_OFFSET] = PEER_SIG_SET_HEADER_TYPE(hdr, PEER_SIG_TYPE_REQUEST);

        appPeerSigWriteUint16(&msg[PEER_SIG_OPID_OFFSET], opid);
        appPeerSigWriteUint16(&msg[PEER_SIG_PAYLOAD_SIZE_OFFSET], payload_size);

        if (payload && payload_size)
        {
            memcpy(&msg[PEER_SIG_PAYLOAD_OFFSET], payload, payload_size);
        }

        SinkFlush(sink, size);
    }
}

/*! \brief Send a peer signalling request packet to the peer.

    \param client_task Task to send the request delivery confirmation.
    \param op_id The opid type of the request.
    \param size_payload Number of octets in the data payload. 0 if there is no payload.
    \param payload Pointer to the data payload. NULL if there is no payload.
 */
static void appPeerSigL2capRequest(Task client_task, uint16 op_id,
                                   uint16 size_payload, const uint8 *payload)
{
    peerSigTaskData *peer_sig = PeerSigGetTaskData();

    PanicFalse(SinkIsValid(peer_sig->link_sink));

    DEBUG_LOG("appPeerSigL2capRequest opid 0x%x size %u", op_id, size_payload);

    /* Store task for rsponse and oepration ID, so when confirmation comes back we can send
     * message to correct task */
    peer_sig->client_task = client_task;
    peer_sig->current_op = op_id;

    /* Set lock to prevent any other operations */
    peer_sig->lock |= peer_sig_lock_op;

    /* Send a request over the L2CAP connection */
    appPeerSigL2capSendRequest(op_id, payload, size_payload);

    /* Cancel inactivity timer, it will be restarted when response is received */
    appPeerSigCancelInactivityTimer();
}

/*! \brief Write a peer signalling response packet to the outgoing peer signalling sink.

    \param opid The opid of the packet response.
    \param status Status of the response.
 */
static void appPeerSigL2capSendResponse(uint16 opid, uint16 status)
{
    uint8 *data = NULL;
    peerSigTaskData *peer_sig = PeerSigGetTaskData();
    Sink sink = peer_sig->link_sink;

    data = appPeerSigClaimSink(sink, PEER_SIG_RESPONSE_SIZE);
    if (data)
    {
        uint8 hdr = 0;

        data[PEER_SIG_HEADER_OFFSET] = PEER_SIG_SET_HEADER_TYPE(hdr, PEER_SIG_TYPE_RESPONSE);
        appPeerSigWriteUint16(&data[PEER_SIG_OPID_OFFSET], opid);
        appPeerSigWriteUint16(&data[PEER_SIG_RESPONSE_STATUS_OFFSET], status);

        SinkFlush(sink, PEER_SIG_RESPONSE_SIZE);
    }
}

/*! \brief Process an incoming peer signalling request packet.

    The request is passed to the correct handler based on the #opid.

    The handler returns TRUE if the request was handled successfully, otherwise
    FALSE.

    The response status is sent back to the peer immediately.

    \param opid The opid type of the request.
    \param data Pointer to the data payload. NULL if there is no payload.
    \param data_size Number of octets in the data payload. 0 if there is no payload.
 */
static void appPeerSigL2capProcessRequest(uint16 opid, const uint8 *data, uint16 data_size)
{
    bool rc = FALSE;

    PanicFalse(data_size >= PEER_SIG_REQUEST_SIZE_MIN);

    uint16 size_payload = appPeerSigReadUint16(&data[PEER_SIG_PAYLOAD_SIZE_OFFSET]);
    const uint8 *payload = &data[PEER_SIG_PAYLOAD_OFFSET];

    UNUSED(payload);

    DEBUG_LOG("appPeerSigL2capProcessRequest opid 0x%x size %u", opid, size_payload);

    switch (opid)
    {
    case PEER_SIG_CMD_ADD_LINK_KEY:
        rc = appPeerSigHandleRxLinkKey(payload, size_payload);
        break;

    case PEER_SIG_CMD_PAIR_HANDSET_ADDRESS:
        rc = appPeerSigHandlePairHandsetCommand(payload, size_payload);
        break;

    case PEER_SIG_CMD_MSG_CHANNEL_MSG:
        rc = appPeerSigHandleMsgChannelRx(payload, size_payload);
        break;

    case PEER_SIG_CMD_CONNECT_HANDSET:
        rc = appPeerSigHandleConnectHandsetCommand(payload, size_payload);
        break;

    /* add handlers for new incoming peer signalling message types here */

    default:
        DEBUG_LOG("appPeerSigL2capProcessRequest unhandled opid 0x%x", opid);
        break;
    }

    /* Reply to the request */
    appPeerSigL2capSendResponse(opid, rc ? peer_sig_success : peer_sig_fail);
}

/*! \brief Process an incoming peer signalling response packet.

    The response is passed to the correct handler based on the #opid.

    The status of the response is sent to the client Task that was stored when
    the request was made.

    \param opid The opid type of the request.
    \param data Pointer to the raw incoming data packet.
    \param data_size Number of octets in the raw incoming data packet.
 */
static void appPeerSigL2capProcessResponse(uint16 opid, const uint8 *data, uint16 data_size)
{
    peerSigTaskData* peer_sig = PeerSigGetTaskData();

    UNUSED(peer_sig);

    PanicFalse(data_size == PEER_SIG_RESPONSE_SIZE);

    uint16 status = appPeerSigReadUint16(&data[PEER_SIG_RESPONSE_STATUS_OFFSET]);

    DEBUG_LOGF("appPeerSigL2capProcessResponse %d opid:%x", status, opid);

    switch (opid)
    {
        case PEER_SIG_CMD_ADD_LINK_KEY:
            appPeerSigMsgLinkKeyConfirmation(peer_sig->client_task, status == peer_sig_success ?
                                             peerSigStatusSuccess : peerSigStatusLinkKeyTxFail,
                                             &peer_sig->handset_addr);
            break;

        case PEER_SIG_CMD_PAIR_HANDSET_ADDRESS:
            appPeerSigMsgPairHandsetConfirmation(peer_sig->client_task, status == peer_sig_success ?
                                                 peerSigStatusSuccess : peerSigStatusPairHandsetTxFail,
                                                 &peer_sig->handset_addr);
            break;

        case PEER_SIG_CMD_MSG_CHANNEL_MSG:
            appPeerSigMsgChannelTxConfirmation(peer_sig->client_task, status == peer_sig_success ?
                                               peerSigStatusSuccess : peerSigStatusMsgChannelTxFail,
                                               peer_sig->current_msg_channel);
            peer_sig->current_msg_channel = 0;
            break;

        /* add handlers for new outgoing peer signalling message confirmations here */

        default:
            DEBUG_LOGF("appPeerSigL2capProcessResponse unknown opid:%x", opid);
            break;
    }

    /* Clear up, no operation in progress now */
    peer_sig->client_task = NULL;
    peer_sig->current_op = 0;

    /* Clear lock, this may result in the next message being delivered */
    peer_sig->lock &= ~peer_sig_lock_op;
}

static void appPeerSigL2capProcessMarshal(const uint8* data, uint16 size)
{
    peerSigTaskData* peer_sig = PeerSigGetTaskData();
    task_list_data_t list_data = {0};
    marshal_msg_channel_data_t* mmcd = NULL;
    Task task = NULL;
    uint16 marshal_size = size - PEER_SIG_MARSHAL_HEADER_SIZE; 
    peerSigMsgChannel channel = appPeerSigReadUint32(&data[PEER_SIG_MARSHAL_CHANNELID_OFFSET]);

//    DEBUG_LOG("appPeerSigL2capProcessMarshal channel %u data %p size %u", channel, data, size);

#ifdef DUMP_MARSHALL_DATA
    dump_buffer(data, size);
#endif
    
    while (TaskList_IterateWithData(peer_sig->marshal_msg_channel_tasks, &task, &list_data))
    {
        mmcd = list_data.ptr;
        if (mmcd->msg_channel_id == channel)
        {
            marshal_type_t type;
            void* rx_msg;

            UnmarshalSetBuffer(mmcd->unmarshaller, &data[PEER_SIG_MARSHAL_PAYLOAD_OFFSET], marshal_size); 

            if (Unmarshal(mmcd->unmarshaller, &rx_msg, &type))
            {
//                size_t consumed = 0;
                MAKE_MESSAGE(PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND);
            
//                consumed = UnmarshalConsumed(mmcd->unmarshaller);
//                DEBUG_LOG("appPeerSigL2capProcessMarshal consumed %u type %u msg %p *msg %x", consumed, type, rx_msg, *(uint32*)rx_msg);

                message->channel = channel;
                message->msg = rx_msg;
                message->type = type;
                MessageSend(task, PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND, message);
                UnmarshalClearStore(mmcd->unmarshaller);
            }
            break;
        }
    }
}

/*! \brief Process incoming peer signalling data packets. */
static void appPeerSigL2capProcessData(void)
{
    uint16 size = 0;
    peerSigTaskData *peer_sig = PeerSigGetTaskData();
    Source source = peer_sig->link_source;

    while((size = SourceBoundary(source)) != 0)
    {
        const uint8 *data = SourceMap(source);

        uint8 type = PEER_SIG_GET_HEADER_TYPE(data[PEER_SIG_HEADER_OFFSET]);
        uint16 opid = appPeerSigReadUint16(&data[PEER_SIG_OPID_OFFSET]);

        /*DEBUG_LOG("appPeerSigL2capProcessData type 0x%x opid 0x%x", type, opid);*/

        switch (type)
        {
        case PEER_SIG_TYPE_REQUEST:
            appPeerSigL2capProcessRequest(opid, data, size);
            break;

        case PEER_SIG_TYPE_RESPONSE:
            appPeerSigL2capProcessResponse(opid, data, size);
            break;

        case PEER_SIG_TYPE_MARSHAL:
//            DEBUG_LOG("appPeerSigL2capProcessData type:marshal size %d", size);
            appPeerSigL2capProcessMarshal(data, size);
            break;

        default:
#ifdef DUMP_MARSHALL_DATA
            dump_buffer(data, size);
#endif
            Panic();
            break;
        }

        SourceDrop(source, size);
    }

    SourceClose(source);

    /* Restart in-activity timer */
    if (appPeerSigGetState() == PEER_SIG_STATE_CONNECTED)
        appPeerSigStartInactivityTimer();
}

/*! \brief Handle notifcation of new data arriving on the incoming peer signalling channel. */
static void appPeerSigHandleMMD(const MessageMoreData *mmd)
{
    peerSigTaskData *peer_sig = PeerSigGetTaskData();

    if (mmd->source == peer_sig->link_source)
    {
        appPeerSigL2capProcessData();
    }
    else
    {
        DEBUG_LOG("MMD received that doesn't match a link");
        if (peer_sig->link_source)
        {
            Panic();
        }
    }
}

/*! \brief Handle notification of more space in the peer signalling channel. */
static void appPeerSigHandleMMS(const MessageMoreSpace *mms)
{
    peerSigTaskData *peer_sig = PeerSigGetTaskData();

    if ((mms->sink == peer_sig->link_sink) && (peer_sig->retx))
    {
        DEBUG_LOGF("PEER_SIG: MMS %u", SinkSlack(PeerSigGetTaskData()->link_sink));
        appPeerSigHandleInternalMarshalledMsgChannelTxRequest(peer_sig->retx, TRUE);
    }
}

/******************************************************************************
 * Handlers for peer signalling internal messages
 ******************************************************************************/

/*! \brief Send link key to peer earbud.
 */
static void appPeerSigHandleInternalLinkKeyRequest(PEER_SIG_INTERNAL_LINK_KEY_REQ_T *req)
{
    peerSigTaskData *peer_sig = PeerSigGetTaskData();
    DEBUG_LOGF("appPeerSigHandleInternalLinkKeyRequest, state %u", appPeerSigGetState());

    switch (appPeerSigGetState())
    {
        case PEER_SIG_STATE_CONNECTED:
        {
            uint8 message[PEER_SIG_CMD_ADD_LINK_KEY_SIZE];
            int index;

            /* Remember handset address */
            peer_sig->handset_addr = req->handset_addr;

            /* Build data for message, handset address and key */
            index = PEER_SIG_CMD_ADD_LINK_KEY_ADDR_TYPE_OFFSET;
            message[index] = PEER_SIG_CMD_ADD_LINK_KEY_ADDR_TYPE_BREDR;
            index = PEER_SIG_CMD_ADD_LINK_KEY_ADDR_OFFSET;
            message[index++] =  req->handset_addr.lap & 0xFF;
            message[index++] = (req->handset_addr.lap >> 8) & 0xFF;
            message[index++] = (req->handset_addr.lap >> 16) & 0xFF;
            message[index++] =  req->handset_addr.uap;
            message[index++] =  req->handset_addr.nap & 0xFF;
            message[index++] = (req->handset_addr.nap >> 8) & 0xFF;
            index = PEER_SIG_CMD_ADD_LINK_KEY_KEY_TYPE_OFFSET;
            message[index] = PEER_SIG_CMD_ADD_LINK_KEY_KEY_TYPE_0;
            index = PEER_SIG_CMD_ADD_LINK_KEY_KEY_OFFSET;
            memcpy(&message[index], req->key, req->key_len);

            /* Send the link key over L2CAP */
            appPeerSigL2capRequest(req->client_task, PEER_SIG_CMD_ADD_LINK_KEY,
                                    PEER_SIG_CMD_ADD_LINK_KEY_SIZE, message);
        }
        break;

        default:
        {
            appPeerSigMsgLinkKeyConfirmation(req->client_task, peerSigStatusLinkKeyTxFail,
                                             &req->handset_addr);
        }
        break;
    }
}

/*! \brief Send handset pair command to peer earbud. */
static void appPeerSigHandleInternalPairHandsetRequest(const PEER_SIG_INTERNAL_PAIR_HANDSET_REQ_T *req)
{
    peerSigTaskData *peer_sig = PeerSigGetTaskData();
    DEBUG_LOGF("appPeerSigHandleInternalPairHandsetRequest, state %u", appPeerSigGetState());

    switch (appPeerSigGetState())
    {
        case PEER_SIG_STATE_CONNECTED:
        {
            uint8 message[PEER_SIG_CMD_PAIR_HANDSET_ADDRESS_SIZE];
            int index;

            /* Remember handset address */
            peer_sig->handset_addr = req->handset_addr;

            /* Build data for message and handset address */
            index = PEER_SIG_CMD_PAIR_HANDSET_ADDRESS_ADDR_TYPE_OFFSET;
            message[index] = PEER_SIG_CMD_ADD_LINK_KEY_ADDR_TYPE_BREDR;
            index = PEER_SIG_CMD_PAIR_HANDSET_ADDRESS_ADDR_OFFSET;
            message[index++] =  req->handset_addr.lap & 0xFF;
            message[index++] = (req->handset_addr.lap >> 8) & 0xFF;
            message[index++] = (req->handset_addr.lap >> 16) & 0xFF;
            message[index++] =  req->handset_addr.uap;
            message[index++] =  req->handset_addr.nap & 0xFF;
            message[index++] = (req->handset_addr.nap >> 8) & 0xFF;

            /* Send the handset address over L2CAP */
            appPeerSigL2capRequest(req->client_task, PEER_SIG_CMD_PAIR_HANDSET_ADDRESS,
                                    PEER_SIG_CMD_PAIR_HANDSET_ADDRESS_SIZE, message);
        }
        break;

        default:
        {
            appPeerSigMsgPairHandsetConfirmation(req->client_task, peerSigStatusLinkKeyTxFail,
                                                 &req->handset_addr);
        }
        break;
    }
}

/*! \brief Send a Msg Channel transmission to the peer earbud.
 */
static void appPeerSigHandleInternalMsgChannelTxRequest(const PEER_SIG_INTERNAL_MSG_CHANNEL_TX_REQ_T* req)
{
    peerSigTaskData *peer_sig = PeerSigGetTaskData();

    DEBUG_LOGF("appPeerSigHandleInternalMsgChannelTxRequest, state %u chan %u size %u",
                appPeerSigGetState(), req->channel, req->msg_size);

    switch (appPeerSigGetState())
    {
        case PEER_SIG_STATE_CONNECTED:
        {
            /* \todo could optimise to reduce malloc for messages small enough to put on the stack */
            uint8* message = PanicUnlessMalloc(PEER_SIG_CMD_MSG_CHANNEL_HEADER_SIZE + req->msg_size);

            /* remember message channel for confirmation messages */
            peer_sig->current_msg_channel = req->channel;

            appPeerSigWriteUint32(&message[PEER_SIG_CMD_MSG_CHANNEL_ID_OFFSET], req->channel);
            appPeerSigWriteUint16(&message[PEER_SIG_CMD_MSG_CHANNEL_DATA_LENGTH_OFFSET], req->msg_size);
            memcpy(&message[PEER_SIG_CMD_MSG_CHANNEL_DATA_OFFSET], req->msg, req->msg_size);

            /* Send the MsgChannel Tx Req over L2CAP */
            appPeerSigL2capRequest(req->client_task, PEER_SIG_CMD_MSG_CHANNEL_MSG,
                                    PEER_SIG_CMD_MSG_CHANNEL_HEADER_SIZE + req->msg_size, message);

            free(message);
        }
        break;

        default:
        {
            /* send tx confirmation fail message to task registered for the channel */
            appPeerSigMsgChannelTxConfirmation(req->client_task, peerSigStatusMsgChannelTxFail,
                                               req->channel);
        }
        break;
    }
}

/*! \brief Write the marshalled message header into a buffer. */
static void appPeerSigWriteMarshalMsgChannelHeader(uint8* bufptr, peerSigMsgChannel channel)
{
    uint8 hdr = 0;

    bufptr[PEER_SIG_HEADER_OFFSET] = PEER_SIG_SET_HEADER_TYPE(hdr, PEER_SIG_TYPE_MARSHAL);
    appPeerSigWriteUint32(&bufptr[PEER_SIG_MARSHAL_CHANNELID_OFFSET], channel);
}

/*! \brief Find the marshaller for a give marshalled message channel task. */
static marshaller_t appPeerSigGetMsgChannelMarshaller(Task task)
{
    peerSigTaskData *peer_sig = PeerSigGetTaskData();
    task_list_data_t data = {0};

    if (TaskList_GetDataForTask(peer_sig->marshal_msg_channel_tasks, task, &data))
    {
        marshal_msg_channel_data_t* mmcd = (marshal_msg_channel_data_t*)data.ptr;
        return mmcd->marshaller;
    }
    DEBUG_LOGF("appPeerSigGetMsgChannelMarshaller failed to find marshaller for task %x", task);
    return NULL;
}

/*! \brief Attempt to marshal a message to the peer. */
static void appPeerSigHandleInternalMarshalledMsgChannelTxRequest(
                  const PEER_SIG_INTERNAL_MARSHALLED_MSG_CHANNEL_TX_REQ_T* req, bool retx)
{
    peerSigTaskData *peer_sig = PeerSigGetTaskData();

    DEBUG_LOGF("appPeerSigHandleInternalMarshalledMsgChannelTxRequest chan %u msgptr %p type %u",
                                req->channel, req->msg_ptr, req->type);

    switch (appPeerSigGetState())
    {
        case PEER_SIG_STATE_CONNECTED:
        {
            marshaller_t marshaller;
            size_t space_required = 0;
            uint8* bufptr = NULL;
                
            /* get the marshaller for this msg channel */
            marshaller = PanicNull(appPeerSigGetMsgChannelMarshaller(req->client_task));

            /* determine how much space the marshaller will need in order to claim
             * it from the l2cap sink, then try and claim that amount */
            MarshalSetBuffer(marshaller, NULL, 0);
            Marshal(marshaller, req->msg_ptr, req->type);
            space_required = MarshalRemaining(marshaller);
            bufptr = appPeerSigClaimSink(peer_sig->link_sink, PEER_SIG_MARSHAL_HEADER_SIZE + space_required);

            if (bufptr)
            {
                /* write the marshal msg header */
                appPeerSigWriteMarshalMsgChannelHeader(bufptr, req->channel);

                /* tell the marshaller where in the buffer it can write */
                MarshalSetBuffer(marshaller, &bufptr[PEER_SIG_MARSHAL_PAYLOAD_OFFSET], space_required); 

                /* actually marshal this time and flush the sink to transmit it */
                PanicFalse(Marshal(marshaller, req->msg_ptr, req->type));
#ifdef DUMP_MARSHALL_DATA
                dump_buffer(bufptr, PEER_SIG_MARSHAL_HEADER_SIZE + space_required);
#endif
                SinkFlush(peer_sig->link_sink, PEER_SIG_MARSHAL_HEADER_SIZE + space_required);

                /* clean the marshaller for next time */
                MarshalClearStore(marshaller);

                /* tell the client the message was sent */
                appPeerSigMarshalledMsgChannelTxCfm(req->client_task, req->msg_ptr, peerSigStatusSuccess, req->channel);

                /* if this was a retx, then free the message and clear the lock to
                 * enable other operations */
                if (retx)
                {
                    DEBUG_LOG("appPeerSigHandleInternalMarshalledMsgChannelTxRequest retx success");
                    free(peer_sig->retx);
                    peer_sig->retx = NULL;
                    peer_sig->lock &= ~peer_sig_lock_marshal;
                }
            }
            else
            {
                DEBUG_LOG("appPeerSigHandleInternalMarshalledMsgChannelTxRequest no l2cap space (retx:%u)", retx);

                /* if not a retx then remember the message and wait for more space in the l2cap */
                if (!retx)
                {
                    MAKE_MESSAGE(PEER_SIG_INTERNAL_MARSHALLED_MSG_CHANNEL_TX_REQ);
                    memcpy(message, req, sizeof(PEER_SIG_INTERNAL_MARSHALLED_MSG_CHANNEL_TX_REQ_T));
                    peer_sig->retx = message;
                    peer_sig->lock |= peer_sig_lock_marshal;
                }
            }
        }
        break;

        default:
        {
            DEBUG_LOG("appPeerSigHandleInternalMarshalledMsgChannelTxRequest not connected");
            appPeerSigMarshalledMsgChannelTxCfm(req->client_task, req->msg_ptr,
                                                peerSigStatusMarshalledMsgChannelTxFail,
                                                req->channel);
        }
        break;
    }
}

/*! \brief Peer signalling task message handler.
 */
static void appPeerSigHandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch (id)
    {
        /* Connection library messages */
        case CL_L2CAP_REGISTER_CFM:
            appPeerSigHandleClL2capRegisterCfm((const CL_L2CAP_REGISTER_CFM_T *)message);
            break;

        case CL_SDP_REGISTER_CFM:
            appPeerSigHandleClSdpRegisterCfm((const CL_SDP_REGISTER_CFM_T *)message);
            break;

        case CL_SDP_SERVICE_SEARCH_ATTRIBUTE_CFM:
            appPeerSigHandleClSdpServiceSearchAttributeCfm((const CL_SDP_SERVICE_SEARCH_ATTRIBUTE_CFM_T *)message);
            return;

        case CL_L2CAP_CONNECT_IND:
            appPeerSigHandleL2capConnectInd((const CL_L2CAP_CONNECT_IND_T *)message);
            break;

        case CL_L2CAP_CONNECT_CFM:
            appPeerSigHandleL2capConnectCfm((const CL_L2CAP_CONNECT_CFM_T *)message);
            break;

        case CL_L2CAP_DISCONNECT_IND:
            appPeerSigHandleL2capDisconnectInd((const CL_L2CAP_DISCONNECT_IND_T *)message);
            break;

        case CL_L2CAP_DISCONNECT_CFM:
            appPeerSigHandleL2capDisconnectCfm((const CL_L2CAP_DISCONNECT_CFM_T *)message);
            break;

        case MESSAGE_MORE_DATA:
            appPeerSigHandleMMD((const MessageMoreData *)message);
            break;

        case MESSAGE_MORE_SPACE:
            appPeerSigHandleMMS((const MessageMoreSpace *)message);
            break;

        /* Internal Peer Signalling Messages */
        case PEER_SIG_INTERNAL_STARTUP_REQ:
            appPeerSigHandleInternalStartupRequest((PEER_SIG_INTERNAL_STARTUP_REQ_T *)message);
            break;

        case PEER_SIG_INTERNAL_INACTIVITY_TIMER:
            appPeerSigInactivityTimeout();
            break;

        case PEER_SIG_INTERNAL_LINK_KEY_REQ:
            appPeerSigHandleInternalLinkKeyRequest((PEER_SIG_INTERNAL_LINK_KEY_REQ_T *)message);
            break;

        case PEER_SIG_INTERNAL_PAIR_HANDSET_REQ:
            appPeerSigHandleInternalPairHandsetRequest((const PEER_SIG_INTERNAL_PAIR_HANDSET_REQ_T *)message);
            break;

        case PEER_SIG_INTERNAL_SHUTDOWN_REQ:
            appPeerSigHandleInternalShutdownReq();
            break;

        case PEER_SIG_INTERNAL_MSG_CHANNEL_TX_REQ:
            appPeerSigHandleInternalMsgChannelTxRequest((PEER_SIG_INTERNAL_MSG_CHANNEL_TX_REQ_T*)message);
            break;

        case PEER_SIG_INTERNAL_MARSHALLED_MSG_CHANNEL_TX_REQ:
            appPeerSigHandleInternalMarshalledMsgChannelTxRequest((PEER_SIG_INTERNAL_MARSHALLED_MSG_CHANNEL_TX_REQ_T*)message, FALSE);
            break;

        default:
            DEBUG_LOGF("appPeerSigHandleMessage. Unhandled message 0x%04x (%d)",id,id);
            break;
    }
}

/******************************************************************************
 * PUBLIC API
 ******************************************************************************/
/* Initialise the peer signalling module.
 */
bool appPeerSigInit(Task init_task)
{
    peerSigTaskData *peer_sig = PeerSigGetTaskData();
    memset(peer_sig, 0, sizeof(*peer_sig));

    /* Set task's message handler */
    peer_sig->task.handler = appPeerSigHandleMessage;

    /* Set initial state and ensure lock is cleared */
    peer_sig->state = PEER_SIG_STATE_NULL;
    peer_sig->lock = peer_sig_lock_none;
    peer_sig->link_loss_occurred = FALSE;

    /* Create the list of peer signalling clients that receive
     * PEER_SIG_CONNECTION_IND messages. */
    peer_sig->peer_sig_client_tasks = TaskList_Create();

    /* Create a TaskListWithData to track client tasks
     * for specific message channels. */
    peer_sig->msg_channel_tasks = TaskList_WithDataCreate();

    /* Create a TaskListWithData to track client tasks for marshalling
     * message channels. */
    peer_sig->marshal_msg_channel_tasks = TaskList_WithDataCreate();

    /* Initialise task lists for connect and disconnect requests */
    TaskList_Initialise(&peer_sig->connect_tasks);
    TaskList_Initialise(&peer_sig->disconnect_tasks);

    /* Move to 'initialising' state */
    appPeerSigSetState(PEER_SIG_STATE_INITIALISING);

    appInitSetInitTask(init_task);
    return TRUE;
}

/* Send handset link key to peer headset.

    External API for link keys is 8 packed 16-bit words, internally and over
    L2CAP we use 8-bit words, make the conversion now.
 */
void appPeerSigLinkKeyToPeerRequest(Task task, const bdaddr *handset_addr,
                                    const uint16 *key, uint16 key_len)
{
    peerSigTaskData* peer_sig = PeerSigGetTaskData();
    uint16 size_key_bytes = key_len * 2;
    MAKE_PEER_SIG_MESSAGE_WITH_LEN(PEER_SIG_INTERNAL_LINK_KEY_REQ, size_key_bytes-1);

    DEBUG_LOGF("appPeerSigLinkKeyToPeerRequest, bdaddr %04x,%02x,%06lx",
               handset_addr->nap, handset_addr->uap, handset_addr->lap);

    checkPeerSigConnected(peer_sig, appPeerSigLinkKeyToPeerRequest);

    /* Build message to trigger TX of link key to peer, will wait for peer signalling
     * connection if none exists yet, or another operation if one already in
     * progress */
    message->client_task = task;
    message->handset_addr = *handset_addr;
    message->key_len = size_key_bytes;
    memcpy(message->key, key, size_key_bytes);
    MessageSendConditionally(&peer_sig->task, PEER_SIG_INTERNAL_LINK_KEY_REQ,
                             message, &peer_sig->lock);
}

/* Inform peer earbud of address of handset with which it should pair.
*/
void appPeerSigTxPairHandsetRequest(Task task, const bdaddr *handset_addr)
{
    peerSigTaskData* peer_sig = PeerSigGetTaskData();
    MAKE_MESSAGE(PEER_SIG_INTERNAL_PAIR_HANDSET_REQ);

    DEBUG_LOGF("appPeerSigTxPairHandsetRequest, bdaddr %04x,%02x,%06lx",
               handset_addr->nap, handset_addr->uap, handset_addr->lap);

    checkPeerSigConnected(peer_sig, appPeerSigTxPairHandsetRequest);

    message->client_task = task;
    message->handset_addr = *handset_addr;
    MessageSendConditionally(&peer_sig->task, PEER_SIG_INTERNAL_PAIR_HANDSET_REQ,
                             message, &peer_sig->lock);
}


/*! \brief Request a transmission on a message channel.
 */
void appPeerSigMsgChannelTxRequest(Task task,
                                   peerSigMsgChannel channel,
                                   const uint8* msg, uint16 msg_size)
{
    peerSigTaskData *peer_sig = PeerSigGetTaskData();
    MAKE_PEER_SIG_MESSAGE_WITH_LEN(PEER_SIG_INTERNAL_MSG_CHANNEL_TX_REQ, msg_size-1);

    DEBUG_LOG("appPeerSigMsgChannelTxRequest");

    checkPeerSigConnected(peer_sig, appPeerSigMsgChannelTxRequest);

    message->client_task = task;
    message->channel = channel;
    message->msg_size = msg_size;
    memcpy(message->msg, msg, msg_size);

    /* Send to task, potentially blocked on bringing up peer signalling channel */
    MessageSendConditionally(&peer_sig->task, PEER_SIG_INTERNAL_MSG_CHANNEL_TX_REQ,
                             message, &peer_sig->lock);
}

/* Register task with peer signalling for Link Key TX/RX operations.
 */
void appPeerSigLinkKeyTaskRegister(Task client_task)
{
    peerSigTaskData *peer_sig = PeerSigGetTaskData();

    DEBUG_LOG("appPeerSigLinkKeyTaskRegister");

    /* remember client task for when peer signalling connects */
    peer_sig->rx_link_key_task = client_task;
}

/* Unregister task with peer signalling for Link Key TX/RX operations.
 */
void appPeerSigLinkKeyTaskUnregister(void)
{
    peerSigTaskData* peer_sig = PeerSigGetTaskData();

    DEBUG_LOG("appPeerSigLinkKeyTaskUnregister");

    /* remember client task for when peer signalling connects */
    peer_sig->rx_link_key_task = NULL;
}

/* Try and connect peer signalling channel with specified peer earbud.
*/
void appPeerSigConnect(Task task, const bdaddr* peer_addr)
{
    DEBUG_LOG("appPeerSigConnect - startup");
    appPeerSigStartup(task, peer_addr);
}

/* Register to receive peer signalling notifications. */
void appPeerSigClientRegister(Task client_task)
{
    peerSigTaskData* peer_sig = PeerSigGetTaskData();
    TaskList_AddTask(peer_sig->peer_sig_client_tasks, client_task);
}

/* Unregister to stop receiving peer signalling notifications. */
void appPeerSigClientUnregister(Task client_task)
{
    peerSigTaskData* peer_sig = PeerSigGetTaskData();
    TaskList_RemoveTask(peer_sig->peer_sig_client_tasks, client_task);
}

/* Register task to receive handset commands. */
void appPeerSigHandsetCommandsTaskRegister(Task handset_commands_task)
{
    peerSigTaskData* peer_sig = PeerSigGetTaskData();
    peer_sig->rx_handset_commands_task = handset_commands_task;
}

/*! \brief Register to receive PEER_SIG_MSG_CHANNEL_RX_IND messages for a channel mask.

    \param[in] task         Task to receive incoming messages on the channels in channel_mask.
    \param[in] channel_mask Mask of channels IDs registered to messages.
 */
void appPeerSigMsgChannelTaskRegister(Task task, peerSigMsgChannel channel_mask)
{
    peerSigTaskData* peer_sig = PeerSigGetTaskData();
    task_list_data_t data = {0};

    if (TaskList_GetDataForTask(peer_sig->msg_channel_tasks, task, &data))
    {
        data.u32 |= channel_mask;
        TaskList_SetDataForTask(peer_sig->msg_channel_tasks, task, &data);
    }
    else
    {
        data.u32 |= channel_mask;
        TaskList_AddTaskWithData(peer_sig->msg_channel_tasks, task, &data);
    }
}

/*! \brief Stop receiving PEER_SIG_MSG_CHANNEL_RX_IND messages on a channel mask.

    \param[in] task         Task to stop receiving incoming messages for the channels in channel_mask.
    \param     channel_mask Mask of channels IDs to unregister for messages.
*/
void appPeerSigMsgChannelTaskUnregister(Task task, peerSigMsgChannel channel_mask)
{
    peerSigTaskData* peer_sig = PeerSigGetTaskData();
    task_list_data_t data = {0};

    if (TaskList_GetDataForTask(peer_sig->msg_channel_tasks, task, &data))
    {
        data.u32 &= ~channel_mask;
        if (data.u32)
        {
            TaskList_SetDataForTask(peer_sig->msg_channel_tasks, task, &data);
        }
        else
        {
            TaskList_RemoveTask(peer_sig->msg_channel_tasks, task);
        }
    }
}

/* Force peer signalling channel to disconnect if it is up. */
void appPeerSigDisconnect(Task task)
{
    DEBUG_LOG("appPeerSigDisconnect");

    appPeerSigShutdown(task);
}

/*! \brief Register a task for a marshalled message channel(s). */
void appPeerSigMarshalledMsgChannelTaskRegister(Task task, peerSigMsgChannel channel_mask,
                                                const marshal_type_descriptor_t * const * type_desc,
                                                size_t num_type_desc)
{
    peerSigTaskData *peer_sig = PeerSigGetTaskData();
    marshal_msg_channel_data_t* mmcd = PanicUnlessMalloc(sizeof(marshal_msg_channel_data_t));
    if (mmcd)
    {
        task_list_data_t data = {0};
        mmcd->msg_channel_id = channel_mask;
        mmcd->marshaller = PanicNull(MarshalInit(type_desc, num_type_desc));
        mmcd->unmarshaller = PanicNull(UnmarshalInit(type_desc, num_type_desc));
        data.ptr = mmcd;
        DEBUG_LOG("MarshalInit %p for task %p", mmcd->marshaller, task);
        PanicFalse((TaskList_AddTaskWithData(peer_sig->marshal_msg_channel_tasks, task, &data)));
    }
}

/*! \brief Unregister peerSigMsgChannel(s) for the a marshalled message channel. */
void appPeerSigMarshalledMsgChannelTaskUnregister(Task task, peerSigMsgChannel channel_mask)
{
    peerSigTaskData *peer_sig = PeerSigGetTaskData();
    task_list_data_t data = {0};

    if (TaskList_GetDataForTask(peer_sig->marshal_msg_channel_tasks, task, &data))
    {
        marshal_msg_channel_data_t* mmcd = data.ptr;

        /* remove the channel(s) from the data associated with this task */
        mmcd->msg_channel_id &= ~channel_mask;

        if (mmcd->msg_channel_id)
        {
            /* still have some channels registered, so just update the data */
            TaskList_SetDataForTask(peer_sig->marshal_msg_channel_tasks, task, &data);
        }
        else
        {
            /* no channels registered now, destroy the (un)marshaller, free the data
             * and delete the task from the list. */
            if (mmcd->marshaller)
            {
                MarshalDestroy(mmcd->marshaller, TRUE);
            }
            if (mmcd->unmarshaller)
            {
                UnmarshalDestroy(mmcd->unmarshaller, TRUE);
            }
            free(data.ptr);
            TaskList_RemoveTask(peer_sig->marshal_msg_channel_tasks, task);
        }
    }
}

/*! \brief Transmit a marshalled message channel message to the peer. */
void appPeerSigMarshalledMsgChannelTx(Task task,
                                      peerSigMsgChannel channel,
                                      void* msg, marshal_type_t type)
{
    peerSigTaskData *peer_sig = PeerSigGetTaskData();
    MAKE_MESSAGE(PEER_SIG_INTERNAL_MARSHALLED_MSG_CHANNEL_TX_REQ);

    DEBUG_LOG("appPeerSigMarshalledMsgChannelTx task %u channel %u type %u", task, channel, type);

    checkPeerSigConnected(peer_sig, appPeerSigMarshalledMsgChannelTx);

    message->client_task = task;
    message->channel = channel;
    message->msg_ptr = msg;
    message->type = type;

    /* Send to task, potentially blocked on bringing up peer signalling channel */
    MessageSendConditionally(&peer_sig->task,
                             PEER_SIG_INTERNAL_MARSHALLED_MSG_CHANNEL_TX_REQ,
                             message, &peer_sig->lock);
}

/*! \brief Test if peer signalling is connected to a peer. */
bool appPeerSigIsConnected(void)
{
    peerSigTaskData *peer_sig = PeerSigGetTaskData();
    return (peer_sig->state == PEER_SIG_STATE_CONNECTED);
}

Sink appPeerSigGetSink(void)
{
    peerSigTaskData *peer_sig = PeerSigGetTaskData();
    return peer_sig->link_sink;
}
