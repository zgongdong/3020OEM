/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      TWS Topology component core. 
*/

#include "tws_topology.h"
#include "tws_topology_private.h"
#include "tws_topology_primary_rules.h"
#include "tws_topology_secondary_rules.h"
#include "tws_topology_dfu_rules.h"
#include "tws_topology_client_msgs.h"
#include "tws_topology_goals.h"
#include "tws_topology_procedure_pair_peer.h"
#include "tws_topology_procedure_set_address.h"
#include "tws_topology_procedure_find_role.h"
#include "tws_topology_config.h"
#include "tws_topology_sdp.h"

#include <av.h>
#include <peer_find_role.h>
#include <peer_pair_le.h>
#include <state_proxy.h>
#include <phy_state.h>
#include <bt_device.h>
#include <bredr_scan_manager.h>
#include <key_sync.h>
#include <shadow_profile.h>
#include <hfp_profile.h>
#include <hdma.h>

#include <task_list.h>
#include <logging.h>

#include <panic.h>
#include <bdaddr.h>

/*! Instance of the TWS Topology. */
twsTopologyTaskData tws_topology = {0};

void twsTopology_RulesSetEvent(rule_events_t event)
{
    switch (TwsTopologyGetTaskData()->role)
    {
        case tws_topology_role_none:
            /* fall-thru, use primary rules in 'none' role */
        case tws_topology_role_primary:
            TwsTopologyPrimaryRules_SetEvent(TwsTopologyGetTask(), event);
            break;

        case tws_topology_role_secondary:
            TwsTopologySecondaryRules_SetEvent(TwsTopologyGetTask(), event);
            break;

        case tws_topology_role_dfu:
            TwsTopologyDfuRules_SetEvent(TwsTopologyGetTask(), event);
            break;

        default:
            break;
    }
}

#if 0
/*! \todo Expect to need these, but not called yet. */
static void twsTopology_RulesResetEvent(rule_events_t event)
{
    switch (TwsTopologyGetTaskData()->role)
    {
        case tws_topology_role_none:
            /* fall-thru, use primary rules in 'none' role */
        case tws_topology_role_primary:
            TwsTopologyPrimaryRules_ResetEvent(event);
            break;
        case tws_topology_role_secondary:
            TwsTopologySecondaryRules_ResetEvent(event);
            break;
        default:
            break;
    }
}
#endif

void twsTopology_RulesMarkComplete(MessageId message)
{
    switch (TwsTopologyGetTaskData()->role)
    {
        case tws_topology_role_none:
            /* fall-thru, use primary rules in 'none' role */
        case tws_topology_role_primary:
            TwsTopologyPrimaryRules_SetRuleComplete(message);
            break;

        case tws_topology_role_secondary:
            TwsTopologySecondaryRules_SetRuleComplete(message);
            break;

        case tws_topology_role_dfu:
            TwsTopologyDfuRules_SetRuleComplete(message);
            break;

        default:
            break;
    }
}

void twsTopology_SetRole(tws_topology_role role)
{
    tws_topology_role current_role = TwsTopologyGetTaskData()->role;

    DEBUG_LOG("twsTopology_SetRole Current role %u -> New role %u", current_role, role);

    TwsTopology_SendRoleChangedInd(role);

    /* only need to change role if actually changes */
    if (current_role != role)
    {
        TwsTopologyGetTaskData()->role = role;
        if(role == tws_topology_role_secondary)
        {
            Av_SetupForSecondaryRole();
            HfpProfile_SetRole(FALSE);
            ShadowProfile_SetRole(FALSE);
        }
        else if(role == tws_topology_role_primary)
        {
            Av_SetupForPrimaryRole();
            HfpProfile_SetRole(TRUE);
            ShadowProfile_SetRole(TRUE);
        }
    }
}

void twsTopology_SetActingInRole(bool acting)
{
    TwsTopologyGetTaskData()->acting_in_role = acting;
}

static void twsTopology_SetHdmaCreated(bool created)
{
    TwsTopologyGetTaskData()->hdma_created = created;
}

/*! \brief Handle failure to find a role due to not having a paired peer Earbud.
 */
static void twsTopology_HandlePeerFindRoleNoPeer(void)
{
    DEBUG_LOG("twsTopology_HandlePeerFindRoleNoPeer");
    twsTopology_RulesSetEvent(TWSTOP_RULE_EVENT_NO_PEER);
}

