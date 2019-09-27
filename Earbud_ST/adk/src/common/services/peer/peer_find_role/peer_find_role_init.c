/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Initialisation functions for the Peer find role service (using LE)
*/

#include <panic.h>
#include <gatt_manager.h>

#include <task_list.h>
#include <logging.h>
#include <phy_state.h>
#include <charger_monitor.h>
#include <acceleration.h>
#include <battery_monitor.h>
#include <gatt_role_selection_client.h>
#include <gatt_role_selection_server.h>
#include <gatt_handler_db.h>
#include <connection_manager.h>
#include <hfp_profile.h>
#include <av.h>

#include "peer_find_role.h"
#include "peer_find_role_private.h"
#include "peer_find_role_scoring.h"
#include "peer_find_role_init.h"
#include "peer_find_role_config.h"
#include "telephony_messages.h"


peerFindRoleTaskData peer_find_role = {0};


void peer_find_role_activity_set(peerFindRoleActiveStatus_t flags_to_set)
{
    uint16 old_busy = PeerFindRoleGetBusy();
    uint16 new_busy = old_busy | flags_to_set;

    if (new_busy != old_busy)
    {
        PeerFindRoleSetBusy(new_busy);
        DEBUG_LOG("peer_find_role_busy_set. Set 0x%x. Now 0x%x", flags_to_set, new_busy);
    }
}


void peer_find_role_activity_clear(peerFindRoleActiveStatus_t flags_to_clear)
{
    uint16 old_busy = PeerFindRoleGetBusy();
    uint16 new_busy = old_busy & ~flags_to_clear;

    if (new_busy != old_busy)
    {
        PeerFindRoleSetBusy(new_busy);
        DEBUG_LOG("peer_find_role_busy_clear. Cleared 0x%x. Now 0x%x", flags_to_clear, new_busy);
    }
}


void peer_find_role_message_send_when_inactive(MessageId id, void *message)
{
    MessageSendConditionally(PeerFindRoleGetTask(), id, message, &PeerFindRoleGetBusy());
}


void peer_find_role_message_cancel_inactive(MessageId id)
{
    if (PeerFindRoleGetBusy())
    {
        if (MessageCancelAll(PeerFindRoleGetTask(), id))
        {
            DEBUG_LOG("peer_find_role_busy_message_cancel. Queued messages were cancelled.");
        }
    }
}


void peer_find_role_cancel_initial_timeout(void)
{
    MessageCancelAll(PeerFindRoleGetTask(), PEER_FIND_ROLE_INTERNAL_TIMEOUT_CONNECTION);
}


static void peer_find_role_notify_clients_immediately(peer_find_role_message_t role)
{
    peerFindRoleTaskData *pfr = PeerFindRoleGetTaskData();

    peer_find_role_cancel_initial_timeout();
    pfr->timeout_means_timeout = FALSE;

    TaskList_MessageSendId(PeerFindRoleGetTaskList(), role);
}


void peer_find_role_notify_clients_if_pending(void)
{
    peerFindRoleTaskData *pfr = PeerFindRoleGetTaskData();
    peer_find_role_message_t role = pfr->selected_role;

    pfr->selected_role = (peer_find_role_message_t)0;

    if (role)
    {
        peer_find_role_notify_clients_immediately(role);
    }
}


static bool peer_find_role_role_means_primary(peer_find_role_message_t role)
{
    switch (role)
    {
        case PEER_FIND_ROLE_ACTING_PRIMARY:
        case PEER_FIND_ROLE_PRIMARY:
            return TRUE;

        default:
            break;
    }

    return FALSE;
}


void peer_find_role_completed(peer_find_role_message_t role)
{
    peerFindRoleTaskData *pfr = PeerFindRoleGetTaskData();

    if (peer_find_role_role_means_primary(role))
    {
        DEBUG_LOG("peer_find_role_completed. Role is a primary one:%d", role);

        pfr->selected_role = (peer_find_role_message_t )0;
        peer_find_role_notify_clients_immediately(role);
    }
    else
    {
        DEBUG_LOG("peer_find_role_completed. Role is not a primary one:%d. Wait for links to go.", role);

        pfr->selected_role = role;
        pfr->timeout_means_timeout = FALSE;
    }

    GattRoleSelectionClientDestroy(&pfr->role_selection_client);

    switch (peer_find_role_get_state())
    {
        case PEER_FIND_ROLE_STATE_INITIALISED:
            peer_find_role_notify_clients_if_pending();
            break;

        case PEER_FIND_ROLE_STATE_SERVER:
            /* If we disconnect the GATT link at this point, then the client
                does not always receive a confirmation */
            peer_find_role_set_state(PEER_FIND_ROLE_STATE_AWAITING_DISCONNECT);
            break;

        default:
            peer_find_role_set_state(PEER_FIND_ROLE_STATE_INITIALISED);
            break;
    }
}


