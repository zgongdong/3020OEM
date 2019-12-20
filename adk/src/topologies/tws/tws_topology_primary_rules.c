/*!
\copyright  Copyright (c) 2005 - 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      
*/

#include "tws_topology_primary_rules.h"
#include "tws_topology_goals.h"
#include "tws_topology_config.h"

#include <bt_device.h>
#include <device_properties.h>
#include <device.h>
#include <device_list.h>
#include <phy_state.h>
#include <rules_engine.h>
#include <connection_manager.h>
#include <scofwd_profile.h>
#include <state_proxy.h>
#include <peer_signalling.h>
#include <peer_find_role.h>

#include <logging.h>

#pragma unitsuppress Unused

/*! \{
    Macros for diagnostic output that can be suppressed.
    Allows debug of the rules module at two levels. */
#define TWSTOP_PRIMARY_RULES_LOG(x)       //DEBUG_LOG(x)
#define TWSTOP_PRIMARY_RULES_LOGF(x, ...) //DEBUG_LOGF(x, __VA_ARGS__)

#define TWSTOP_PRIMARY_RULE_LOG(x)         DEBUG_LOG(x)
#define TWSTOP_PRIMARY_RULE_LOGF(x, ...)   DEBUG_LOGF(x, __VA_ARGS__)
/*! \} */

/* Forward declaration for use in RULE_ACTION_RUN_PARAM macro below */
static rule_action_t twsTopologyPrimaryRules_CopyRunParams(const void* param, size_t size_param);

/*! \brief Macro used by rules to return RUN action with parameters to return.
 
    Copies the parameters/data into the rules instance where the rules engine 
    can use it when building the action message.
*/
#define RULE_ACTION_RUN_PARAM(x)   twsTopologyPrimaryRules_CopyRunParams(&(x), sizeof(x))

/*! Get pointer to the connection rules task data structure. */
#define TwsTopologyPrimaryRulesGetTaskData()  (&tws_topology_primary_rules_task_data)

TwsTopologyPrimaryRulesTaskData tws_topology_primary_rules_task_data;

/*! \{
    Rule function prototypes, so we can build the rule tables below. */
DEFINE_RULE(ruleTwsTopPriPairPeer);
DEFINE_RULE(ruleTwsTopPriPeerPairedInCase);
DEFINE_RULE(ruleTwsTopPriPeerPairedOutCase);

DEFINE_RULE(ruleTwsTopPriConnectPeerProfiles);
DEFINE_RULE(ruleTwsTopPriEnableConnectablePeer);
DEFINE_RULE(ruleTwsTopPriDisableConnectablePeer);
//DEFINE_RULE(ruleTwsTopPriDisconnectPeerProfiles);
DEFINE_RULE(ruleTwsTopPriReleasePeer);

DEFINE_RULE(ruleTwsTopPriFindRole);
DEFINE_RULE(ruleTwsTopPriNoRoleIdle);
DEFINE_RULE(ruleTwsTopPriSelectedPrimary);
DEFINE_RULE(ruleTwsTopPriSelectedActingPrimary);
DEFINE_RULE(ruleTwsTopPriNoRoleSelectedSecondary);
DEFINE_RULE(ruleTwsTopPriPrimarySelectedSecondary);
DEFINE_RULE(ruleTwsTopPriCancelFindRole);
DEFINE_RULE(ruleTwsTopPriPeerLostFindRole);
DEFINE_RULE(ruleTwsTopPriHandsetLinkLossReconnect);
DEFINE_RULE(ruleTwsTopHandoverStart);
DEFINE_RULE(ruleTwsTopHandoverFailed);
DEFINE_RULE(ruleTwsTopHandoverFailureHandled);

DEFINE_RULE(ruleTwsTopPriEnableConnectableHandset);
DEFINE_RULE(ruleTwsTopPriDisableConnectableHandset);
DEFINE_RULE(ruleTwsTopPriRoleSwitchConnectHandset);
DEFINE_RULE(ruleTwsTopPriOutCaseConnectHandset);
DEFINE_RULE(ruleTwsTopPriInCaseDisconnectHandset);
DEFINE_RULE(ruleTwsTopPriAllowHandsetConnect);

DEFINE_RULE(ruleTwsTopPriSwitchToDfuRole);
DEFINE_RULE(ruleTwsTopPriEnableLeConnectableHandset);

DEFINE_RULE(ruleTwsTopPriDisconnectHandset);

/*! \} */