static void twsTopology_HandlePeerFindRoleTimeout(void)
{
    DEBUG_LOG("twsTopology_HandlePeerFindRoleTimeout - really acting primary");

    if (TwsTopologyGetTaskData()->start_cfm_needed)
    {
        TwsTopology_SendStartCfm(tws_topology_status_success, tws_topology_role_primary);
    }
    twsTopology_RulesSetEvent(TWSTOP_RULE_EVENT_ROLE_SELECTED_ACTING_PRIMARY);
}

static void twsTopology_HandlePeerFindRolePrimary(void)
{
    DEBUG_LOG("twsTopology_HandlePeerFindRolePrimary");

    if (TwsTopologyGetTaskData()->start_cfm_needed)
    {
        TwsTopology_SendStartCfm(tws_topology_status_success, tws_topology_role_primary);
    }
    twsTopology_RulesSetEvent(TWSTOP_RULE_EVENT_ROLE_SELECTED_PRIMARY);
}

static void twsTopology_HandlePeerFindRoleSecondary(void)
{
    DEBUG_LOG("twsTopology_HandlePeerFindRoleSecondary");

    if (TwsTopologyGetTaskData()->start_cfm_needed)
    {
        TwsTopology_SendStartCfm(tws_topology_status_success, tws_topology_role_secondary);
    }
    twsTopology_RulesSetEvent(TWSTOP_RULE_EVENT_ROLE_SELECTED_SECONDARY);
}

static void twsTopology_Start(void)
{
    bdaddr bd_addr_secondary;
    if (appDeviceGetSecondaryBdAddr(&bd_addr_secondary))
    {
        twsTopology_RulesSetEvent(TWSTOP_RULE_EVENT_PEER_PAIRED);
    }
    else
    {
        twsTopology_RulesSetEvent(TWSTOP_RULE_EVENT_NO_PEER);
    }
}

static void twsTopology_HandleInternalStart(void)
{
    DEBUG_LOG("twsTopology_HandleInternalStart");
    twsTopology_Start();
}

static void twsTopology_HandleProcPeerPairResult(PROC_PAIR_PEER_RESULT_T* pppr)
{
    if (pppr->success == TRUE)
    {
        DEBUG_LOG("twsTopology_HandleProcPeerPairResult PEER PAIR SUCCESS");
    }
    else
    {
        DEBUG_LOG("twsTopology_HandleProcPeerPairResult PEER PAIR FAILED");
    }
    twsTopology_Start();
}

/* \brief Generate physical state related events into rules engine. */
static void twsTopology_HandlePhyStateChangedInd(PHY_STATE_CHANGED_IND_T* ind)
{
    DEBUG_LOG("twsTopology_HandlePhyStateChangedInd ev %u", ind->event);

    switch (ind->event)
    {
        case phy_state_event_out_of_case:
            twsTopology_RulesSetEvent(TWSTOP_RULE_EVENT_OUT_CASE);
            break;
        case phy_state_event_in_case:
            twsTopology_RulesSetEvent(TWSTOP_RULE_EVENT_IN_CASE);
            break;
        default:
        break;
    }
}

void TwsTopology_GetPeerBdAddr(bdaddr* addr)
{
    twsTopologyTaskData* td = TwsTopologyGetTaskData();

    PanicFalse(td->role != tws_topology_role_none);

    if (td->role == tws_topology_role_primary)
    {
        appDeviceGetSecondaryBdAddr(addr);
    }
    else if (td ->role != tws_topology_role_dfu)
    {
        appDeviceGetPrimaryBdAddr(addr);
    }
    else
    {
        appDeviceGetPeerBdAddr(addr);
    }

#if 0
    /*! \todo test code. These functions should no longer be needed */
    {
        bdaddr device_addr;

        appDeviceGetPeerBdAddr(&device_addr);
        if (tws_topology_role_dfu != td ->role
            && !BdaddrIsSame(&device_addr,addr))
        {
            DEBUG_LOG("TwsTopology_GetPeerBdAddr. Not DFU mode, but device peer bdaddr disagres");
            Panic();
        }
    }
#endif

}

bool TwsTopology_IsPeerBdAddr(bdaddr* addr)
{
    twsTopologyTaskData* td = TwsTopologyGetTaskData();
    bdaddr candidate_peer;

    PanicFalse(td->role != tws_topology_role_none);

    if (!appDeviceIsPeer(addr))
    {
        return FALSE;
    }

    switch (td->role)
    {
        case tws_topology_role_primary:
            appDeviceGetSecondaryBdAddr(&candidate_peer);
            break;

        case tws_topology_role_secondary:
            appDeviceGetPrimaryBdAddr(&candidate_peer);
            break;

        case tws_topology_role_dfu:
            appDeviceGetPeerBdAddr(&candidate_peer);
            break;

        default:
            Panic();
            break;
    }

    bool tws_result = BdaddrIsSame(addr, &candidate_peer);

#if 0
    /*! \todo test code. These functions should no longer be needed */

    appDeviceGetPeerBdAddr(&candidate_peer);
    bool device_result = BdaddrIsSame(addr, &candidate_peer);
    if (   (tws_topology_role_dfu != td->role)
        && device_result != tws_result)
    {
        DEBUG_LOG("TwsTopology_IsPeerBdAddr. Not DFU mode and address comparisons differ");
        Panic();
    }
#endif

    return tws_result;
}

