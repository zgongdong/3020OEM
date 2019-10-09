/*!
\copyright  Copyright (c) 2008 - 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
*/

#include "sdp.h"
#include "earbud_config.h"
#include "bt_device.h"
#include "earbud_init.h"
#include "pairing.h"
#include "le_advertising_manager.h"
#include "le_scan_manager.h"

#include "connection_manager.h"
#include "connection_manager_data.h"
#include "connection_manager_list.h"
#include "connection_manager_config.h"
#include "connection_manager_notify.h"
#include "connection_manager_qos.h"
#include "connection_manager_msg.h"

#include <logging.h>

#include <message.h>
#include <panic.h>
#include <app/bluestack/dm_prim.h>

/*!< Connection manager task data */
conManagerTaskData  con_manager;

/******************************************************************************/
bool ConManagerAnyLinkConnected(void)
{
    return conManagerAnyLinkInState(cm_transport_all, ACL_CONNECTED);
}

/******************************************************************************/
static uint16 conManagerGetPageTimeout(const tp_bdaddr* tpaddr)
{
    uint32 page_timeout = appConfigDefaultPageTimeout();

    bool is_peer = appDeviceIsPeer(&tpaddr->taddr.addr);
    cm_connection_t *connection = ConManagerFindConnectionFromBdAddr(tpaddr);

    /* setup page timeout depending on the type of device the connection is for */
    if (is_peer)
    {
        page_timeout = appConfigEarbudPageTimeout();
    }
    else
    {
        if (conManagerGetConnectionState(connection) == ACL_DISCONNECTED_LINK_LOSS)
        {
            /* Increase page timeout as connection was previously disconnected due to link-loss */
            page_timeout *= appConfigHandsetLinkLossPageTimeoutMultiplier();
        }
    }

    if (page_timeout > 0xFFFF)
        page_timeout = 0xFFFF;

    DEBUG_LOGF("conManagerGetPageTimeout, link-loss connection, increasing page timeout to %u ms", page_timeout * 625UL / 1000UL);

    return (uint16)page_timeout;
}

/******************************************************************************/
static bool conManagerIsConnectingBle(void)
{
    if(conManagerAnyLinkInState(cm_transport_ble, ACL_CONNECTING_PENDING_PAUSE))
    {
        return TRUE;
    }
    
    if(conManagerAnyLinkInState(cm_transport_ble, ACL_CONNECTING))
    {
        return TRUE;
    }
    
    return FALSE;
}

/******************************************************************************/
static bool conManagerPauseLeScan(cm_connection_t* connection)
{
     if(!con_manager.is_le_scan_paused)
     {
         ConManagerDebugConnection(connection);
         DEBUG_LOG("conManagerPauseLeScan");

         LeScanManager_Pause(&con_manager.task);
         ConManagerSetConnectionState(connection, ACL_CONNECTING_PENDING_PAUSE);
         return TRUE;
     }
     return FALSE;
}

/******************************************************************************/
static void conManagerResumeLeScanIfPaused(void)
{
   LeScanManager_Resume(&con_manager.task);
   con_manager.is_le_scan_paused = FALSE;
}

/******************************************************************************/
static bool conManagerPrepareForConnection(cm_connection_t* connection)
{
    const tp_bdaddr* tpaddr = ConManagerGetConnectionTpAddr(connection);

    PanicNull((void *)tpaddr);

    if(tpaddr->transport == TRANSPORT_BLE_ACL)
    {
        if(conManagerPauseLeScan(connection))
        {
            return FALSE;
        }
    }
    else
    {
        conManagerSendWritePageTimeout(conManagerGetPageTimeout(tpaddr));
    }

    return TRUE;
}

/******************************************************************************/
static void conManagerPrepareForConnectionComplete(void)
{
    cm_list_iterator_t iterator;
    bool connecting = FALSE;
    cm_connection_t* connection = ConManagerListHeadConnection(&iterator);
    
    DEBUG_LOG("conManagerPrepareForConnectionComplete");
    
    while(connection)
    {
        const tp_bdaddr* tpaddr = ConManagerGetConnectionTpAddr(connection);
        cm_connection_state_t state = conManagerGetConnectionState(connection);
        
        ConManagerDebugConnection(connection);
        
        switch(state)
        {
            case ACL_CONNECTING_PENDING_PAUSE:
                DEBUG_LOG("conManagerPrepareForConnectionComplete Continue Connection");
                conManagerSendOpenTpAclRequest(tpaddr);
                ConManagerSetConnectionState(connection, ACL_CONNECTING);
                connecting = TRUE;
            break;
            
            default:
            break;
        }
        
        connection = ConManagerListNextConnection(&iterator);
    }
    
    if(!connecting)
    {
        conManagerResumeLeScanIfPaused();
    }
}

