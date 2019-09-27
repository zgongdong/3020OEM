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
DEFINE_RULE(rulePeerPair);
DEFINE_RULE(ruleSendStatusToHandset);
DEFINE_RULE(ruleDecideRole);
DEFINE_RULE(ruleOutOfCaseAllowHandsetConnect);
DEFINE_RULE(ruleOutOfCasePeerSignallingConnect);
DEFINE_RULE(ruleInCaseRejectHandsetConnect);
DEFINE_RULE(ruleCheckUpgradable);
DEFINE_RULE(ruleInCaseEnterDfu);
DEFINE_RULE(ruleRoleSwitchDisconnectHandset);
DEFINE_RULE(ruleRoleSwitchConnectPeer);
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

static rule_action_t ruleRoleSwitchConnectPeer(void)
{
    uint8 profiles = DEVICE_PROFILE_PEERSIG;
    SECONDARY_RULE_LOG("ruleRoleSwitchConnectPeer, run");
    return RULE_ACTION_RUN_PARAM(profiles);
}

/*! @brief Rule to determine if Earbud should start automatic peer pairing
    This rule determins if automatic peer pairing should start, it is triggered
    by the startup event.
    @startuml

    start
        if (IsPairedWithPeer()) then (no)
            :Start peer pairing;
            end
        else (yes)
            :Already paired;
            stop
    @enduml
*/
static rule_action_t rulePeerPair(void)
{
    if (!BtDevice_IsPairedWithPeer())
    {
        SECONDARY_RULE_LOG("ruleStartupPeerPaired, run");
        return rule_action_run;
    }
    else
    {
        SECONDARY_RULE_LOG("ruleStartupPeerPaired, done");
        return rule_action_complete;
    }
}
/*! @brief Rule to determine if Earbud should send status to handset over HFP and/or AVRCP
    @startuml

    start
    if (IsPairedHandset() and IsTwsPlusHandset()) then (yes)
        if (IsHandsetHfpConnected() or IsHandsetAvrcpConnected()) then (yes)
            :HFP and/or AVRCP connected, send status update;
            stop
        endif
    endif

    :Not connected with AVRCP or HFP to handset;
    end
    @enduml
*/
static rule_action_t ruleSendStatusToHandset(void)
{
    bdaddr handset_addr;

    if (appDeviceGetHandsetBdAddr(&handset_addr) && appDeviceIsTwsPlusHandset(&handset_addr))
    {
        if (appDeviceIsHandsetHfpConnected() || appDeviceIsHandsetAvrcpConnected())
        {
            SECONDARY_RULE_LOG("ruleSendStatusToHandset, run as TWS+ handset");
            return rule_action_run;
        }
    }

    SECONDARY_RULE_LOG("ruleSendStatusToHandset, ignore as not connected to TWS+ handset");
    return rule_action_ignore;
}

/*! @brief Rule to determine if this Earbud is the primary.
 *
 * \note This is a temporary rule to emulate a simple role
 * selection for use in determining which rule set to use.
 * It will be removed once we have the full role selection
 * implementation.
 *
 * Decision on Primary is determined by the following truth table.
 * (X=Don't care).
 *
 * A = this Earbud out of case
 * B = peer Earbud out of case
 * C = this Earbud is left
 *
 * A | B | C | Is this Earbud (A) Primary
 * - | - | - | --------------------------
 * 1 | 0 | X | 1
 * 0 | 1 | X | 0
 * 0 | 0 | X | 0
 * 1 | 1 | 1 | 1
 * 1 | 1 | 0 | 0
 */
static rule_action_t ruleDecideRole(void)
{
    const bool primary_role = TRUE;
    const bool secondary_role = FALSE;

    if (appSmIsOutOfCase())
    {
        SECONDARY_RULE_LOG("ruleDecideRole out of the case -> DECIDE ROLE");
        return RULE_ACTION_RUN_PARAM(primary_role);
    }
    else if(appSmIsInCase() && appSmIsDfuPending())
    {
    	   return rule_action_ignore;
    }
    else
    {
        SECONDARY_RULE_LOG("ruleDecideRole in the case -> SECONDARY ROLE");
        return RULE_ACTION_RUN_PARAM(secondary_role);
    }
}

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

/*! Rule to determine if the Secondary is in the case with handset connection and should disconnect. */
static rule_action_t ruleRoleSwitchDisconnectHandset(void)
{
    SECONDARY_RULE_LOG("ruleRoleSwitchDisconnectHandset, run as Secondary should never have connection to handset");
    return rule_action_run;
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

