/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Handset service state machine
*/

#include <bdaddr.h>
#include <device.h>

#include <bt_device.h>
#include <connection_manager.h>
#include <device_properties.h>
#include <profile_manager.h>

#include "handset_service_sm.h"
#include "handset_service_protected.h"


/*! \brief Cast a Task to a handset_service_state_machine_t.
    This depends on task_data being the first member of handset_service_state_machine_t. */
#define handsetServiceSm_GetSmFromTask(task) ((handset_service_state_machine_t *)task)

/*! \brief Test if the current state is in the "CONNECTING" pseudo-state. */
#define handsetServiceSm_IsConnectingState(state) \
    ((state & HANDSET_SERVICE_CONNECTING_STATE_MASK) == HANDSET_SERVICE_CONNECTING_STATE_MASK)

/*! \brief Add one mask of profiles to another. */
#define handsetServiceSm_MergeProfiles(profiles, profiles_to_merge) \
    ((profiles) |= (profiles_to_merge))

/*! \brief Remove a set of profiles from another. */
#define handsetServiceSm_RemoveProfiles(profiles, profiles_to_remove) \
    ((profiles) &= ~(profiles_to_remove))

#define handsetServiceSm_ProfileIsSet(profiles, profile) \
    (((profiles) & (profile)) == (profile))

/*
    Helper Functions
*/

/*! \brief Convert a profile bitmask to Profile Manager profile connection list. */
static void handsetServiceSm_ConvertProfilesToProfileList(uint8 profiles, uint8 *profile_list, size_t profile_list_count)
{
    int entry = 0;
    profile_t pm_profile = profile_manager_hfp_profile;

    /* Loop over the profile manager profile_t enum values and if the matching
       profile mask from bt_device.h is set, add it to profile_list.

       Write up to (profile_list_count - 1) entries and leave space for the
       'last entry' marker at the end. */
    while ((pm_profile < profile_manager_max_number_of_profiles) && (entry < (profile_list_count - 1)))
    {
        switch (pm_profile)
        {
        case profile_manager_hfp_profile:
            if (handsetServiceSm_ProfileIsSet(profiles, DEVICE_PROFILE_HFP))
            {
                profile_list[entry++] = profile_manager_hfp_profile;
            }
            break;

        case profile_manager_a2dp_profile:
            if (handsetServiceSm_ProfileIsSet(profiles, DEVICE_PROFILE_A2DP))
            {
                profile_list[entry++] = profile_manager_a2dp_profile;
            }
            break;

        case profile_manager_avrcp_profile:
            if (handsetServiceSm_ProfileIsSet(profiles, DEVICE_PROFILE_AVRCP))
            {
                profile_list[entry++] = profile_manager_avrcp_profile;
            }
            break;

        default:
            break;
        }

        pm_profile++;
    }

    /* The final entry in the list is the 'end of list' marker */
    profile_list[entry] = profile_manager_max_number_of_profiles;
}

static bool handsetServiceSm_AllConnectionsDisconnected(handset_service_state_machine_t *sm)
{
    bdaddr handset_addr = DeviceProperties_GetBdAddr(sm->handset_device);

    return ((!ConManagerIsConnected(&handset_addr)) 
            && (BtDevice_GetConnectedProfiles(sm->handset_device) == 0));
}

/*
    State Enter & Exit functions.
*/

static void handsetServiceSm_EnterDisconnected(handset_service_state_machine_t *sm)
{
    HS_LOG("handsetServiceSm_EnterDisconnected");

    /* Complete any outstanding connect stop request */
    HandsetServiceSm_CompleteConnectStopRequests(sm, handset_service_status_disconnected);

    /* Complete any outstanding connect requests. */
    HandsetServiceSm_CompleteConnectRequests(sm, handset_service_status_failed);

    /* Complete any outstanding disconnect requests. */
    HandsetServiceSm_CompleteDisconnectRequests(sm, handset_service_status_success);

    /* Notify registered clients of this disconnect event. */
    HandsetService_SendDisconnectedIndNotification(
        sm->handset_device, handset_service_status_disconnected);

    /* If there are no open connections to this handset, destroy this state machine. */
    if (handsetServiceSm_AllConnectionsDisconnected(sm))
    {
        HS_LOG("handsetServiceSm_EnterDisconnected destroying sm for dev 0x%x", sm->handset_device);
        HandsetServiceSm_DeInit(sm);
    }
}

