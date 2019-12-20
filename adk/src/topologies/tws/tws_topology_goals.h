/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Interface to TWS Topology goal handling.
*/

#ifndef TWS_TOPOLOGY_GOALS_H
#define TWS_TOPOLOGY_GOALS_H

#include "tws_topology_procedures.h"
#include "tws_topology_procedure_script_engine.h"

#include <rules_engine.h>

#include <message.h>

/*! Definition of goals which the topology may be instructed (by rules) to achieve. */
typedef enum
{
    /*! Empty goal. */
    tws_topology_goal_none,

    /*! Goal is to pair with a peer Earbud. */
    tws_topology_goal_pair_peer,

    /*! Goal is determine the Primary or Secondary role of the Earbud. */
    tws_topology_goal_find_role,

    /*! Goal is to create BREDR ACL to Primary. */
    tws_topology_goal_secondary_connect_peer,

    /*! Goal is to connect peer BREDR profiles to Secondary. */
    tws_topology_goal_primary_connect_peer_profiles,

    /*! Goal is to disconnect peer BREDR profiles to Secondary. */
    tws_topology_goal_primary_disconnect_peer_profiles,

    /*! Goal is to enable page scan on Primary for Secondary to connect BREDR ACL. */
    tws_topology_goal_primary_connectable_peer,

    /*! Goal is to discard current role, disconnect links and cancel any
        activity. Typically used going into the case. */
    tws_topology_goal_no_role_idle,

    /*! Goal is to set a specific role. */
    tws_topology_goal_set_role,

    /*! Goal is to connect to a handset. */
    tws_topology_goal_connect_handset,

    /*! Goal is to disconnect from a handset. */
    tws_topology_goal_disconnect_handset,

    /*! Goal is to enable pagescan to enable handset connectivity. */
    tws_topology_goal_connectable_handset,

    /*! Goal is to take on the Primary role. */
    tws_topology_goal_become_primary,

    /*! Goal is to take on the Secondary role. */
    tws_topology_goal_become_secondary,

    /*! Goal is to take on the Acting Primary role. */
    tws_topology_goal_become_acting_primary,

    /*! Goal is to set a specific BT address. */
    tws_topology_goal_set_address,

    /*! Goal is to set the primary address and start role selection. */
    tws_topology_goal_set_primary_address_and_find_role,

    /*! Goal is to switch roles to secondary (from primary role and
        potentially when currently connected to a handset). */
    tws_topology_goal_role_switch_to_secondary,

    /*! Goal is to start role selection when already in a secondary role.
        Typically this goal is used in failure cases, such as failure
        to establish peer BREDR link following previous role selection. */
    tws_topology_goal_no_role_find_role,

    /*! Goal is to cancel an active role selection process. */
    tws_topology_goal_cancel_find_role,

    /*! Goal is to start continuous role selection (low duty cycle)
        whilst in the Primary role. Typically used in failure cases
        where the Secondary has been lost, or Earbud is an acting
        Primary. */
    tws_topology_goal_primary_find_role,

    tws_topology_goal_dfu_role,
    tws_topology_goal_dfu_primary,
    tws_topology_goal_dfu_secondary,

    tws_topology_goal_dfu_in_case,

    /*! Goal to disconnect the peer and begin role selection, could
        be used on handset linkloss to determine best Earbud to reconnect. */
    tws_topology_goal_disconnect_peer_find_role,

    /*! Goal to release the peer link.
        This is needed as we otherwise keep a lock on the peer link
        until profiles are started (which won't now happen) */
    tws_topology_goal_release_peer,

    /*! Goal for Secondary to participate in a static handover to
        Primary role. */
    tws_topology_goal_secondary_static_handover,

    /*! Goal for Primary to participate in a static handover when going in the case */ 
    tws_topology_goal_primary_static_handover_in_case,

    /*! Goal for Primary to participate in a static handover staying out of the case */
    tws_topology_goal_primary_static_handover,

    /*! Goal to handle the handover recommendation from HDMA  */
    tws_topology_goal_dynamic_handover,
    
    /*! Goal to handle dynamic handover failure */
    tws_topology_goal_dynamic_handover_failure,

    /*! Goal is to enable advertising to enable LE connectivity with handset. */
    tws_topology_goal_le_connectable_handset,
	
    /*! Goal to allow or disallow handset connections */
    tws_topology_goal_allow_handset_connect,

    /* ADD ENTRIES ABOVE HERE */
    /*! Final entry to get the number of IDs */
    TWS_TOPOLOGY_GOAL_NUMBER_IDS,
} tws_topology_goal_id;