void peer_find_role_notify_timeout(void)
{
    peerFindRoleTaskData *pfr = PeerFindRoleGetTaskData();

    DEBUG_LOG("peer_find_role_notify_timeout. Now acting primary");

        /*! \todo We will use different states for fast/slow
            We only timeout once, clear the flag */
    pfr->role_timeout_ms = 0;
    peer_find_role_notify_clients_immediately(PEER_FIND_ROLE_ACTING_PRIMARY);
}


/*! Handle response to setting advertising data

    If successful start connectable advertising using the just configured
    advertising data.

    In case of failure, indicate that we are not advertising

    \param cfm Confirmation for setting advertising data

    \todo Is clearing activity enough to kick any error handling off ?
 */
static void peer_find_role_handle_advert_set_data_cfm(const APP_ADVMGR_ADVERT_SET_DATA_CFM_T *cfm)
{
    if (success != cfm->status)
    {
        DEBUG_LOG("peer_find_role_handle_advert_set_data_cfm. FAILED.");

        peer_find_role_activity_clear(PEER_FIND_ROLE_ACTIVE_ADVERTISING);
    }
    else
    {
        DEBUG_LOG("peer_find_role_handle_advert_set_data_cfm: Wait for remoteClient");

        GattManagerWaitForRemoteClient(PeerFindRoleGetTask(), NULL, gatt_connection_ble_slave_undirected);
    }
}


/*! handler GATT client connection

    Update our status to indicate that we are no longer advertising.
    If the connection was successful, and to the expected device,
    change state to #PEER_FIND_ROLE_STATE_CLIENT.

    In case of error fall back to looking for devices by changing to 
    state #PEER_FIND_ROLE_STATE_DISCOVER

    \param cfm GATT connection status
 */
static void peer_find_role_handle_client_connect_cfm(const GATT_MANAGER_REMOTE_CLIENT_CONNECT_CFM_T *cfm)
{
    peerFindRoleTaskData *pfr = PeerFindRoleGetTaskData();

    DEBUG_LOG("peer_find_role_handle_client_connect_cfm. sts:%d addr:%06x", cfm->status, cfm->taddr.addr.lap);

    peer_find_role_activity_clear(PEER_FIND_ROLE_ACTIVE_ADVERTISING);

    if (gatt_status_success == cfm->status)
    {
        pfr->gatt_cid = cfm->cid;
        peer_find_role_set_state(PEER_FIND_ROLE_STATE_CLIENT);
    }
    else
    {
        peer_find_role_set_state(PEER_FIND_ROLE_STATE_DISCOVER);
    }
}


/*! Handle GATT server connection

    Update our status to indicate that we are no longer connecting.
    If the connection was successful, and to the expected device,
    change state to #PEER_FIND_ROLE_STATE_SERVER.

    In case of error fall back to looking for devices by changing to 
    state #PEER_FIND_ROLE_STATE_DISCOVER

    \param cfm Server connect status
 */
static void peer_find_role_handle_server_connect_cfm(const GATT_MANAGER_REMOTE_SERVER_CONNECT_CFM_T *cfm)
{
    peerFindRoleTaskData *pfr = PeerFindRoleGetTaskData();

    DEBUG_LOG("peer_find_role_handle_server_connect_cfm. sts:%d", cfm->status);

    peer_find_role_activity_clear(PEER_FIND_ROLE_ACTIVE_CONNECTING);

    if (   gatt_status_success == cfm->status
        /*&& BdaddrIsSame(&pfr->peer_addr, &cfm->taddr.addr)*/)
    {
        pfr->gatt_cid = cfm->cid;
        peer_find_role_set_state(PEER_FIND_ROLE_STATE_SERVER);
    }
    else
    {
            /* We get a generic failure on timeout (at least).
               You might expect gatt_status_connection_timeout.
             */
        if (gatt_status_failure != cfm->status)
        {
            DEBUG_LOG("peer_find_role_handle_server_connect_cfm. Unexpected gatt status:%d", cfm->status);
            Panic();
        }

        peer_find_role_set_state(PEER_FIND_ROLE_STATE_DISCOVER);
    }
}


/*! Handle message giving status of service discovery

    Initialise our gatt client now we know the details of the peer devices 
    service.

    \param discovery Details of the remote service, including the handle range

    \todo Handler errors
 */
static void peer_find_role_handle_primary_service_discovery(const GATT_DISCOVER_PRIMARY_SERVICE_CFM_T* discovery)
{
    peerFindRoleTaskData *pfr = PeerFindRoleGetTaskData();

    DEBUG_LOG("peer_find_role_handle_primary_service_discovery. sts:%d more:%d",
                discovery->status, discovery->more_to_come);

    if (gatt_status_success == discovery->status)
    {
        GattRoleSelectionClientInit(&pfr->role_selection_client,
                                    PeerFindRoleGetTask(),
                                    discovery->cid, discovery->handle, discovery->end);
    }
}


/*! Handle message confirming initialisation of the GATT client

    Change state to #PEER_FIND_ROLE_STATE_AWAITING_ENCRYPTION, which will 
    block further activity until the link is encrypted.

    \param init Message confirming initialisation
 */