static void handsetServiceSm_ExitDisconnected(handset_service_state_machine_t *sm)
{
    UNUSED(sm);
    HS_LOG("handsetServiceSm_ExitDisconnected");
}

static void handsetServiceSm_EnterConnectingAcl(handset_service_state_machine_t *sm)
{
    bdaddr handset_addr = DeviceProperties_GetBdAddr(sm->handset_device);

    HS_LOG("handsetServiceSm_EnterConnectingAcl");

    /* Post message back to ourselves, blocked on creating ACL */
    MessageSendConditionally(&sm->task_data,
                             HANDSET_SERVICE_INTERNAL_CONNECT_ACL_COMPLETE,
                             NULL, ConManagerCreateAcl(&handset_addr));

    sm->acl_create_called = TRUE;
}

static void handsetServiceSm_ExitConnectingAcl(handset_service_state_machine_t *sm)
{
    UNUSED(sm);
    HS_LOG("handsetServiceSm_ExitConnectingAcl");
}

static void handsetServiceSm_EnterConnectingProfiles(handset_service_state_machine_t *sm)
{
    uint8 profile_list[4];

    HS_LOG("handsetServiceSm_EnterConnectingProfiles");

    /* Connect the requested profiles.
       The requested profiles bitmask needs to be converted to the format of
       the profiles_connect_order device property and set on the device before
       calling profile manager to do the connect. */
    handsetServiceSm_ConvertProfilesToProfileList(sm->profiles_requested,
                                                  profile_list, ARRAY_DIM(profile_list));
    Device_SetProperty(sm->handset_device,
                       device_property_profiles_connect_order, profile_list, sizeof(profile_list));
    ProfileManager_ConnectProfilesRequest(&sm->task_data, sm->handset_device);
}

static void handsetServiceSm_ExitConnectingProfiles(handset_service_state_machine_t *sm)
{
    UNUSED(sm);
    HS_LOG("handsetServiceSm_ExitConnectingProfiles");
}

/* Enter the CONNECTING pseudo-state */
static void handsetServiceSm_EnterConnecting(handset_service_state_machine_t *sm)
{
    HS_LOG("handsetServiceSm_EnterConnecting");

    sm->acl_create_called = FALSE;
}

/* Exit the CONNECTING pseudo-state */
static void handsetServiceSm_ExitConnecting(handset_service_state_machine_t *sm)
{
    HS_LOG("handsetServiceSm_ExitConnecting");

    if (sm->acl_create_called)
    {
        /* We have finished (successfully or not) attempting to connect, so
        we can relinquish our lock on the ACL.  Bluestack will then close
        the ACL when there are no more L2CAP connections */
        bdaddr handset_addr = DeviceProperties_GetBdAddr(sm->handset_device);
        ConManagerReleaseAcl(&handset_addr);
    }
}

static void handsetServiceSm_EnterConnected(handset_service_state_machine_t *sm)
{
    uint8 connected_profiles = BtDevice_GetConnectedProfiles(sm->handset_device);

    HS_LOG("handsetServiceSm_EnterConnected");

    /* Complete any outstanding stop connect request */
    HandsetServiceSm_CompleteConnectStopRequests(sm, handset_service_status_connected);

    /* Complete outstanding connect requests */
    HandsetServiceSm_CompleteConnectRequests(sm, handset_service_status_success);

    /* Notify registered clients about this connection */
    HandsetService_SendConnectedIndNotification(sm->handset_device, connected_profiles);

    /* Update the "last connected" profile list */
    BtDevice_SetLastConnectedProfilesForDevice(sm->handset_device, connected_profiles, TRUE);
}