/******************************************************************************/
static uint16 *ConManagerCreateAclImpl(const tp_bdaddr* tpaddr)
{
    /* Attempt to find existing connection */
    cm_connection_t *connection = ConManagerFindConnectionFromBdAddr(tpaddr);

    /* Reset connection for re-use if in link loss state */
    if(conManagerGetConnectionState(connection) == ACL_DISCONNECTED_LINK_LOSS)
    {
        ConManagerSetConnectionState(connection, ACL_DISCONNECTED);
        connection = NULL;
    }

    if(!connection)
    {
        /* Create new connection */
        connection = ConManagerAddConnection(tpaddr, ACL_CONNECTING, TRUE);
        
        if(conManagerPrepareForConnection(connection))
        {
            conManagerSendOpenTpAclRequest(tpaddr);
        }
    }

    conManagerAddConnectionUser(connection);

    DEBUG_LOGF("ConManagerCreateAclImpl");
    ConManagerDebugConnection(connection);

    /* Return pointer to lock, will always be set */
    return conManagerGetConnectionLock(connection);
}

/******************************************************************************/
uint16 *ConManagerCreateAcl(const bdaddr *addr)
{
    tp_bdaddr tpaddr;
    BdaddrTpFromBredrBdaddr(&tpaddr, addr);
    return ConManagerCreateAclImpl(&tpaddr);
}

/******************************************************************************/
static void conManagerReleaseAclImpl(const tp_bdaddr* tpaddr)
{
    cm_connection_t *connection = ConManagerFindConnectionFromBdAddr(tpaddr);
    if (connection)
    {
        conManagerRemoveConnectionUser(connection);

        DEBUG_LOGF("conManagerReleaseAclImpl");
        ConManagerDebugConnection(connection);

        if (!conManagerConnectionIsInUse(connection))
        {
            if(conManagerGetConnectionState(connection) == ACL_CONNECTING_PENDING_PAUSE)
            {
                /* Remove the connection, everything will get tidied up on 
                   receiving the pause confirmation */
                conManagerRemoveConnection(connection);
            }
            else
            {
                /* Depending on address type conn_tpaddr may not be same as tpaddr */
                const tp_bdaddr* conn_tpaddr = ConManagerGetConnectionTpAddr(connection);
                conManagerSendCloseTpAclRequest(conn_tpaddr, FALSE);
            }
            conManagerNotifyObservers(tpaddr, cm_notify_message_disconnect_requested, hci_success);
        }
    }
}

/******************************************************************************/
void ConManagerReleaseAcl(const bdaddr *addr)
{
    /* Attempt to find existing connection */
    tp_bdaddr tpaddr;
    BdaddrTpFromBredrBdaddr(&tpaddr, addr);
    conManagerReleaseAclImpl(&tpaddr);
}