/*! \brief Generate connection related events into rule engine. */
static void twsTopology_HandleConManagerConnectionInd(const CON_MANAGER_CONNECTION_IND_T* ind)
{
    DEBUG_LOG("twsTopology_HandleConManagerConnectionInd Conn:%u BLE:%u %04x,%02x,%06lx", ind->connected,
                                                                                          ind->ble,
                                                                                          ind->bd_addr.nap,
                                                                                          ind->bd_addr.uap,
                                                                                          ind->bd_addr.lap);

    if (ind->ble)
    {
        DEBUG_LOG("twsTopology_HandleConManagerConnectionInd not interested in BLE events atm");
        return;
    }

    /* generate handset BREDR connection events into rules engines */
    if (   (ind->connected)
        && (TwsTopology_IsPeerBdAddr(&ind->bd_addr)
        && (!ind->ble)))
    {
        DEBUG_LOG("twsTopology_HandleConManagerConnectionInd PEER BREDR Connected");
        twsTopology_RulesSetEvent(TWSTOP_RULE_EVENT_PEER_CONNECTED_BREDR);
    }

    if (   (!ind->connected)
        && (!ind->ble)
        && (appDeviceIsHandset(&ind->bd_addr)))
    {
        if (ind->reason == hci_error_conn_timeout)
        {
            DEBUG_LOG("twsTopology_HandleConManagerConnectionInd HANDSET BREDR LINKLOSS");
            twsTopology_RulesSetEvent(TWSTOP_RULE_EVENT_HANDSET_LINKLOSS);
        }
        else
        {
            DEBUG_LOG("twsTopology_HandleConManagerConnectionInd HANDSET BREDR Disconnected");
            twsTopology_RulesSetEvent(TWSTOP_RULE_EVENT_HANDSET_DISCONNECTED_BREDR);
        }
    }

    /* generate peer BREDR connection events into rules engines */
    if (    (!ind->connected)
         && (!ind->ble)
         && (TwsTopology_IsPeerBdAddr(&ind->bd_addr)))
    {
        if (ind->reason == hci_error_conn_timeout)
        {
            DEBUG_LOG("twsTopology_HandleConManagerConnectionInd PEER BREDR LINKLOSS");
            twsTopology_RulesSetEvent(TWSTOP_RULE_EVENT_PEER_LINKLOSS);
        }
        else
        {
            DEBUG_LOG("twsTopology_HandleConManagerConnectionInd PEER BREDR DISCONNECTED");
            twsTopology_RulesSetEvent(TWSTOP_RULE_EVENT_PEER_DISCONNECTED_BREDR);
        }
    }
}

static void twsTopology_HandleShadowProfileConnectedInd(void)
{
    /*  Shadow ACL connection established, Invoke Hdma_Init  */
    if(TwsTopology_IsPrimary())
    {
        twsTopology_SetHdmaCreated(TRUE);
        Hdma_Init();
    }
}

static void twsTopology_HandleShadowProfileDisconnectedInd(void)
{
    /*  Shadow ACL connection disconnected, Invoke HDMA_Destroy  */
    if(TwsTopology_IsPrimary())
    {
        twsTopology_SetHdmaCreated(FALSE);
        Hdma_Destroy();
    }
}

/*! \brief TWS Topology message handler.
 */