/*! A set of goals, stored as bits in a mask. */
typedef unsigned long long tws_topology_goal_msk;

/*! Macro to create a goal mask from a #tws_topology_goal_id. */
#define GOAL_MASK(id) (((tws_topology_goal_msk)1)<<(id))


/*! Macro to iterate over all goals.
    Helper allows for the fact that the first entry is unused 
    
    \param loop_index name of variable to define and use as the loop index
 */
#define FOR_ALL_GOALS(loop_index)\
    for (int loop_index = 1; (loop_index) < TWS_TOPOLOGY_GOAL_NUMBER_IDS; (loop_index)++)


/*! Types of handling contention when adding a goal with already
    existing active goals. */
typedef enum
{
    /*! Queue the goal to be run when current active goals complete. */
    goal_contention_wait,

    /*! Queue the goal to be run once no goals are active and cancel
        any active goals. Will also cancel queued goals. */
    goal_contention_cancel,

    /*! Queue the current goal to be run once only goals with which it
        can conncurrently run are active. */
    goal_contention_concurrent,
} goal_contention_t;

/*! Definition of a goal and corresponding procedure to achieve the goal. 

    \note Elements are ordered to slightly reduce the structure size
*/
typedef struct
{
    /*! Procedure to use to achieve the goal. */
    tws_topology_procedure          proc;

    /*! How to handle contention with existing active goals
        when this goal is added to be run */
    goal_contention_t               contention;

    /*! Goal which if active, can be summarily cleared, before starting
       this this goal's procedure.
       For example #tws_topology_goal_primary_connect_peer_profiles is
       exclusive with #tws_topology_goal_primary_disconnect_peer_profiles. */
    tws_topology_goal_id            exclusive_goal;

    /*! Event to set in the topology rules engine on successful
        completion of this goal. */
    rule_events_t                   success_event;

    /*! Event to set in the topology rules engine on failed
        completion of this goal due to timeout. */
    rule_events_t                   timeout_event;

    /*! Event to set in the topology rules engine on general failed
        completion of this goal. */
    rule_events_t                   failed_event;

    /*! Structure of function pointers implementing procedure interface. */
    const tws_topology_procedure_fns_t* proc_fns;

    /*! Scripted procedure handle. */
    const tws_topology_proc_script_t* proc_script;

    /*! Bitmask of goals which this goal can run concurrently with. */
    const tws_topology_goal_id*     concurrent_goals;
} tws_topology_goal_entry_t;

/*! Macro to add goal to the goals table. */
#define GOAL(goal_name, proc_name, fns, exclusive_goal_name) \
   [goal_name] = { .proc = proc_name, .proc_fns = fns, \
                   .exclusive_goal = exclusive_goal_name, \
                   .contention = goal_contention_wait }

/*! Macro to add goal to the goals table, which can run concurrently with other goals. */
#define GOAL_WITH_CONCURRENCY(goal_name, proc_name, fns, exclusive_goal_name, concurrent_goals_mask) \
    [goal_name] =  { .proc = proc_name, .proc_fns = fns, \
                     .exclusive_goal = exclusive_goal_name, \
                     .contention = goal_contention_concurrent, \
                     .concurrent_goals = concurrent_goals_mask }