/*! \brief Handle service search attributes confirmation message

    Handles the results of a service search attributes request.  If the request
    was successful, the TWS+ version is extracted and stored in the device database.
    If there was no search data it is assumed the device is a standard device.
*/
static void appHandleClSdpServiceSearchAttributeCfm(const CL_SDP_SERVICE_SEARCH_ATTRIBUTE_CFM_T *cfm)
{
    DEBUG_LOGF("appHandleClSdpServiceSearchAttributeCfm, status %d", cfm->status);

    if (cfm->status == sdp_response_success)
    {
        /* Received response, so extract TWS version from attribute */
        uint16 tws_version = DEVICE_TWS_STANDARD;
        appSdpFindTwsVersion(cfm->attributes, cfm->attributes + cfm->size_attributes, &tws_version);
        appDeviceSetTwsVersion(&cfm->bd_addr, tws_version);

        DEBUG_LOGF("appHandleClSdpServiceSearchAttributeCfm, TWS+ device %x,%x,%lx, version %d",
                     cfm->bd_addr.nap, cfm->bd_addr.uap, cfm->bd_addr.lap, tws_version);
    }
    else if (cfm->status == sdp_no_response_data)
    {
        DEBUG_LOGF("appHandleClSdpServiceSearchAttributeCfm, standard device %x,%x,%lx",
                     cfm->bd_addr.nap, cfm->bd_addr.uap, cfm->bd_addr.lap);

        /* No response data, so handset doesn't have UUID and/or version attribute, therefore
           treat as standard handset */
        appDeviceSetTwsVersion(&cfm->bd_addr, DEVICE_TWS_STANDARD);
    }

    /* Attempt to find existing connection */
    tp_bdaddr tpaddr;
    BdaddrTpFromBredrBdaddr(&tpaddr, &cfm->bd_addr);
    cm_connection_t *connection = ConManagerFindConnectionFromBdAddr(&tpaddr);
    if (connection)
    {
        /* Check connection in in ACL_CONNECTED_SDP_SEARCH state */
        if (conManagerGetConnectionState(connection) == ACL_CONNECTED_SDP_SEARCH)
        {
            /* Indicate to clients the connection is up */
            conManagerNotifyObservers(&tpaddr, 
                                      connection->bitfields.local
                                            ? cm_notify_message_connected_outgoing
                                            : cm_notify_message_connected_incoming,
                                      hci_success);
            conManagerSendInternalMsgUpdateQos(connection);

            /* Move to ACL_CONNECTED state */
            ConManagerSetConnectionState(connection, ACL_CONNECTED);
        }
    }
}

/******************************************************************************/
static bool conManagerFindHandsetTwsVersion(tp_bdaddr* tpaddr)
{
    /* Check if this is a BREDR BDADDR for handset */
    if ((tpaddr->transport == TRANSPORT_BREDR_ACL) && appDeviceIsHandset(&tpaddr->taddr.addr))
    {
        DEBUG_LOG("conManagerFindHandsetTwsVersion, handset");

#ifndef DISABLE_TWS_PLUS
        /* Perform service search for TWS+ Source UUID and version attribute */
        ConManagerQueryHandsetTwsVersion(&tpaddr->taddr.addr);
        return TRUE;
#else
        /* TWS+ disabled so assume so handset is standard */
        appDeviceSetTwsVersion(&tpaddr->taddr.addr, DEVICE_TWS_STANDARD);
#endif
    }
    return FALSE;
}

static void conManagerCheckForForcedDisconnect(tp_bdaddr *tpaddr)
{
    if (con_manager.forced_disconnect)
    {
        DEBUG_LOG("conManagerCheckForForcedDisconnect x%06x now dropped", tpaddr->taddr.addr.lap);

        ConManagerTerminateAllAcls(con_manager.forced_disconnect);
    }
}


/*  ACL opened indication handler

    If a new ACL is opened successfully and it is to a handset (where the TWS+
    version needs to be checked everytime) a service attribute search is started.
*/
static void ConManagerHandleClDmAclOpenedIndication(const CL_DM_ACL_OPENED_IND_T *ind)
{
    tp_bdaddr tpaddr;
    cm_connection_t *connection;
    
    BdaddrTpFromTypedAndFlags(&tpaddr, &ind->bd_addr, ind->flags);
    
    connection = ConManagerFindConnectionFromBdAddr(&tpaddr);
    
    DEBUG_LOGF("ConManagerHandleClDmAclOpenedIndication, status %d, incoming %u, flags:%x",
               ind->status, (ind->flags & DM_ACL_FLAG_INCOMING) ? 1 : 0, ind->flags);
    ConManagerDebugAddress(&tpaddr);
    
    if(!connection && !ind->incoming)
        DEBUG_LOGF("ConManagerHandleClDmAclOpenedIndication, local connection not initiated from connection_manager");

    if (ind->status == hci_success)
    {
        const bool is_local = !!(~ind->flags & DM_ACL_FLAG_INCOMING);

        /* Update local ACL flag */
        ConManagerSetConnectionLocal(connection, is_local);

        appLinkPolicyHandleClDmAclOpendedIndication(ind);

        /* Add this ACL to list of connections */
        connection = ConManagerAddConnection(&tpaddr, ACL_CONNECTED, is_local);

        if(conManagerFindHandsetTwsVersion(&tpaddr))
        {
            ConManagerSetConnectionState(connection, ACL_CONNECTED_SDP_SEARCH);
        }
        else
        {
            conManagerNotifyObservers(&tpaddr, 
                                      is_local ? cm_notify_message_connected_outgoing
                                               : cm_notify_message_connected_incoming,
                                      hci_success);
            conManagerSendInternalMsgUpdateQos(connection);
        }
    }
    else
    {
        /* Remove this ACL from list of connections */
        conManagerRemoveConnection(connection);
        
        /* check if we were trying to force disconnect and got an ACL_OPENED_IND
         * with error code rather than ACL_CLOSED_IND */
        conManagerCheckForForcedDisconnect(&tpaddr);
    }
    
    if(!conManagerIsConnectingBle())
    {
        conManagerResumeLeScanIfPaused();
    }
}