/*! \brief TWS Topology rules deciding behaviour in a Primary role.
    \note This rule set is also used for an Earbud that has not yet determined which
          role to use.
*/
const rule_entry_t twstop_primary_rules_set[] =
{
    /*********************************
     * Startup and Peer Pairing Rules
     *********************************/

    /* Decide if peer pairing should run. */
    RULE(TWSTOP_RULE_EVENT_NO_PEER,         ruleTwsTopPriPairPeer,                  TWSTOP_PRIMARY_GOAL_PAIR_PEER),

    /* Decide if Primary address should be set, when peer pairing completes whilst in the case. */
    RULE(TWSTOP_RULE_EVENT_PEER_PAIRED,     ruleTwsTopPriPeerPairedInCase,          TWSTOP_PRIMARY_GOAL_SET_PRIMARY_ADDRESS),

    /* Decide if Primary address should be set and role selection started, when peer pairing completes
     * out of the case. */
    RULE(TWSTOP_RULE_EVENT_PEER_PAIRED,     ruleTwsTopPriPeerPairedOutCase,         TWSTOP_PRIMARY_GOAL_SET_PRIMARY_ADDRESS_FIND_ROLE),

    /*****************************
     * Role Selection rules 
     *****************************/

    /* If static handover fails, decide if should this Earbud needs to run no_role_idle script. */
    RULE(TWSTOP_RULE_EVENT_STATIC_HANDOVER_FAILED,         ruleTwsTopPriNoRoleIdle,                 TWSTOP_PRIMARY_GOAL_NO_ROLE_IDLE),

    /* If static handover fails when not in the case, decide if this Earbud needs to start role selection. */
    RULE(TWSTOP_RULE_EVENT_STATIC_HANDOVER_FAILED,         ruleTwsTopPriFindRole,                   TWSTOP_PRIMARY_GOAL_FIND_ROLE),

    /* When Primary Earbud put in the case, decide if role selection should be cancelled. */
    RULE(TWSTOP_RULE_EVENT_IN_CASE,                        ruleTwsTopPriCancelFindRole,             TWSTOP_PRIMARY_GOAL_CANCEL_FIND_ROLE),

    /* When Earbud is taken out of the case, decide if role selection should be executed. */
    RULE(TWSTOP_RULE_EVENT_OUT_CASE,                       ruleTwsTopPriFindRole,                   TWSTOP_PRIMARY_GOAL_FIND_ROLE),

    /* When Primary role has been selected, decide if switch to become Primary role should
     * be executed. */
    RULE(TWSTOP_RULE_EVENT_ROLE_SELECTED_PRIMARY,          ruleTwsTopPriSelectedPrimary,            TWSTOP_PRIMARY_GOAL_BECOME_PRIMARY),

    /* When Secondary role has been selected, decide if switch to become Secondary role should
     * be executed. */
    RULE(TWSTOP_RULE_EVENT_ROLE_SELECTED_SECONDARY,        ruleTwsTopPriNoRoleSelectedSecondary,    TWSTOP_PRIMARY_GOAL_BECOME_SECONDARY),

    /* When Acting Primary role has been selected, decide if switch to become Acting Primary role should
     * be executed. */
    RULE(TWSTOP_RULE_EVENT_ROLE_SELECTED_ACTING_PRIMARY,   ruleTwsTopPriSelectedActingPrimary,      TWSTOP_PRIMARY_GOAL_BECOME_ACTING_PRIMARY),

    /* When an Earbud in an acting primary role has received decision to be secondary,
     * decide if the switch to secondary role should be executed. */
    RULE(TWSTOP_RULE_EVENT_ROLE_SELECTED_SECONDARY,        ruleTwsTopPriPrimarySelectedSecondary,   TWSTOP_PRIMARY_GOAL_ROLE_SWITCH_TO_SECONDARY),

    /* When peer BREDR link is lost, decide if Primary Earbud should start role selection. */
    /*! \todo need a variant of this to do a find role with no timeout when we're already paging the handset */
    RULE(TWSTOP_RULE_EVENT_PEER_LINKLOSS,                  ruleTwsTopPriPeerLostFindRole,           TWSTOP_PRIMARY_GOAL_PRIMARY_FIND_ROLE),

    /* When peer BREDR link is disconnected, decide if Primary Earbud should start role selection. */
    RULE(TWSTOP_RULE_EVENT_PEER_DISCONNECTED_BREDR,        ruleTwsTopPriPeerLostFindRole,           TWSTOP_PRIMARY_GOAL_PRIMARY_FIND_ROLE),

    /* When requested to select DFU rules, select them */
    RULE(TWSTOP_RULE_EVENT_DFU_ROLE,                       ruleTwsTopPriSwitchToDfuRole,            TWSTOP_PRIMARY_GOAL_DFU_ROLE),

    /* When Secondary fails to connect BREDR ACL after role selection, decide if Primary Earbud should restart
     * role selection. */
    RULE(TWSTOP_RULE_EVENT_FAILED_PEER_CONNECT,            ruleTwsTopPriPeerLostFindRole,           TWSTOP_PRIMARY_GOAL_PRIMARY_FIND_ROLE),

    /*****************************
     * Peer connectivity rules 
     *****************************/

    /* After switching to primary role, decide if Primary should enable page
     * scan for Secondary to connect. */
    RULE(TWSTOP_RULE_EVENT_ROLE_SWITCH,             ruleTwsTopPriEnableConnectablePeer,     TWSTOP_PRIMARY_GOAL_CONNECTABLE_PEER),

    /* After peer BREDR connection established, decide if Primary should disable page scan
     * for Secondary. */
    RULE(TWSTOP_RULE_EVENT_PEER_CONNECTED_BREDR,    ruleTwsTopPriDisableConnectablePeer,    TWSTOP_PRIMARY_GOAL_CONNECTABLE_PEER),

    /* When peer BREDR link established by Secondary, decide which profiles
     * to connect. */
    RULE(TWSTOP_RULE_EVENT_PEER_CONNECTED_BREDR,    ruleTwsTopPriConnectPeerProfiles,       TWSTOP_PRIMARY_GOAL_CONNECT_PEER_PROFILES),

    /* When peer BREDR link established by Secondary, but we have since entered
       case, release the peer link (dont wait for disconnect as other rules handle). */
    RULE(TWSTOP_RULE_EVENT_PEER_CONNECTED_BREDR,    ruleTwsTopPriReleasePeer,               TWSTOP_PRIMARY_GOAL_RELEASE_PEER),

    /*! When handset BREDR link is disconnected, decide if the Primary Earbud should enable page scan. */
    RULE(TWSTOP_RULE_EVENT_HANDSET_DISCONNECTED_BREDR, ruleTwsTopPriEnableConnectableHandset, TWSTOP_PRIMARY_GOAL_CONNECTABLE_HANDSET),

    /*! When handset BREDR link is lost, decide if Primary Earbud should attempt to reconnect. */
    RULE(TWSTOP_RULE_EVENT_HANDSET_LINKLOSS,        ruleTwsTopPriHandsetLinkLossReconnect,  TWSTOP_PRIMARY_GOAL_CONNECT_HANDSET),

    /* When Primary Earbud goes in the case, device which peer profiles should
     * be disconnected. */
    RULE(TWSTOP_RULE_EVENT_IN_CASE, ruleTwsTopPriNoRoleIdle, TWSTOP_PRIMARY_GOAL_NO_ROLE_IDLE),

    /*****************************
     * Handset connectivity rules 
     *****************************/

    /* When role switch completed, decide if page scan should be enabled for handset connections. */
    RULE(TWSTOP_RULE_EVENT_ROLE_SWITCH,                ruleTwsTopPriEnableConnectableHandset,    TWSTOP_PRIMARY_GOAL_CONNECTABLE_HANDSET),

    /* When role switch complete, decide if connection manager should accept handset connections. */
    RULE(TWSTOP_RULE_EVENT_ROLE_SWITCH,                ruleTwsTopPriAllowHandsetConnect,    TWSTOP_PRIMARY_GOAL_ALLOW_HANDSET_CONNECT),

    /* When role switch completed, decide if LE advertisement should be enabled for handset connections. */
    RULE(TWSTOP_RULE_EVENT_ROLE_SWITCH,     ruleTwsTopPriEnableLeConnectableHandset,    TWSTOP_PRIMARY_GOAL_LE_CONNECTABLE_HANDSET),

    /* Connect handset after role switch to Primary */
    RULE(TWSTOP_RULE_EVENT_ROLE_SWITCH,                ruleTwsTopPriRoleSwitchConnectHandset,    TWSTOP_PRIMARY_GOAL_CONNECT_HANDSET),

    /* After handset BREDR connection established, decide if Primary should disable page scan. */
    RULE(TWSTOP_RULE_EVENT_HANDSET_CONNECTED_BREDR,    ruleTwsTopPriDisableConnectableHandset,    TWSTOP_PRIMARY_GOAL_CONNECTABLE_HANDSET),

    /* Decide what to do after handset connection has been prohibited */
    RULE(TWSTOP_RULE_EVENT_PROHIBIT_CONNECT_TO_HANDSET, ruleTwsTopPriDisconnectHandset, TWSTOP_PRIMARY_GOAL_DISCONNECT_HANDSET),

    /*****************************
     * Handover rules 
     *****************************/
     
    /* After HDMA recommendation, decide whether to initiate Handover. */
    RULE(TWSTOP_RULE_EVENT_HANDOVER, ruleTwsTopHandoverStart, TWSTOP_PRIMARY_GOAL_HANDOVER_START),

    /* Decide what to do when handover failed when earbud entered in case */
    RULE(TWSTOP_RULE_EVENT_HANDOVER_FAILED, ruleTwsTopHandoverFailed, TWSTOP_PRIMARY_GOAL_HANDOVER_FAILED),

    /* Decide what to do after a dynamic handover failure handling */
    RULE(TWSTOP_RULE_EVENT_HANDOVER_FAILURE_HANDLED,ruleTwsTopHandoverFailureHandled,TWSTOP_PRIMARY_GOAL_NO_ROLE_IDLE)
};

