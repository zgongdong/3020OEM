/*!
\copyright  Copyright (c) 2015 - 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       peer_signalling.h
\brief	    Interface to module providing signalling to headset peer device.
*/

#ifndef PEER_SIGNALLING_H_
#define PEER_SIGNALLING_H_

#include "av.h"

#include <marshal_common.h>
#include <domain_message.h>
#include <task_list.h>
#include <rtime.h>

#include <bdaddr.h>
#include <csrtypes.h>
#include <marshal.h>

/*! Flag used on peer signalling states to indicate if the state represents
    an activity that will finish. This is reflected in the lock member of
    \ref peerSigTaskData. */
#define PEER_SIG_STATE_LOCK (0x10)

/*!
    @startuml

    [*] -d-> INITIALISING : Module init

    INITIALISING : Register SDP record for L2CAP
    INITIALISING -d-> DISCONNECTED : CL_SDP_REGISTER_CFM

    DISCONNECTED : No peer connection
    DISCONNECTED --> CONNECTING_ACL : Startup request (ACL not connected)
    DISCONNECTED --> CONNECTING_SDP_SEARCH : Startup request (ACL connected)
    DISCONNECTED --> CONNECTING_REMOTE : Remote L2CAP connect

    CONNECTING_ACL : Creating ACL connection to peer
    CONNECTING_ACL --> DISCONNECTED : Startup request (ACL remote connected)
    CONNECTING_ACL --> DISCONNECTED : Startup request (ACL connect failed)
    CONNECTING_ACL --> CONNECTING_SDP_SEARCH : Startup request (ACL connected)

    CONNECTING_SDP_SEARCH : Performing SDP search for Peer Signaling service
    CONNECTING_SDP_SEARCH --> CONNECTING_LOCAL : SDP success
    CONNECTING_SDP_SEARCH --> DISCONNECTED : SDP error
    CONNECTING_SDP_SEARCH --> DISCONNECTING : Shutdown request
    CONNECTING_SDP_SEARCH --> CONNECTING_SDP_SEARCH : SDP retry

    CONNECTING_LOCAL : Local initiated connection
    CONNECTING_LOCAL --> CONNECTED : L2CAP connect cfm (success)
    CONNECTING_LOCAL --> DISCONNECTED : L2CAP connect cfm (fail)
    CONNECTING_LOCAL --> DISCONNECTED : Remote L2CAP disconnect ind
    CONNECTING_LOCAL --> DISCONNECTING : Shutdown request

    CONNECTING_REMOTE : Remote initiated connection
    CONNECTING_REMOTE --> CONNECTED : L2CAP connect (success)
    CONNECTING_REMOTE --> DISCONNECTED : L2CAP connect (fail)
    CONNECTING_REMOTE --> DISCONNECTED : Remote L2CAP disconnect ind

    CONNECTED : Peer Signaling channel active
    CONNECTED --> DISCONNECTING : Shutdown request
    CONNECTED --> DISCONNECTING : Inactivity timeout
    CONNECTED --> DISCONNECTED : Remote L2CAP disconnect ind

    DISCONNECTING : Waiting for disconnect result
    DISCONNECTING --> DISCONNECTING : L2CAP connect cfm (client shutdown before L2CAP was connected)
    DISCONNECTING --> DISCONNECTED : L2CAP disconnect cfm
    DISCONNECTING --> DISCONNECTED : SDP search cfm

    @enduml
*/

/*! Peer signalling state machine states */
typedef enum
{
    PEER_SIG_STATE_NULL = 0,                                       /*!< Initial state */
    PEER_SIG_STATE_INITIALISING = 1 + PEER_SIG_STATE_LOCK,         /*!< Awaiting L2CAP registration */
    PEER_SIG_STATE_DISCONNECTED = 2,                               /*!< No connection */
    PEER_SIG_STATE_CONNECTING_ACL = 3 + PEER_SIG_STATE_LOCK,       /*!< Connecting ACL */
    PEER_SIG_STATE_CONNECTING_SDP_SEARCH = 4 + PEER_SIG_STATE_LOCK,/*!< Searching for Peer Signalling service */
    PEER_SIG_STATE_CONNECTING_LOCAL = 5 + PEER_SIG_STATE_LOCK,     /*!< Locally initiated connection in progress */
    PEER_SIG_STATE_CONNECTING_REMOTE = 6 + PEER_SIG_STATE_LOCK,    /*!< Remotely initiated connection is progress */
    PEER_SIG_STATE_CONNECTED = 7,                                  /*!< Connnected */
    PEER_SIG_STATE_DISCONNECTING = 8 + PEER_SIG_STATE_LOCK,        /*!< Disconnection in progress */
} appPeerSigState;