static void peer_find_role_handle_client_init(const GATT_ROLE_SELECTION_CLIENT_INIT_CFM_T *init)
{
    DEBUG_LOG("peer_find_role_handle_client_init. sts:%d", init->status);

    if (gatt_role_selection_client_status_success == init->status)
    {
        peer_find_role_set_state(PEER_FIND_ROLE_STATE_AWAITING_ENCRYPTION);
    }
    else
    {
        /*! \todo Handle init failure ! */
    }

}


/*! Handle command from our peer to change role

    Indicate completion of the PeerFindRole_FindRole() procedure by sending an appropriate
    message to our clients, either #PEER_FIND_ROLE_PRIMARY or
    PEER_FIND_ROLE_SECONDARY

    \param role Role message received from our peer
 */
static void peer_find_role_handle_change_role_ind(const GATT_ROLE_SELECTION_SERVER_CHANGE_ROLE_IND_T *role)
{
    DEBUG_LOG("peer_find_role_handle_change_role_ind. cmd:%d", role->command);

    peer_find_role_completed((role->command == GrssOpcodeBecomePrimary)
                                        ? PEER_FIND_ROLE_PRIMARY
                                        : PEER_FIND_ROLE_SECONDARY);
}


/*! Handle confirmation that our peer received its new state

    Complete the PeerFindRole_FindRole() procedure by sending an appropriate
    message to our clients, either #PEER_FIND_ROLE_PRIMARY or
    PEER_FIND_ROLE_SECONDARY

    \param cfm Confirmation message - effectively received from our peer

    \todo Error case !success needs to disconnect and retry. 
 */
static void peer_find_role_handle_command_cfm(const GATT_ROLE_SELECTION_CLIENT_COMMAND_CFM_T *cfm)
{
    peerFindRoleTaskData *pfr = PeerFindRoleGetTaskData();

    DEBUG_LOG("peer_find_role_handle_command_cfm. sts:%d. state:%d", 
              cfm->result, peer_find_role_get_state());

    if (   gatt_status_success == cfm->result
        && PEER_FIND_ROLE_STATE_AWAITING_CONFIRM == peer_find_role_get_state())
    {
        if (GrssOpcodeBecomeSecondary == pfr->remote_role)
        {
            peer_find_role_completed(PEER_FIND_ROLE_PRIMARY);
        }
        else
        {
            peer_find_role_completed(PEER_FIND_ROLE_SECONDARY);
        }
    }
}


/*! Send an internal message to update our score

    Messages used to avoid updating the score consecutively as the
    score may be passed to the peer.

    Any pending messages are cancelled before a new message is sent
 */
static void peer_find_role_request_score_update(void)
{
    MessageCancelAll(PeerFindRoleGetTask(), PEER_FIND_ROLE_INTERNAL_UPDATE_SCORE);
    MessageSend(PeerFindRoleGetTask(), PEER_FIND_ROLE_INTERNAL_UPDATE_SCORE, NULL);
}


/*! Handler for physical state messages

   Update our information about the physical state and update the score 
   used to later decide role

   \param phy Physical state indication
*/
static void peer_find_role_handle_phy_state(const PHY_STATE_CHANGED_IND_T *phy)
{
    peerFindRoleTaskData *pfr = PeerFindRoleGetTaskData();

    DEBUG_LOG("peer_find_role_handle_phy_state. New physical state:%d", phy->new_state);

    pfr->scoring_info.phy_state = phy->new_state;

    peer_find_role_request_score_update();
}


/*! Handler for charger status

    Update our information about the charge and update the score used to later decide role

    \param attached Is the charger currently attached ?
 */
static void peer_find_role_handle_charger_state(bool attached)
{
    peerFindRoleTaskData *pfr = PeerFindRoleGetTaskData();

    DEBUG_LOG("peer_find_role_handle_charger_state. Attached:%d", attached);

    pfr->scoring_info.charger_present = attached;

    peer_find_role_request_score_update();
}


/*! Handler for accelerometer status

    Update our latest moving state and update the score used to later decide role

    \param moving Did the update indicate we are moving?
 */
static void peer_find_role_handle_accelerometer_state(bool moving)
{
    peerFindRoleTaskData *pfr = PeerFindRoleGetTaskData();

    DEBUG_LOG("peer_find_role_handle_accelerometer_state. Moving:%d", moving);

    pfr->scoring_info.accelerometer_moving = moving;

    peer_find_role_request_score_update();
}


/*! Handler for battery level updates

    Store our latest battery level and update the score used to later decide role 

    \param battery Battery reading (contains a percentage)
 */
static void peer_find_role_handle_battery_level(const MESSAGE_BATTERY_LEVEL_UPDATE_PERCENT_T *battery)
{
    peerFindRoleTaskData *pfr = PeerFindRoleGetTaskData();

    DEBUG_LOG("peer_find_role_handle_battery_level. Level:%d", battery->percent);

    pfr->scoring_info.battery_level_percent = battery->percent;

    peer_find_role_request_score_update();
}


/*! Handler for the figure of merit from our peer

    This is received when we are ready to make a decision on role. The
    value is processed in relation to ours and we request our peer
    to change to the best state.

    \param ind The merit message from our peer
 */
