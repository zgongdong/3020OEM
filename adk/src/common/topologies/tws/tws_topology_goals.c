/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      TWS Topology goal handling.
*/

#include "tws_topology.h"
#include "tws_topology_private.h"

#include "tws_topology_dfu_rules.h"
#include "tws_topology_primary_rules.h"
#include "tws_topology_secondary_rules.h"

#include "tws_topology_goals.h"
#include "tws_topology_rule_events.h"

#include "tws_topology_procedures.h"
#include "tws_topology_procedure_acting_primary_role.h"
#include "tws_topology_procedure_cancel_find_role.h"
#include "tws_topology_procedure_connect_handset.h"
#include "tws_topology_procedure_connectable_handset.h"
#include "tws_topology_procedure_disconnect_handset.h"
#include "tws_topology_procedure_disconnect_peer_profiles.h"
#include "tws_topology_procedure_disconnect_peer_find_role.h"
#include "tws_topology_procedure_dfu_role.h"
#include "tws_topology_procedure_dfu_secondary_after_boot.h"
#include "tws_topology_procedure_dfu_primary_after_boot.h"
#include "tws_topology_procedure_find_role.h"
#include "tws_topology_procedure_no_role_find_role.h"
#include "tws_topology_procedure_no_role_idle.h"
#include "tws_topology_procedure_pair_peer.h"
#include "tws_topology_procedure_permit_bt.h"
#include "tws_topology_procedure_pri_connect_peer_profiles.h"
#include "tws_topology_procedure_pri_connectable_peer.h"
#include "tws_topology_procedure_primary_addr_find_role.h"
#include "tws_topology_procedure_primary_find_role.h"
#include "tws_topology_procedure_primary_role.h"
#include "tws_topology_procedure_prohibit_bt.h"
#include "tws_topology_procedure_release_peer.h"
#include "tws_topology_procedure_sec_connect_peer.h"
#include "tws_topology_procedure_secondary_role.h"
#include "tws_topology_procedure_set_address.h"
#include "tws_topology_procedure_set_role.h"
#include "tws_topology_procedure_switch_to_secondary.h"
#include "tws_topology_procedure_handover.h"
#include "tws_topology_procedure_handover_entering_case.h"
#include "tws_topology_procedure_handover_entering_case_timeout.h"
#include "tws_topology_procedure_sec_forced_role_switch.h"
#include "tws_topology_procedure_pri_forced_role_switch.h"

#include <logging.h>

#include <message.h>
#include <panic.h>

#pragma unitsuppress Unused

static int twsTopology_GetGoalIndexFromName(tws_topology_goal goal);
static const tws_topology_goal_entry_t* twsTopology_GetGoalEntryFromGoal(tws_topology_goal goal);
static tws_topology_goal twsTopology_FindGoalForProcedure(tws_topology_procedure proc);

static void twsTopology_QueueGoal(MessageId rule_id, Message goal_data, size_t goal_data_size, tws_topology_goal wait_mask, tws_topology_goal new_goal);
static void twsTopology_DequeGoal(tws_topology_goal goal);
static void TwsTopology_ClearGoal(tws_topology_goal goal);
static tws_topology_goal twstopology_GetHandoverGoal(hdma_handover_reason_t reason);

#define ADD_GOAL_TO_MASK(goal_mask, goal)           ((goal_mask) |= (goal))
#define REMOVE_GOAL_FROM_MASK(goal_mask, goal)      ((goal_mask) &= (~goal))