/*! Enumeration of peer signalling status codes. */
typedef enum
{
    /*! Operation success. */
    peerSigStatusSuccess,

    /*! Operation fail. */
    peerSigStatusFail,

    /*! Failed to send link key to peer. */
    peerSigStatusLinkKeyTxFail,

    /*! Signalling channel with peer earbud established. */
    peerSigStatusConnected,

    /*! Signalling channel with peer earbud disconnected. */
    peerSigStatusDisconnected,

    /*! Signalling channel with peer earbud disconnected due to link-loss. */
    peerSigStatusLinkLoss,

    /*! Failed to send #AVRCP_PEER_CMD_PAIR_HANDSET_ADDRESS. */
    peerSigStatusPairHandsetTxFail,

    /*! Message channel transmission failed. */
    peerSigStatusMsgChannelTxFail,
    
    /*! Marshalled Message channel transmission failed. */
    peerSigStatusMarshalledMsgChannelTxFail,

    /*! Requested action is already in progress */
    peerSigStatusInProgress,
} peerSigStatus;

/*! Messages that can be sent by peer signalling to client tasks. */

enum peer_signalling_messages
{
    /*! Module initialisation complete */
    PEER_SIG_INIT_CFM = PEER_SIG_MESSAGE_BASE,

    /*! Result of operation to send link key to peer. */
    PEER_SIG_LINK_KEY_TX_CFM,

    /*! Peer link key received. */
    PEER_SIG_LINK_KEY_RX_IND,

    /*! Signalling link to peer established. */
    PEER_SIG_CONNECTION_IND,

    /*! Pair Handset command received. */
    PEER_SIG_PAIR_HANDSET_IND,

    /*! Result of operation to send AVRCP_PEER_CMD_PAIR_HANDSET_ADDRESS to peer. */
    PEER_SIG_PAIR_HANDSET_CFM,

    /*! Data received over a peer signalling message channel. */
    PEER_SIG_MSG_CHANNEL_RX_IND,

    /*! Confirmation of delivery of a peer signalling message channel transmission. */
    PEER_SIG_MSG_CHANNEL_TX_CFM,

    /*! Peer request to connect to the handset */
    PEER_SIG_CONNECT_HANDSET_IND,

    /*! Confirmation of delivery of connect handset message */
    PEER_SIG_CONNECT_HANDSET_CFM,

    /*! Confirmation of transmission of peer signalling marshalled message channel
     *  transmission.
     *  \note this isn't confirmation of delivery, just transmission. */
    PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM,

    /*! Data received over a peer signalling marshalled message channel. */
    PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND,

    /*! Confirmation of a connection request. */
    PEER_SIG_CONNECT_CFM,

    /*! Confirmation of a disconnect request. */
    PEER_SIG_DISCONNECT_CFM,
};

/*! Channel IDs for peer signalling.
    Bitmask used so that a single client task could register for multiple channels,
    if we support that feature in the future.
 */
typedef enum
{
    /*! Channel ID for SCO Forwarding control messages. */
    PEER_SIG_MSG_CHANNEL_SCOFWD = 1UL << 0,

    /*! Channel ID for Peer Sync messages. */
    PEER_SIG_MSG_CHANNEL_PEER_SYNC = 1UL << 1,
    
    /*! Channel ID for State Proxy messages. */
    PEER_SIG_MSG_CHANNEL_STATE_PROXY = 1UL << 2,

    /*! Channel ID for Earbud application messages.
     *  Current usage is for commands from Primary Earbud to be sent to Secondary Earbud,
     *  at the application level.
     *  Temporarily a lot of the messages will be here, but move down into the TWS topology
     *  component. */
    PEER_SIG_MSG_CHANNEL_APPLICATION = 1UL << 3,

    PEER_SIG_MSG_CHANNEL_LOGICAL_INPUT_SWITCH = 1UL << 4,

    /*! Channel ID for Key Sync messages. */
    PEER_SIG_MSG_CHANNEL_KEY_SYNC = 1UL << 5,

    /*! Channel ID for shadow profile messages. */
    PEER_SIG_MSG_CHANNEL_SHADOW_PROFILE = 1UL << 6,

    /* force peerSigMsgChannel to be a 32-bit enum */
    PEER_SIG_MSG_CHANNEL_MAX    = 1UL << 30
} peerSigMsgChannel;

