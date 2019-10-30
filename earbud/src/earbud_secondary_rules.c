/*!
\copyright  Copyright (c) 2005 - 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       earbud_secondary_rules.c
\brief	    Earbud application rules run when in a Secondary earbud role.
*/

#include "earbud_secondary_rules.h"
#include "earbud_primary_rules.h"

#include <earbud_config.h>
#include <earbud_init.h>
#include <earbud_log.h>
#include <kymera_config.h>
#include <earbud_sm.h>
#include <gatt_handler.h>

#include <domain_message.h>
#include <av.h>
#include <phy_state.h>
#include <bt_device.h>
#include <bredr_scan_manager.h>
#include <hfp_profile.h>
#include <peer_signalling.h>
#include <scofwd_profile.h>
#include <state_proxy.h>
#include <rules_engine.h>

#include <bdaddr.h>
#include <panic.h>
#include <system_clock.h>

#pragma unitsuppress Unused

/*! \{
    Macros for diagnostic output that can be suppressed.
    Allows debug of the rules module at two levels. */
#define SECONDARY_RULES_LOG(x)       //DEBUG_LOG(x)
#define SECONDARY_RULES_LOGF(x, ...) //DEBUG_LOGF(x, __VA_ARGS__)

#define SECONDARY_RULE_LOG(x)         DEBUG_LOG(x)
#define SECONDARY_RULE_LOGF(x, ...)   DEBUG_LOGF(x, __VA_ARGS__)
/*! \} */

/* Forward declaration for use in RULE_ACTION_RUN_PARAM macro below */
static rule_action_t secondaryRulesCopyRunParams(const void* param, size_t size_param);

/*! \brief Macro used by rules to return RUN action with parameters to return.
    Copies the parameters/data into secondary_rules where the rules engine can uses
    it when building the action message.
*/
#define RULE_ACTION_RUN_PARAM(x)   secondaryRulesCopyRunParams(&(x), sizeof(x))

/*! Get pointer to the connection rules task data structure. */
#define SecondaryRulesGetTaskData()           (&secondary_rules_task_data)

/*!< Connection rules. */
SecondaryRulesTaskData secondary_rules_task_data;

/*! \{
    Rule function prototypes, so we can build the rule tables below. */
DEFINE_RULE(ruleOutOfCaseAllowHandsetConnect);
DEFINE_RULE(ruleOutOfCasePeerSignallingConnect);
DEFINE_RULE(ruleInCaseRejectHandsetConnect);
DEFINE_RULE(ruleCheckUpgradable);
DEFINE_RULE(ruleInCaseEnterDfu);
DEFINE_RULE(ruleOutOfCaseTerminateDfu);
/*! \} */

/*! \brief Set of rules to run on Earbud startup. */
const rule_entry_t secondary_rules_set[] =
{
    RULE_WITH_FLAGS(RULE_EVENT_CHECK_DFU,              ruleCheckUpgradable,         CONN_RULES_DFU_ALLOW,
                    rule_flag_always_evaluate),

    /* Rules to start DFU when going in the case. */
    RULE(RULE_EVENT_IN_CASE,                    ruleInCaseEnterDfu,                 SEC_CONN_RULES_ENTER_DFU),

    /* Rules that control handset connection permitted/denied */
    RULE(RULE_EVENT_OUT_CASE,                   ruleOutOfCaseAllowHandsetConnect,   CONN_RULES_ALLOW_HANDSET_CONNECT),    
    RULE(RULE_EVENT_IN_CASE,                    ruleInCaseRejectHandsetConnect,     CONN_RULES_REJECT_HANDSET_CONNECT),

    RULE(RULE_EVENT_OUT_CASE,                   ruleOutOfCaseTerminateDfu,          CONN_RULES_EXIT_DFU),
};

/*! \brief Types of event that can cause connect rules to run. */
typedef enum
{
    RULE_CONNECT_USER,              /*!< User initiated connection */
    RULE_CONNECT_PAIRING,           /*!< Connect on startup */
    RULE_CONNECT_PEER_SYNC,         /*!< Peer sync complete initiated connection */
    RULE_CONNECT_OUT_OF_CASE,       /*!< Out of case initiated connection */
    RULE_CONNECT_LINK_LOSS,         /*!< Link loss recovery initiated connection */
    RULE_CONNECT_PEER_OUT_OF_CASE,  /*!< Peer out of case initiated connection */
} ruleConnectReason;

/*****************************************************************************
 * RULES FUNCTIONS
 *****************************************************************************/

static rule_action_t ruleOutOfCaseAllowHandsetConnect(void)
{
    SECONDARY_RULE_LOG("ruleOutOfCaseAllowHandsetConnect, run as out of case");
    return rule_action_run;
}

static rule_action_t ruleOutOfCasePeerSignallingConnect(void)
{
    SECONDARY_RULE_LOG("ruleOutOfCasePeerSignallingConnect, run as out of case");

    if (!appPeerSigIsConnected())
    {
        return /*rule_action_run*/rule_action_ignore;
    }

    return rule_action_ignore;
}