/*! \brief This table defines each goal supported by the topology.
    Each entry links the goal set by a topology rule decision with the procedure required to achieve it.
    Entries also provide the configuration for how the goal should be handled, identifying the following
    characteristics:
     - is the goal exclusive with another, requiring the exclusive goal to be cancelled
     - the contention policy of the goal
        - can cancel other goals
        - can execute concurrently with other goals
        - must wait for other goal completion
     - function pointers to the procedure or script to achieve the goal
     - events to generate back into the role rules engine following goal completion
        - success, failure or timeout are supported
     
    Not all goals require configuration of all parameters so utility macros are used to define a
    goal and set default parameters for unrequired fields.
*/
const tws_topology_goal_entry_t goals[] =
{
    SCRIPT_GOAL(tws_topology_goal_pair_peer, tws_topology_procedure_pair_peer_script,
                &pair_peer_script, tws_topology_goal_none),

    GOAL(tws_topology_goal_find_role, tws_topology_procedure_find_role,
         &proc_find_role_fns, tws_topology_goal_none),

    GOAL_WITH_TIMEOUT_AND_FAIL(tws_topology_goal_secondary_connect_peer, tws_topology_procedure_sec_connect_peer,
                 &proc_sec_connect_peer_fns, tws_topology_goal_none, 
                 TWSTOP_RULE_EVENT_FAILED_PEER_CONNECT,TWSTOP_RULE_EVENT_FAILED_PEER_CONNECT),

    GOAL_WITH_CONCURRENCY(tws_topology_goal_primary_connect_peer_profiles, tws_topology_procedure_pri_connect_peer_profiles,
                          &proc_pri_connect_peer_profiles_fns, tws_topology_goal_primary_disconnect_peer_profiles,
                            tws_topology_goal_primary_connectable_peer
                          | tws_topology_goal_connectable_handset
                          | tws_topology_goal_connect_handset),

    GOAL(tws_topology_goal_primary_disconnect_peer_profiles, tws_topology_procedure_disconnect_peer_profiles,
         &proc_disconnect_peer_profiles_fns, tws_topology_goal_primary_connect_peer_profiles),

    GOAL_WITH_CONCURRENCY_TIMEOUT(tws_topology_goal_primary_connectable_peer, tws_topology_procedure_pri_connectable_peer,
                                  &proc_pri_connectable_peer_fns, tws_topology_goal_none,
                                  TWSTOP_RULE_EVENT_FAILED_PEER_CONNECT,
                                    tws_topology_goal_primary_connect_peer_profiles
                                  | tws_topology_goal_connect_handset
                                  | tws_topology_goal_connectable_handset),

    SCRIPT_GOAL_CANCEL(tws_topology_goal_no_role_idle, tws_topology_procedure_no_role_idle,
                       &no_role_idle_script, tws_topology_goal_none),

    GOAL(tws_topology_goal_set_role, tws_topology_procedure_set_role,
         &proc_set_role_fns, tws_topology_goal_none),

    GOAL_WITH_CONCURRENCY(tws_topology_goal_connect_handset, tws_topology_procedure_connect_handset,
                          &proc_connect_handset_fns, tws_topology_goal_disconnect_handset,
                            tws_topology_goal_primary_connect_peer_profiles
                          | tws_topology_goal_primary_connectable_peer
                          | tws_topology_goal_connectable_handset),

    GOAL(tws_topology_goal_disconnect_handset, tws_topology_procedure_disconnect_handset,
         &proc_disconnect_handset_fns, tws_topology_goal_connect_handset),

    GOAL_WITH_CONCURRENCY(tws_topology_goal_connectable_handset, tws_topology_procedure_connectable_handset,
                          &proc_connectable_handset_fns, tws_topology_goal_none,
                            tws_topology_goal_primary_connectable_peer
                          | tws_topology_goal_primary_connect_peer_profiles
                          | tws_topology_goal_connect_handset),

    SCRIPT_GOAL_CANCEL_SUCCESS(tws_topology_goal_become_primary, tws_topology_procedure_become_primary,
                        &primary_role_script, tws_topology_goal_none, TWSTOP_RULE_EVENT_ROLE_SWITCH),

    SCRIPT_GOAL_CANCEL_SUCCESS(tws_topology_goal_become_secondary, tws_topology_procedure_become_secondary,
                        &secondary_role_script, tws_topology_goal_none, TWSTOP_RULE_EVENT_ROLE_SWITCH),
    
    SCRIPT_GOAL_SUCCESS(tws_topology_goal_become_acting_primary, tws_topology_procedure_become_acting_primary,
                        &acting_primary_role_script, tws_topology_goal_none, TWSTOP_RULE_EVENT_ROLE_SWITCH),
    
    SCRIPT_GOAL(tws_topology_goal_set_address, tws_topology_procedure_set_address,
                &set_primary_address_script, tws_topology_goal_none),

    SCRIPT_GOAL(tws_topology_goal_set_primary_address_and_find_role, tws_topology_procedure_set_primary_address_and_find_role,
                &primary_address_find_role_script, tws_topology_goal_none),
    
    SCRIPT_GOAL(tws_topology_goal_role_switch_to_secondary, tws_topology_procedure_role_switch_to_secondary,
                &switch_to_secondary_script, tws_topology_goal_none),

    SCRIPT_GOAL(tws_topology_goal_no_role_find_role, tws_topology_procedure_no_role_find_role,
                &no_role_find_role_script, tws_topology_goal_none),
    
    GOAL(tws_topology_goal_cancel_find_role, tws_topology_procedure_cancel_find_role,
         &proc_cancel_find_role_fns, tws_topology_goal_none),

    SCRIPT_GOAL(tws_topology_goal_primary_find_role, tws_topology_procedure_primary_find_role,
                &primary_find_role_script, tws_topology_goal_none),

    SCRIPT_GOAL(tws_topology_goal_dfu_role, tws_topology_procedure_dfu_role,
                &dfu_role_script, tws_topology_goal_none),

    SCRIPT_GOAL(tws_topology_goal_dfu_primary, tws_topology_procedure_dfu_primary,
                &dfu_primary_after_boot_script, tws_topology_goal_none),

    SCRIPT_GOAL(tws_topology_goal_dfu_secondary, tws_topology_procedure_dfu_secondary,
                &dfu_secondary_after_boot_script, tws_topology_goal_none),

    SCRIPT_GOAL(tws_topology_goal_disconnect_peer_find_role, tws_topology_procedure_disconnect_peer_find_role,
                &disconnect_peer_find_role_script, tws_topology_goal_none),

    GOAL(tws_topology_goal_release_peer, tws_topology_procedure_release_peer, 
         &proc_release_peer_fns, tws_topology_goal_none),
         
    SCRIPT_GOAL_TIMEOUT_OR_FAILED(tws_topology_goal_handover_entering_case, tws_topology_procedure_handover_entering_case,
                        &handover_entering_case_script, tws_topology_goal_none, TWSTOP_RULE_EVENT_HANDOVER_INCASE_TIMEOUT, TWSTOP_RULE_EVENT_HANDOVER_INCASE_FAILED),

    SCRIPT_GOAL_TIMEOUT_OR_FAILED(tws_topology_goal_handover_incase_timeout, tws_topology_procedure_handover_entering_case_timeout,
                &handover_entering_case_timeout_script, tws_topology_goal_none, TWSTOP_RULE_EVENT_HANDOVER_INCASE_TIMEOUT, TWSTOP_RULE_EVENT_HANDOVER_INCASE_FAILED),

    SCRIPT_GOAL_CANCEL_SUCCESS_FAILED(tws_topology_goal_secondary_forced_role_switch, tws_topology_procedure_secondary_forced_role_switch,
                               &secondary_forced_role_switch_script, tws_topology_goal_none, TWSTOP_RULE_EVENT_ROLE_SWITCH, TWSTOP_RULE_EVENT_FORCED_ROLE_SWITCH_FAILED),

    SCRIPT_GOAL_CANCEL_FAILED(tws_topology_goal_primary_forced_role_switch, tws_topology_procedure_primary_forced_role_switch,
                       &primary_forced_role_switch_script, tws_topology_goal_none, TWSTOP_RULE_EVENT_FORCED_ROLE_SWITCH_FAILED),
};