/*! Types of event that can initiate a connection rule decision. */
typedef enum
{
    /*! Completion of a role switch. */
    rule_connect_role_switch,
    /*! Earbud taken out of the case. */
    rule_connect_out_of_case,
    /*! Completion of handset pairing. (TWS+) */
    rule_connect_pairing,
    /*! Link loss with handset. */
    rule_connect_linkloss,
} rule_connect_reason_t;

/*****************************************************************************
 * RULES FUNCTIONS
 *****************************************************************************/

static rule_action_t ruleTwsTopPriFindRole(void)
{
    if (appPhyStateGetState() == PHY_STATE_IN_CASE)
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriFindRole, ignore as in case");
        return rule_action_ignore;
    }

    /* if peer pairing is active ignore */
    if (TwsTopology_IsGoalActive(tws_topology_goal_pair_peer))
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriFindRole, ignore as peer pairing active");
        return rule_action_ignore;
    }

    if (TwsTopology_IsGoalActive(tws_topology_goal_dynamic_handover) 
        || TwsTopology_IsGoalActive(tws_topology_goal_dynamic_handover_failure))
    {
        /* Ignore as there is a handover goal running  */
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriFindRole, Ignore as dynamic handover is still in progress ");
        return rule_action_ignore;
    }

    if (TwsTopology_IsGoalActive(tws_topology_goal_primary_static_handover_in_case))
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriFindRole, ignore as static handover (in case) is still in progress ");
        return rule_action_ignore;
    }
    if (TwsTopology_IsGoalActive(tws_topology_goal_primary_static_handover))
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriFindRole, ignore as static handover is still in progress ");
        return rule_action_ignore;
    }

    if (TwsTopology_IsGoalActive(tws_topology_goal_no_role_find_role))
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriFindRole, ignore as no role find role in progress ");
        return rule_action_ignore;
    }

    TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriFindRole, run as not in case");
    return rule_action_run;
}

