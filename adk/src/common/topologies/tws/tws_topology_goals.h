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
    tws_topology_goal_none                                  = 0,

    /*! Goal is to pair with a peer Earbud. */
    tws_topology_goal_pair_peer                             = 1UL << 0,

    /*! Goal is determine the Primary or Secondary role of the Earbud. */
    tws_topology_goal_find_role                             = 1UL << 1,

    /*! Goal is to create BREDR ACL to Primary. */
    tws_topology_goal_secondary_connect_peer                = 1UL << 2,

    /*! Goal is to connect peer BREDR profiles to Secondary. */
    tws_topology_goal_primary_connect_peer_profiles         = 1UL << 3,

    /*! Goal is to disconnect peer BREDR profiles to Secondary. */
    tws_topology_goal_primary_disconnect_peer_profiles      = 1UL << 4,

    /*! Goal is to enable page scan on Primary for Secondary to connect BREDR ACL. */
    tws_topology_goal_primary_connectable_peer              = 1UL << 5,

    /*! Goal is to discard current role, disconnect links and cancel any
        activity. Typically used going into the case. */
    tws_topology_goal_no_role_idle                          = 1UL << 6,

    /*! Goal is to set a specific role. */
    tws_topology_goal_set_role                              = 1UL << 7,

    /*! Goal is to connect to a handset. */
    tws_topology_goal_connect_handset                       = 1UL << 8,

    /*! Goal is to disconnect from a handset. */
    tws_topology_goal_disconnect_handset                    = 1UL << 9,

    /*! Goal is to enable pagescan to enable handset connectivity. */
    tws_topology_goal_connectable_handset                   = 1UL << 10,

    /*! Goal is to take on the Primary role. */
    tws_topology_goal_become_primary                        = 1UL << 11,

    /*! Goal is to take on the Secondary role. */
    tws_topology_goal_become_secondary                      = 1UL << 12,

    /*! Goal is to take on the Acting Primary role. */
    tws_topology_goal_become_acting_primary                 = 1UL << 13,

    /*! Goal is to set a specific BT address. */
    tws_topology_goal_set_address                           = 1UL << 14,

    /*! Goal is to set the primary address and start role selection. */
    tws_topology_goal_set_primary_address_and_find_role     = 1UL << 15,

    /*! Goal is to switch roles to secondary (from primary role and
        potentially when currently connected to a handset). */
    tws_topology_goal_role_switch_to_secondary              = 1UL << 16,

    /*! Goal is to start role selection when already in a secondary role.
        Typically this goal is used in failure cases, such as failure
        to establish peer BREDR link following previous role selection. */
    tws_topology_goal_no_role_find_role                     = 1UL << 17,

    /*! Goal is to cancel an active role selection process. */
    tws_topology_goal_cancel_find_role                      = 1UL << 18,

    /*! Goal is to start continuous role selection (low duty cycle)
        whilst in the Primary role. Typically used in failure cases
        where the Secondary has been lost, or Earbud is an acting
        Primary. */
    tws_topology_goal_primary_find_role                     = 1UL << 19,

    tws_topology_goal_dfu_role                              = 1UL << 20,
    tws_topology_goal_dfu_primary                           = 1UL << 21,
    tws_topology_goal_dfu_secondary                         = 1UL << 22,

    /*! Goal to disconnect the peer and begin role selection, could
        be used on handset linkloss to determine best Earbud to reconnect. */
    tws_topology_goal_disconnect_peer_find_role             = 1UL << 23,
} tws_topology_goal;

/*! Maximum number of goals
 * \todo generate this via XMACROs */
#define TWS_TOPOLOGY_GOALS_MAX  24

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

/*! Definition of a goal and corresponding procedure to achieve the goal. */
typedef struct
{
    /*! Type of goal. */
    tws_topology_goal       goal;

    /*! Procedure to use to achieve the goal. */
    tws_topology_procedure  proc;

    /*! Structure of function pointers implementing procedure interface. */
    tws_topology_procedure_fns_t* proc_fns;

    /*! Scripted procedure handle. */
    const tws_topology_proc_script_t* proc_script;

    /*! Goal which if active, can be summarily cleared, before starting
       this this goal's procedure.
       For example #tws_topology_goal_primary_connect_peer_profiles is
       exclusive with #tws_topology_goal_primary_disconnect_peer_profiles. */
    tws_topology_goal       exclusive_goal;

    /*! How to handle contention with existing active goals
        when this goal is added to be run */
    goal_contention_t       contention;

    /*! Event to set in the topology rules engine on successful
        completion of this goal. */
    rule_events_t           success_event;

    /*! Event to set in the topology rules engine on failed
        completion of this goal due to timeout. */
    rule_events_t           timeout_event;

    /*! Event to set in the topology rules engine on general failed
        completion of this goal. */
    rule_events_t           failed_event;

    /*! Bitmask of goals which this goal can run concurrently with. */
    tws_topology_goal       concurrent_goals_mask;
} tws_topology_goal_entry_t;

