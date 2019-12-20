/*!
\copyright  Copyright (c) 2008 - 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       connection_manager.h
\brief      Header file for Connection Manager
*/

#ifndef __AV_HEADSET_CON_MANAGER_H
#define __AV_HEADSET_CON_MANAGER_H

#include "domain_message.h"


#include <message.h>
#include <connection.h>
#include <bdaddr.h>
#include <task_list.h>

#include "domain_message.h"
#include "link_policy.h"

#include <marshal.h>

#define TransportToCmTransport(transport) (1 << transport)

/*! \brief Transports for which connections are managed */
typedef enum
{
    /*! No transport */
    cm_transport_none   = 0,
    /*! BR/EDR transport */
    cm_transport_bredr  = TransportToCmTransport(TRANSPORT_BREDR_ACL),
    /*! BLE transport */
    cm_transport_ble    = TransportToCmTransport(TRANSPORT_BLE_ACL),
    /*! All transports */
    cm_transport_all = cm_transport_bredr | cm_transport_ble
} cm_transport_t;

/*! \brief QoS settings 

    Values of settings in this enum are important. Where two
    conflicting QoS settings are requested, the higher value
    setting will override the lower value setting.
*/
typedef enum
{
    /*! Invalid QoS */
    cm_qos_invalid              = 0,
    /*! Optimise for low power */
    cm_qos_low_power            = 1,
    /*! Optimise for low latency */
    cm_qos_low_latency          = 2,
    /*! Optimise for audio data */
    cm_qos_audio                = 3,
    /*! Optimise for short data exchange */
    cm_qos_short_data_exchange  = 4,
    /*! Always accept remote device's settings, use 
        cm_qos_low_power for outgoing connections */
    cm_qos_passive              = 5,
    /*! Max QoS (always last) */
    cm_qos_max
} cm_qos_t;

/*! \brief Message IDs for connection manager messages to other tasks. */
enum    av_headset_conn_manager_messages
{
    /*! Message ID for a \ref CON_MANAGER_CONNECTION_IND_T message sent when
        the state of an ACL changes */
    CON_MANAGER_CONNECTION_IND = CON_MANAGER_MESSAGE_BASE,
    /*! Message ID for a \ref CON_MANAGER_TP_CONNECT_IND_T message sent when
        an ACL is connected */
    CON_MANAGER_TP_CONNECT_IND,
    /*! Message ID for a \ref CON_MANAGER_TP_DISCONNECT_IND_T message sent when
        an ACL is disconnected */
    CON_MANAGER_TP_DISCONNECT_IND,
    /*! Message ID for a \ref CON_MANAGER_TP_DISCONNECT_REQUESTED_IND_T message sent when
        an ACL disconnect has been requested */
    CON_MANAGER_TP_DISCONNECT_REQUESTED_IND,
    /*! Message ID sent when all ACLs have been disconnected. This message is 
        sent even if there were no ACLs to disconnect */
    CON_MANAGER_CLOSE_ALL_CFM,
    /*! Message ID sent when handset connections are allowed */
    CON_MANAGER_HANDSET_CONNECT_ALLOW_IND,
    /*! Message ID sent when handset connections are not allowed */
    CON_MANAGER_HANDSET_CONNECT_DISALLOW_IND,
    /*! Message ID sent when all BLE connections have been disconnected. */
    CON_MANAGER_DISCONNECT_ALL_LE_CONNECTIONS_CFM,
};

/*! Definition of message sent to clients to indicate connection status. */
typedef struct
{
    /*! BT address of (dis)connected device. */
    bdaddr bd_addr;
    /*! Connection status of the device, TRUE connected, FALSE disconnected. */
    bool connected;
    /*! Whether the connection is to a BLE device */
    bool ble;
    /*! Reason given for disconnection. For a connection, this will always be hci_success. */
    hci_status reason;
} CON_MANAGER_CONNECTION_IND_T;
extern const marshal_type_descriptor_t marshal_type_descriptor_CON_MANAGER_CONNECTION_IND_T;

/*! Definition of message sent to clients to indicate connection. */
typedef struct
{
    /*! Transport Bluetooth Address of connected device. */
    tp_bdaddr tpaddr;
    /*! Whether this connection is incoming */
    bool      incoming;
} CON_MANAGER_TP_CONNECT_IND_T;