static void handsetServiceSm_ExitConnected(handset_service_state_machine_t *sm)
{
    UNUSED(sm);
    HS_LOG("handsetServiceSm_ExitConnected");
}

static void handsetServiceSm_EnterDisconnecting(handset_service_state_machine_t *sm)
{
    uint8 profiles_connected = BtDevice_GetConnectedProfiles(sm->handset_device);
    uint8 profile_list[4];

    HS_LOG("handsetServiceSm_EnterDisconnecting requested 0x%x connected 0x%x, to_disconnect 0x%x",
           sm->profiles_requested, profiles_connected, (sm->profiles_requested | profiles_connected));

    /* Disconnect any profiles that were either requested or are currently
       connected. */
    handsetServiceSm_ConvertProfilesToProfileList((sm->profiles_requested | profiles_connected),
                                                  profile_list, ARRAY_DIM(profile_list));
    Device_SetProperty(sm->handset_device,
                       device_property_profiles_disconnect_order,
                       profile_list, sizeof(profile_list));
    ProfileManager_DisconnectProfilesRequest(&sm->task_data, sm->handset_device);
}

static void handsetServiceSm_ExitDisconnecting(handset_service_state_machine_t *sm)
{
    UNUSED(sm);
    HS_LOG("handsetServiceSm_ExitDisconnecting");
}


/*
    Public functions
*/

/* */
void HandsetServiceSm_SetState(handset_service_state_machine_t *sm, handset_service_state_t state)
{
    handset_service_state_t old_state = sm->state;

    /* It is not valid to re-enter the same state */
    assert(old_state != state);

    HS_LOG("HandsetServiceSm_SetState state(%02x) old_state(%02x)", state, old_state);

    /* Handle state exit functions */
    switch (sm->state)
    {
    case HANDSET_SERVICE_STATE_DISCONNECTED:
        handsetServiceSm_ExitDisconnected(sm);
        break;
    case HANDSET_SERVICE_STATE_CONNECTING_ACL:
        handsetServiceSm_ExitConnectingAcl(sm);
        break;
    case HANDSET_SERVICE_STATE_CONNECTING_PROFILES:
        handsetServiceSm_ExitConnectingProfiles(sm);
        break;
    case HANDSET_SERVICE_STATE_CONNECTED:
        handsetServiceSm_ExitConnected(sm);
        break;
    case HANDSET_SERVICE_STATE_DISCONNECTING:
        handsetServiceSm_ExitDisconnecting(sm);
        break;
    default:
        break;
    }

    /* Check for a exit transition from the CONNECTING pseudo-state */
    if (handsetServiceSm_IsConnectingState(old_state) && !handsetServiceSm_IsConnectingState(state))
        handsetServiceSm_ExitConnecting(sm);

    /* Set new state */
    sm->state = state;

    /* Check for a transition to the CONNECTING pseudo-state */
    if (!handsetServiceSm_IsConnectingState(old_state) && handsetServiceSm_IsConnectingState(state))
        handsetServiceSm_EnterConnecting(sm);

    /* Handle state entry functions */
    switch (sm->state)
    {
    case HANDSET_SERVICE_STATE_DISCONNECTED:
        if (old_state != HANDSET_SERVICE_STATE_NULL)
        {
            handsetServiceSm_EnterDisconnected(sm);
        }
        break;
    case HANDSET_SERVICE_STATE_CONNECTING_ACL:
        handsetServiceSm_EnterConnectingAcl(sm);
        break;
    case HANDSET_SERVICE_STATE_CONNECTING_PROFILES:
        handsetServiceSm_EnterConnectingProfiles(sm);
        break;
    case HANDSET_SERVICE_STATE_CONNECTED:
        handsetServiceSm_EnterConnected(sm);
        break;
    case HANDSET_SERVICE_STATE_DISCONNECTING:
        handsetServiceSm_EnterDisconnecting(sm);
        break;
    default:
        break;
    }
}

