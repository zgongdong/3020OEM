/*!
\copyright  Copyright (c) 2005 - 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      
*/

#include "tws_topology_secondary_rules.h"

#include <bt_device.h>
#include <phy_state.h>
#include <rules_engine.h>
#include <connection_manager.h>

#include <logging.h>

#pragma unitsuppress Unused

/*! \{
    Macros for diagnostic output that can be suppressed.
    Allows debug of the rules module at two levels. */
#define TWSTOP_SECONDARY_RULES_LOG(x)       //DEBUG_LOG(x)
#define TWSTOP_SECONDARY_RULES_LOGF(x, ...) //DEBUG_LOGF(x, __VA_ARGS__)

#define TWSTOP_SECONDARY_RULE_LOG(x)         DEBUG_LOG(x)
#define TWSTOP_SECONDARY_RULE_LOGF(x, ...)   DEBUG_LOGF(x, __VA_ARGS__)
/*! \} */

/* Forward declaration for use in RULE_ACTION_RUN_PARAM macro below */
static rule_action_t twsTopologySecondaryRules_CopyRunParams(const void* param, size_t size_param);

/*! \brief Macro used by rules to return RUN action with parameters to return.
    Copies the parameters/data into conn_rules where the rules engine can uses
    it when building the action message.
*/
#define RULE_ACTION_RUN_PARAM(x)   twsTopologySecondaryRules_CopyRunParams(&(x), sizeof(x))

/*! Get pointer to the connection rules task data structure. */
#define TwsTopologySecondaryRulesGetTaskData()  (&tws_topology_secondary_rules_task_data)

TwsTopologySecondaryRulesTaskData tws_topology_secondary_rules_task_data;

/*! \name Rule function prototypes

    These allow us to build the rule table twstop_secondary_rules_set. */
/*! @{ */
DEFINE_RULE(ruleTwsTopSecPeerLostFindRole);
DEFINE_RULE(ruleTwsTopSecRoleSwitchPeerConnect);
DEFINE_RULE(ruleTwsTopSecNoRoleIdle);
DEFINE_RULE(ruleTwsTopSecSwitchToDfuRole);
DEFINE_RULE(ruleTwsTopSecFailedConnectFindRole);
DEFINE_RULE(ruleTwsTopSecStaticHandoverCommand);
DEFINE_RULE(ruleTwsTopSecStaticHandoverFailedOutCase);
/*! @} */

/*! \brief TWS Topology rules deciding behaviour in a Secondary role. */
const rule_entry_t twstop_secondary_rules_set[] =
{
    /* On linkloss with Primary Earbud, decide if Secondary should run role selection. */
    RULE(TWSTOP_RULE_EVENT_PEER_LINKLOSS,   ruleTwsTopSecPeerLostFindRole,  TWSTOP_SECONDARY_GOAL_NO_ROLE_FIND_ROLE),

    /* On BREDR disconnect from Primary Earbud, decide if Secondary should run role selection. */
    RULE(TWSTOP_RULE_EVENT_PEER_DISCONNECTED_BREDR,   ruleTwsTopSecPeerLostFindRole,  TWSTOP_SECONDARY_GOAL_NO_ROLE_FIND_ROLE),

    /* If failed to connect the ACL to the Primary, decide if Secondary should return to no role and restart role selection. */
    RULE(TWSTOP_RULE_EVENT_FAILED_PEER_CONNECT,   ruleTwsTopSecFailedConnectFindRole,  TWSTOP_SECONDARY_GOAL_NO_ROLE_FIND_ROLE),

    /* After switching to secondary role, decide if Secondary Earbud should create BREDR link to Primary Earbud. */
    RULE(TWSTOP_RULE_EVENT_ROLE_SWITCH,     ruleTwsTopSecRoleSwitchPeerConnect, TWSTOP_SECONDARY_GOAL_CONNECT_PEER),

    /* When Earbud put in the case, decide if it should drop links and transition to no role */
    RULE(TWSTOP_RULE_EVENT_IN_CASE,         ruleTwsTopSecNoRoleIdle,            TWSTOP_SECONDARY_GOAL_NO_ROLE_IDLE),

    /* When requested to select DFU rules, select them */
    RULE(TWSTOP_RULE_EVENT_DFU_ROLE,        ruleTwsTopSecSwitchToDfuRole,       TWSTOP_SECONDARY_GOAL_DFU_ROLE),
    
    /* When commanded to role switch by Primary, decide if we should do it */
    RULE(TWSTOP_RULE_EVENT_STATIC_HANDOVER, ruleTwsTopSecStaticHandoverCommand, TWSTOP_SECONDARY_GOAL_STATIC_HANDOVER),

    /* If role switch fails, and out of the case start role selection */
    RULE(TWSTOP_RULE_EVENT_STATIC_HANDOVER_FAILED, ruleTwsTopSecStaticHandoverFailedOutCase, TWSTOP_SECONDARY_GOAL_NO_ROLE_FIND_ROLE),

    /*! \todo Handset connect after commanded by peer - TWS+ Secondary rule */
};

