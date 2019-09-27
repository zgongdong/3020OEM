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
DEFINE_RULE(ruleTwsTopDfuDebug);
DEFINE_RULE(ruleTwsTopDfuAlways);
DEFINE_RULE(ruleTwsTopOutOfCase);
DEFINE_RULE(ruleTwsTopInCase);
DEFINE_RULE(ruleTwsTopDfuPrimary);
DEFINE_RULE(ruleTwsTopDfuSecondary);
/*! \} */

/*! \brief TWS Topology rules deciding behaviour in a Dfu role. */
const rule_entry_t twstop_dfu_rules_set[] =
{
    /* Add debug for in case event */
    RULE(TWSTOP_RULE_EVENT_IN_CASE,          ruleTwsTopDfuDebug,             TWSTOP_DFU_GOAL_NOP),

    /* When Earbud put out of the case, terminate DFU */
    RULE(TWSTOP_RULE_EVENT_OUT_CASE,         ruleTwsTopDfuAlways,            TWSTOP_DFU_GOAL_NO_ROLE_FIND_ROLE),

    /* When a DFU is ended, for whatever reason, leave DFU and find role if out of case */
    RULE(TWSTOP_RULE_EVENT_DFU_ROLE_COMPLETE,ruleTwsTopOutOfCase,            TWSTOP_DFU_GOAL_NO_ROLE_FIND_ROLE),

    /* When a DFU is ended, for whatever reason, change role if in case */
    RULE(TWSTOP_RULE_EVENT_DFU_ROLE_COMPLETE,ruleTwsTopInCase,               TWSTOP_DFU_GOAL_NO_ROLE_IDLE),

    /* Move to correct state when restarted during a DFU and had primary role */
    RULE(TWSTOP_RULE_EVENT_ROLE_SELECTED_PRIMARY, ruleTwsTopDfuPrimary,      TWSTOP_DFU_GOAL_PRIMARY),

    /* Move to correct state when restarted during a DFU and had secondary role */
    RULE(TWSTOP_RULE_EVENT_ROLE_SELECTED_SECONDARY, ruleTwsTopDfuSecondary,  TWSTOP_DFU_GOAL_SECONDARY),

};

/*****************************************************************************
 * RULES FUNCTIONS
 *****************************************************************************/

/*! \brief Rule that runs so long as DFU mode is still selected 

    Running this rule set is a key indicator that we are in DFU mode */
static rule_action_t ruleTwsTopDfuDebug(void)
{
    TWSTOP_DFU_RULE_LOG("ruleTwsTopDfuDebug, DFU IN CASE EVENT IGNORE Always");

    return rule_action_ignore;
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