/*
    Message handler functions
*/

static void handsetServiceSm_HandleInternalConnectReq(handset_service_state_machine_t *sm,
    const HANDSET_SERVICE_INTERNAL_CONNECT_REQ_T *req)
{
    HS_LOG("handsetServiceSm_HandleInternalConnectReq state 0x%x device 0x%x profiles 0x%x",
           sm->state, req->device, req->profiles);

    /* Confirm requested addr is actually for this instance. */
    assert(sm->handset_device == req->device);

    switch (sm->state)
    {
    case HANDSET_SERVICE_STATE_DISCONNECTED:
    case HANDSET_SERVICE_STATE_DISCONNECTING: /* Allow a new connect req to cancel an in-progress disconnect. */
        {
            bdaddr handset_addr = DeviceProperties_GetBdAddr(sm->handset_device);

            HS_LOG("handsetServiceSm_HandleInternalConnectReq bdaddr %04x,%02x,%06lx",
                    handset_addr.nap, handset_addr.uap, handset_addr.lap);

            /* Store profiles to be connected */
            sm->profiles_requested = req->profiles;

            if (ConManagerIsConnected(&handset_addr))
            {
                HS_LOG("handsetServiceSm_HandleInternalConnectReq, ACL connected");

                if (sm->profiles_requested)
                {
                    HandsetServiceSm_SetState(sm, HANDSET_SERVICE_STATE_CONNECTING_PROFILES);
                }
                else
                {
                    HS_LOG("handsetServiceSm_HandleInternalConnectReq, no profiles to connect");
                    HandsetServiceSm_SetState(sm, HANDSET_SERVICE_STATE_CONNECTED);
                }
            }
            else
            {
                HS_LOG("handsetServiceSm_HandleInternalConnectReq, ACL not connected, attempt to open ACL");
                HandsetServiceSm_SetState(sm, HANDSET_SERVICE_STATE_CONNECTING_ACL);
            }
        }
        break;

    case HANDSET_SERVICE_STATE_CONNECTING_ACL:
        /* Already connecting ACL link - nothing more to do but wait for that to finish. */
        break;

    case HANDSET_SERVICE_STATE_CONNECTING_PROFILES:
        /* Profiles already being connected.
           TBD: Too late to merge new profile mask with in-progress one so what to do? */
        break;

    case HANDSET_SERVICE_STATE_CONNECTED:
        /* TBD: Check requested profiles are all connected;
           if not go back to connecting the missing ones? */
        {
            /* Already connected, so complete the request immediately. */
            HandsetServiceSm_CompleteConnectRequests(sm, handset_service_status_success);
        }
        break;

    default:
        HS_LOG("handsetServiceSm_HandleInternalConnectReq, unhandled");
        break;
    }
}