static void twsTopology_HandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);

    /* push goal decisions into goal engine */
    if (   ((TWSTOP_INTERNAL_PRIMARY_RULE_MSG_BASE <= id) && (id < TWSTOP_PRIMARY_GOAL_NOP)) 
        || ((TWSTOP_INTERNAL_SECONDARY_RULE_MSG_BASE <= id) && (id < TWSTOP_SECONDARY_GOAL_NOP))
        || ((TWSTOP_INTERNAL_DFU_RULE_MSG_BASE <= id) && (id < TWSTOP_DFU_GOAL_NOP)))
    {
        TwsTopology_HandleGoalDecision(task, id, message);
        return;
    }

    /* handle all other messages */
    switch (id)
    {
            /* ROLE SELECT SERVICE */
        case PEER_FIND_ROLE_NO_PEER:
            twsTopology_HandlePeerFindRoleNoPeer();
            break;
        case PEER_FIND_ROLE_ACTING_PRIMARY:
            twsTopology_HandlePeerFindRoleTimeout();
            break;
        case PEER_FIND_ROLE_PRIMARY:
            twsTopology_HandlePeerFindRolePrimary();
            break;
        case PEER_FIND_ROLE_SECONDARY:
            twsTopology_HandlePeerFindRoleSecondary();
            break;
        case PEER_FIND_ROLE_CANCELLED:
            /* no action required */
            DEBUG_LOG("twsTopology_HandleMessage: PEER_FIND_ROLE_CANCELLED");
            break;

        case PROC_PAIR_PEER_RESULT:
            twsTopology_HandleProcPeerPairResult((PROC_PAIR_PEER_RESULT_T*)message);
            break;

            /* STATE PROXY MESSAGES */
            /*! \todo deliberately left in, will be needed just not yet. */
//        case STATE_PROXY_EVENT_INITIAL_STATE_SENT:
//            break;

        /* Shadow Profile messages */
        case SHADOW_PROFILE_CONNECT_IND:
            twsTopology_HandleShadowProfileConnectedInd();
            break;

        case SHADOW_PROFILE_DISCONNECT_IND:
            twsTopology_HandleShadowProfileDisconnectedInd();
            break;

        /* PHY STATE MESSAGES */
        case PHY_STATE_CHANGED_IND:
            twsTopology_HandlePhyStateChangedInd((PHY_STATE_CHANGED_IND_T*)message);
            break;

        case CON_MANAGER_CONNECTION_IND:
            twsTopology_HandleConManagerConnectionInd((CON_MANAGER_CONNECTION_IND_T*)message);
            break;

            /* INTERNAL MESSAGES */
        case TWSTOP_INTERNAL_START:
            twsTopology_HandleInternalStart();
            break;

        case CL_SDP_REGISTER_CFM:
            TwsTopology_HandleClSdpRegisterCfm(TwsTopologyGetTask(), (CL_SDP_REGISTER_CFM_T *)message);
            break;

        case CL_SDP_UNREGISTER_CFM:
            TwsTopology_HandleClSdpUnregisterCfm(TwsTopologyGetTask(), (CL_SDP_UNREGISTER_CFM_T *)message);
            break;

        case CL_SDP_SERVICE_SEARCH_ATTRIBUTE_CFM:
            TwsTopology_HandleClSdpServiceSearchAttributeCfm((CL_SDP_SERVICE_SEARCH_ATTRIBUTE_CFM_T *)message);
            break;

        default:
            DEBUG_LOG("twsTopology_HandleMessage. Unhandled message %d(0x%x)",id,id);
            break;
    }
}

static void twsTopologyHandlePairingSuccessMessage(bdaddr handset_address)
{
#if 0
/*#ifndef DISABLE_TWS_PLUS /todo there is a race here waiting for appDeviceSetTwsVersion where
    incoming connection may be rejected*/
    TwsTopology_SearchForTwsPlusSdp(handset_address);
#else
    /* TWS+ disabled, so assume handset is standard handset */
    if (appDeviceSetTwsVersion(&handset_address, DEVICE_TWS_STANDARD))
    {
        KeySync_Sync();
    }
#endif
}

static void twsTopology_HandlePairingActivity(const PAIRING_ACTIVITY_T *message)
{
    DEBUG_LOGF("twsTopology_HandlePairingActivity status=%d", message->status);

    switch(message->status)
    {
        case pairingSuccess:
            twsTopologyHandlePairingSuccessMessage(message->device_addr);
            break;

        default:
            break;
    }
}

static void twsTopology_HandlePairingActivityNotification(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch(id)
    {
        case PAIRING_ACTIVITY:
            DEBUG_LOG("TwsTopology PAIRING_ACTIVITY");
            twsTopology_HandlePairingActivity(message);
            break;

        default:
            break;
    }
}

