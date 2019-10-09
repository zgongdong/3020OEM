/*!
\copyright  Copyright (c) 2005 - 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      
*/

#ifndef _TWS_TOPOLOGY_PRIMARY_RULES_H_
#define _TWS_TOPOLOGY_PRIMARY_RULES_H_

#include "tws_topology_rule_events.h"
#include "tws_topology_private.h"

#include <rules_engine.h>

#include <message.h>

enum tws_topology_primary_goals
{
    TWSTOP_PRIMARY_GOAL_FIND_ROLE = TWSTOP_INTERNAL_PRIMARY_RULE_MSG_BASE,
    TWSTOP_PRIMARY_GOAL_PAIR_PEER,
    TWSTOP_PRIMARY_GOAL_CONNECT_PEER_PROFILES,
    TWSTOP_PRIMARY_GOAL_CONNECTABLE_PEER,
    TWSTOP_PRIMARY_GOAL_DISCONNECT_PEER_PROFILES,
    TWSTOP_PRIMARY_GOAL_RELEASE_PEER,
    TWSTOP_PRIMARY_GOAL_NO_ROLE_IDLE,
    TWSTOP_PRIMARY_GOAL_CONNECTABLE_HANDSET,
    TWSTOP_PRIMARY_GOAL_CONNECT_HANDSET,
    TWSTOP_PRIMARY_GOAL_DISCONNECT_HANDSET,
    TWSTOP_PRIMARY_GOAL_BECOME_PRIMARY,
    TWSTOP_PRIMARY_GOAL_BECOME_ACTING_PRIMARY,
    TWSTOP_PRIMARY_GOAL_BECOME_SECONDARY,
    TWSTOP_PRIMARY_GOAL_SET_PRIMARY_ADDRESS,
    TWSTOP_PRIMARY_GOAL_SET_PRIMARY_ADDRESS_FIND_ROLE,
    TWSTOP_PRIMARY_GOAL_ROLE_SWITCH_TO_SECONDARY,
    TWSTOP_PRIMARY_GOAL_CANCEL_FIND_ROLE,
    TWSTOP_PRIMARY_GOAL_PRIMARY_FIND_ROLE,
    TWSTOP_PRIMARY_GOAL_DFU_ROLE,
    TWSTOP_PRIMARY_GOAL_DISCONNECT_PEER_FIND_ROLE,
    TWSTOP_PRIMARY_GOAL_HANDOVER_START,
    TWSTOP_PRIMARY_GOAL_HANDOVER_INCASE_TIMEOUT,
    TWSTOP_PRIMARY_GOAL_FORCED_ROLE_SWITCH,
    
    TWSTOP_PRIMARY_GOAL_NOP,
};

typedef struct
{
    uint8 profiles;
} TWSTOP_PRIMARY_GOAL_CONNECT_PEER_PROFILES_T;

typedef struct
{
    uint8 profiles;
} TWSTOP_PRIMARY_GOAL_DISCONNECT_PEER_PROFILES_T;

typedef struct
{
    uint8 profiles;
} TWSTOP_PRIMARY_GOAL_DISCONNECT_HANDSET_PROFILES_T;

typedef struct
{
    bool enable;
} TWSTOP_PRIMARY_GOAL_CONNECTABLE_PEER_T;

typedef struct
{
    bool enable;
} TWSTOP_PRIMARY_GOAL_CONNECTABLE_HANDSET_T;

/*! \brief TWS Topology primary rules task data. */
typedef struct
{
    rule_set_t rule_set;
} TwsTopologyPrimaryRulesTaskData;

typedef struct
{
    uint8 profiles;
} TWSTOP_PRIMARY_GOAL_CONNECT_HANDSET_T;

/*! \brief Initialise the connection rules module. */
bool TwsTopologyPrimaryRules_Init(Task init_task);

/*! \brief Get handle on the primary rule set, in order to directly set/clear events.
    \return rule_set_t The primary rule set.
 */
rule_set_t TwsTopologyPrimaryRules_GetRuleSet(void);

/*! \brief Set an event or events
    \param[in] client_task The client task to receive rule actions from this event
    \param[in] event Events to set that will trigger rules to run
    This function is called to set an event or events that will cause the relevant
    rules in the rules table to run.  Any actions generated will be sent as message
    to the client_task
*/
void TwsTopologyPrimaryRules_SetEvent(Task client_task, rule_events_t event_mask);

/*! \brief Reset/clear an event or events
    \param[in] event Events to clear
    This function is called to clear an event or set of events that was previously
    set. Clear event will reset any rule that was run for event.
*/
void TwsTopologyPrimaryRules_ResetEvent(rule_events_t event);

/*! \brief Get set of active events
    \return The set of active events.
*/
rule_events_t TwsTopologyPrimaryRules_GetEvents(void);

/*! \brief Mark rules as complete from messaage ID
    \param[in] message Message ID that rule(s) generated
    This function is called to mark rules as completed, the message parameter
    is used to determine which rules can be marked as completed.
*/
void TwsTopologyPrimaryRules_SetRuleComplete(MessageId message);

/*! \brief Mark rules as complete from message ID and set of events
    \param[in] message Message ID that rule(s) generated
    \param[in] event Event or set of events that trigger the rule(s)
    This function is called to mark rules as completed, the message and event parameter
    is used to determine which rules can be marked as completed.
*/
void TwsTopologyPrimaryRules_SetRuleWithEventComplete(MessageId message, rule_events_t event);

#endif /* _TWS_TOPOLOGY_PRIMARY_RULES_H_ */