static void handsetServiceSm_HandleInternalDisconnectReq(handset_service_state_machine_t *sm,
    const HANDSET_SERVICE_INTERNAL_DISCONNECT_REQ_T *req)
{
    HS_LOG("handsetServiceSm_HandleInternalDisconnectReq state 0x%x device 0x%x", sm->state, req->device);

    /* Confirm requested addr is actually for this instance. */
    assert(sm->handset_device == req->device);

    switch (sm->state)
    {
    case HANDSET_SERVICE_STATE_DISCONNECTED:
        {
            /* Already disconnected, so complete the request immediately. */
            HandsetServiceSm_CompleteDisconnectRequests(sm, handset_service_status_success);
        }
        break;

    case HANDSET_SERVICE_STATE_CONNECTING_ACL:
        /* Cancelled before profile connect was requested; go to disconnected */
        HandsetServiceSm_SetState(sm, HANDSET_SERVICE_STATE_DISCONNECTED);
        break;

    case HANDSET_SERVICE_STATE_CONNECTING_PROFILES:
        /* Cancelled in-progress connect; go to disconnecting to wait for CFM */
        HandsetServiceSm_SetState(sm, HANDSET_SERVICE_STATE_DISCONNECTING);
        break;

    case HANDSET_SERVICE_STATE_CONNECTED:
        {
            if (BtDevice_GetConnectedProfiles(sm->handset_device))
            {
                HandsetServiceSm_SetState(sm, HANDSET_SERVICE_STATE_DISCONNECTING);
            }
            else
            {
                HandsetServiceSm_SetState(sm, HANDSET_SERVICE_STATE_DISCONNECTED);
            }
        }
        break;

    case HANDSET_SERVICE_STATE_DISCONNECTING:
        /* Already in the process of disconnecting so nothing more to do. */
        break;

    default:
        HS_LOG("handsetServiceSm_HandleInternalConnectReq, unhandled");
        break;
    }
}

static void handsetServiceSm_HandleInternalConnectAclComplete(handset_service_state_machine_t *sm)
{
    HS_LOG("handsetServiceSm_HandleInternalConnectAclComplete state 0x%x", sm->state);

    switch (sm->state)
    {
    case HANDSET_SERVICE_STATE_CONNECTING_ACL:
        {
            bdaddr handset_addr = DeviceProperties_GetBdAddr(sm->handset_device);

            if (ConManagerIsConnected(&handset_addr))
            {
                HS_LOG("handsetServiceSm_HandleInternalConnectAclComplete, ACL connected");

                if (sm->profiles_requested)
                {
                    HandsetServiceSm_SetState(sm, HANDSET_SERVICE_STATE_CONNECTING_PROFILES);
                }
                else
                {
                    HS_LOG("handsetServiceSm_HandleInternalConnectReq, no profiles to connect");
                    HandsetServiceSm_SetState(sm, HANDSET_SERVICE_STATE_CONNECTED);
                }
            }
            else
            {
                HS_LOG("handsetServiceSm_HandleInternalConnectReq, ACL failed to connect");
                HandsetServiceSm_SetState(sm, HANDSET_SERVICE_STATE_DISCONNECTED);
            }
        }
        break;

    default:
        HS_LOG("handsetServiceSm_HandleInternalConnectAclComplete, unhandled");
        break;
    }
}

/*! \brief Handle a HANDSET_SERVICE_INTERNAL_CONNECT_STOP_REQ */
static void handsetService_HandleInternalConnectStop(handset_service_state_machine_t *sm,
    const HANDSET_SERVICE_INTERNAL_CONNECT_STOP_REQ_T *req)
{
    HS_LOG("handsetService_HandleInternalConnectStop state 0x%x", sm->state);

    UNUSED(req);

    switch (sm->state)
    {
    case HANDSET_SERVICE_STATE_CONNECTING_ACL:
        /* ACL has not connected yet so go to disconnected to stop it */
        HS_LOG("handsetService_HandleInternalConnectStop, Cancel ACL connecting");
        HandsetServiceSm_SetState(sm, HANDSET_SERVICE_STATE_DISCONNECTED);
        break;
    
    case HANDSET_SERVICE_STATE_CONNECTING_PROFILES:
        /* Wait for profiles connect to complete */
        break;

    case HANDSET_SERVICE_STATE_CONNECTED:
        /* Already in a stable state, so send a CFM back immediately. */
        HandsetServiceSm_CompleteConnectStopRequests(sm, handset_service_status_connected);
        break;

    default:
        break;
    }
}