/*! Message sent to client task with result of operation to send link key to peer. */
typedef struct
{
    peerSigStatus status;           /*!< Status of request */
    bdaddr handset_addr;            /*!< Handset that this response refers to */
} PEER_SIG_LINK_KEY_TX_CFM_T;

/*! Message sent to client task with handset link key received from peer. */
typedef struct
{
    peerSigStatus status;           /*!< Status of request */
    bdaddr handset_addr;            /*!< Handset that this response refers to */
    uint16 key_len;                 /*!< Length of the link key in <B>uint16s, not octets</B> */
    uint16 key[1];                  /*!< Link key. This will be the full length, \ref key_len */
} PEER_SIG_LINK_KEY_RX_IND_T;

/*! Message sent to clients registered to receive notification of peer signalling
    connection and disconnection events.

    The status can be either #peerSigStatusConnected or
    #peerSigStatusDisconnected.
 */
typedef struct
{
    peerSigStatus status;           /*!< Connected / disconected status (see message description) */
} PEER_SIG_CONNECTION_IND_T;

/*! Message sent to pairing module to pair with a handset. */
typedef struct
{
    bdaddr handset_addr;            /*!< Address of handset to pair with */
} PEER_SIG_PAIR_HANDSET_IND_T;

/*! Message content when peer is requesting connect to handset */
typedef struct
{
    bool play_media;
} PEER_SIG_CONNECT_HANDSET_IND_T;

/*! Message sent to client task with result of operation to send pair handset command to peer. */
typedef struct
{
    peerSigStatus status;           /*!< Status of pairing message */
    bdaddr handset_addr;            /*!< Address of handset the status applies to */
} PEER_SIG_PAIR_HANDSET_CFM_T;

/*! Message sent to client task with result of operation to send connect handset command to peer. */
typedef struct
{
    peerSigStatus status;           /*!< Status of connect message */
} PEER_SIG_CONNECT_HANDSET_CFM_T;

/*! \brief Notification of incoming message on a peer signalling channel. */
typedef struct
{
    peerSigMsgChannel channel;      /*!< Channel over which message received. */
    uint16 msg_size;                /*!< Size of data in msg field. */
    uint8 msg[1];                   /*!< Message contents. */
} PEER_SIG_MSG_CHANNEL_RX_IND_T;

/*! \brief Confirmation of delivery of a message channel transmission. */
typedef struct
{
    peerSigStatus status;           /*!< Result of msg channel transmission. */
    peerSigMsgChannel channel;      /*!< Msg channel transmission channel used. */
} PEER_SIG_MSG_CHANNEL_TX_CFM_T;

/*! \brief Confirmation of transmission of a marshalled message channel message. */
typedef struct
{
    peerSigStatus status;           /*!< Result of msg channel transmission. */
    uint8* msg_ptr;                 /*!< Pointer to the message memory. */
    peerSigMsgChannel channel;      /*!< Msg channel transmission channel used. */
} PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM_T;

/*! \brief Indication of incoming marshalled message channel message. */
typedef struct
{
    peerSigMsgChannel channel;      /*!< Msg channel transmission channel used. */
    void* msg;                      /*!< Pointer to the message. */
    marshal_type_t type;            /*!< Message type. */
} PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND_T;

/*! brief Confirmation of the result of a connection request. */
typedef struct
{
    /*! Status of the connection request */
    peerSigStatus status;
} PEER_SIG_CONNECT_CFM_T;

/*! brief Confirmation of the result of a disconnect request. */
typedef struct
{
    /*! Status of the disconnect request */
    peerSigStatus status;
} PEER_SIG_DISCONNECT_CFM_T;