static rule_action_t ruleTwsTopPriPeerPairedInCase(void)
{
    if (appPhyStateGetState() != PHY_STATE_IN_CASE)
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriPeerPairedInCase, ignore as not in case");
        return rule_action_ignore;
    }

    TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriPeerPairedInCase, run as peer paired and in the case");
    return rule_action_run;
}

static rule_action_t ruleTwsTopPriPeerPairedOutCase(void)
{
    if (appPhyStateGetState() == PHY_STATE_IN_CASE)
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriPeerPairedOutCase, ignore as in case");
        return rule_action_ignore;
    }

    TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriPeerPairedOutCase, run as peer paired and out of case");
    return rule_action_run;
}

static rule_action_t ruleTwsTopPriPairPeer(void)
{
    TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriPairPeer, run");
    return rule_action_run;
}

static rule_action_t ruleTwsTopPriDisableConnectablePeer(void)
{
    const bool disable_connectable = FALSE;
    bdaddr secondary_addr;
    
    if (!appDeviceGetSecondaryBdAddr(&secondary_addr))
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriDisableConnectablePeer, ignore as unknown secondary address");
        return rule_action_ignore;
    }
    if (!ConManagerIsConnected(&secondary_addr))
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriDisableConnectablePeer, ignore as not connected to peer");
        return rule_action_ignore;
    }
    TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriDisableConnectablePeer, run as have connection to secondary peer");
    return RULE_ACTION_RUN_PARAM(disable_connectable);
}

static rule_action_t ruleTwsTopPriEnableConnectablePeer(void)
{
    const bool enable_connectable = TRUE;
    bdaddr secondary_addr;

    if (!appDeviceGetSecondaryBdAddr(&secondary_addr))
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriConnectablePeer, ignore as unknown secondary address");
        return rule_action_ignore;
    }

    if (appPhyStateGetState() == PHY_STATE_IN_CASE)
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriConnectablePeer ignore as in case");
        return rule_action_ignore;
    }
    if (ConManagerIsConnected(&secondary_addr))
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriConnectablePeer ignore as peer connected");
        return rule_action_ignore;
    }

    if (TwsTopology_IsActingPrimary())
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriConnectablePeer ignore as acting primary");
        return rule_action_ignore;
    }

    TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriConnectablePeer, run as out of case and peer not connected");
    return RULE_ACTION_RUN_PARAM(enable_connectable);
}

static rule_action_t ruleTwsTopPriConnectPeerProfiles(void)
{
    uint8 profiles = TwsTopologyConfig_PeerProfiles();

    if (appPhyStateGetState() == PHY_STATE_IN_CASE)
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriConnectPeerProfiles ignore as in case");
        return rule_action_ignore;
    }

    TWSTOP_PRIMARY_RULE_LOGF("ruleTwsTopPriConnectPeerProfiles run as out of case (profiles:x%x)", profiles);
    return RULE_ACTION_RUN_PARAM(profiles);
}