static void peer_find_role_handle_figure_of_merit(const GATT_ROLE_SELECTION_CLIENT_FIGURE_OF_MERIT_IND_T *ind)
{
    peerFindRoleTaskData *pfr = PeerFindRoleGetTaskData();

    peer_find_role_scoring_t *scoring = PeerFindRoleGetScoring();
    grss_figure_of_merit_t theirs = ind->figure_of_merit;
    grss_figure_of_merit_t mine = scoring->last_local_score;
    pfr->remote_role = GrssOpcodeBecomeSecondary;

    DEBUG_LOG("peer_find_role_handle_figure_of_merit. Mine:%x Theirs:%x", mine, theirs);

    if (theirs > mine)
    {
        pfr->remote_role = GrssOpcodeBecomePrimary;
    }

    GattRoleSelectionClientChangePeerRole(&pfr->role_selection_client,
                                                pfr->remote_role);
    peer_find_role_set_state(PEER_FIND_ROLE_STATE_AWAITING_CONFIRM);
}


/*! Handler for the internal message to connect to the peer

    This message is sent conditionally when advertising and scan
    activities have terminated. Changing to the state
    #PEER_FIND_ROLE_STATE_CONNECTING_TO_DISCOVERED starts the
    connection process.
 */
static void peer_find_role_handle_connect_to_peer(void)
{
    peer_find_role_set_state(PEER_FIND_ROLE_STATE_CONNECTING_TO_DISCOVERED);
}


/*! Handler for the internal message to enable scanning

    If we are not already scanning, we check the current state
    and if necessary transition to a state that will enable scanning.
 */
static void peer_find_role_handle_enable_scanning(void)
{
    peerFindRoleTaskData *pfr = PeerFindRoleGetTaskData();

    if (!pfr->scan)
    {
        DEBUG_LOG("peer_find_role_handle_enable_scanning. State: %d", peer_find_role_get_state());

        switch (peer_find_role_get_state())
        {
            case PEER_FIND_ROLE_STATE_DISCOVER:
                /* We should be scanning in this state...
                   ...but will transition to DISCOVER_CONNECTABLE anyway */
                break;

            case PEER_FIND_ROLE_STATE_DISCOVER_CONNECTABLE:
                peer_find_role_set_state(PEER_FIND_ROLE_STATE_DISCOVER);
                break;

            default:
                break;
        }
    }
}


/*! Handle internal message for the command PeerFindRole_FindRoleCancel

    Use a standard handler to send a message to clients and finish
    any current activities.
 */
static void peer_find_role_handle_cancel_find_role(void)
{
    peer_find_role_completed(PEER_FIND_ROLE_CANCELLED);
}


/*! Helper function to update media status

    When we set a media flag we queue a message to be sent when
    we later go inactive.

    \param busy Whether the mask is to be set or cleared
    \param mask The bit mask to apply
 */
static void peer_find_role_update_media_flag(bool busy, peerFindRoleMediaBusyStatus_t mask)
{
    peerFindRoleTaskData *pfr = PeerFindRoleGetTaskData();

    if (busy)
    {
        pfr->media_busy |= (uint16)mask;
        peer_find_role_stop_scan_if_active();

        MessageCancelAll(PeerFindRoleGetTask(), PEER_FIND_ROLE_INTERNAL_ENABLE_SCANNING);
        MessageSendConditionally(PeerFindRoleGetTask(), 
                                 PEER_FIND_ROLE_INTERNAL_ENABLE_SCANNING, NULL,
                                 &pfr->media_busy);
    }
    else
    {
        pfr->media_busy &= ~((uint16)mask);
    }

    DEBUG_LOG("peer_find_role_update_media_flag. Now x%x. Scan:%p. State:%d", pfr->media_busy,pfr->scan, pfr->state);

}


/*! Handler for message #PEER_FIND_ROLE_INTERNAL_TELEPHONY_ACTIVE
    and #PEER_FIND_ROLE_INTERNAL_TELEPHONY_IDLE.

    Update our status for media with the bit PEER_FIND_ROLE_CALL_ACTIVE

    \param busy Whether we are now busy (TRUE) or idle (FALSE)
*/
static void peer_find_role_handle_telephony_busy(bool busy)
{
    peer_find_role_update_media_flag(busy, PEER_FIND_ROLE_CALL_ACTIVE);
}


/*! Handler for message #PEER_FIND_ROLE_INTERNAL_STREAMING_ACTIVE
    and #PEER_FIND_ROLE_INTERNAL_STREAMING_IDLE.

    Update our status for media with the bit PEER_FIND_ROLE_AUDIO_STREAMING

    \param busy Whether we are now busy (TRUE) or idle (FALSE)
*/
static void peer_find_role_handle_streaming_busy(bool busy)
{
    peer_find_role_update_media_flag(busy, PEER_FIND_ROLE_AUDIO_STREAMING);
}