/******************************************************************************
 * Callbacks for procedure confirmations
 *****************************************************************************/

/*! \brief Handle confirmation of procedure start.
    
    Provided as a callback to procedures.
*/
static void twsTopology_GoalProcStartCfm(tws_topology_procedure proc, proc_result_t result)
{
    DEBUG_LOG("twsTopology_GoalProcStartCfm proc 0x%x", proc);

    UNUSED(result);
}

/*! \brief Handle completion of a goal.
  
    Provided as a callback for procedures to use to indicate goal completion.

    Remove the goal and associated procedure from the lists tracking
    active ones.
    May generate events into the rules engine based on the completion
    result of the goal.
*/
static void twsTopology_GoalProcComplete(tws_topology_procedure proc, proc_result_t result)
{
    tws_topology_goal completed_goal = twsTopology_FindGoalForProcedure(proc);
    const tws_topology_goal_entry_t* goal_entry = twsTopology_GetGoalEntryFromGoal(completed_goal);

    DEBUG_LOG("twsTopology_GoalProcComplete proc 0x%x for goal 0x%x", proc, completed_goal);

    /* generate any events off the completion status of the goal */
    if ((proc_result_success == result) && (goal_entry->success_event))
    {
        DEBUG_LOG("twsTopology_GoalProcComplete generating success event 0x%x", goal_entry->success_event);
        twsTopology_RulesSetEvent(goal_entry->success_event);
    }
    if ((proc_result_timeout == result) && (goal_entry->timeout_event))
    {
        DEBUG_LOG("twsTopology_GoalProcComplete generating timeout event 0x%x", goal_entry->timeout_event);
        twsTopology_RulesSetEvent(goal_entry->timeout_event);
    }
    if ((proc_result_failed == result) && (goal_entry->failed_event))
    {
        DEBUG_LOG("twsTopology_GoalProcComplete generating failed event 0x%x", goal_entry->failed_event);
        twsTopology_RulesSetEvent(goal_entry->failed_event);
    }

    /* clear the goal from list of active goals, this may cause further
     * goals to be delivered from the pending_goal_queue_task */
    TwsTopology_ClearGoal(completed_goal);
}

/*! \brief Handle confirmation of goal cancellation.

    Provided as a callback for procedures to use to indicate cancellation has
    been completed.
*/
static void twsTopology_GoalProcCancelCfm(tws_topology_procedure proc, proc_result_t result)
{
    DEBUG_LOG("twsTopology_GoalProcCancelCfm proc 0x%x", proc);

    UNUSED(result);

    TwsTopology_ClearGoal(twsTopology_FindGoalForProcedure(proc));
}

/******************************************************************************
 * Goal and goal mask utility functions
 *****************************************************************************/

/*! \brief Reverse lookup of procedure to goal from the goals table. */
static tws_topology_goal twsTopology_FindGoalForProcedure(tws_topology_procedure proc)
{
    int goals_count = ARRAY_DIM(goals);
    int goal_index;

    for (goal_index = 0; goal_index < goals_count; goal_index++)
    {
        if (goals[goal_index].proc == proc)
        {
            return goals[goal_index].goal;
        }
    }
    DEBUG_LOG("twsTopology_FindGoalForProcedure failed to find goal for proc 0x%x", proc);
    Panic();
    return 0;
}

/*! \brief Validate that a goal only has a single bit set, and therefore is not a mask of goals.
    \return bool TRUE if there is 1 bit set in the goal, and only 1 bit.
*/
static bool twsTopology_NotGoalMask(tws_topology_goal goal)
{
    return (goal && !(goal & (goal-1)));
}