#if 0
/*! \todo Complete this use-case of profile disconnect initiated by user on handset */
static rule_action_t ruleTwsTopPriDisconnectPeerProfiles(void)
{
    /* with no handset link always disconnected media and voice profiles. */
    uint8 profiles = DEVICE_PROFILE_A2DP | DEVICE_PROFILE_SCOFWD;

    /* if this earbud is now in the case then also disconnect peer signalling. */
    if (appPhyStateGetState() == PHY_STATE_IN_CASE)
    {
        profiles += DEVICE_PROFILE_PEERSIG;
    }

    TWSTOP_PRIMARY_RULE_LOGF("ruleTwsTopPriDisconnectPeerProfiles, run and disconnect %x profiles", profiles);
    return RULE_ACTION_RUN_PARAM(profiles);
}
#endif

static rule_action_t ruleTwsTopPriReleasePeer(void)
{
    if (appPhyStateGetState() == PHY_STATE_IN_CASE)
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriReleasePeer run. Device is now in case");
        return rule_action_run;
    }

    TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriReleasePeer ignore. Device not in case (normal)");
    return rule_action_ignore;
}

static rule_action_t ruleTwsTopPriNoRoleIdle(void)
{
    if (appPhyStateGetState() != PHY_STATE_IN_CASE)
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriNoRoleIdle, ignore as out of case");
        return rule_action_ignore;
    }
    if (TwsTopology_IsGoalActive(tws_topology_goal_pair_peer))
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriNoRoleIdle, ignore as peer pairing active");
        return rule_action_ignore;
    }

    if(!PeerFindRole_HasFixedRole())
    {   /* this permits HDMA to react to the IN_CASE and potentially generate a handover event
         * in the first instance */
        if (TwsTopologyGetTaskData()->hdma_created)
        {
            TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriNoRoleIdle, defer as HDMA is Active and will generate handover recommendation shortly");
            return rule_action_defer;
        }
    }

    /* this prevent IN_CASE stopping an in-progress dynamic handover from continuing to run
     * where we've past the point that HDMA has been destroyed */
    if(TwsTopology_IsGoalActive(tws_topology_goal_dynamic_handover) 
       || TwsTopology_IsGoalActive(tws_topology_goal_dynamic_handover_failure))
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriNoRoleIdle, Defer as dynamic handover is Active");
        return rule_action_defer;
    }
    /* these two rules prevent IN_CASE stopping an in-progress static handover from continuing to run
     * where we've past the point that HDMA has been destroyed */
    if (TwsTopology_IsGoalActive(tws_topology_goal_primary_static_handover_in_case))
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriNoRoleIdle, defer already running static handover in case");
        return rule_action_defer;
    }
    if (TwsTopology_IsGoalActive(tws_topology_goal_primary_static_handover))
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriNoRoleIdle, defer already running static handover");
        return rule_action_defer;
    }

    TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriNoRoleIdle, run as primary in case");
    return rule_action_run;
}

static rule_action_t ruleTwsTopPriSelectedPrimary(void)
{
    if (appPhyStateGetState() == PHY_STATE_IN_CASE)
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriSelectedPrimary, ignore as in the case");
        return rule_action_ignore;
    }
    TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriSelectedPrimary, run as selected as Primary out of case");
    return rule_action_run;
}

static rule_action_t ruleTwsTopPriSelectedActingPrimary(void)
{
    if (appPhyStateGetState() == PHY_STATE_IN_CASE)
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriSelectedActingPrimary, ignore as out of case");
        return rule_action_ignore;
    }
    TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriSelectedActingPrimary, run as selected as Acting Primary out of case");
    return rule_action_run;
}

static rule_action_t ruleTwsTopPriNoRoleSelectedSecondary(void)
{
    if (appPhyStateGetState() == PHY_STATE_IN_CASE)
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriNoRoleSelectedSecondary, ignore as in case");
        return rule_action_ignore;
    }

    if (TwsTopology_GetRole() != tws_topology_role_none)
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriNoRoleSelectedSecondary, ignore as already have role");
        return rule_action_ignore;
    }

    TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriNoRoleSelectedSecondary, run as selected as Secondary out of case");
    return rule_action_run;
}

static rule_action_t ruleTwsTopPriPrimarySelectedSecondary(void)
{
    if (appPhyStateGetState() == PHY_STATE_IN_CASE)
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriPrimarySelectedSecondary, ignore as in case");
        return rule_action_ignore;
    }

    if (TwsTopology_GetRole() != tws_topology_role_primary)
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriPrimarySelectedSecondary, ignore as not primary");
        return rule_action_ignore;
    }

    TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriPrimarySelectedSecondary, run as Primary out of case");
    return rule_action_run;
}