static rule_action_t ruleInCaseRejectHandsetConnect(void)
{
#ifdef INCLUDE_DFU
    if (appSmIsDfuPending())
    {
        SECONDARY_RULE_LOG("ruleInCaseRejectHandsetConnect, ignored as DFU pending");
        return rule_action_ignore;
    }
#endif

    SECONDARY_RULE_LOG("ruleInCaseRejectHandsetConnect, run as in case and no DFU");
    return rule_action_run;
}

static rule_action_t ruleCheckUpgradable(void)
{
    bool allow_dfu = TRUE;
    bool block_dfu = FALSE;

    if (appSmIsOutOfCase())
    {
        SECONDARY_RULE_LOG("ruleCheckUpgradable, block as out of case and not permitted");
        return RULE_ACTION_RUN_PARAM(block_dfu);
    }
    else
    {
        SECONDARY_RULE_LOG("ruleCheckUpgradable, allow as in case");
        return RULE_ACTION_RUN_PARAM(allow_dfu);
    }
}

/*! @brief Rule to determine if Earbud should start DFU  when put in case
    Rule is triggered by the 'in case' event
    @startuml

    start
    if (IsInCase() and DfuUpgradePending()) then (yes)
        :DFU upgrade can start as it was pending and now in case;
        stop
    endif
    end
    @enduml
*/
static rule_action_t ruleInCaseEnterDfu(void)
{
#ifdef INCLUDE_DFU
    if (appSmIsInCase() && appSmIsDfuPending())
    {
        SECONDARY_RULE_LOG("ruleInCaseCheckDfu, run as still in case & DFU pending/active");
        return rule_action_run;
    }
    else
    {
        SECONDARY_RULE_LOG("ruleInCaseCheckDfu, ignore as not in case or no DFU pending");
        return rule_action_ignore;
    }
#else
    return rule_action_ignore;
#endif
}

static rule_action_t ruleOutOfCaseTerminateDfu(void)
{
    if (appSmIsInDfuMode())
    {
        DEBUG_LOG("ruleOutOfCaseTerminateDfu. Run as in DFU mode");
        return rule_action_run;
    }
    DEBUG_LOG("ruleOutOfCaseTerminateDfu. Ignore, not in DFU mode");
    return rule_action_ignore;
}


/*****************************************************************************
 * END RULES FUNCTIONS
 *****************************************************************************/

/*! \brief Initialise the secondary rules module. */
bool SecondaryRules_Init(Task init_task)
{
    SecondaryRulesTaskData *secondary_rules = SecondaryRulesGetTaskData();
    rule_set_init_params_t rule_params;

    UNUSED(init_task);

    memset(&rule_params, 0, sizeof(rule_params));
    rule_params.rules = secondary_rules_set;
    rule_params.rules_count = ARRAY_DIM(secondary_rules_set);
    rule_params.nop_message_id = CONN_RULES_NOP;
    secondary_rules->rule_set = RulesEngine_CreateRuleSet(&rule_params);

    return TRUE;
}

rule_set_t SecondaryRules_GetRuleSet(void)
{
    SecondaryRulesTaskData *secondary_rules = SecondaryRulesGetTaskData();
    return secondary_rules->rule_set;
}

void SecondaryRules_SetEvent(Task client_task, rule_events_t event_mask)
{
    SecondaryRulesTaskData *secondary_rules = SecondaryRulesGetTaskData();
    RulesEngine_SetEvent(secondary_rules->rule_set, client_task, event_mask);
}

void SecondaryRules_ResetEvent(rule_events_t event)
{
    SecondaryRulesTaskData *secondary_rules = SecondaryRulesGetTaskData();
    RulesEngine_ResetEvent(secondary_rules->rule_set, event);
}

rule_events_t SecondaryRules_GetEvents(void)
{
    SecondaryRulesTaskData *secondary_rules = SecondaryRulesGetTaskData();
    return RulesEngine_GetEvents(secondary_rules->rule_set);
}

void SecondaryRules_SetRuleComplete(MessageId message)
{
    SecondaryRulesTaskData *secondary_rules = SecondaryRulesGetTaskData();
    RulesEngine_SetRuleComplete(secondary_rules->rule_set, message);
}

void SecondaryRules_SetRuleWithEventComplete(MessageId message, rule_events_t event)
{
    SecondaryRulesTaskData *secondary_rules = SecondaryRulesGetTaskData();
    RulesEngine_SetRuleWithEventComplete(secondary_rules->rule_set, message, event);
}

/*! \brief Copy rule param data for the engine to put into action messages.
    \param param Pointer to data to copy.
    \param size_param Size of the data in bytes.
    \return rule_action_run_with_param to indicate the rule action message needs parameters.
 */
static rule_action_t secondaryRulesCopyRunParams(const void* param, size_t size_param)
{
    SecondaryRulesTaskData *secondary_rules = SecondaryRulesGetTaskData();
    RulesEngine_CopyRunParams(secondary_rules->rule_set, param, size_param);
    return rule_action_run_with_param;
}