/*! \brief From a bitmask of goals return the least significant set goal.
 
    For example from a mask of goals with bit pattern 0b1010 it will return 0b10.
    Given an empty mask it will return 0.
*/
static tws_topology_goal twsTopology_FirstGoalFromMask(tws_topology_goal goal_mask)
{
    return ((goal_mask ^ (goal_mask-1)) & goal_mask);
}

/*! \brief Convert a tws_topology_goal enum to a 0..TWS_TOPOLOGY_GOALS_MAX-1 array index
*/
static int twsTopology_GetGoalIndexFromName(tws_topology_goal goal)
{
    int index = 0;

    /* validate parameter is a single goal and not a mask */
    PanicFalse(twsTopology_NotGoalMask(goal));

    while (goal)
    {
        if (goal & 0x1)
        {
            break;
        }
        goal >>= 1;
        index++;
    }
    if (index >= TWS_TOPOLOGY_GOALS_MAX)
    {
        DEBUG_LOG("twsTopology_GetGoalIndexFromName invalid goal 0x%x", goal);
        Panic();
    }
    return index;
}

/*! \brief Return a pointer to a goal_entry for a given goal.
*/
static const tws_topology_goal_entry_t* twsTopology_GetGoalEntryFromGoal(tws_topology_goal goal)
{
    int index = twsTopology_GetGoalIndexFromName(goal);
    return &goals[index];
}

/******************************************************************************
 * Goal Queue Handling
 *****************************************************************************/

/*! \brief Determine if a goal is scripted. */
static bool twsTopology_IsScriptedGoal(tws_topology_goal goal)
{
    const tws_topology_goal_entry_t* goal_entry = twsTopology_GetGoalEntryFromGoal(goal);
    return (goal_entry->proc_script != NULL);
}

/*! \brief Determine if a goal is a type that will cancel in-progress goals. */
static bool twsTopology_GoalCancelsActive(tws_topology_goal goal)
{
    const tws_topology_goal_entry_t* goal_entry = twsTopology_GetGoalEntryFromGoal(goal);
    return (goal_entry->contention == goal_contention_cancel);
}

/*! \brief Determine if a goal is a type that will cancel in-progress goals. */
static bool twsTopology_GoalRunsConcurrently(tws_topology_goal goal)
{
    const tws_topology_goal_entry_t* goal_entry = twsTopology_GetGoalEntryFromGoal(goal);
    return (goal_entry->contention == goal_contention_concurrent);
}

/*! \brief return TRUE if goal is already active. */
static bool twsTopology_GoalAlreadyActive(tws_topology_goal goal)
{
    return TwsTopologyGetTaskData()->active_goals_mask & goal;
}

/*! \brief Put goal onto queue to be re-delivered once wait_mask goals are cleared.
    
    Active goals may become cleared either by normal completion or due to
    cancellation.
*/
static void twsTopology_QueueGoal(MessageId rule_id, Message goal_data, size_t goal_data_size,
                                  tws_topology_goal wait_mask, tws_topology_goal new_goal)
{
    twsTopologyTaskData* td = TwsTopologyGetTaskData();
    int index = twsTopology_GetGoalIndexFromName(new_goal);

    DEBUG_LOG("twsTopology_QueueGoal rule_id 0x%x", rule_id);

    td->pending_goal_lock_mask[index] = wait_mask;
    td->pending_goal_lock_id[index] = rule_id;

    if (goal_data)
    {
        void* message = PanicUnlessMalloc(goal_data_size);
        memcpy(message, goal_data, goal_data_size);
        MessageSendConditionallyOnTask(&td->pending_goal_queue_task, rule_id, message,
                                       (Task*)&td->pending_goal_lock_mask[index]);
    }
    else
    {
        MessageSendConditionallyOnTask(&td->pending_goal_queue_task, rule_id, NULL,
                                       (Task*)&td->pending_goal_lock_mask[index]);
    }

    td->pending_goal_queue_size++;
}

/*! \brief Remove goal message from queue and update queued goal count. */
static void twsTopology_DequeGoal(tws_topology_goal goal)
{
    twsTopologyTaskData* td = TwsTopologyGetTaskData();
    int index = twsTopology_GetGoalIndexFromName(goal);
    int cancelled_count = 0;

    cancelled_count = MessageCancelAll(&td->pending_goal_queue_task, td->pending_goal_lock_id[index]);
    td->pending_goal_queue_size -= cancelled_count;

    DEBUG_LOG("twsTopology_DequeGoal id 0x%x count %u queue size %u", td->pending_goal_lock_id[index],
                                                                      cancelled_count,
                                                                      td->pending_goal_queue_size);
}

/*! \brief Find the number of goals in the #pending_goal_queue_task. */
static int twsTopology_GoalQueueSize(void)
{
    twsTopologyTaskData* td = TwsTopologyGetTaskData();
    return td->pending_goal_queue_size;
}

/******************************************************************************
 * Goal processing functions; add/cancel/clear/start
 *****************************************************************************/

/*! \brief Decide if a goal can be run now

    Goals can NOT be started if
    * a goal is alredy running
    * there are queued goals, and this is a new goal

    \return TRUE if a goal can be started immediately
  */ 