/*! Definition of message sent to clients to indicate disconnection. */
typedef struct
{
    /*! Transport Bluetooth Address of disconnected device. */
    tp_bdaddr tpaddr;
    /*! Reason given for disconnection. */
    hci_status reason;
} CON_MANAGER_TP_DISCONNECT_IND_T;

/*! Definition of message sent to clients to indicate disconnect requested */
typedef struct
{
    /*! Transport Bluetooth Address of disconnect requested device. */
    tp_bdaddr tpaddr;
} CON_MANAGER_TP_DISCONNECT_REQUESTED_IND_T;

/*! \brief Initialise the connection manager module.
 */
bool ConManagerInit(Task init_task);


/*! \brief Request to create ACL to device

    Called to request an ACL to the specified BR/EDR device.  If the ACL already 
    exists then this function does nothing apart from increment usage count on 
    the ACL.

    If the ACL doens't exist then this function will request Bluestack to open
    an ACL.

    \note This function should not be called to create a BLE ACL. To create a BLE
    ACL ConManagerCreateTpAcl should be used.

    \param[in] addr Pointer to a BT address.

    \return uint16 Pointer to lock that will be cleared when ACL is available, or paging failed.
*/
uint16 *ConManagerCreateAcl(const bdaddr *addr);


/*! \brief Release ownership on ACL

    Called to release ownership on an ACL, if that ACL has no other users the
    ACL will be 'closed'.  Bluestack will only actually close the ACL if there
    are no L2CAP connections, so it's safe to call this function once a profile
    connection has been setup, the ACL will be closed automatically once the
    profiles are closed.

    \param[in] addr Pointer to a BT address.
*/
void ConManagerReleaseAcl(const bdaddr *addr);


/*! Handler for connection library messages not sent directly

    This function is called to handle any connection library messages sent to
    the application that the connection manager is interested in. If a message 
    is processed then the function returns TRUE.

    \note Some connection library messages can be sent directly as the 
        request is able to specify a destination for the response.

    \param  id              Identifier of the connection library message 
    \param  message         The message content (if any)
    \param  already_handled Indication whether this message has been processed by
                            another module. The handler may choose to ignore certain
                            messages if they have already been handled.

    \returns TRUE if the message has been processed, otherwise returns the
        value in already_handled
 */
bool ConManagerHandleConnectionLibraryMessages(MessageId id,Message message, bool already_handled);


/*! \brief Perform SDP query of TWS version of handset

    \param[in] bd_addr Pointer to a BT address.
 */
void ConManagerQueryHandsetTwsVersion(const bdaddr *bd_addr);

/*! \brief Register a client task to receive notifications of connections.

    \param[in] client_task Task which will receive CON_MANAGER_CONNECTION_IND message
 */
void ConManagerRegisterConnectionsClient(Task client_task);

/*! \brief Unregister a client task to stop receiving notifications of connections.

    \param[in] client_task Task to unregister.
 */
void ConManagerUnregisterConnectionsClient(Task client_task);

/*! \brief Query if a device is currently connected.

    \param[in] addr Pointer to a BT address.

    \return bool TRUE device is connected, FALSE device is not connected.
 */
bool ConManagerIsConnected(const bdaddr *addr);

/*! \brief Query if a device is currently connected.

    \param[in] tpaddr Pointer to a BT address.

    \return bool TRUE device is connected, FALSE device is not connected.
 */
bool ConManagerIsTpConnected(const tp_bdaddr *tpaddr);

/*! \brief Query if a ACL to device was locally initiated.

    \param[in] addr Pointer to a BT address.

    \return bool TRUE ACL to device was locally initiated, FALSE is remotely initiated.
 */
bool ConManagerIsAclLocal(const bdaddr *addr);

/*! \brief Query if a ACL to device was locally initiated.

    \param[in] tpaddr Pointer to a BT address.

    \return bool TRUE ACL to device was locally initiated, FALSE is remotely initiated.
 */
bool ConManagerIsTpAclLocal(const tp_bdaddr *tpaddr);

/*! \brief Set the link policy per-connection state.

    \param[in] addr     Pointer to a BT address.
    \param[in] lp_state  Address of state to store.
*/
void ConManagerSetLpState(const bdaddr *addr, const lpPerConnectionState *lp_state);

/*! \brief Get the link policy per-connection state.

    \param[in]  addr    Pointer to a BT address.
    \param[out] lp_state Address of state to update with retrieved state.
*/
void ConManagerGetLpState(const bdaddr *addr, lpPerConnectionState *lp_state);

