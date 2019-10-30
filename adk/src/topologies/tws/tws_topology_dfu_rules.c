/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Implementation for rules used when DFU is pending or in progress

            \implementation The rule set behaviour changes based on whether the 
            current device is using the primary or secondary address. This 
            can be observed in the rule table as the same input event will often 
            have two entries. Only one will fire as the functions used to select
            the rule will only activate for primary/secondary address.
*/

#include "tws_topology_dfu_rules.h"

#include <bt_device.h>
#include <phy_state.h>
#include <rules_engine.h>
#include <connection_manager.h>

#include <logging.h>

#pragma unitsuppress Unused

/*! \{
    Macros for diagnostic output that can be suppressed.
    Allows debug of the rules module at two levels. */
#define TWSTOP_DFU_RULES_LOG(x)       //DEBUG_LOG(x)
#define TWSTOP_DFU_RULES_LOGF(x, ...) //DEBUG_LOGF(x, __VA_ARGS__)

#define TWSTOP_DFU_RULE_LOG(x)         DEBUG_LOG(x)
#define TWSTOP_DFU_RULE_LOGF(x, ...)   DEBUG_LOGF(x, __VA_ARGS__)
/*! \} */

/* Forward declaration for use in RULE_ACTION_RUN_PARAM macro below */
static rule_action_t twsTopologyDfuRules_CopyRunParams(const void* param, size_t size_param);

/*! \brief Macro used by rules to return RUN action with parameters to return.
    Copies the parameters/data into conn_rules where the rules engine can uses
    it when building the action message.
*/
#define RULE_ACTION_RUN_PARAM(x)   twsTopologyDfuRules_CopyRunParams(&(x), sizeof(x))

/*! Get pointer to the connection rules task data structure. */
#define TwsTopologyDfuRulesGetTaskData()  (&tws_topology_dfu_rules_task_data)

/*! Storage for the data used by the DFU rules task. */
TwsTopologyDfuRulesTaskData tws_topology_dfu_rules_task_data;

/*! \{
    Rule function prototypes, so we can build the rule tables below. */
DEFINE_RULE(ruleTwsTopDfuInCase);
DEFINE_RULE(ruleTwsTopDfuAlways);
DEFINE_RULE(ruleTwsTopOutOfCase);
DEFINE_RULE(ruleTwsTopInCase);
DEFINE_RULE(ruleTwsTopDfuPrimary);
DEFINE_RULE(ruleTwsTopDfuSecondary);
DEFINE_RULE(ruleTwsTopDfuSecondaryLinkLoss);
DEFINE_RULE(ruleTwsTopDfuEnableConnectableHandset);
/*! \} */

/*! \brief TWS Topology rules deciding behaviour in a Dfu role. */
const rule_entry_t twstop_dfu_rules_set[] =
{
    /* When Earbud is put dfu in case mode, disconnect HFP */
    RULE(TWSTOP_RULE_EVENT_IN_CASE,          ruleTwsTopDfuInCase,             TWSTOP_DFU_GOAL_IN_CASE),

    /* When Earbud put out of the case, terminate DFU */
    RULE(TWSTOP_RULE_EVENT_OUT_CASE,         ruleTwsTopDfuAlways,            TWSTOP_DFU_GOAL_NO_ROLE_FIND_ROLE),

    /* When a DFU is ended, for whatever reason, leave DFU and find role if out of case */
    RULE(TWSTOP_RULE_EVENT_DFU_ROLE_COMPLETE,ruleTwsTopOutOfCase,            TWSTOP_DFU_GOAL_NO_ROLE_FIND_ROLE),

    /* When a DFU is ended, for whatever reason, change role if in case */
    RULE(TWSTOP_RULE_EVENT_DFU_ROLE_COMPLETE,ruleTwsTopInCase,               TWSTOP_DFU_GOAL_NO_ROLE_IDLE),

    /* Move to correct state when restarted during a DFU and had primary role */
    RULE(TWSTOP_RULE_EVENT_ROLE_SELECTED_PRIMARY, ruleTwsTopDfuPrimary,      TWSTOP_DFU_GOAL_PRIMARY),

    /* On linkloss with Primary Earbud, decide if Secondary should run connect peer. */
    RULE(TWSTOP_RULE_EVENT_PEER_LINKLOSS, ruleTwsTopDfuSecondaryLinkLoss,  TWSTOP_DFU_GOAL_LINKLOSS_SECONDARY),

    /* Move to correct state when restarted during a DFU and had secondary role */
    RULE(TWSTOP_RULE_EVENT_ROLE_SELECTED_SECONDARY, ruleTwsTopDfuSecondary,  TWSTOP_DFU_GOAL_SECONDARY),

    /*! When handset BREDR link is disconnected, enable page scan on Primary Earbud. */
    RULE(TWSTOP_RULE_EVENT_HANDSET_DISCONNECTED_BREDR, ruleTwsTopDfuEnableConnectableHandset, TWSTOP_DFU_GOAL_CONNECTABLE_HANDSET),
};

