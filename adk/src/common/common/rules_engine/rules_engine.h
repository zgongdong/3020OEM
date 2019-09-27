/*!
\copyright  Copyright (c) 2018 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       rules_engine.h
\brief	    Engine for processing application defined rules.
*/

#ifndef RULES_ENGINE_H_
#define RULES_ENGINE_H_

#include <message.h>
#include <vmtypes.h> /* for STATIC_ASSERT */

#include <task_list.h>


/*! Action required by the rules engine. This type is used as the
    return value from a rule. */
typedef enum
{
    /*! Let the rule engine know that it should send the message defined for the rule */
    rule_action_run,
    /*! Let the rule engine know that no message is needed, as the rule has been completed */
    rule_action_complete,
    /*! Let the rule engine know that no message is needed, as the rule not required  */
    rule_action_ignore,
    /*! Let the rule engine know that no message is needed - AT THE MOMENT - but that it
        must reevaluate this rule when subsequent events occur. */
    rule_action_defer,
    /*! Let the rule engine know that it should send the message defined for the rule
        and that the message has a body with parameters. */
    rule_action_run_with_param
} rule_action_t;

/*! Type to hold a bitmask of active rules */
typedef unsigned long long rule_events_t;
/* Protect against unsigned long long being less than 8 bytes, e.g. BlueCore. */
STATIC_ASSERT(sizeof(rule_events_t) >= 8, RulesEngine_eventSizeBadness);

#define RULE_EVENT_ALL_EVENTS_MASK               (0xFFFFFFFFFFFFFFFFULL)

/*! \brief Flags to control rule calling.
*/
typedef enum
{
    rule_flag_no_flags          = 0x00,

    /*! Always evaluate this rule on any event. */
    rule_flag_always_evaluate   = 0x01,

    /*! This rule should be considered when reporting
     * rules that are in progress. */
    rule_flag_progress_matters  = 0x02,
} rule_flags_t;

/*! \brief Function pointer definition for a rule */
typedef rule_action_t (*rule_func_t)(void);

/*! \brief Definition of a rule entry. */
typedef struct
{
    /*! Events that trigger this rule */
    rule_events_t events;

    /*! Flags that control how this rule is processed */
    rule_flags_t flags;

    /*! Pointer to the function to evaluate the rule. */
    rule_func_t rule;

    /*! Message to send when rule determines action to be run. */
    MessageId message;
} rule_entry_t;

/*! Macro to declare a function, based on the name of a rule */
#define DEFINE_RULE(name) \
    static rule_action_t name(void)

/*! Macro used to create an entry in the rules table */
#define RULE(event, name, message) \
    { event, rule_flag_no_flags, name, message }

/*! Macro used to create an entry in the rules table */
#define RULE_ALWAYS(event, name, message) \
    { event, rule_flag_always_evaluate, name, message }

/*! Macro used to create an entry in the rules table that requires additional
    flags. */
#define RULE_WITH_FLAGS(event, name, message, flags) \
    { event, flags, name, message }

/*! \brief Opaque handle to a rule set instance.

    This object contains both a fixed set of rules and the current state
    of rule processing.
*/
typedef struct rule_set_tag * rule_set_t;

/*! \brief Initialisation parameters for a rule set. */
typedef struct
{
    /*! \brief Pointer to an array of rules for the rule set. */
    const rule_entry_t *rules;

    /*! \brief Number of rules in the #rules array. */
    size_t rules_count;

    /*! \brief MessageId to be sent to a client that has registered
           for notification of when no rules are in progress. See
           #RulesEngine_NopClientRegister. */
    MessageId nop_message_id;
} rule_set_init_params_t;


/*! \brief Initialise a rule set with the given rules.

    \param rules Pointer to an array of rules to add to the rule set.
    \param rules_count Number of rules in the #rules array.
    \param nop_message_id MessageId to be sent to a client that has registered
           for notification of when no rules are in progress. See
           #RulesEngine_NopClientRegister.

    \return Handle to a new rule set instance.
 */
rule_set_t RulesEngine_CreateRuleSet(const rule_set_init_params_t *params);

/*! \brief Free any resources used by a rule set.

    \param rule_set The rule set instance to destroy.
*/
void RulesEngine_DestroyRuleSet(rule_set_t rule_set);

/*! \brief Set an event or events

    This function is called to set an event or events that will cause the relevant
    rules in the rule set to run. Any actions generated will be sent as messages
    to the client_task.

    \param rule_set The rule set to process the event with.
    \param client_task The client task to receive rule actions from this event
    \param event Events to set that will trigger rules to run
*/
void RulesEngine_SetEvent(rule_set_t rule_set, Task client_task, rule_events_t event_mask);

/*! \brief Reset/clear an event or events

    This function is called to clear an event or set of events that was previously
    set by #RulesEngine_SetEvent(). Clear event will reset any rule that was run for event.

    \param rule_set The rule set to act on.
    \param event Events to clear
*/
void RulesEngine_ResetEvent(rule_set_t rule_set, rule_events_t event);

/*! \brief Get set of active events

    \param rule_set The rule set to act on.
    \return The set of active events.
*/
rule_events_t RulesEngine_GetEvents(rule_set_t rule_set);

/*! \brief Mark rules as complete from messaage ID

    This function is called to mark rules as completed, the message parameter
    is used to determine which rules can be marked as completed.

    \param rule_set The rule set to act on.
    \param message Message ID that rule(s) generated
*/
void RulesEngine_SetRuleComplete(rule_set_t rule_set, MessageId message);

/*! \brief Mark rules as complete from message ID and set of events

    This function is called to mark rules as completed, the message and event parameter
    is used to determine which rules can be marked as completed.

    \param rule_set The rule set to act on.
    \param message Message ID that rule(s) generated
    \param event Event or set of events that trigger the rule(s)
*/
void RulesEngine_SetRuleWithEventComplete(rule_set_t rule_set, MessageId message, rule_events_t event);

/*! \brief Copy rule param data for the engine to put into action messages.

    \param rule_set The rule set to act on.
    \param param Pointer to data to copy.
    \param size_param Size of the data in bytes.
 */
void RulesEngine_CopyRunParams(rule_set_t rule_set, const void* param, size_t size_param);

/*! \brief Determine if there are still rules in progress.

    \param rule_set The rule set to act on.
    \return bool TRUE if there are rules with #rule_flag_progress_matters set
                 which are currently in progress, otherwise FALSE.
*/
bool RulesEngine_InProgress(rule_set_t rule_set);

/*! \brief Register a task to receive notifications that no rules are in progress.

    When no rules are in progress the #nop_message_id passed in to
    #RulesEngine_CreateRuleSet is sent to #task.

    \param rule_set The rule set to act on.
    \param task Task to receive notifications.
*/
void RulesEngine_NopClientRegister(rule_set_t rule_set, Task task);

#endif /* RULES_ENGINE_H_ */