/*! Internal messages used by peer signalling. */
typedef enum
{
    /*! Message to bring up link to peer */
    PEER_SIG_INTERNAL_STARTUP_REQ,

    /*! Message to shut down link to peer */
    PEER_SIG_INTERNAL_SHUTDOWN_REQ,

    /*! Message to send Link Key to peer */
    PEER_SIG_INTERNAL_LINK_KEY_REQ,

    /*! Message to teardown peer signalling channel due to inactivity */
    PEER_SIG_INTERNAL_INACTIVITY_TIMER,

    /*! Message to send AVRCP_PEER_CMD_PAIR_HANDSET_ADDRESS to peer. */
    PEER_SIG_INTERNAL_PAIR_HANDSET_REQ,

    /*! Request to send traditional peer signalling message. */
    PEER_SIG_INTERNAL_MSG_CHANNEL_TX_REQ,

    /*! Request to send marshalled peer signalling message. */
    PEER_SIG_INTERNAL_MARSHALLED_MSG_CHANNEL_TX_REQ,
} PEER_SIG_INTERNAL_MSG;

/*! Internal message sent to start signalling to a peer */
typedef struct
{
    bdaddr peer_addr;           /*!< Address of peer */
} PEER_SIG_INTERNAL_STARTUP_REQ_T;

/*! Message definition for action to send link key to peer. */
typedef struct
{
    Task client_task;           /*!< Task to receive any response */
    bdaddr handset_addr;        /*!< Handset that link key is for  */
    uint16 key_len;             /*!< Length of link key, in octets */
    uint8 key[1];               /*!< Link key. This will be the full length, key_len */
} PEER_SIG_INTERNAL_LINK_KEY_REQ_T;

/*! Message definition for action to send pair handset command. */
typedef struct
{
    Task client_task;           /*!< Task to receive any response */
    bdaddr handset_addr;        /*!< Handset that peer should try to pair with */
} PEER_SIG_INTERNAL_PAIR_HANDSET_REQ_T;

/*! Message definition for action to send connect handset command. */
typedef struct
{
    Task client_task;           /*!< Task to receive any response */
    bool play_media;            /*!< Play media once handset is connected */
} PEER_SIG_INTERNAL_CONNECT_HANDSET_REQ_T;

/*! Structure used to request message channel transmission to peer. */
typedef struct
{
    Task client_task;           /*!< Task to receive the msg tx result. */
    peerSigMsgChannel channel;  /*!< Channel over which to transmit message. */
    uint16 msg_size;            /*!< Size of data in msg. */
    uint8 msg[1];               /*!< Message data to transmit. */
} PEER_SIG_INTERNAL_MSG_CHANNEL_TX_REQ_T;

/*! Structure used to request marshalled message channel transmission to peer. */
typedef struct
{
    Task client_task;           /*!< Task to receive the msg tx result. */
    peerSigMsgChannel channel;  /*!< Channel over which to transmit message. */
    void* msg_ptr;              /*!< Pointer to the message to be transmitted. */
    marshal_type_t type;        /*!< Marshal type of the message. */
} PEER_SIG_INTERNAL_MARSHALLED_MSG_CHANNEL_TX_REQ_T;

/*! \brief Initialise the peer signalling module.
 */
bool appPeerSigInit(Task init_task);

/*! \brief Send handset link key to peer headset.

    \param      task         Task to send confirmation message to.
    \param[in]  handset_addr Address of handset to pair with.
    \param[in]  key          Pointer to the link key to send
    \param      key_len      Length of the link key <B>in uint16's (not octets)</B>
*/
void appPeerSigLinkKeyToPeerRequest(Task task, const bdaddr *handset_addr,
                                  const uint16 *key, uint16 key_len);

/*! \brief Inform peer earbud of address of handset with which it should pair.

    \param[in] task         Task to send confirmation message to.
    \param[in] handset_addr Address of handset to pair with.
*/
void appPeerSigTxPairHandsetRequest(Task task, const bdaddr* handset_addr);

/*! \brief Register task with peer signalling for Link Key TX/RX operations.

    \param client_task Task to send messages to regarding link key operations.
 */
void appPeerSigLinkKeyTaskRegister(Task client_task);

/*! \brief Unregister task with peer signalling for Link Key TX/RX operations.
 */
void appPeerSigLinkKeyTaskUnregister(void);