/*! Macro to add goal to the goals table and define an event to generate
    on timeout or failure completion. */
#define GOAL_WITH_TIMEOUT_AND_FAIL(goal_name, proc_name, fns, exclusive_goal_name, _timeout_event, _failure_event) \
    [goal_name] =  { .proc = proc_name, .proc_fns = fns, \
                     .exclusive_goal = exclusive_goal_name, \
                     .contention = goal_contention_wait, \
                     .timeout_event = _timeout_event, \
                     .failed_event = _failure_event }

/*! Macro to add goal to the goals table, which can run concurrently with other goals
    and which will generate an event on timeout failure completion. */
#define GOAL_WITH_CONCURRENCY_TIMEOUT(goal_name, proc_name, fns, exclusive_goal_name, _timeout_event, concurrent_goals_mask) \
    [goal_name] =  { .proc = proc_name, .proc_fns = fns, \
                     .exclusive_goal = exclusive_goal_name, \
                     .contention = goal_contention_concurrent, \
                     .concurrent_goals = concurrent_goals_mask, \
                     .timeout_event = _timeout_event }

/*! Macro to add goal to the goals table and define an event to generate
    on successful completion. */
#define GOAL_SUCCESS(goal_name, proc_name, fns, exclusive_goal_name, _success_event) \
    [goal_name] =  { .proc = proc_name, .proc_fns = fns, \
                     .exclusive_goal = exclusive_goal_name, \
                     .contention = goal_contention_wait, \
                     .success_event = _success_event }

/*! Macro to add goal to the goals table, which when set will cancel
    any actives goals. */
#define GOAL_CANCEL(goal_name, proc_name, fns, exclusive_goal_name) \
    [goal_name] =  { .proc = proc_name, .proc_fns = fns, \
                     .exclusive_goal = exclusive_goal_name, \
                     .contention = goal_contention_cancel }

/*! Macro to add scripted goal to the goals table. */
#define SCRIPT_GOAL(goal_name, proc_name, script, exclusive_goal_name) \
    [goal_name] = { .proc = proc_name, .\
                     proc_script = script, \
                    .exclusive_goal = exclusive_goal_name, \
                    .contention = goal_contention_wait }

/*! Macro to add scripted goal to the goals table and define an event to
    generate on successful completion. */
#define SCRIPT_GOAL_SUCCESS(goal_name, proc_name, script, exclusive_goal_name, _success_event) \
    [goal_name] =  { .proc = proc_name, \
                     .proc_script = script, \
                     .exclusive_goal = exclusive_goal_name, \
                     .contention = goal_contention_wait, \
                     .success_event = _success_event }

/*! Macro to add a scripted goal to the goals table.
    If there are any goals active when this goal is added, they will be cancelled and the
    new goal executed when the cancel has completed..
    The goal also defines an event to generate on successful completion. */
#define SCRIPT_GOAL_CANCEL_SUCCESS(goal_name, proc_name, script, exclusive_goal_name, _success_event) \
    [goal_name] =  { .proc = proc_name, \
                     .proc_script = script, \
                     .exclusive_goal = exclusive_goal_name, \
                     .contention = goal_contention_cancel, \
                     .success_event = _success_event }

/*! Macro to add scripted goal to the goals table.
    If the timeout parameter is provided and the goal results in a timeout, the timeout
    event will be sent to the rules engine.
    If the failed parameter is provided and the goal results in a failure,a failed event
    will be sent to the rules engine. */
#define SCRIPT_GOAL_TIMEOUT_OR_FAILED(goal_name, proc_name, script, exclusive_goal_name, _timeout_event, _failed_event) \
    [goal_name] =  { .proc = proc_name, \
                     .proc_script = script, \
                     .exclusive_goal = exclusive_goal_name, \
                     .contention = goal_contention_wait, \
                     .timeout_event = _timeout_event, \
                     .failed_event = _failed_event }