static bool twsTopology_GoalCanRunNow(bool is_new_goal)
{
    twsTopologyTaskData *td = TwsTopologyGetTaskData();
    return (   (td->active_goals_mask == tws_topology_goal_none)
            && (   (twsTopology_GoalQueueSize() == 0)
                || !is_new_goal));
}

/*! \brief Cancel a single goal. */
static void twsTopology_CancelGoal(tws_topology_goal cancel_goal)
{
    const tws_topology_goal_entry_t* cancel_goal_entry = twsTopology_GetGoalEntryFromGoal(cancel_goal);

    PanicFalse(twsTopology_NotGoalMask(cancel_goal));

    if (twsTopology_IsScriptedGoal(cancel_goal))
    {
        DEBUG_LOG("twsTopology_CancelGoal cancelling scripted goal 0x%x", cancel_goal);
        TwsTopology_ProcScriptEngineCancel(TwsTopologyGetTask(),
                                           twsTopology_GoalProcCancelCfm);
    }
    else
    {
        DEBUG_LOG("twsTopology_CancelGoal cancelling singular goal 0x%x", cancel_goal);
        cancel_goal_entry->proc_fns->proc_cancel_fn(twsTopology_GoalProcCancelCfm);
    }
}

/*! \brief Cancel any active or queued exclusive goals.
    \param new_goal The new goal to be achieved and considered here for exclusive property.
    \param wait_mask[out] tws_topology_goal Mask of goals that must complete before running new goal.
    \return bool TRUE if the goal has exclusive goals, FALSE otherwise.
*/
static bool twsTopology_HandleExclusiveGoals(tws_topology_goal new_goal,
                                             tws_topology_goal* wait_mask)
{
    const tws_topology_goal_entry_t* new_goal_entry = twsTopology_GetGoalEntryFromGoal(new_goal);
    tws_topology_goal exclusive_goal = new_goal_entry->exclusive_goal;
    bool rc = FALSE;

    /* does the goal have an exclusive goal specified and that exclusive goal
     * is currently active? */
    if (    (exclusive_goal != tws_topology_goal_none)
         && (TwsTopology_IsGoalActive(exclusive_goal)))
    {
        DEBUG_LOG("twsTopology_HandleExclusiveGoals exclusive goal 0x%x", new_goal);
        /* clear any pending queued exclusive goals */
        twsTopology_DequeGoal(exclusive_goal);

        /* cancel active exclusive goals */ 
        twsTopology_CancelGoal(exclusive_goal);

        /* if exclusive goal synchronously cancelled remove it
         * from the wait mask */
        if (!TwsTopology_IsGoalActive(exclusive_goal))
        {
            REMOVE_GOAL_FROM_MASK(*wait_mask, exclusive_goal);
        }
        rc = TRUE;
    }

    return rc;
}

/*! \brief Cancel all active and queued goals.
    \param new_goal The new goal to be achieved and considered here for cancellation property.
    \param wait_mask[out] tws_topology_goal Mask of goals that must complete cancellation before running new goal.
    \return bool TRUE if the goal has caused cancellation, FALSE otherwise.
*/
static bool twsTopology_HandleCancellationGoals(tws_topology_goal new_goal,
                                                tws_topology_goal* wait_mask)
{
    twsTopologyTaskData* td = TwsTopologyGetTaskData();
    tws_topology_goal working_cancel_mask = td->active_goals_mask;
    int count = 0;
    bool rc = FALSE;

    if (twsTopology_GoalCancelsActive(new_goal))
    {
        /* cancel anything waiting on the pending queue */
        count = MessageFlushTask(&td->pending_goal_queue_task);
        td->pending_goal_queue_size = 0;
        DEBUG_LOG("twsTopology_CancelAllGoals %u cancelled", count);

        /* cancel anything active */
        while (working_cancel_mask)
        {
            tws_topology_goal goal = twsTopology_FirstGoalFromMask(working_cancel_mask);
            twsTopology_CancelGoal(goal);
            REMOVE_GOAL_FROM_MASK(working_cancel_mask, goal);
        }

        /* whatever hasn't synchronously completed cancellation must
         * be waited to complete before executing new cancel goal */
        *wait_mask = td->active_goals_mask;

        rc = TRUE;
    }

    return rc;
}

/*! \brief Remove from the wait mask any goals with which the new goal *can* run concurrently.
*/
static bool twsTopology_HandleConcurrentGoals(tws_topology_goal new_goal,
                                              tws_topology_goal* wait_mask)
{
    const tws_topology_goal_entry_t* goal = twsTopology_GetGoalEntryFromGoal(new_goal);
    tws_topology_goal concurrent_goals_mask = goal->concurrent_goals_mask;
    bool rc = FALSE;

    if (twsTopology_GoalRunsConcurrently(new_goal))
    {
        *wait_mask &= ~(concurrent_goals_mask);

        DEBUG_LOG("twsTopology_HandleConcurrentGoals Ok goal 0x%x mask 0x%x", new_goal, concurrent_goals_mask);
        rc = TRUE;
    }
    else
    {
        DEBUG_LOG("twsTopology_HandleConcurrentGoals Issue failed 0x%x contention:%d mask 0x%x", new_goal, goal->contention, concurrent_goals_mask);
    }

    return rc;
}