static rule_action_t ruleTwsTopPriPeerLostFindRole(void)
{
    bdaddr secondary_addr;

    if (appPhyStateGetState() == PHY_STATE_IN_CASE)
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriPeerLostFindRole, ignore as in case");
        return rule_action_ignore;
    }

    if (TwsTopology_GetRole() != tws_topology_role_primary)
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriPeerLostFindRole, ignore as not primary");
        return rule_action_ignore;
    }

    if (   TwsTopology_IsGoalActive(tws_topology_goal_no_role_idle)
        || TwsTopology_IsGoalActive(tws_topology_goal_no_role_find_role))
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriPeerLostFindRole, defer as switching role");
        return rule_action_defer;
    }
    
    if (TwsTopology_IsGoalActive(tws_topology_goal_primary_static_handover))
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriPeerLostFindRole, ignore as static handover (out of case) active");
        return rule_action_ignore;
    }
        
    if (TwsTopology_IsGoalActive(tws_topology_goal_primary_static_handover_in_case))
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriPeerLostFindRole, ignore as static handover in case still active");
        return rule_action_ignore;
    }

    if (!appDeviceGetSecondaryBdAddr(&secondary_addr))
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriPeerLostFindRole, ignore as unknown secondary address");
        return rule_action_ignore;
    }
    if (ConManagerIsConnected(&secondary_addr))
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriPeerLostFindRole, ignore as still connected to secondary");
        return rule_action_ignore;
    }
    TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriPeerLostFindRole, run as Primary out of case, and not connected to secondary");
    return rule_action_run;
}

static rule_action_t ruleTwsTopPriCancelFindRole(void)
{
    if (TwsTopology_IsGoalActive(tws_topology_goal_primary_static_handover_in_case))
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriCancelFindRole, ignore already running static handover in case");
        return rule_action_ignore;
    }

    if (appPhyStateGetState() != PHY_STATE_IN_CASE)
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriCancelFindRole, ignore as not in case");
        return rule_action_ignore;
    }
    
    /*! \todo add check that find role is actually running */

    TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriCancelFindRole, run as in case");
    return rule_action_run;
}

static rule_action_t ruleTwsTopPriEnableConnectableHandset(void)
{
    bdaddr handset_addr;
    const TWSTOP_PRIMARY_GOAL_CONNECTABLE_HANDSET_T enable_connectable = {.enable = TRUE};

    if (   TwsTopology_IsGoalActive(tws_topology_goal_primary_static_handover)
        || TwsTopology_IsGoalActive(tws_topology_goal_primary_static_handover_in_case))
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriEnableConnectableHandset, ignore as static handover (out of case) active");
        return rule_action_ignore;
    }

    if (!appDeviceGetHandsetBdAddr(&handset_addr))
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriEnableConnectableHandset, ignore as not paired with handset");
        return rule_action_ignore;
    }

    if (ConManagerIsConnected(&handset_addr))
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriEnableConnectableHandset, ignore as connected to handset");
        return rule_action_ignore;
    }

    if (appPhyStateGetState() == PHY_STATE_IN_CASE)
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriEnableConnectableHandset, ignore as in case ");
        return rule_action_ignore;
    }

    if (TwsTopology_GetRole() != tws_topology_role_primary)
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriEnableConnectableHandset, ignore as role is not primary");
        return rule_action_ignore;
    }

    TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriEnableConnectableHandset, run as primary out of case not connected to handset");
    return RULE_ACTION_RUN_PARAM(enable_connectable);
}

static rule_action_t ruleTwsTopPriEnableLeConnectableHandset(void)
{
    bdaddr handset_addr;
    const TWSTOP_PRIMARY_GOAL_LE_CONNECTABLE_HANDSET_T enable_le_adverts =
        {.enable = TRUE};

    if (appPhyStateGetState() == PHY_STATE_IN_CASE)
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriEnableLeConnectableHandset, ignore as in case ");
        return rule_action_ignore;
    }

    TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriEnableLeConnectableHandset, run as primary out of case not connected to handset");
    return RULE_ACTION_RUN_PARAM(enable_le_adverts);
}

static rule_action_t ruleTwsTopPriDisableConnectableHandset(void)
{
    const TWSTOP_PRIMARY_GOAL_CONNECTABLE_HANDSET_T disable_connectable = {.enable = FALSE};
    bdaddr handset_addr;
    
    if (!appDeviceGetHandsetBdAddr(&handset_addr))
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriDisableConnectableHandset, ignore as not paired with handset");
        return rule_action_ignore;
    }
    if (!ConManagerIsConnected(&handset_addr))
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriDisableConnectableHandset, ignore as not connected to handset");
        return rule_action_ignore;
    }
    TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriDisableConnectableHandset, run as have connection to handset");
    return RULE_ACTION_RUN_PARAM(disable_connectable);
}