/*! \brief ACL closed indication handler
*/
static void ConManagerHandleClDmAclClosedIndication(const CL_DM_ACL_CLOSED_IND_T *ind)
{
    tp_bdaddr tpaddr;
    
    BdaddrTpFromTypedAndFlags(&tpaddr, &ind->taddr, ind->flags);

    DEBUG_LOGF("ConManagerHandleClDmAclClosedIndication, status %d", ind->status);
    ConManagerDebugAddress(&tpaddr);

    /* Check if this BDADDR is for handset */
    if((ind->taddr.type == TYPED_BDADDR_PUBLIC) && appDeviceIsHandset(&ind->taddr.addr))
    {
        DEBUG_LOG("ConManagerHandleClDmAclClosedIndication, handset");
    }

    /* If connection timeout/link-loss move to special disconnected state, so that re-opening ACL
     * will use longer page timeout */
    cm_connection_t *connection = ConManagerFindConnectionFromBdAddr(&tpaddr);

    if (connection)
    {
        if (ind->status == hci_error_conn_timeout)
        {
            ConManagerSetConnectionState(connection, ACL_DISCONNECTED_LINK_LOSS);
            conManagerResetConnectionUsers(connection);
        }
        else
        {
            /* Remove this ACL from list of connections */
            conManagerRemoveConnection(connection);
        }

        conManagerCheckForForcedDisconnect(&tpaddr);
    }

    /* Indicate to client the connection to this connection has gone */
    conManagerNotifyObservers(&tpaddr, cm_notify_message_disconnected, ind->status);
}

/*! \brief Decide whether we allow a BR/EDR device to connect
    based on the device type
 */
static bool conManagerIsBredrAddressAuthorised(const bdaddr* bd_addr)
{
    /* Always allow connection from peer */
    if (appDeviceIsPeer(bd_addr))
    {
        DEBUG_LOG("conManagerIsBredrAddressAuthorised, ALLOW peer");
        return TRUE;
    }
    else if (appDeviceIsHandset(bd_addr))
    {
        DEBUG_LOG("conManagerIsBredrAddressAuthorised, device is handset");
        
        if(con_manager.handset_connect_allowed)
        {
            DEBUG_LOG("conManagerIsBredrAddressAuthorised, ALLOW handset");
            return TRUE;
        }
    }

    DEBUG_LOG("conManagerIsBredrAddressAuthorised, REJECT");
    return FALSE;
}

/*! \brief Decide whether we allow connection to a given transport 
    (BR/EDR or BLE)
 */
static bool conManagerIsTransportAuthorised(cm_transport_t transport)
{
    return (con_manager.connectable_transports & transport) == transport;
}

/*! \brief Decide whether we allow a device to connect a given protocol
 */
static bool conManagerIsConnectionAuthorised(const bdaddr* bd_addr, dm_protocol_id protocol)
{
    cm_transport_t transport_mask = cm_transport_bredr;
    
    if(protocol == protocol_le_l2cap)
        transport_mask = cm_transport_ble;

    if(conManagerIsTransportAuthorised(transport_mask))
    {
        if(transport_mask == cm_transport_bredr)
        {
            return conManagerIsBredrAddressAuthorised(bd_addr);
        }
        else
        {
            DEBUG_LOG("conManagerIsConnectionAuthorised, ALLOW BLE");
            return TRUE;
        }
    }
    
    return FALSE;
}