/*! \brief Start the procedure for achieving a goal.
 
    Handles starting either of the two types of procedure definitions
    we have, either singlular or scripted.
*/
static void twsTopology_StartGoal(tws_topology_goal goal, Message goal_data)
{
    twsTopologyTaskData* td = TwsTopologyGetTaskData();
    const tws_topology_goal_entry_t* goal_entry = twsTopology_GetGoalEntryFromGoal(goal);

    DEBUG_LOG("twsTopology_StartGoal goal 0x%x", goal);

    ADD_GOAL_TO_MASK(td->active_goals_mask, goal);

    if (twsTopology_IsScriptedGoal(goal))
    {
        /* start scripted goal */
        TwsTopology_ProcScriptEngineStart(TwsTopologyGetTask(),
                goal_entry->proc_script,
                goal_entry->proc,
                twsTopology_GoalProcStartCfm,
                twsTopology_GoalProcComplete);
    }
    else
    {
        /* start single goal */
        goal_entry->proc_fns->proc_start_fn(TwsTopologyGetTask(),
                twsTopology_GoalProcStartCfm,
                twsTopology_GoalProcComplete,
                goal_data);
    }
}

/*! \brief Handle new goal set by topology rules.

    Deals with contention between new goal and any currently active goals.
    Existing active goals may require cancellation (depending on the requirements
    of the new goal), or may just be left to complete in the normal manner.

    Where cancellation or completion of existing active goals is required
    the new goal will be queued and re-delivered once conditions meet those
    required by the goal.
*/
static void TwsTopology_AddGoal(tws_topology_goal new_goal, MessageId rule_id, 
                                Message goal_data, size_t goal_data_size, 
                                bool is_new_goal)
{
    twsTopologyTaskData* td = TwsTopologyGetTaskData();

    DEBUG_LOG("TwsTopology_AddGoal new_goal 0x%x existing goals 0x%x", new_goal, td->active_goals_mask);
    DEBUG_LOG("TwsTopology_AddGoal pending_queue_size %u", td->pending_goal_queue_size);

    if (twsTopology_GoalCanRunNow(is_new_goal))
    {
        DEBUG_LOG("TwsTopology_AddGoal start 0x%x immediately", new_goal);
        /* no goals active, just start this new one */
        twsTopology_StartGoal(new_goal, goal_data);
    }
    else
    {
        /* default behaviour if goals cannot run yet is to be queued
         * pending completion of active goals. Setup a wait mask with
         * active goals, it may be modified by consideration of cancellation,
         * exclusive or concurrent goals later. */
        tws_topology_goal wait_mask = td->active_goals_mask;
        DEBUG_LOG("TwsTopology_AddGoal must queue, wait_mask 0x%x", wait_mask);

        /* first see if this goal will cancel everything */
        if (twsTopology_HandleCancellationGoals(new_goal, &wait_mask))
        {
            DEBUG_LOG("TwsTopology_AddGoal 0x%x is a cancel goal", new_goal);
            /* start immediately if possible, else queue and stop processing */
            if (!wait_mask && twsTopology_GoalCanRunNow(is_new_goal))
            {
                twsTopology_StartGoal(new_goal, goal_data);
            }
            else
            {
                twsTopology_QueueGoal(rule_id, goal_data, goal_data_size, wait_mask, new_goal);
            }
        }
        else
        {
            /* remove any goals from the wait_mask with which the new goal
             * can run concurrently, i.e. this goal will not need to wait
             * for them to complete in order to start. */
            bool concurrent = twsTopology_HandleConcurrentGoals(new_goal, &wait_mask);

            DEBUG_LOG("TwsTopology_AddGoal after conncurrent handling, wait_mask 0x%x", wait_mask);

            /* handle cancellation of any exclusive goals specified by
             * the new goal */
            twsTopology_HandleExclusiveGoals(new_goal, &wait_mask);

            DEBUG_LOG("TwsTopology_AddGoal after exclusive handling, wait_mask 0x%x", wait_mask);

            /* if goal supports concurrency and the wait_mask is clear then the
             * goal can be started now
             * OR 
             * anything that needed cancelling has completed synchronously
             * then we may be able to start now
             *
             * otherwise queue the goal on
             * completion of the remaining goals in the wait_mask 
             */
            if (   (concurrent && !wait_mask)
                || twsTopology_GoalCanRunNow(is_new_goal))
            {
                twsTopology_StartGoal(new_goal, goal_data);
            }
            else 
            {
                twsTopology_QueueGoal(rule_id, goal_data, goal_data_size, wait_mask, new_goal);
            }
        }
    }
}