/*****************************************************************************
 * RULES FUNCTIONS
 *****************************************************************************/

/*! \brief Rule to decide if Secondary should start role selection on peer linkloss. */
static rule_action_t ruleTwsTopSecPeerLostFindRole(void)
{
    if (TwsTopology_IsGoalActive(tws_topology_goal_secondary_static_handover))
    {
        TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecPeerLostFindRole, ignore as already running secondary static handover");
        return rule_action_ignore;
    }
    if (appPhyStateGetState() == PHY_STATE_IN_CASE)
    {
        TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecPeerLostFindRole, ignore as in case");
        return rule_action_ignore;
    }
    TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecPeerLostFindRole, run as out of case");
    return rule_action_run;
}

/*! \brief Rule to decide if Secondary should connect to Primary. */
static rule_action_t ruleTwsTopSecRoleSwitchPeerConnect(void)
{
    bdaddr primary_addr;
    
    if (!appDeviceGetPrimaryBdAddr(&primary_addr))
    {
        TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecRoleSwitchPeerConnect, ignore as unknown primary address");
        return rule_action_ignore;
    }

    if (appPhyStateGetState() == PHY_STATE_IN_CASE)
    {
        TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecRoleSwitchPeerConnect, ignore as in case");
        return rule_action_ignore;
    }

    if (ConManagerIsConnected(&primary_addr))
    {
        TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecRoleSwitchPeerConnect, ignore as peer already connected");
        return rule_action_ignore;
    }

    TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecRoleSwitchPeerConnect, run as secondary out of case and peer not connected");
    return rule_action_run;
}

static rule_action_t ruleTwsTopSecNoRoleIdle(void)
{
    if (appPhyStateGetState() != PHY_STATE_IN_CASE)
    {
        TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecNoRoleIdle, ignore as out of case");
        return rule_action_ignore;
    }
    TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecNoRoleIdle, run as secondary in case");
    return rule_action_run;
}

/*! Decide whether the use of the DFU role is permitted 

    The initial implementation of this rule works on the basis of selecting the 
    role regardless. 

    The rationale for this
    \li Normal topology rules should not care about DFU details
    \li Keeps the application state machine and topology synchronised

    This may be re-addressed.
*/
static rule_action_t ruleTwsTopSecSwitchToDfuRole(void)
{
    TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecSwitchToDfuRole, run unconditionally");

    return rule_action_run;
}


static rule_action_t ruleTwsTopSecFailedConnectFindRole(void)
{
    bdaddr primary_addr;

    if (appPhyStateGetState() == PHY_STATE_IN_CASE)
    {
        TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecFailedConnectFindRole, ignore as in the case");
        return rule_action_ignore;
    }
    
    if (!appDeviceGetPrimaryBdAddr(&primary_addr))
    {
        TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecFailedConnectFindRole, ignore as unknown primary address");
        return rule_action_ignore;
    }

    if (ConManagerIsConnected(&primary_addr))
    {
        TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecFailedConnectFindRole, ignore as peer already connected");
        return rule_action_ignore;
    }

    TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecFailedConnectFindRole, run as secondary out of case with no peer link");
    return rule_action_run;
}