/*! \brief Handle authentication.
 */
static void ConManagerHandleClSmAuthoriseIndication(const CL_SM_AUTHORISE_IND_T *ind)
{
    bool authorise;

    DEBUG_LOGF("ConManagerHandleClSmAuthoriseIndication, protocol %d, channel %d, incoming %d",
                 ind->protocol_id, ind->channel, ind->incoming);

    authorise = conManagerIsConnectionAuthorised(&ind->bd_addr, ind->protocol_id);

    ConnectionSmAuthoriseResponse(&ind->bd_addr, ind->protocol_id, ind->channel, ind->incoming, authorise);
}

/******************************************************************************/
bool ConManagerHandleConnectionLibraryMessages(MessageId id,Message message, bool already_handled)
{
    switch (id)
    {
        case CL_SM_AUTHORISE_IND:
            if (!already_handled)
            {
                ConManagerHandleClSmAuthoriseIndication((CL_SM_AUTHORISE_IND_T *)message);
            }
            return TRUE;

        case CL_DM_ACL_OPENED_IND:
            ConManagerHandleClDmAclOpenedIndication((CL_DM_ACL_OPENED_IND_T *)message);
            return TRUE;

        case CL_DM_ACL_CLOSED_IND:
            ConManagerHandleClDmAclClosedIndication((CL_DM_ACL_CLOSED_IND_T *)message);
            return TRUE;

        case CL_DM_BLE_ACCEPT_CONNECTION_PAR_UPDATE_IND:
        {
            CL_DM_BLE_ACCEPT_CONNECTION_PAR_UPDATE_IND_T *ind = (CL_DM_BLE_ACCEPT_CONNECTION_PAR_UPDATE_IND_T *)message;

            ConnectionDmBleAcceptConnectionParUpdateResponse(TRUE, &ind->taddr,
                                                             ind->id,
                                                             ind->conn_interval_min, ind->conn_interval_max,
                                                             ind->conn_latency,
                                                             ind->supervision_timeout);
            return TRUE;
        }

        default:
            return already_handled;
    }
}


/******************************************************************************/
void ConManagerQueryHandsetTwsVersion(const bdaddr *bd_addr)
{
    ConnectionSdpServiceSearchAttributeRequest(&con_manager.task, bd_addr, 0x32,
                                               appSdpGetTwsSourceServiceSearchRequestSize(), appSdpGetTwsSourceServiceSearchRequest(),
                                               appSdpGetTwsSourceAttributeSearchRequestSize(), appSdpGetTwsSourceAttributeSearchRequest());
}

/******************************************************************************/
static void conManagerHandleScanManagerPauseCfm(void)
{
    con_manager.is_le_scan_paused = TRUE;
    conManagerPrepareForConnectionComplete();
}

/*! \brief Connection manager message handler.
 */
static void ConManagerHandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch (id)
    {
        case CL_SDP_SERVICE_SEARCH_ATTRIBUTE_CFM:
            appHandleClSdpServiceSearchAttributeCfm((CL_SDP_SERVICE_SEARCH_ATTRIBUTE_CFM_T *)message);
            break;
        
        case LE_SCAN_MANAGER_PAUSE_CFM:
            conManagerHandleScanManagerPauseCfm();
            break;
        
        case PAIRING_PAIR_CFM:
        default:
            break;
    }
}

/******************************************************************************/
bool ConManagerInit(Task init_task)
{
    DEBUG_LOG("ConManagerInit");
    memset(&con_manager, 0, sizeof(conManagerTaskData));

    ConManagerConnectionInit();
    conManagerNotifyInit();
    ConnectionManagerQosInit();

    /* Set up task handler */
    con_manager.task.handler = ConManagerHandleMessage;

    /*Set Pause Status as FALSE in init*/
    con_manager.is_le_scan_paused = FALSE;

    /* Default to allow BR/EDR connection until told otherwise */
    ConManagerAllowConnection(cm_transport_bredr, TRUE);

    /* setup role switch policy */
    ConManagerSetupRoleSwitchPolicy();
    UNUSED(init_task);
    return TRUE;
}

/******************************************************************************/
bool ConManagerIsConnected(const bdaddr *addr)
{
    tp_bdaddr tpaddr;
    BdaddrTpFromBredrBdaddr(&tpaddr, addr);
    const cm_connection_t *connection = ConManagerFindConnectionFromBdAddr(&tpaddr);
    return (conManagerGetConnectionState(connection) == ACL_CONNECTED);
}