static rule_action_t ruleTwsTopPriConnectHandset(rule_connect_reason_t reason)
{
    bdaddr handset_addr;

    TWSTOP_PRIMARY_RULE_LOGF("ruleTwsTopPriConnectHandset, reason %u", reason);

    if (appPhyStateGetState() == PHY_STATE_IN_CASE)
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriConnectHandset, ignore as in case");
        return rule_action_ignore;
    }

    if (!appDeviceGetHandsetBdAddr(&handset_addr))
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriConnectHandset, ignore as not paired with handset");
        return rule_action_ignore;
    }

    if(TwsTopologyGetTaskData()->prohibit_connect_to_handset)
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriConnectHandset, ignore as handset connection disabled");
        return rule_action_ignore;
    }

    if (   (reason != rule_connect_linkloss)
        &&
           (   appDeviceIsHandsetA2dpConnected()
            || appDeviceIsHandsetAvrcpConnected()
            || appDeviceIsHandsetHfpConnected()))
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriConnectHandset, ignore as already connected to handset");
        return rule_action_ignore;
    }

    /*! \todo ignore if pairing? */
    /*! \todo ignore if DFU in progress? */
    /*! \todo ignore if peer pairing? */

    if (   BtDevice_GetWasConnected(&handset_addr)
        || (reason == rule_connect_out_of_case))
    {
        device_t handset_device = DeviceList_GetFirstDeviceWithPropertyValue(device_property_bdaddr, &handset_addr, sizeof(bdaddr));
        uint8 profiles = BtDevice_GetLastConnectedProfilesForDevice(handset_device);

        /* always connect HFP and A2DP if out of case or pairing connect */
        if (   (reason == rule_connect_out_of_case)
            || (reason == rule_connect_pairing))
        {
            profiles |= DEVICE_PROFILE_HFP | DEVICE_PROFILE_A2DP;
        }
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriConnectHandset, run as handset we were connected to before");
        return RULE_ACTION_RUN_PARAM(profiles);
    }
    else
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriConnectHandset, ignored as wasn't connected before");
        return rule_action_ignore;
    }
}

static rule_action_t ruleTwsTopPriRoleSwitchConnectHandset(void)
{
    /*! \todo add clause to ignore if no paired handset */

    TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriRoleSwitchConnectHandset");
    return ruleTwsTopPriConnectHandset(rule_connect_role_switch);
}

static rule_action_t ruleTwsTopPriOutCaseConnectHandset(void)
{
    TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriOutCaseConnectHandset");
    return ruleTwsTopPriConnectHandset(rule_connect_out_of_case);
}

static rule_action_t ruleTwsTopPriHandsetLinkLossReconnect(void)
{
    TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriHandsetLinkLossReconnect");
    return ruleTwsTopPriConnectHandset(rule_connect_linkloss);
}

static rule_action_t ruleTwsTopPriDisconnectHandset(void)
{
    TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriDisconnectHandset");
    return rule_action_run;
}

static rule_action_t ruleTwsTopPriInCaseDisconnectHandset(void)
{
    if (appPhyStateGetState() != PHY_STATE_IN_CASE)
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriInCaseDisconnectHandset, ignore as not in case");
        return rule_action_ignore;
    }

    if (ScoFwdIsSending())
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriInCaseDisconnectHandset, ignore as we're forwarding SCO to secondary");
        return rule_action_ignore;
    }

    if (!appDeviceIsHandsetAnyProfileConnected())
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriInCaseDisconnectHandset, ignore as not connected to handset");
        return rule_action_ignore;
    }

    /*! \todo don't disconnect if DFU pending */

    TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriInCaseDisconnectHandset, run as in case");
    return rule_action_run;
}

/*! Decide whether to allow handset BR/EDR connections */
static rule_action_t ruleTwsTopPriAllowHandsetConnect(void)
{
    const bool allow_connect = TRUE;

    if (appPhyStateGetState() == PHY_STATE_IN_CASE)
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriAllowHandsetConnect, ignore as in case");
        return rule_action_ignore;
    }

    /* If role is not any kind of primary don't allow handsets to connect */
    if (!TwsTopology_IsPrimary())
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriAllowHandsetConnect, ignore as not a primary role");
        return rule_action_ignore;
    }

    TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriAllowHandsetConnect, run as primary out of case");
    return RULE_ACTION_RUN_PARAM(allow_connect);
}


/*! Decide whether the use of the DFU role is permitted 

    The initial implementation of this rule works on the basis of selecting the 
    role regardless. 

    The rationale for this
    \li Normal topology rules should not care about DFU details
    \li Keeps the application state machine and topology synchronised

    This may be re-addressed.
*/
static rule_action_t ruleTwsTopPriSwitchToDfuRole(void)
{
    TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriSwitchToDfuRole, run unconditionally");

    return rule_action_run;
}