/*! \brief Get the power mode of the connection

    \param[in]  tpaddr    Pointer to a BT address.
    \param[in]  mode      Pointer variable to receive power mode 
    \return bool returns TRUE if connection is valid, else FALSE
*/
bool ConManagerGetPowerMode(const tp_bdaddr *tpaddr,lp_power_mode* mode);

/*! \brief Get the sniff interval of the connection(in number of slots)

    \param[in]  tpaddr          Pointer to a BT address.
    \param[in]  sniff_interval  Pointer variable to receive sniff interval
    \return bool returns TRUE if connection is valid, else FALSE
*/
bool ConManagerGetSniffInterval(const tp_bdaddr *tpaddr, uint16* sniff_interval);

/*! \brief Manually close the ACL to a device.

    \param[in] addr  Pointer to a BT address.
    \param[in] force TRUE aggressively close regardless of L2CAP connectivity.
*/
void ConManagerSendCloseAclRequest(const bdaddr *addr, bool force);


/*! \brief Check if there are any CONNECTED links.

    \return bool TRUE if any link managed by Con Manager is active,
            FALSE if no links

    \note This function ignores links that are connecting or discovering
        services.
 */
bool ConManagerAnyLinkConnected(void);


/*! \brief Control if handset connections are allowed.

    \param[in] allowed TRUE to allow connections, FALSE to reject.
 */
void ConManagerAllowHandsetConnect(bool allowed);

/*! \brief Control if connections to a transport are allowed.
           BR/EDR enabled by default, BLE disabled by default

    \param[in] transport_mask The transport(s) for which to allow connections
    \param[in] enable TRUE to allow connection on the transports set in
               transport_mask, FALSE to disable connection on the transports 
               set in transport_mask. Transports not set in transport_mask are
               unaffected.
 */
void ConManagerAllowConnection(cm_transport_t transport_mask, bool enable);

/*! \brief Check if connections to a transport are allowed.

    \param[in] transport_mask The transport(s) to check
    \return bool TRUE if the all transports set in the transport_mask
            are enabled, otherwise FALSE
 */
bool ConManagerIsConnectionAllowed(cm_transport_t transport_mask);

/*! \brief Create an ACL. As ConManagerCreateAcl but supports
           creation of BLE or BR/EDR ACL. If used to establish
           a BLE ACL this function will pause any le_scan_manager 
           scans in progress until the ACL has either been 
           established or has failed.

    \param[in] tpaddr The transport, type and address of the 
               target device
    \return uint16 Pointer to lock that will be cleared when ACL 
            is available, or paging failed.
 */
uint16 *ConManagerCreateTpAcl(const tp_bdaddr *tpaddr);

/*! \brief Release an ACL. As ConManagerReleaseAcl but supports
           release of BLE or BR/EDR ACL

    \param[in] tpaddr The transport, type and address of the 
               target device
 */
void ConManagerReleaseTpAcl(const tp_bdaddr *tpaddr);

/*! \brief Register a client task to receive notifications of 
    connections and disconnections
    
    Clients will receive:
    - CON_MANAGER_TP_CONNECT_IND
    - CON_MANAGER_TP_DISCONNECT_IND

    \param[in] transport_mask The transport(s) to receive notifications for
    \param[in] client_task Task which will receive notifications
 */
void ConManagerRegisterTpConnectionsObserver(cm_transport_t transport_mask, Task client_task);

/*! \brief Unregister a client task to stop receiving notifications of connections and disconnections.
 
    Client will no longer receive:
    - CON_MANAGER_TP_CONNECT_IND
    - CON_MANAGER_TP_DISCONNECT_IND

    \param[in] transport_mask The transport(s) to unregister from notifications.
    \param[in] client_task Task to unregister from notifications.
*/
void ConManagerUnregisterTpConnectionsObserver(cm_transport_t transport_mask, Task client_task);

/*! \brief Register a client task to receive notifications of 
    connections allowed or dis-allowed
    
    Clients will receive:
    - CON_MANAGER_HANDSET_CONNECT_ALLOW_IND
    - CON_MANAGER_HANDSET_CONNECT_DISALLOW_IND

    \param[in] client_task Task which will receive notifications
 */

void ConManagerRegisterAllowedConnectionsObserver(Task client_task);

/*! \brief Unregister a client task to stop receiving notifications of 
    connections allowed or dis-allowed
    
    Clients will receive:
    - CON_MANAGER_HANDSET_CONNECT_ALLOW_IND
    - CON_MANAGER_HANDSET_CONNECT_DISALLOW_IND

    \param[in] client_task Task to unregister from notifications
 */