/*****************************************************************************
 * RULES FUNCTIONS
 *****************************************************************************/

/*! \brief Rule that runs so long as DFU In Case mode is still there

    Running this rule set is a key indicator that we are in DFU mode */
static rule_action_t ruleTwsTopDfuEnableConnectableHandset(void)
{
    bdaddr handset_addr;
    const bool enable_connectable = TRUE;

    TWSTOP_DFU_RULE_LOG("ruleTwsTopPriEnableConnectableHandset, run as primary dfu in case and not connected to handset");
    return RULE_ACTION_RUN_PARAM(enable_connectable);
}

/*! \brief Rule that runs so long as DFU In Case mode is still there 

    Running this rule set is a key indicator that we are in DFU In Case mode */
static rule_action_t ruleTwsTopDfuInCase(void)
{
    TWSTOP_DFU_RULE_LOG("ruleTwsTopDfuInCase, DFU IN CASE EVENT RUN Always");

    return rule_action_run;
}


/*! \brief Rule that runs so long as DFU mode is still selected 

    Running this rule set is a key indicator that we are in DFU mode */
static rule_action_t ruleTwsTopDfuAlways(void)
{
    TWSTOP_DFU_RULE_LOG("ruleTwsTopDfuAlways, run Always");

    return rule_action_run;
}


/*! \brief Rule that runs if out of the case */
static rule_action_t ruleTwsTopOutOfCase(void)
{

    if (!appPhyStateIsOutOfCase())
    {
        TWSTOP_DFU_RULE_LOG("ruleTwsTopOutOfCase, ignore as still in case");
        return rule_action_ignore;
    }

    TWSTOP_DFU_RULE_LOG("ruleTwsTopOutOfCase, run as out of case");
    return rule_action_run;
}


/*! \brief Rule that runs if out of the case */
static rule_action_t ruleTwsTopInCase(void)
{

    if (appPhyStateIsOutOfCase())
    {
        TWSTOP_DFU_RULE_LOG("ruleTwsTopInCase, ignore as out of case");
        return rule_action_ignore;
    }

    TWSTOP_DFU_RULE_LOG("ruleTwsTopInCase, run as in case");
    return rule_action_run;
}


static rule_action_t ruleTwsTopDfuPrimary(void)
{
    TWSTOP_DFU_RULE_LOG("ruleTwsTopDfuPrimary, run always");

    return rule_action_run;
}


static rule_action_t ruleTwsTopDfuSecondary(void)
{
    TWSTOP_DFU_RULE_LOG("ruleTwsTopDfuSecondary, run always");

    return rule_action_run;
}