/*! \brief Try and connect peer signalling channel with specified peer earbud.

    A PEER_SIG_CONNECT_CFM will be sent to the client Task with the result of
    the connection attempt.

    The status in PEER_SIG_CONNECT_CFM can have one of these values:
    \li peerSigStatusSuccess
    \li peerSigStatusFail

    \param[in] task The client task that PEER_SIG_CONNECT_CFM will be sent to.
    \param[in] peer_addr BT address of peer earbud to connect.
 */
void appPeerSigConnect(Task task, const bdaddr* peer_addr);

/*! \brief Register to receive peer signalling notifications.

    \param  client_task Task to register to receive peer signalling messages.
 */
void appPeerSigClientRegister(Task client_task);

/*! \brief Unregister task that is currently receiving peer signalling notifications.

    \param  client_task Task that was registered for peer signalling messages.
 */
void appPeerSigClientUnregister(Task client_task);

/*! \brief Register task to receive handset commands.

    \param handset_commands_task    Task to receive handset commands.
  */
void appPeerSigHandsetCommandsTaskRegister(Task handset_commands_task);

/*! \brief Disconnect peer signalling channel

    If peer signalling is not connected or is already in the process of
    disconnecting this function does nothing.

    \param[in] task The client task that PEER_SIG_CONNECT_CFM will be sent to.
*/
void appPeerSigDisconnect(Task task);

/*! \brief Register to receive PEER_SIG_MSG_CHANNEL_RX_IND messages for a channel.

    \param[in] task         Task to receive PEER_SIG_MSG_CHANNEL_RX_IND messages
    \param     channel_mask Mask of channels to receive messages for.
 */
void appPeerSigMsgChannelTaskRegister(Task task, peerSigMsgChannel channel_mask);

/*! \brief Stop receiving PEER_SIG_MSG_CHANNEL_RX_IND messages.

    \param[in] task         Task to cancel receiving PEER_SIG_MSG_CHANNEL_RX_IND messages
    \param[in] channel_mask Mask of channels to unregister.
*/
void appPeerSigMsgChannelTaskUnregister(Task task, peerSigMsgChannel channel_mask);

/*! \brief Request a transmission on a message channel.

    \param[in] task      Task to send confirmation message to.
    \param channel       Channel to transmit on.
    \param[in] msg       Payload of message.
    \param msg_size      Length of message in bytes.
 */
void appPeerSigMsgChannelTxRequest(Task task,
                                   peerSigMsgChannel channel,
                                   const uint8* msg, uint16 msg_size);

/*! \brief Register a task for a marshalled message channel(s).
    \param[in] task             Task to associate with the channel(s).
    \param[in] channel_mask     Bitmask of #peerSigMsgChannel to register for.
    \param[in] type_desc        Array of marshal type descriptors for messages.
    \param[in] num_type_desc    Number of entries in the type_desc array.
*/
void appPeerSigMarshalledMsgChannelTaskRegister(Task task, peerSigMsgChannel channel_mask,
                                                const marshal_type_descriptor_t * const * type_desc,
                                                size_t num_type_desc);

/*! \brief Unregister peerSigMsgChannel(s) for the a marshalled message channel.
    \param[in] task             Task associated with the channel(s).
    \param[in] channel_mask     Bitmask of #peerSigMsgChannel to unregister.
*/
void appPeerSigMarshalledMsgChannelTaskUnregister(Task task,
                                                  peerSigMsgChannel channel_mask);

/*! \brief Transmit a marshalled message channel message to the peer.
    \param[in] task      Task to send confirmation message to.
    \param[in] channel   Channel to transmit on.
    \param[in] msg       Pointer to the message to transmit.
    \param[in] type      Marshal type of the message.
*/
void appPeerSigMarshalledMsgChannelTx(Task task,
                                      peerSigMsgChannel channel,
                                      void* msg, marshal_type_t type);

/*! \brief Test if peer signalling is connected to a peer.

    \return TRUE if connected; FALSE otherwise.
*/
bool appPeerSigIsConnected(void);

/*! \brief Get the peer signalling link sink.
    \return The sink.
    The sink should only be used for passing to connection library functions
    that require a sink.
*/
Sink appPeerSigGetSink(void);

#endif /* PEER_SIGNALLING_H_ */