/*! Handler for the connection manager message for disconnection

    This message is received for creation of a connection as well as
    disconnection. We only act upon disconnection, using the 
    loss of a connection to change state.

    \param ind  The connection manager indication
*/
static void peer_find_role_handle_connection_ind(const CON_MANAGER_CONNECTION_IND_T *ind)
{
    peerFindRoleTaskData *pfr = PeerFindRoleGetTaskData();

    /* update score with handset connection status */
    if (!ind->ble && appDeviceIsHandset(&ind->bd_addr))
    {
        pfr->scoring_info.handset_connected = ind->connected; 
        peer_find_role_request_score_update();
    }

    if (ind->ble && !ind->connected && BdaddrIsSame(&ind->bd_addr, &pfr->primary_addr))
    {
        switch (peer_find_role_get_state())
        {
            case PEER_FIND_ROLE_STATE_CLIENT:
            case PEER_FIND_ROLE_STATE_SERVER:
            case PEER_FIND_ROLE_STATE_DECIDING:
            case PEER_FIND_ROLE_STATE_AWAITING_CONFIRM:
                DEBUG_LOG("peer_find_role_handle_connection_ind. BLE disconnect");

                peer_find_role_set_state(PEER_FIND_ROLE_STATE_DISCOVER);
                break;

            case PEER_FIND_ROLE_STATE_UNINITIALISED:
            case PEER_FIND_ROLE_STATE_CHECKING_PEER:
                /* See no reason to expect in this case */
                break;

            case PEER_FIND_ROLE_STATE_INITIALISED:
                /* We have disconnected. If this is an initial timeout, notify clients */
                if (pfr->timeout_means_timeout)
                {
                    peer_find_role_notify_timeout();
                }
                else
                {
                    peer_find_role_notify_clients_if_pending();
                }
                break;

            case PEER_FIND_ROLE_STATE_AWAITING_DISCONNECT:
                pfr->gatt_cid = 0;
                peer_find_role_set_state(PEER_FIND_ROLE_STATE_INITIALISED);
                break;

            default:
                DEBUG_LOG("peer_find_role_handle_connection_ind. BLE disconnect. Unhandled state:%d",
                                    ind->bd_addr.lap, peer_find_role_get_state());
                break;
        }
    }
}


/*! Helper function to cancel internal messages for telephony status */
static void peer_find_role_cancel_telephony_events(void)
{
    MessageCancelAll(PeerFindRoleGetTask(), PEER_FIND_ROLE_INTERNAL_TELEPHONY_ACTIVE);
    MessageCancelAll(PeerFindRoleGetTask(), PEER_FIND_ROLE_INTERNAL_TELEPHONY_IDLE);
}


/*! Handler for events sent by telephony

    The messages are interpreted to indicate whether we have an active
    call and hence busy.

    Messages are sent internally to record the new state. The message
    for busy is sent immediately but the message for being idle is
    delayed, allowing for common usage where another activity may be
    started.

    \param id The identifier of the message from sys_msg
 */
static void peer_find_role_handle_telephony_event(MessageId id)
{
    switch(id)
    {
        case TELEPHONY_INCOMING_CALL:
        case TELEPHONY_INCOMING_CALL_OUT_OF_BAND_RINGTONE:
        case TELEPHONY_CALL_ANSWERED:
        case TELEPHONY_CALL_ONGOING:
        case TELEPHONY_UNENCRYPTED_CALL_STARTED:
        case TELEPHONY_CALL_AUDIO_RENDERED_LOCAL:
        case APP_HFP_SCO_CONNECTED_IND:
        case APP_HFP_SCO_INCOMING_RING_IND:
            DEBUG_LOG("peer_find_role_handle_telephony_event: %x(%d) setting flag", id, id&0xFF);

            peer_find_role_cancel_telephony_events();
            MessageSend(PeerFindRoleGetTask(), 
                        PEER_FIND_ROLE_INTERNAL_TELEPHONY_ACTIVE, NULL);
            break;

        case TELEPHONY_CALL_REJECTED:
        case TELEPHONY_CALL_ENDED:
        case TELEPHONY_CALL_HUNG_UP:
        case TELEPHONY_CALL_CONNECTION_FAILURE:
        case TELEPHONY_LINK_LOSS_OCCURRED:
        case TELEPHONY_DISCONNECTED:
        case TELEPHONY_CALL_AUDIO_RENDERED_REMOTE:
        case APP_HFP_SCO_INCOMING_ENDED_IND:
            DEBUG_LOG("peer_find_role_handle_telephony_event: %x(%d) clearing flag", id, id&0xFF);

            peer_find_role_cancel_telephony_events();
            MessageSendLater(PeerFindRoleGetTask(), 
                             PEER_FIND_ROLE_INTERNAL_TELEPHONY_IDLE, NULL,
                             PeerFindRoleConfigAllowScanAfterActivityDelayMs());
            break;

        case PAGING_START:
        case PAGING_STOP:
        case TELEPHONY_CONNECTED:
        case TELEPHONY_ERROR:
        case TELEPHONY_MUTE_ACTIVE:
        case TELEPHONY_MUTE_INACTIVE:
        case TELEPHONY_TRANSFERED:
            /* These don't affect us. No debug as would be noise */
            break;

        default:
            break;
    }
}