static rule_action_t ruleTwsTopSecStaticHandoverCommand(void)
{
    if (appPhyStateGetState() == PHY_STATE_IN_CASE)
    {
        TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecStaticHandoverCommand, ignore as in the case");
        return rule_action_ignore;
    }

    if(PeerFindRole_HasFixedRole())
    {
        TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecStaticHandoverCommand, ignore as have a fixed role");
        return rule_action_ignore;
    }

    TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecStaticHandoverCommand, run as out of case");
    return rule_action_run;
}

static rule_action_t ruleTwsTopSecStaticHandoverFailedOutCase(void)
{
    if (appPhyStateGetState() == PHY_STATE_IN_CASE)
    {
        TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecStaticHandoverFailedOutCase, ignore as in the case");
        return rule_action_ignore;
    }
    
    TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecStaticHandoverFailedOutCase, run as out of case");
    return rule_action_run;
}

/*****************************************************************************
 * END RULES FUNCTIONS
 *****************************************************************************/

bool TwsTopologySecondaryRules_Init(Task init_task)
{
    TwsTopologySecondaryRulesTaskData *secondary_rules = TwsTopologySecondaryRulesGetTaskData();
    rule_set_init_params_t rule_params;

    UNUSED(init_task);

    memset(&rule_params, 0, sizeof(rule_params));
    rule_params.rules = twstop_secondary_rules_set;
    rule_params.rules_count = ARRAY_DIM(twstop_secondary_rules_set);
    rule_params.nop_message_id = TWSTOP_SECONDARY_GOAL_NOP;
    secondary_rules->rule_set = RulesEngine_CreateRuleSet(&rule_params);

    return TRUE;
}

rule_set_t TwsTopologySecondaryRules_GetRuleSet(void)
{
    TwsTopologySecondaryRulesTaskData *secondary_rules = TwsTopologySecondaryRulesGetTaskData();
    return secondary_rules->rule_set;
}

void TwsTopologySecondaryRules_SetEvent(Task client_task, rule_events_t event_mask)
{
    TwsTopologySecondaryRulesTaskData *secondary_rules = TwsTopologySecondaryRulesGetTaskData();
    RulesEngine_SetEvent(secondary_rules->rule_set, client_task, event_mask);
}

void TwsTopologySecondaryRules_ResetEvent(rule_events_t event)
{
    TwsTopologySecondaryRulesTaskData *secondary_rules = TwsTopologySecondaryRulesGetTaskData();
    RulesEngine_ResetEvent(secondary_rules->rule_set, event);
}

rule_events_t TwsTopologySecondaryRules_GetEvents(void)
{
    TwsTopologySecondaryRulesTaskData *secondary_rules = TwsTopologySecondaryRulesGetTaskData();
    return RulesEngine_GetEvents(secondary_rules->rule_set);
}

void TwsTopologySecondaryRules_SetRuleComplete(MessageId message)
{
    TwsTopologySecondaryRulesTaskData *secondary_rules = TwsTopologySecondaryRulesGetTaskData();
    RulesEngine_SetRuleComplete(secondary_rules->rule_set, message);
}

void TwsTopologySecondaryRules_SetRuleWithEventComplete(MessageId message, rule_events_t event)
{
    TwsTopologySecondaryRulesTaskData *secondary_rules = TwsTopologySecondaryRulesGetTaskData();
    RulesEngine_SetRuleWithEventComplete(secondary_rules->rule_set, message, event);
}

/*! \brief Copy rule param data for the engine to put into action messages.
    \param param Pointer to data to copy.
    \param size_param Size of the data in bytes.
    \return rule_action_run_with_param to indicate the rule action message needs parameters.
 */
static rule_action_t twsTopologySecondaryRules_CopyRunParams(const void* param, size_t size_param)
{
    TwsTopologySecondaryRulesTaskData *secondary_rules = TwsTopologySecondaryRulesGetTaskData();
    RulesEngine_CopyRunParams(secondary_rules->rule_set, param, size_param);
    return rule_action_run_with_param;
}