bool TwsTopology_Init(Task init_task)
{
    twsTopologyTaskData *tws_taskdata = TwsTopologyGetTaskData();

    tws_taskdata->task.handler = twsTopology_HandleMessage;
    tws_taskdata->app_task = init_task;

    tws_taskdata->role = tws_topology_role_none;

    /* Set hdma_created to FALSE */
    tws_taskdata->hdma_created = FALSE; 

    /* Handover is allowed by default, app may prohibit handover by calling
    TwsTopology_ProhibitHandover() function with TRUE parameter */
    tws_taskdata->app_prohibit_handover = FALSE;

    /* setup task used to queue goals waiting on contention with currently
       active goals to clear. */
    tws_taskdata->pending_goal_queue_task.handler = TwsTopology_HandleGoalDecision;

    TwsTopologyPrimaryRules_Init(NULL);
    TwsTopologySecondaryRules_Init(NULL);
    TwsTopologyDfuRules_Init();

    PeerFindRole_RegisterTask(TwsTopologyGetTask());
    /*! \todo deliberately left in, will be needed just not yet. */
//    StateProxy_StateProxyEventRegisterClient(TwsTopologyGetTask());

    /* Register for connect / disconnect events from shadow profile */
    ShadowProfile_ClientRegister(TwsTopologyGetTask());
    
    appPhyStateRegisterClient(TwsTopologyGetTask());
    ConManagerRegisterConnectionsClient(TwsTopologyGetTask());
    BredrScanManager_PageScanParametersRegister(&page_scan_params);
    BredrScanManager_InquiryScanParametersRegister(&inquiry_scan_params);

    TwsTopology_SetState(TWS_TOPOLOGY_STATE_SETTING_SDP);

    TaskList_Initialise(&tws_taskdata->role_changed_tasks);

    return TRUE;
}

void TwsTopology_Start(Task requesting_task)
{
    twsTopologyTaskData *twst = TwsTopologyGetTaskData();

    twst->app_task = requesting_task;
    twst->start_cfm_needed = TRUE;

    twst->pairing_notification_task.handler = twsTopology_HandlePairingActivityNotification;

    Pairing_ActivityClientRegister(&twst->pairing_notification_task);

    MessageSend(TwsTopologyGetTask(), TWSTOP_INTERNAL_START, NULL);
}

void TwsTopology_RoleChangedRegisterClient(Task client_task)
{
    twsTopologyTaskData *twst = TwsTopologyGetTaskData();
    TaskList_AddTask(&twst->role_changed_tasks, client_task);
}

void TwsTopology_RoleChangedUnRegisterClient(Task client_task)
{
    twsTopologyTaskData *twst = TwsTopologyGetTaskData();
    TaskList_RemoveTask(&twst->role_changed_tasks, client_task);
}

tws_topology_role TwsTopology_GetRole(void)
{
    return TwsTopologyGetTaskData()->role;
}


void TwsTopology_SwitchToDfuRole(void)
{
    DEBUG_LOG("TwsTopology_SwitchToDfuRole. Setting event TWSTOP_RULE_EVENT_DFU_ROLE");

    twsTopology_RulesSetEvent(TWSTOP_RULE_EVENT_DFU_ROLE);
}


void TwsTopology_EndDfuRole(void)
{
    DEBUG_LOG("TwsTopology_EndDfuRole. Setting event TWSTOP_RULE_EVENT_DFU_ROLE_COMPLETE");

    twsTopology_RulesSetEvent(TWSTOP_RULE_EVENT_DFU_ROLE_COMPLETE);

    TwsTopologyPrimaryRules_ResetEvent(TWSTOP_RULE_EVENT_DFU_ROLE);
}


void TwsTopology_SelectPrimaryAddress(bool primary)
{
    /*! \todo Consider whether should handle "now in DFU role" message
            and call this. That depends on <something> remembering/
            knowing that we are already in an upgrade */
    TwsTopologyDfuRules_SetEvent(TwsTopologyGetTask(),
                                 primary ? TWSTOP_RULE_EVENT_ROLE_SELECTED_PRIMARY
                                         : TWSTOP_RULE_EVENT_ROLE_SELECTED_SECONDARY);
}


bool TwsTopology_IsPrimary(void)
{
    if (TwsTopology_GetRole() == tws_topology_role_dfu)
    {
        DEBUG_LOG("TwsTopology_IsPrimary. Panicing as DFU mode");
        Panic();
    }

    return (TwsTopology_GetRole() == tws_topology_role_primary);
}

bool TwsTopology_IsFullPrimary(void)
{
    return (   (TwsTopology_GetRole() == tws_topology_role_primary)
            && !TwsTopologyGetTaskData()->acting_in_role);
}

bool TwsTopology_IsSecondary(void)
{
    return (TwsTopology_GetRole() == tws_topology_role_secondary);
}

bool TwsTopology_IsActingPrimary(void)
{
    return ((TwsTopology_GetRole() == tws_topology_role_primary)
            && (TwsTopologyGetTaskData()->acting_in_role));
}

void TwsTopology_ProhibitHandover(bool prohibit)
{
    TwsTopologyGetTaskData()->app_prohibit_handover = prohibit;
}
