/*!
\copyright  Copyright (c) 2005 - 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      
*/

#ifndef _TWS_TOPOLOGY_SECONDARY_RULES_H_
#define _TWS_TOPOLOGY_SECONDARY_RULES_H_

#include "tws_topology_private.h"
#include "tws_topology_rule_events.h"

#include <rules_engine.h>

#include <message.h>

enum tws_topology_secondary_rules_goals
{
    TWSTOP_SECONDARY_GOAL_FIND_ROLE = TWSTOP_INTERNAL_SECONDARY_RULE_MSG_BASE,
    TWSTOP_SECONDARY_GOAL_PAIR_PEER,
    TWSTOP_SECONDARY_GOAL_CONNECT_PEER,
    TWSTOP_SECONDARY_GOAL_NO_ROLE_IDLE,
    TWSTOP_SECONDARY_GOAL_NO_ROLE_FIND_ROLE,
    TWSTOP_SECONDARY_GOAL_DFU_ROLE,
    TWSTOP_SECONDARY_GOAL_STATIC_HANDOVER,

    TWSTOP_SECONDARY_GOAL_NOP,
};

/*! \brief TWS Topology secondary rules task data. */
typedef struct
{
    rule_set_t rule_set;
} TwsTopologySecondaryRulesTaskData;

/*! \brief Initialise the TWS Topology secondary rules component. */
bool TwsTopologySecondaryRules_Init(Task init_task);

/*! \brief Get handle on the secondary rule set, in order to directly set/clear events.
    \return rule_set_t The secondary rule set.
 */
rule_set_t TwsTopologySecondaryRules_GetRuleSet(void);

/*! \brief Set an event or events
    \param[in] client_task The client task to receive rule actions from this event
    \param[in] event Events to set that will trigger rules to run
    This function is called to set an event or events that will cause the relevant
    rules in the rules table to run.  Any actions generated will be sent as message
    to the client_task
*/
void TwsTopologySecondaryRules_SetEvent(Task client_task, rule_events_t event_mask);

/*! \brief Reset/clear an event or events
    \param[in] event Events to clear
    This function is called to clear an event or set of events that was previously
    set. Clear event will reset any rule that was run for event.
*/
void TwsTopologySecondaryRules_ResetEvent(rule_events_t event);

/*! \brief Get set of active events
    \return The set of active events.
*/
rule_events_t TwsTopologySecondaryRules_GetEvents(void);

/*! \brief Mark rules as complete from messaage ID
    \param[in] message Message ID that rule(s) generated
    This function is called to mark rules as completed, the message parameter
    is used to determine which rules can be marked as completed.
*/
void TwsTopologySecondaryRules_SetRuleComplete(MessageId message);

/*! \brief Mark rules as complete from message ID and set of events
    \param[in] message Message ID that rule(s) generated
    \param[in] event Event or set of events that trigger the rule(s)
    This function is called to mark rules as completed, the message and event parameter
    is used to determine which rules can be marked as completed.
*/
void TwsTopologySecondaryRules_SetRuleWithEventComplete(MessageId message, rule_events_t event);

#endif /* _TWS_TOPOLOGY_SECONDARY_RULES_H_ */