/*! Helper function to cancel internal messages for streaming status */
static void peer_find_role_cancel_streaming_events(void)
{
    MessageCancelAll(PeerFindRoleGetTask(), PEER_FIND_ROLE_INTERNAL_STREAMING_ACTIVE);
    MessageCancelAll(PeerFindRoleGetTask(), PEER_FIND_ROLE_INTERNAL_STREAMING_IDLE);
}


/*! Handler for events sent by AV

    The messages are interpreted to indicate whether we are streaming
    media and hence busy.

    Messages are sent internally to record the new state. The message
    for busy is sent immediately but the message for being idle is
    delayed, allowing for common usage where another activity may be
    started.

    \param id The identifier of the message from #av_messages
 */
static void peer_find_role_handle_streaming_event(MessageId id)
{
    switch(id)
    {
        case AV_STREAMING_ACTIVE:
        case AV_STREAMING_ACTIVE_APTX:
            DEBUG_LOG("peer_find_role_handle_streaming_event: %x(%d) setting flag", id, id&0xFF);

            peer_find_role_cancel_streaming_events();
            MessageSend(PeerFindRoleGetTask(), 
                        PEER_FIND_ROLE_INTERNAL_STREAMING_ACTIVE, NULL);
            break;

        case AV_STREAMING_INACTIVE:
        case AV_DISCONNECTED:
            DEBUG_LOG("peer_find_role_handle_streaming_event: %x(%d) clearing flag", id, id&0xFF);

            peer_find_role_cancel_streaming_events();
            MessageSendLater(PeerFindRoleGetTask(), 
                             PEER_FIND_ROLE_INTERNAL_STREAMING_IDLE, NULL,
                             PeerFindRoleConfigAllowScanAfterActivityDelayMs());
            break;

        default:
            break;
    }
}


/*! Handle confirmation of the BLE security request (for encryption)

    If we receive a failure, setting our state to INITIALISED will deal
    with error handling 

    \param[in] cfm  The confirmation to our call to ConnectionDmBleSecurityReq()
 */
static void peer_find_role_handle_ble_security(const CL_DM_BLE_SECURITY_CFM_T *cfm)
{
    if (cfm->status != ble_security_success)
    {
        DEBUG_LOG("peer_find_role_handle_ble_security. FAILED sts:%d", cfm->status);
        peer_find_role_set_state(PEER_FIND_ROLE_STATE_INITIALISED);
    }
    else
    {
        DEBUG_LOG("peer_find_role_handle_ble_security. success");
    }
}


/*! \name Handlers for internal timeouts 

    These functions handle timers used internally */
/*! @{ */

/*! Handler for the timeout message #PEER_FIND_ROLE_INTERNAL_TIMEOUT_CONNECTION

    This message may be started when PeerFindRole_FindRole() is called.
    When it expires, we inform clients that we have not yet managed to
    find a role. The service continues to try to find a role until
    cancelled.
 */
static void peer_find_role_handle_initial_timeout(void)
{
    peerFindRoleTaskData *pfr = PeerFindRoleGetTaskData();

    if (pfr->timeout_means_timeout)
    {
        peer_find_role_set_state(PEER_FIND_ROLE_STATE_INITIALISED);
    }
    else
    {
        /* We only timeout once, so clear the timeout value */
        pfr->role_timeout_ms = 0;

        peer_find_role_notify_timeout();
    }
}


/*! Handler for the timeout message #PEER_FIND_ROLE_INTERNAL_TIMEOUT_ADVERT_BACKOFF

    This timeout message is received to start advertising, and changes the
    state to #PEER_FIND_ROLE_STATE_DISCOVER_CONNECTABLE which starts 
    the advertising.
 */
static void peer_find_role_handle_advertising_backoff(void)
{
    if (PEER_FIND_ROLE_STATE_DISCOVER == peer_find_role_get_state())
    {
        peer_find_role_set_state(PEER_FIND_ROLE_STATE_DISCOVER_CONNECTABLE);
    }
    else
    {
        DEBUG_LOG("peer_find_role_handle_advertising_backoff. Expired after already left state");
    }
}


/*! Internal handler for the timeout message 
    #PEER_FIND_ROLE_INTERNAL_TIMEOUT_NOT_DISCONNECTED

    This timeout message is received if we have not disconnected.
    We disconnect by changing state, which forces the disconnection.
 */
static void peer_find_role_handle_disconnect_timeout(void)
{
    if (PEER_FIND_ROLE_STATE_AWAITING_DISCONNECT == peer_find_role_get_state())
    {
        peer_find_role_set_state(PEER_FIND_ROLE_STATE_INITIALISED); 
    }
    else
    {
        DEBUG_LOG("peer_find_role_handle_disconnect_timeout. Expired after already left state");
    }
}

/*! @} */

/*! The message handler for the \ref peer_find_role service

    This handler is called for messages sent to the service, including internal
    messages.

    \param task     The task called. Ignored.
    \param id       The message identifier
    \param message  Pointer to the message content. Often NULL
 */