/*! \brief Clear a goal from the list of current goals. 
    \param[in] goal Type of goal to clear from the list.

    \note Clearing a goal may be due to successful completion
    or successful cancellation.
*/
static void TwsTopology_ClearGoal(tws_topology_goal goal)
{
    twsTopologyTaskData* td = TwsTopologyGetTaskData();
    DEBUG_LOG("TwsTopology_ClearGoal goal 0x%x", goal);
    REMOVE_GOAL_FROM_MASK(td->active_goals_mask, goal);

    /*! clear this goal from all pending goal locks,
        may result in a queued goal being delivered to start */
    for (int i=0; i< TWS_TOPOLOGY_GOALS_MAX; i++)
    {
        REMOVE_GOAL_FROM_MASK(td->pending_goal_lock_mask[i], goal);
    }
}

/*! \brief Find and return the relevant handover goal,by mapping the 
           HDMA reason code to topology goal. 
    \param[in] HDMA reason code
*/
tws_topology_goal twstopology_GetHandoverGoal(hdma_handover_reason_t reason)
{
    DEBUG_LOG("Twstopology_GetHandoverGoal for: %d",reason);
    tws_topology_goal goal = tws_topology_goal_none;

    switch(reason)
    {
        case HDMA_HANDOVER_REASON_IN_CASE:
            goal = tws_topology_goal_handover_entering_case;
        break;
        default:
            DEBUG_LOG("twstopology_GetHandoverGoal invalid HDMA handover reason code 0x%x", reason);
            Panic();
        break;
    }

    return goal;
}

/*! \brief Determine if a goal is currently being executed. */
bool TwsTopology_IsGoalActive(tws_topology_goal goal)
{
    return (TwsTopologyGetTaskData()->active_goals_mask & goal);
}