/******************************************************************************/
bool ConManagerIsTpConnected(const tp_bdaddr *tpaddr)
{
    const cm_connection_t *connection = ConManagerFindConnectionFromBdAddr(tpaddr);
    return (conManagerGetConnectionState(connection) == ACL_CONNECTED);
}

/******************************************************************************/
bool ConManagerIsAclLocal(const bdaddr *addr)
{
    tp_bdaddr tpaddr;
    BdaddrTpFromBredrBdaddr(&tpaddr, addr);
    const cm_connection_t *connection = ConManagerFindConnectionFromBdAddr(&tpaddr);
    return conManagerConnectionIsLocallyInitiated(connection);
}

bool ConManagerIsTpAclLocal(const tp_bdaddr *tpaddr)
{
    const cm_connection_t *connection = ConManagerFindConnectionFromBdAddr(tpaddr);
    return conManagerConnectionIsLocallyInitiated(connection);
}

/******************************************************************************/
void ConManagerSetLpState(const bdaddr *addr, const lpPerConnectionState *lp_state)
{
    tp_bdaddr tpaddr;
    BdaddrTpFromBredrBdaddr(&tpaddr, addr);
    cm_connection_t *connection = ConManagerFindConnectionFromBdAddr(&tpaddr);
    conManagerSetLpState(connection, *lp_state);
}

/******************************************************************************/
void ConManagerGetLpState(const bdaddr *addr, lpPerConnectionState *lp_state)
{
    tp_bdaddr tpaddr;
    BdaddrTpFromBredrBdaddr(&tpaddr, addr);
    cm_connection_t *connection = ConManagerFindConnectionFromBdAddr(&tpaddr);
    conManagerGetLpState(connection, lp_state);
}

/******************************************************************************/
void ConManagerAllowHandsetConnect(bool allowed)
{
    con_manager.handset_connect_allowed = allowed;

    if(con_manager.handset_connect_allowed)
    {
        /* Indicate to observer client that handset connection is allowed */
        conManagerNotifyAllowedConnectionsObservers(cm_handset_allowed);
    }
    else
    {
        /* Indicate to observer client that handset connection is not allowed */
        conManagerNotifyAllowedConnectionsObservers(cm_handset_disallowed);
        
    }
}

/******************************************************************************/
void ConManagerAllowConnection(cm_transport_t transport_mask, bool enable)
{
    if(enable)
        con_manager.connectable_transports |= transport_mask;
    else
        con_manager.connectable_transports &= ~transport_mask;
    
    if(transport_mask == cm_transport_ble)
        LeAdvertisingManager_EnableConnectableAdvertising(&con_manager.task, enable);
}

/******************************************************************************/
bool ConManagerIsConnectionAllowed(cm_transport_t transport_mask)
{
    return conManagerIsTransportAuthorised(transport_mask);
}

/******************************************************************************/
uint16 *ConManagerCreateTpAcl(const tp_bdaddr *tpaddr)
{
    return ConManagerCreateAclImpl(tpaddr);
}

/******************************************************************************/
void ConManagerReleaseTpAcl(const tp_bdaddr *tpaddr)
{
    conManagerReleaseAclImpl(tpaddr);
}

/******************************************************************************/
bool ConManagerAnyTpLinkConnected(cm_transport_t transport_mask)
{
    return conManagerAnyLinkInState(transport_mask, ACL_CONNECTED);
}

/******************************************************************************/
void ConManagerTerminateAllAcls(Task requester)
{
    DEBUG_LOG("ConManagerTerminateAllAcls");

    PanicFalse(!con_manager.forced_disconnect || (con_manager.forced_disconnect  == requester));

    if (!ConManagerFindFirstActiveLink())
    {
        con_manager.forced_disconnect = NULL;
        MessageSend(requester, CON_MANAGER_CLOSE_ALL_CFM, NULL);
        return;
    }

    /* Address is ignored, but can't pass a NULL pointer */
    bdaddr addr = {0};

    con_manager.forced_disconnect = requester;
    ConnectionDmAclDetach(&addr, hci_error_unspecified, TRUE);
}