void ConManagerUnregisterAllowedConnectionsObserver(Task client_task);


/*! \brief Check if there are any CONNECTED links on a given transport

    \return bool TRUE if any link managed by Con Manager is active
            on the specified transport(s), FALSE if no links

    \note This function ignores links that are connecting or discovering
        services.
 */
bool ConManagerAnyTpLinkConnected(cm_transport_t transport_mask);

/*! \brief Request the QoS to use for new connections
    
           This function may be called multiple times. The QoS setting
           selected will be the highest priority requested. 
           
           The selected QoS setting will be used to select the parameters 
           for locally initiated connections. This QoS setting will be used
           to select parameters to request for remotely initiated 
           connections, although they may not be accepted by a remote central
           device.
    
    \param transport_mask Transport on which to use this QoS setting. 
    
    \param qos The QoS setting
    
    \note Only cm_transport_ble is currently supported
 */
void ConManagerRequestDefaultQos(cm_transport_t transport_mask, cm_qos_t qos);

/*! \brief Request to update the QoS to use for a specific device
    
           This function may be called multiple times. The QoS setting
           selected will be the highest priority requested. This will 
           override the default setting regardless of priority.
           
           This QoS setting will be used to select parameters to request, 
           although they may not be accepted by a remote central device.
    
    \param tpaddr The address of the remote device
    
    \param qos The QoS setting
    
    \note Only addresses with TRANSPORT_BLE_ACL are currently supported
 */
void ConManagerRequestDeviceQos(const tp_bdaddr *tpaddr, cm_qos_t qos);

/*! \brief Release the QoS to use for a specific device
    
           This function may be called multiple times. The QoS setting 
           to fall back to will either be the highest priority QoS setting
           requested for this device, or the default if there are no outstanding
           requests
           
           This QoS setting will be used to select parameters to request, 
           although they may not be accepted by a remote central device.
           
           Calling this function for a QoS setting which has not previously 
           been requested will result in a Panic.
    
    \param tpaddr The address of the remote device
    
    \param qos The QoS setting to release
    
    \note Only addresses with TRANSPORT_BLE_ACL are currently supported
 */
void ConManagerReleaseDeviceQos(const tp_bdaddr *tpaddr, cm_qos_t qos);

/*! \brief Set the maximum permitted QoS
    
           Any subsequent requests for a QoS above the maximum will result
           in parameters for the maximum QoS being requested. Any requests
           below or equal to the maximum QoS are unaffected.
           
           When the maximum QoS is changed the connection parameters for any 
           active connections will be updated to use whichever is smaller of 
           the maximum QoS or the requested QoS for the device. If no QoS has 
           been requested for the device then whichever is smaller of the 
           default QoS or the maximum QoS will be used.
    
    \param qos The maximum QoS setting permitted
    
    \note Connection parameter updates for a peripheral device may be rejected
          by the central device.
 */
void ConManagerSetMaxQos(cm_qos_t qos);


/*! \brief Forcibly close any connections with open ACLs

        This API is intended for use only by Topology. It should normally 
        only be called when there has been an attempt to close known 
        connections.

        Sends a CON_MANAGER_CLOSE_ALL_CFM response to the requester.
        CON_MANAGER_CONNECTION_IND messages may be sent to any registered
        observers

        \note The function does not protect against any new connections
        being created. This should be blocked by calls to 
        ConManagerAllowConnection().

        \param[in] requester task to be sent CON_MANAGER_CLOSE_ALL_CFM 
                message
 */
void ConManagerTerminateAllAcls(Task requester);

/*! \brief Close all BLE links.

    For all open BLE connections:
        * notify any clients that the link will be disconnected, such
          that they can clean up and release any L2CAP channels using
          the BLE ACL.
        * wait for DM_ACL_CLOSED_IND notification from Bluestack

    Once all LE links are disconnected, send CON_MANAGER_DISCONNECT_ALL_LE_CONNECTIONS_CFM
    to the requester task.
        
    \note This API will cleanly disconnect LE links, it *does not* forcibly
    disconnect BLE ACLs.

    \note The function does not protect against any new connections
    being created. This should be blocked by calls to 
    ConManagerAllowConnection().

    \param[in] requester task to be sent CON_MANAGER_DISCONNECT_ALL_LE_CONNECTIONS_CFM 
                message
*/    
void ConManagerDisconnectAllLeConnectionsRequest(Task requester);

#endif