/*! \brief Handle a CONNECT_PROFILES_CFM */
static void handsetServiceSm_HandleProfileManagerConnectCfm(handset_service_state_machine_t *sm,
    const CONNECT_PROFILES_CFM_T *cfm)
{
    HS_LOG("handsetServiceSm_HandleProfileManagerConnectCfm state 0x%x result %d", sm->state, cfm->result);

    switch (sm->state)
    {
    case HANDSET_SERVICE_STATE_CONNECTING_PROFILES:
        if (cfm->result == profile_manager_success)
        {
            /* Assume all requested profiles were connected. */
            HandsetServiceSm_SetState(sm, HANDSET_SERVICE_STATE_CONNECTED);
        }
        else
        {
            HandsetServiceSm_SetState(sm, HANDSET_SERVICE_STATE_DISCONNECTED);
        }
        break;

    case HANDSET_SERVICE_STATE_CONNECTED:
        /* The connected profiles are stored in the device properties 
           so nothing else to do. */
        break;

    case HANDSET_SERVICE_STATE_DISCONNECTING:
        /* Connect has been cancelled already but this CFM may have been
           in-flight already. */
        HandsetServiceSm_SetState(sm, HANDSET_SERVICE_STATE_DISCONNECTED);
        break;

    default:
        HS_LOG("handsetServiceSm_HandleProfileManagerConnectCfm, unhandled");
        break;
    }
}

/*! \brief Handle a DISCONNECT_PROFILES_CFM */
static void handsetServiceSm_HandleProfileManagerDisconnectCfm(handset_service_state_machine_t *sm,
    const DISCONNECT_PROFILES_CFM_T *cfm)
{
    HS_LOG("handsetServiceSm_HandleProfileManagerDisconnectCfm state 0x%x result %d", sm->state, cfm->result);

    switch (sm->state)
    {
    case HANDSET_SERVICE_STATE_DISCONNECTING:
        if ((cfm->result == profile_manager_success)
            && handsetServiceSm_AllConnectionsDisconnected(sm))
        {
            HandsetServiceSm_SetState(sm, HANDSET_SERVICE_STATE_DISCONNECTED);
        }
        else
        {
            HS_LOG("handsetServiceSm_HandleProfileManagerDisconnectCfm, failed to disconnect");
        }
        break;

    default:
        HS_LOG("handsetServiceSm_HandleProfileManagerDisconnectCfm, unhandled");
        break;
    }
}

/*! \brief Handle a CONNECTED_PROFILE_IND */
void HandsetServiceSm_HandleProfileManagerConnectedInd(handset_service_state_machine_t *sm,
    const CONNECTED_PROFILE_IND_T *ind)
{
    HS_LOG("HandsetServiceSm_HandleProfileManagerConnectedInd state 0x%x profile 0x%x",
           sm->state, ind->profile);

    assert(sm->handset_device == ind->device);

    switch (sm->state)
    {
    case HANDSET_SERVICE_STATE_DISCONNECTED:
        HandsetServiceSm_SetState(sm, HANDSET_SERVICE_STATE_CONNECTED);
        break;

    case HANDSET_SERVICE_STATE_CONNECTING_ACL:
    case HANDSET_SERVICE_STATE_CONNECTING_PROFILES:
        /* Crossover between a locally initiated connect and a remote one. */
        /* TBD: what to do here? */
        break;

    case HANDSET_SERVICE_STATE_CONNECTED:
        {
            uint8 connected_profiles = BtDevice_GetConnectedProfiles(sm->handset_device);

            /* Stay in the same state but send an IND with all the profile(s) currently connected. */
            HandsetService_SendConnectedIndNotification(sm->handset_device, connected_profiles);

            /* Update the "last connected" profile list */
            BtDevice_SetLastConnectedProfilesForDevice(sm->handset_device, connected_profiles, TRUE);
        }
        break;

    default:
        HS_LOG("HandsetServiceSm_HandleProfileManagerConnectedInd, unhandled");
        break;
    }
}