/*! Macro to add scripted cancel goal to the goals table.
    If there are any goals active when this goal is added, they will be cancelled and the
    new goal executed when the cancel has completed..
    If the success parameter is provided and the goal results in a success, the success
    event will be sent to the rules engine.
    If the failed parameter is provided and the goal results in a failure, a failed event
    will be sent to the rules engine. */
#define SCRIPT_GOAL_CANCEL_SUCCESS_FAILED(goal_name, proc_name, script, exclusive_goal_name, _success_event, _failed_event) \
    [goal_name] =  { .proc = proc_name, \
                     .proc_script = script, \
                     .exclusive_goal = exclusive_goal_name, \
                     .contention = goal_contention_cancel, \
                     .success_event = _success_event, \
                     .failed_event = _failed_event }

/*! Macro to add scripted cancel goal to the goals table.
    If there are any goals active when this goal is added, they will be cancelled and the
    new goal executed when the cancel has completed..
    If the failed parameter is provided and the goal results in a failure, a failed event
    will be sent to the rules engine. */
#define SCRIPT_GOAL_CANCEL_FAILED(goal_name, proc_name, script, exclusive_goal_name, _failed_event) \
    [goal_name] =  { .proc = proc_name, \
                     .proc_script = script, \
                     .exclusive_goal = exclusive_goal_name, \
                     .contention = goal_contention_cancel, \
                     .failed_event = _failed_event }

/*! Macro to add scripted cancel goal to the goals table.
    If there are any goals active when this goal is added, they will be cancelled and the
    new goal executed when the cancel has completed..
    If the failed parameter is provided and the goal results in a failure, a failed event
    will be sent to the rules engine. 
    If the timeout parameter is provided and the goal results in a timeout, the timeout
    event will be sent to the rules engine. */
#define SCRIPT_GOAL_CANCEL_TIMEOUT_FAILED(goal_name, proc_name, script, exclusive_goal_name, \
                                            _timeout_event, _failed_event) \
    [goal_name] =  { .proc = proc_name, \
                     .proc_script = script, \
                     .exclusive_goal = exclusive_goal_name, \
                     .contention = goal_contention_cancel, \
                     .timeout_event = _timeout_event, \
                     .failed_event = _failed_event }

/*! Macro to add scripted goal to the goals table and define an event to
    generate on timeout failure completion. */
#define SCRIPT_GOAL_TIMEOUT(goal_name, proc_name, script, exclusive_goal_name, _timeout_event) \
    [goal_name] =  { .proc = proc_name, \
                     .proc_script = script, \
                     .exclusive_goal = exclusive_goal_name, \
                     .contention = goal_contention_wait, \
                     .timeout_event = _timeout_event }

/*! Macro to add scripted goal to the goals table, which when set will cancel
    any active goals. */
#define SCRIPT_GOAL_CANCEL(goal_name, proc_name, script, exclusive_goal_name) \
    [goal_name] =  { .proc = proc_name, \
                     .proc_script = script, \
                     .exclusive_goal = exclusive_goal_name, \
                     .contention = goal_contention_cancel }

/*! \brief Query if a goal is currently active.

    \param goal The goal being queried for status.

    \return TRUE if goal is currently active.
*/
bool TwsTopology_IsGoalActive(tws_topology_goal_id goal);

/*! \brief Query if a goal is currently active (using the mask)

    \param goal The bitmask for the goal being queried for status.

    \return TRUE if the goal is currently active.
*/
bool TwsTopology_IsGoalMaskActive(tws_topology_goal_msk goal);

/*! \brief Handler for new and queued goals.
    
    \note This is a common handler for new goals generated by a topology
          rule decision and goals queued waiting on completion or cancelling
          already in-progress goals. 
*/
void TwsTopology_HandleGoalDecision(Task task, MessageId id, Message message);

#endif /* TWS_TOPOLOGY_GOALS_H */