/*! \brief Given a new goal decision from a rules engine, find the goal and attempt to start it. */
void TwsTopology_HandleGoalDecision(Task task, MessageId id, Message message)
{
    twsTopologyTaskData* td = TwsTopologyGetTaskData();
    bool new_goal = (task != &td->pending_goal_queue_task);

    DEBUG_LOG("TwsTopology_HandleGoalDecision id 0x%x", id);

    /* decrement goal queue if this was delivered from it rather
     * than externally from the rules engine */
    if (!new_goal)
    {
        td->pending_goal_queue_size--;
    }

    switch (id)
    {
        case TWSTOP_PRIMARY_GOAL_FORCED_ROLE_SWITCH:
            TwsTopology_AddGoal(tws_topology_goal_primary_forced_role_switch, id, NULL, 0, new_goal);
            break;

        case TWSTOP_PRIMARY_GOAL_PAIR_PEER:
            TwsTopology_AddGoal(tws_topology_goal_pair_peer, id, NULL, 0, new_goal);
            break;

        case TWSTOP_PRIMARY_GOAL_SET_PRIMARY_ADDRESS:
            TwsTopology_AddGoal(tws_topology_goal_set_address, id, PROC_SET_ADDRESS_TYPE_DATA_PRIMARY, sizeof(SET_ADDRESS_TYPE_T), new_goal);
            break;

        case TWSTOP_PRIMARY_GOAL_SET_PRIMARY_ADDRESS_FIND_ROLE:
            TwsTopology_AddGoal(tws_topology_goal_set_primary_address_and_find_role, id, NULL, 0, new_goal);
            break;

        case TWSTOP_PRIMARY_GOAL_FIND_ROLE:
            TwsTopology_AddGoal(tws_topology_goal_find_role, id, PROC_FIND_ROLE_TIMEOUT_DATA_TIMEOUT, sizeof(FIND_ROLE_PARAMS_T), new_goal);
            break;

        case TWSTOP_PRIMARY_GOAL_CANCEL_FIND_ROLE:
            TwsTopology_AddGoal(tws_topology_goal_cancel_find_role, id, NULL, 0, new_goal);
            break;

        case TWSTOP_SECONDARY_GOAL_FIND_ROLE:
            TwsTopology_AddGoal(tws_topology_goal_find_role, id, PROC_FIND_ROLE_TIMEOUT_DATA_TIMEOUT, sizeof(FIND_ROLE_PARAMS_T), new_goal);
            break;

        case TWSTOP_SECONDARY_GOAL_CONNECT_PEER:
            TwsTopology_AddGoal(tws_topology_goal_secondary_connect_peer, id, NULL, 0, new_goal);
            break;

        case TWSTOP_SECONDARY_GOAL_NO_ROLE_IDLE:
        case TWSTOP_DFU_GOAL_NO_ROLE_IDLE:
            TwsTopology_AddGoal(tws_topology_goal_no_role_idle, id, NULL, 0, new_goal);
            break;

        case TWSTOP_SECONDARY_GOAL_NO_ROLE_FIND_ROLE:
        case TWSTOP_DFU_GOAL_NO_ROLE_FIND_ROLE:
            TwsTopology_AddGoal(tws_topology_goal_no_role_find_role, id, NULL, 0, new_goal);
            break;

        case TWSTOP_PRIMARY_GOAL_NO_ROLE_IDLE:
            TwsTopology_AddGoal(tws_topology_goal_no_role_idle, id, NULL, 0, new_goal);
            break;

        case TWSTOP_PRIMARY_GOAL_BECOME_PRIMARY:
            TwsTopology_AddGoal(tws_topology_goal_become_primary, id, NULL, 0, new_goal);
            break;

        case TWSTOP_PRIMARY_GOAL_BECOME_SECONDARY:
            TwsTopology_AddGoal(tws_topology_goal_become_secondary, id, NULL, 0, new_goal);
            break;

        case TWSTOP_PRIMARY_GOAL_BECOME_ACTING_PRIMARY:
            TwsTopology_AddGoal(tws_topology_goal_become_acting_primary, id, NULL, 0, new_goal);
            break;

        case TWSTOP_PRIMARY_GOAL_ROLE_SWITCH_TO_SECONDARY:
            TwsTopology_AddGoal(tws_topology_goal_role_switch_to_secondary, id, NULL, 0, new_goal);
            break;

        case TWSTOP_PRIMARY_GOAL_PRIMARY_FIND_ROLE:
            TwsTopology_AddGoal(tws_topology_goal_primary_find_role, id, PROC_FIND_ROLE_TIMEOUT_DATA_CONTINUOUS, sizeof(FIND_ROLE_PARAMS_T), new_goal);
            break;

        case TWSTOP_PRIMARY_GOAL_CONNECT_PEER_PROFILES:
            TwsTopology_AddGoal(tws_topology_goal_primary_connect_peer_profiles, id, message, sizeof(TWSTOP_PRIMARY_GOAL_CONNECT_PEER_PROFILES_T), new_goal);
            break;

        case TWSTOP_PRIMARY_GOAL_CONNECTABLE_PEER:
            TwsTopology_AddGoal(tws_topology_goal_primary_connectable_peer, id, message, sizeof(TWSTOP_PRIMARY_GOAL_CONNECTABLE_PEER_T), new_goal);
            break;

        case TWSTOP_PRIMARY_GOAL_DISCONNECT_PEER_PROFILES:
            TwsTopology_AddGoal(tws_topology_goal_primary_disconnect_peer_profiles, id, message, sizeof(TWSTOP_PRIMARY_GOAL_DISCONNECT_PEER_PROFILES_T), new_goal);
            break;

        case TWSTOP_PRIMARY_GOAL_RELEASE_PEER:
            TwsTopology_AddGoal(tws_topology_goal_release_peer, id, NULL, 0, new_goal);
            break;

        case TWSTOP_PRIMARY_GOAL_CONNECTABLE_HANDSET:
            TwsTopology_AddGoal(tws_topology_goal_connectable_handset, id, message, sizeof(TWSTOP_PRIMARY_GOAL_CONNECTABLE_HANDSET_T), new_goal);
            break;
        
        case TWSTOP_PRIMARY_GOAL_CONNECT_HANDSET:
            TwsTopology_AddGoal(tws_topology_goal_connect_handset, id, message, sizeof(TWSTOP_PRIMARY_GOAL_CONNECT_HANDSET_T), new_goal);
            break;

        case TWSTOP_PRIMARY_GOAL_DISCONNECT_PEER_FIND_ROLE:
            TwsTopology_AddGoal(tws_topology_goal_disconnect_peer_find_role, id, NULL, 0, new_goal);
            break;
        
        case TWSTOP_PRIMARY_GOAL_HANDOVER_START:
            TwsTopology_AddGoal(twstopology_GetHandoverGoal(td->handover_info.hdma_message.reason), id, NULL, 0, new_goal);
            break;

        case TWSTOP_PRIMARY_GOAL_HANDOVER_INCASE_TIMEOUT:
            TwsTopology_AddGoal(tws_topology_goal_handover_incase_timeout, id, NULL, 0, new_goal);
            break;
#if 0
        case TWSTOP_PRIMARY_GOAL_DISCONNECT_HANDSET:
            TwsTopology_AddGoal(tws_topology_goal_disconnect_handset, id, message, new_goal);
            break;
#endif

        case TWSTOP_PRIMARY_GOAL_DFU_ROLE:
        case TWSTOP_SECONDARY_GOAL_DFU_ROLE:
            TwsTopology_AddGoal(tws_topology_goal_dfu_role, id, message, 0, new_goal);
            break;

        case TWSTOP_DFU_GOAL_SECONDARY:
            TwsTopology_AddGoal(tws_topology_goal_dfu_secondary, id, message, 0, new_goal);
            break;

        case TWSTOP_DFU_GOAL_PRIMARY:
            TwsTopology_AddGoal(tws_topology_goal_dfu_primary, id, message, 0, new_goal);
            break;

        case TWSTOP_SECONDARY_GOAL_FORCE_SECONDARY_ROLE_SWITCH:
            TwsTopology_AddGoal(tws_topology_goal_secondary_forced_role_switch, TWSTOP_SECONDARY_GOAL_FORCE_SECONDARY_ROLE_SWITCH, NULL, 0, new_goal);
            break;

        default:
            DEBUG_LOG("TwsTopology_HandleGoalDecision, unknown goal decision %d(0x%x)", id, id);
            break;
    }

    /* Always mark the rule as complete, once the goal has been added.
     * Important to do it now, as some goals may change the role and therefore
     * the rule engine which generated the goal and in which the completion must
     * be marked. */
    twsTopology_RulesMarkComplete(id);
}