/*! Macro to add goal to the goals table. */
#define GOAL(goal_name, proc_name, fns, exclusive_goal) \
    { goal_name, proc_name, fns, NULL, exclusive_goal, goal_contention_wait, 0, 0, 0, tws_topology_goal_none}

/*! Macro to add goal to the goals table, which can run concurrently with other goals. */
#define GOAL_WITH_CONCURRENCY(goal_name, proc_name, fns, exclusive_goal, concurrent_goals_mask) \
    { goal_name, proc_name, fns, NULL, exclusive_goal, goal_contention_concurrent, 0, 0, 0, concurrent_goals_mask}

/*! Macro to add goal to the goals table and define an event to generate
    on timeout failure completion. */
#define GOAL_TIMEOUT(goal_name, proc_name, fns, exclusive_goal, timeout_event) \
    { goal_name, proc_name, fns, NULL, exclusive_goal, goal_contention_wait, 0, timeout_event, 0, tws_topology_goal_none}

/*! Macro to add goal to the goals table, which can run concurrently with other goals
    and which will generate an event on timeout failure completion. */
#define GOAL_WITH_CONCURRENCY_TIMEOUT(goal_name, proc_name, fns, exclusive_goal, timeout_event, concurrent_goals_mask) \
    { goal_name, proc_name, fns, NULL, exclusive_goal, goal_contention_concurrent, 0, timeout_event, 0, concurrent_goals_mask}

/*! Macro to add goal to the goals table and define an event to generate
    on successful completion. */
#define GOAL_SUCCESS(goal_name, proc_name, fns, exclusive_goal, success_event) \
    { goal_name, proc_name, fns, NULL, exclusive_goal, goal_contention_wait, success_event, 0, 0, tws_topology_goal_none}

/*! Macro to add goal to the goals table, which when set will cancel
    any actives goals. */
#define GOAL_CANCEL(goal_name, proc_name, fns, exclusive_goal) \
    { goal_name, proc_name, fns, NULL, exclusive_goal, goal_contention_cancel, 0, 0, 0, tws_topology_goal_none}

/*! Macro to add scripted goal to the goals table. */
#define SCRIPT_GOAL(goal_name, proc_name, script, exclusive_goal) \
    { goal_name, proc_name, NULL, script, exclusive_goal, goal_contention_wait, 0, 0, 0, tws_topology_goal_none}

/*! Macro to add scripted goal to the goals table and define an event to
    generate on successful completion. */
#define SCRIPT_GOAL_SUCCESS(goal_name, proc_name, script, exclusive_goal, success_event) \
    { goal_name, proc_name, NULL, script, exclusive_goal, goal_contention_wait, success_event, 0, 0, tws_topology_goal_none}

/*! Macro to add a scripted goal to the goals table.
    If there are any goals active when this goal is added, they will be cancelled and the
    new goal executed when the cancel has completed..
    The goal also defines an event to generate on successful completion. */
#define SCRIPT_GOAL_CANCEL_SUCCESS(goal_name, proc_name, script, exclusive_goal, success_event) \
    { goal_name, proc_name, NULL, script, exclusive_goal, goal_contention_cancel, success_event, 0, 0, tws_topology_goal_none}

/*! Macro to add scripted goal to the goals table and define an event to
    generate on timeout failure completion. */
#define SCRIPT_GOAL_TIMEOUT(goal_name, proc_name, script, exclusive_goal, timeout_event) \
    { goal_name, proc_name, NULL, script, exclusive_goal, goal_contention_wait, 0, timeout_event, 0, tws_topology_goal_none}

/*! Macro to add scripted goal to the goals table, which when set will cancel
    any active goals. */
#define SCRIPT_GOAL_CANCEL(goal_name, proc_name, script, exclusive_goal) \
    { goal_name, proc_name, NULL, script, exclusive_goal, goal_contention_cancel, 0, 0, 0, tws_topology_goal_none}

/*! \brief Query if a goal is currently active.
    \param goal Type of goal being queried for status.
    \return bool TRUE if goal is currently active.
*/
bool TwsTopology_IsGoalActive(tws_topology_goal goal);

/*! \brief Handler for new and queued goals.
    
    \note This is a common handler for new goals generated by a topology
          rule decision and goals queued waiting on completion or cancelling
          already in-progress goals. 
*/
void TwsTopology_HandleGoalDecision(Task task, MessageId id, Message message);

#endif /* TWS_TOPOLOGY_GOALS_H */