static rule_action_t ruleTwsTopDfuSecondaryLinkLoss(void)
{
    bdaddr primary_addr;
    bdaddr bd_addr;

    appDeviceGetMyBdAddr(&bd_addr);

    if(appDeviceIsPrimary(&bd_addr))
    {
        TWSTOP_DFU_RULE_LOG("ruleTwsTopDfuSecondaryLinkLoss, ignore as primary address");
        return rule_action_ignore;
    }

    if (!appDeviceGetPrimaryBdAddr(&primary_addr))
    {
        TWSTOP_DFU_RULE_LOG("ruleTwsTopDfuSecondaryLinkLoss, ignore as unknown primary address");
        return rule_action_ignore;
    }

    if (appPhyStateGetState() != PHY_STATE_IN_CASE)
    {
        TWSTOP_DFU_RULE_LOG("ruleTwsTopDfuSecondaryLinkLoss, ignore as NOT in case");
        return rule_action_ignore;
    }

    if (ConManagerIsConnected(&primary_addr))
    {
        TWSTOP_DFU_RULE_LOG("ruleTwsTopDfuSecondaryLinkLoss, ignore as peer already connected");
        return rule_action_ignore;
    }

    TWSTOP_DFU_RULE_LOG("ruleTwsTopDfuSecondaryLinkLoss, run as secondary in case during DFU and peer got disconnected");

    return rule_action_run;
}


/*****************************************************************************
 * END RULES FUNCTIONS
 *****************************************************************************/

void TwsTopologyDfuRules_Init(void)
{
    TwsTopologyDfuRulesTaskData *dfu_rules = TwsTopologyDfuRulesGetTaskData();
    rule_set_init_params_t rule_params;

    memset(&rule_params, 0, sizeof(rule_params));
    rule_params.rules = twstop_dfu_rules_set;
    rule_params.rules_count = ARRAY_DIM(twstop_dfu_rules_set);
    rule_params.nop_message_id = TWSTOP_DFU_GOAL_NOP;
    dfu_rules->rule_set = RulesEngine_CreateRuleSet(&rule_params);
}

rule_set_t TwsTopologyDfuRules_GetRuleSet(void)
{
    TwsTopologyDfuRulesTaskData *dfu_rules = TwsTopologyDfuRulesGetTaskData();
    return dfu_rules->rule_set;
}

void TwsTopologyDfuRules_SetEvent(Task client_task, rule_events_t event_mask)
{
    TwsTopologyDfuRulesTaskData *dfu_rules = TwsTopologyDfuRulesGetTaskData();
    RulesEngine_SetEvent(dfu_rules->rule_set, client_task, event_mask);
}

void TwsTopologyDfuRules_ResetEvent(rule_events_t event)
{
    TwsTopologyDfuRulesTaskData *dfu_rules = TwsTopologyDfuRulesGetTaskData();
    RulesEngine_ResetEvent(dfu_rules->rule_set, event);
}

rule_events_t TwsTopologyDfuRules_GetEvents(void)
{
    TwsTopologyDfuRulesTaskData *dfu_rules = TwsTopologyDfuRulesGetTaskData();
    return RulesEngine_GetEvents(dfu_rules->rule_set);
}

void TwsTopologyDfuRules_SetRuleComplete(MessageId message)
{
    TwsTopologyDfuRulesTaskData *dfu_rules = TwsTopologyDfuRulesGetTaskData();
    RulesEngine_SetRuleComplete(dfu_rules->rule_set, message);
}

void TwsTopologyDfuRules_SetRuleWithEventComplete(MessageId message, rule_events_t event)
{
    TwsTopologyDfuRulesTaskData *dfu_rules = TwsTopologyDfuRulesGetTaskData();
    RulesEngine_SetRuleWithEventComplete(dfu_rules->rule_set, message, event);
}

/*! \brief Copy rule param data for the engine to put into action messages.
    \param param Pointer to data to copy.
    \param size_param Size of the data in bytes.
    \return rule_action_run_with_param to indicate the rule action message needs parameters.
 */
static rule_action_t twsTopologyDfuRules_CopyRunParams(const void* param, size_t size_param)
{
    TwsTopologyDfuRulesTaskData *dfu_rules = TwsTopologyDfuRulesGetTaskData();
    RulesEngine_CopyRunParams(dfu_rules->rule_set, param, size_param);
    return rule_action_run_with_param;
}