/*! Decide whether to run the handover now. 

    The implementation of this rule works on the basis of the following:

     a) Handover is allowed by application now.
     b) No active goals executing.
*/
static rule_action_t ruleTwsTopHandoverStart(void)
{
    TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopHandoverStart");

    /* Check if Handover is allowed by application */
    if(TwsTopologyGetTaskData()->app_prohibit_handover)
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopHandoverStart, Ignored as App has blocked ");
        return rule_action_ignore;
    }

    if(PeerFindRole_HasFixedRole())
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopHandoverStart, Ignored as have a fixed role ");
        return rule_action_ignore;
    }

    if (TwsTopology_IsGoalActive(tws_topology_goal_dynamic_handover) 
         || TwsTopology_IsGoalActive(tws_topology_goal_dynamic_handover_failure))
    {
        /* Ignore any further handover requests as there is already one in progress */
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopHandoverStart, Ignored as dynamic handover is still in progress ");
        return rule_action_ignore;
    }

    /* Run the rule now */
    return rule_action_run;
}

/*! A previous handover failed. Run failed rule. */ 
static rule_action_t ruleTwsTopHandoverFailed(void)
{
    TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopHandoverFailed");

    /* Run the rule now */
    return rule_action_run;
}

/*! A previous handover failure has been handled. Run failure handled rule. */ 
static rule_action_t ruleTwsTopHandoverFailureHandled(void)
{
    TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopHandoverFailureHandled");

    if (appPhyStateGetState() != PHY_STATE_IN_CASE)
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopHandoverFailureHandled, ignore as not in case anymore");
        return rule_action_ignore;
    }
    /* Run the rule now */
    return rule_action_run;

}


/*****************************************************************************
 * END RULES FUNCTIONS
 *****************************************************************************/

/*! \brief Initialise the primary rules module. */
bool TwsTopologyPrimaryRules_Init(Task init_task)
{
    TwsTopologyPrimaryRulesTaskData *primary_rules = TwsTopologyPrimaryRulesGetTaskData();
    rule_set_init_params_t rule_params;

    UNUSED(init_task);

    memset(&rule_params, 0, sizeof(rule_params));
    rule_params.rules = twstop_primary_rules_set;
    rule_params.rules_count = ARRAY_DIM(twstop_primary_rules_set);
    rule_params.nop_message_id = TWSTOP_PRIMARY_GOAL_NOP;
    primary_rules->rule_set = RulesEngine_CreateRuleSet(&rule_params);

    return TRUE;
}

rule_set_t TwsTopologyPrimaryRules_GetRuleSet(void)
{
    TwsTopologyPrimaryRulesTaskData *primary_rules = TwsTopologyPrimaryRulesGetTaskData();
    return primary_rules->rule_set;
}

void TwsTopologyPrimaryRules_SetEvent(Task client_task, rule_events_t event_mask)
{
    TwsTopologyPrimaryRulesTaskData *primary_rules = TwsTopologyPrimaryRulesGetTaskData();
    RulesEngine_SetEvent(primary_rules->rule_set, client_task, event_mask);
}

void TwsTopologyPrimaryRules_ResetEvent(rule_events_t event)
{
    TwsTopologyPrimaryRulesTaskData *primary_rules = TwsTopologyPrimaryRulesGetTaskData();
    RulesEngine_ResetEvent(primary_rules->rule_set, event);
}

rule_events_t TwsTopologyPrimaryRules_GetEvents(void)
{
    TwsTopologyPrimaryRulesTaskData *primary_rules = TwsTopologyPrimaryRulesGetTaskData();
    return RulesEngine_GetEvents(primary_rules->rule_set);
}

void TwsTopologyPrimaryRules_SetRuleComplete(MessageId message)
{
    TwsTopologyPrimaryRulesTaskData *primary_rules = TwsTopologyPrimaryRulesGetTaskData();
    RulesEngine_SetRuleComplete(primary_rules->rule_set, message);
}

void TwsTopologyPrimaryRules_SetRuleWithEventComplete(MessageId message, rule_events_t event)
{
    TwsTopologyPrimaryRulesTaskData *primary_rules = TwsTopologyPrimaryRulesGetTaskData();
    RulesEngine_SetRuleWithEventComplete(primary_rules->rule_set, message, event);
}

/*! \brief Copy rule param data for the engine to put into action messages.
    \param param Pointer to data to copy.
    \param size_param Size of the data in bytes.
    \return rule_action_run_with_param to indicate the rule action message needs parameters.
 */
static rule_action_t twsTopologyPrimaryRules_CopyRunParams(const void* param, size_t size_param)
{
    TwsTopologyPrimaryRulesTaskData *primary_rules = TwsTopologyPrimaryRulesGetTaskData();
    RulesEngine_CopyRunParams(primary_rules->rule_set, param, size_param);
    return rule_action_run_with_param;
}