/*! \brief Handle a DISCONNECTED_PROFILE_IND */
void HandsetServiceSm_HandleProfileManagerDisconnectedInd(handset_service_state_machine_t *sm,
    const DISCONNECTED_PROFILE_IND_T *ind)
{
    HS_LOG("HandsetServiceSm_HandleProfileManagerDisconnectedInd state 0x%x profile 0x%x",
           sm->state, ind->profile);

    assert(sm->handset_device == ind->device);

    switch (sm->state)
    {
    case HANDSET_SERVICE_STATE_DISCONNECTED:
        break;

    case HANDSET_SERVICE_STATE_CONNECTING_ACL:
    case HANDSET_SERVICE_STATE_CONNECTING_PROFILES:
        /* TBD: what to do here? */
        break;

    case HANDSET_SERVICE_STATE_CONNECTED:
        {
            /* Because we are in the CONNECTED state, assume this is a remote
               initiated disconnect and remove it from the last connected
               profiles.
               
               TBD: If disconnect was due to link-loss don't remove the profile
                    from last connected profiles. But the IND doesn't have a 
                    "reason". */
            BtDevice_SetLastConnectedProfilesForDevice(sm->handset_device, ind->profile, FALSE);

            /* Only go to disconnected state when there are no profiles connected. */
            if (handsetServiceSm_AllConnectionsDisconnected(sm))
            {
                HandsetServiceSm_SetState(sm, HANDSET_SERVICE_STATE_DISCONNECTED);
            }
        }
        break;

    case HANDSET_SERVICE_STATE_DISCONNECTING:
        /* A disconnect request to the profile manager is in progress
           already, so just wait for the DISCONNECT_PROFILES_CFM. */
        break;

    default:
        HS_LOG("HandsetServiceSm_HandleProfileManagerDisconnectedInd, unhandled");
        break;
    }
}

void HandsetServiceSm_HandleConManagerConnectionInd(handset_service_state_machine_t *sm,
    const CON_MANAGER_CONNECTION_IND_T *ind)
{
    HS_LOG("handsetServiceSm_HandleConManagerConnectionInd state 0x%x", sm->state);

    assert(sm->handset_device == BtDevice_GetDeviceForBdAddr(&ind->bd_addr));

    switch (sm->state)
    {
    case HANDSET_SERVICE_STATE_CONNECTED:
    case HANDSET_SERVICE_STATE_DISCONNECTING:
        if ((!ind->connected) && (!ind->ble))
        {
            HandsetServiceSm_SetState(sm, HANDSET_SERVICE_STATE_DISCONNECTED);
        }
        break;

    default:
        HS_LOG("handsetServiceSm_HandleConManagerConnectionInd unhandled");
        break;

    }
}

static void handsetServiceSm_MessageHandler(Task task, MessageId id, Message message)
{
    handset_service_state_machine_t *sm = handsetServiceSm_GetSmFromTask(task);

    HS_LOG("handsetServiceSm_MessageHandler id 0x%x", id);

    switch (id)
    {
    /* connection_manager messages */

    /* profile_manager messages */
    case CONNECT_PROFILES_CFM:
        handsetServiceSm_HandleProfileManagerConnectCfm(sm, (const CONNECT_PROFILES_CFM_T *)message);
        break;

    case DISCONNECT_PROFILES_CFM:
        handsetServiceSm_HandleProfileManagerDisconnectCfm(sm, (const DISCONNECT_PROFILES_CFM_T *)message);
        break;

    /* Internal messages */
    case HANDSET_SERVICE_INTERNAL_CONNECT_REQ:
        handsetServiceSm_HandleInternalConnectReq(sm, (const HANDSET_SERVICE_INTERNAL_CONNECT_REQ_T *)message);
        break;

    case HANDSET_SERVICE_INTERNAL_DISCONNECT_REQ:
        handsetServiceSm_HandleInternalDisconnectReq(sm, (const HANDSET_SERVICE_INTERNAL_DISCONNECT_REQ_T *)message);
        break;

    case HANDSET_SERVICE_INTERNAL_CONNECT_ACL_COMPLETE:
        handsetServiceSm_HandleInternalConnectAclComplete(sm);
        break;

    case HANDSET_SERVICE_INTERNAL_CONNECT_STOP_REQ:
        handsetService_HandleInternalConnectStop(sm, (const HANDSET_SERVICE_INTERNAL_CONNECT_STOP_REQ_T *)message);
        break;

    default:
        HS_LOG("handsetService_MessageHandler unhandled msg id 0x%x", id);
        break;
    }
}