static void peer_find_role_handler(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch (id)
    {
        /* ---- Advertising Manager messages ---- */
        case APP_ADVMGR_ADVERT_SET_DATA_CFM:
            peer_find_role_handle_advert_set_data_cfm((const APP_ADVMGR_ADVERT_SET_DATA_CFM_T *)message);
            break;

        /* ---- GATT Manager messages ---- */
        case GATT_MANAGER_REMOTE_CLIENT_CONNECT_CFM:
            peer_find_role_handle_client_connect_cfm((const GATT_MANAGER_REMOTE_CLIENT_CONNECT_CFM_T *)message);
            break;

        case GATT_MANAGER_REMOTE_SERVER_CONNECT_CFM:
            peer_find_role_handle_server_connect_cfm((const GATT_MANAGER_REMOTE_SERVER_CONNECT_CFM_T *)message);
            break;

        case GATT_MANAGER_CANCEL_REMOTE_CLIENT_CONNECT_CFM:
            /* Don't really care about status, but we requested this - so report */
            DEBUG_LOG("peer_find_role_handler. GATT_MANAGER_CANCEL_REMOTE_CLIENT_CONNECT_CFM sts:%d",
                        ((GATT_MANAGER_CANCEL_REMOTE_CLIENT_CONNECT_CFM_T*)message)->status);

            peer_find_role_activity_clear(PEER_FIND_ROLE_ACTIVE_ADVERTISING);
            break;

        case GATT_MANAGER_CANCEL_REMOTE_SERVER_CONNECT_CFM:
            /* Don't really care about status, but we requested this - so report */
            DEBUG_LOG("peer_find_role_handler. GATT_MANAGER_CANCEL_REMOTE_SERVER_CONNECT_CFM sts:%d",
                        ((GATT_MANAGER_CANCEL_REMOTE_SERVER_CONNECT_CFM_T*)message)->status);

            peer_find_role_activity_clear(PEER_FIND_ROLE_ACTIVE_CONNECTING);
            break;

        /* ---- GATT messages ---- */
        case GATT_DISCOVER_PRIMARY_SERVICE_CFM:
            peer_find_role_handle_primary_service_discovery((const GATT_DISCOVER_PRIMARY_SERVICE_CFM_T* )message);
            break;

        /* ---- GATT Role Selection messages ---- */
        case GATT_ROLE_SELECTION_CLIENT_INIT_CFM:
            peer_find_role_handle_client_init((const GATT_ROLE_SELECTION_CLIENT_INIT_CFM_T *)message);
            break;

        case GATT_ROLE_SELECTION_SERVER_CHANGE_ROLE_IND:
            peer_find_role_handle_change_role_ind((const GATT_ROLE_SELECTION_SERVER_CHANGE_ROLE_IND_T*)message);
            break;

        case GATT_ROLE_SELECTION_CLIENT_COMMAND_CFM:
            peer_find_role_handle_command_cfm((const GATT_ROLE_SELECTION_CLIENT_COMMAND_CFM_T*)message);
            break;

        case GATT_ROLE_SELECTION_CLIENT_FIGURE_OF_MERIT_IND:
            peer_find_role_handle_figure_of_merit((const GATT_ROLE_SELECTION_CLIENT_FIGURE_OF_MERIT_IND_T*)message);
            break;

        /* ---- Physical state messages ---- */
        case PHY_STATE_CHANGED_IND:
            peer_find_role_handle_phy_state((const PHY_STATE_CHANGED_IND_T*)message);
            break;

        /* ---- Direct Connection Library Messages ---- */
        case CL_DM_BLE_SECURITY_CFM:
            peer_find_role_handle_ble_security((const CL_DM_BLE_SECURITY_CFM_T*)message);
            break;

        /* ---- Charger monitor messages ---- */
        case CHARGER_MESSAGE_ATTACHED:
        case CHARGER_MESSAGE_DETACHED:
            peer_find_role_handle_charger_state(CHARGER_MESSAGE_ATTACHED == id);
            break;

        case CHARGER_MESSAGE_CHARGING_OK: /* Spam */
        case CHARGER_MESSAGE_CHARGING_LOW:
            break;

        /* ---- Accelerometer messages ---- */
        case ACCELEROMETER_MESSAGE_IN_MOTION:
        case ACCELEROMETER_MESSAGE_NOT_IN_MOTION:
            peer_find_role_handle_accelerometer_state(ACCELEROMETER_MESSAGE_IN_MOTION == id);
            break;

        /* ---- Battery messages ---- */
        case MESSAGE_BATTERY_LEVEL_UPDATE_PERCENT:
            peer_find_role_handle_battery_level((const MESSAGE_BATTERY_LEVEL_UPDATE_PERCENT_T *)message);
            break;

        /* ---- Connection Manager messages ---- */
        case CON_MANAGER_CONNECTION_IND:
            peer_find_role_handle_connection_ind((const CON_MANAGER_CONNECTION_IND_T *)message);
            break;

        /* ---- HFP Profile messages ---- */
        case PAGING_START:
        case PAGING_STOP:
        case TELEPHONY_CONNECTED:
        case TELEPHONY_INCOMING_CALL:
        case TELEPHONY_INCOMING_CALL_OUT_OF_BAND_RINGTONE:
        case TELEPHONY_CALL_ANSWERED:
        case TELEPHONY_CALL_REJECTED:
        case TELEPHONY_CALL_ONGOING:
        case TELEPHONY_CALL_ENDED:
        case TELEPHONY_CALL_HUNG_UP:
        case TELEPHONY_UNENCRYPTED_CALL_STARTED:
        case TELEPHONY_CALL_CONNECTION_FAILURE:
        case TELEPHONY_LINK_LOSS_OCCURRED:
        case TELEPHONY_DISCONNECTED:
        case TELEPHONY_CALL_AUDIO_RENDERED_LOCAL:
        case TELEPHONY_CALL_AUDIO_RENDERED_REMOTE:
        case TELEPHONY_ERROR:
        case TELEPHONY_MUTE_ACTIVE:
        case TELEPHONY_MUTE_INACTIVE:
        case TELEPHONY_TRANSFERED:
        case APP_HFP_SCO_INCOMING_ENDED_IND:
        case APP_HFP_SCO_CONNECTED_IND:
        case APP_HFP_SCO_INCOMING_RING_IND:
            peer_find_role_handle_telephony_event(id);
            break;

        /* ---- AV Profile messages ---- */
        case AV_STREAMING_ACTIVE:
        case AV_STREAMING_ACTIVE_APTX:
        case AV_STREAMING_INACTIVE:
        case AV_DISCONNECTED:
            peer_find_role_handle_streaming_event(id);
            break;

        /* ---- Internal messages ---- */
        case PEER_FIND_ROLE_INTERNAL_UPDATE_SCORE:
            peer_find_role_calculate_score();
            break;

        case PEER_FIND_ROLE_INTERNAL_CANCEL_FIND_ROLE:
            peer_find_role_handle_cancel_find_role();
            break;

        case PEER_FIND_ROLE_INTERNAL_CONNECT_TO_PEER:
            peer_find_role_handle_connect_to_peer();
            break;

        case PEER_FIND_ROLE_INTERNAL_ENABLE_SCANNING:
            peer_find_role_handle_enable_scanning();
            break;

        case PEER_FIND_ROLE_INTERNAL_TELEPHONY_ACTIVE:
        case PEER_FIND_ROLE_INTERNAL_TELEPHONY_IDLE:
            peer_find_role_handle_telephony_busy(PEER_FIND_ROLE_INTERNAL_TELEPHONY_ACTIVE == id);
            break;

        case PEER_FIND_ROLE_INTERNAL_STREAMING_ACTIVE:
        case PEER_FIND_ROLE_INTERNAL_STREAMING_IDLE:
            peer_find_role_handle_streaming_busy(PEER_FIND_ROLE_INTERNAL_STREAMING_ACTIVE == id);
            break;

        case PEER_FIND_ROLE_INTERNAL_TIMEOUT_CONNECTION:
            peer_find_role_handle_initial_timeout();
            break;

        case PEER_FIND_ROLE_INTERNAL_TIMEOUT_ADVERT_BACKOFF:
            peer_find_role_handle_advertising_backoff();
            break;

        case PEER_FIND_ROLE_INTERNAL_TIMEOUT_NOT_DISCONNECTED:
            peer_find_role_handle_disconnect_timeout();
            break;

        default:
            DEBUG_LOG("peer_find_role_handler. Unhandled message %d(0x%x)", id, id);
            break;
    }
}