void HandsetServiceSm_Init(handset_service_state_machine_t *sm)
{
    assert(sm != NULL);

    memset(sm, 0, sizeof(*sm));
    sm->state = HANDSET_SERVICE_STATE_NULL;
    sm->task_data.handler = handsetServiceSm_MessageHandler;

    TaskList_Initialise(&sm->connect_list);
    TaskList_Initialise(&sm->disconnect_list);
}

void HandsetServiceSm_DeInit(handset_service_state_machine_t *sm)
{
    TaskList_RemoveAllTasks(&sm->connect_list);
    TaskList_RemoveAllTasks(&sm->disconnect_list);

    MessageFlushTask(&sm->task_data);
    sm->handset_device = (device_t)0;
    sm->profiles_requested = 0;
    sm->acl_create_called = FALSE;
    sm->state = HANDSET_SERVICE_STATE_NULL;
}

void HandsetServiceSm_CompleteConnectRequests(handset_service_state_machine_t *sm, handset_service_status_t status)
{
    if (TaskList_Size(&sm->connect_list))
    {
        MESSAGE_MAKE(cfm, HANDSET_SERVICE_CONNECT_CFM_T);
        cfm->addr = DeviceProperties_GetBdAddr(sm->handset_device);
        cfm->status = status;

        /* Send HANDSET_SERVICE_CONNECT_CFM to all clients who made a
           connect request, then remove them from the list. */
        TaskList_MessageSend(&sm->connect_list, HANDSET_SERVICE_CONNECT_CFM, cfm);
        TaskList_RemoveAllTasks(&sm->connect_list);
    }

    /* Flush any queued internal connect requests */
    MessageCancelAll(&sm->task_data, HANDSET_SERVICE_INTERNAL_CONNECT_REQ);
}

void HandsetServiceSm_CompleteDisconnectRequests(handset_service_state_machine_t *sm, handset_service_status_t status)
{
    if (TaskList_Size(&sm->disconnect_list))
    {
        MESSAGE_MAKE(cfm, HANDSET_SERVICE_DISCONNECT_CFM_T);
        cfm->addr = DeviceProperties_GetBdAddr(sm->handset_device);
        cfm->status = status;

        /* Send HANDSET_SERVICE_DISCONNECT_CFM to all clients who made a
           disconnect request, then remove them from the list. */
        TaskList_MessageSend(&sm->disconnect_list, HANDSET_SERVICE_DISCONNECT_CFM, cfm);
        TaskList_RemoveAllTasks(&sm->disconnect_list);
    }

    /* Flush any queued internal disconnect requests */
    MessageCancelAll(&sm->task_data, HANDSET_SERVICE_INTERNAL_DISCONNECT_REQ);
}

void HandsetServiceSm_CompleteConnectStopRequests(handset_service_state_machine_t *sm, handset_service_status_t status)
{
    if (sm->connect_stop_task)
    {
        MESSAGE_MAKE(cfm, HANDSET_SERVICE_CONNECT_STOP_CFM_T);
        cfm->addr = DeviceProperties_GetBdAddr(sm->handset_device);
        cfm->status = status;

        MessageSend(sm->connect_stop_task, HANDSET_SERVICE_CONNECT_STOP_CFM, cfm);
        sm->connect_stop_task = (Task)0;
    }
}