/*! Helper function to initialise the GATT server for role selection
 */
static void peer_find_role_gatt_role_selection_server_init(void)
{
    peerFindRoleTaskData *pfr = PeerFindRoleGetTaskData();
    
    if (!GattRoleSelectionServerInit(&pfr->role_selection_server,
                    PeerFindRoleGetTask(),
                    HANDLE_ROLE_SELECTION_SERVICE, HANDLE_ROLE_SELECTION_SERVICE_END))
    {
        DEBUG_LOG("peer_find_role_gatt_role_selection_server_init. Server init failed");
        Panic();
    }
}

bool PeerFindRole_Init(Task init_task)
{
    UNUSED(init_task);

    peer_find_role._task.handler = peer_find_role_handler;
    peer_find_role.registered_tasks = TaskList_Create();

    peer_find_role_scoring_setup();
    
    peer_find_role_gatt_role_selection_server_init();

    /* track disconnections */
    ConManagerRegisterConnectionsClient(PeerFindRoleGetTask());
    ConManagerRequestDefaultQos(cm_transport_ble, cm_qos_low_latency);

    /* track streaming activities */
    appAvStatusClientRegister(PeerFindRoleGetTask());
    appHfpStatusClientRegister(PeerFindRoleGetTask());
    Telephony_RegisterForMessages(PeerFindRoleGetTask());

    peer_find_role_set_state(PEER_FIND_ROLE_STATE_INITIALISED);

    /* Start generating resolvable addresses */
    ConnectionDmBleConfigureLocalAddressAutoReq(ble_local_addr_generate_resolvable, NULL, 
                            PeerFindRoleConfigResolvableAddressTimeoutSecs());

    return TRUE;
}

