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
#include "tws_topology_primary_rules.h"
#include "tws_topology_secondary_rules.h"
#include "tws_topology_dfu_rules.h"
#include "tws_topology_goals.h"
#include "tws_topology_rule_events.h"
#include "tws_topology_procedures.h"
#include "tws_topology_procedure_pair_peer.h"
#include "tws_topology_procedure_find_role.h"
#include "tws_topology_procedure_pri_connect_peer_profiles.h"
#include "tws_topology_procedure_sec_connect_peer.h"
#include "tws_topology_procedure_pri_connectable_peer.h"
#include "tws_topology_procedure_disconnect_peer_profiles.h"
#include "tws_topology_procedure_connectable_handset.h"
#include "tws_topology_procedure_permit_bt.h"
#include "tws_topology_procedure_prohibit_bt.h"
#include "tws_topology_procedure_set_address.h"
#include "tws_topology_procedure_set_role.h"
#include "tws_topology_procedure_no_role_idle.h"
#include "tws_topology_procedure_connect_handset.h"
#include "tws_topology_procedure_disconnect_handset.h"
#include "tws_topology_procedure_primary_role.h"
#include "tws_topology_procedure_acting_primary_role.h"
#include "tws_topology_procedure_secondary_role.h"
#include "tws_topology_procedure_primary_addr_find_role.h"
#include "tws_topology_procedure_switch_to_secondary.h"
#include "tws_topology_procedure_no_role_find_role.h"
#include "tws_topology_procedure_cancel_find_role.h"
#include "tws_topology_procedure_primary_find_role.h"
#include "tws_topology_procedure_disconnect_peer_find_role.h"
#include "tws_topology_procedure_dfu_role.h"
#include "tws_topology_procedure_dfu_secondary_after_boot.h"
#include "tws_topology_procedure_dfu_primary_after_boot.h"

#include <logging.h>

#include <message.h>
#include <panic.h>

#pragma unitsuppress Unused

static int twsTopology_GetGoalIndexFromName(tws_topology_goal goal);

#define ADD_ACTIVE_GOAL(goal)           (TwsTopologyGetTaskData()->active_goals |= (goal))
#define REMOVE_ACTIVE_GOAL(goal)        (TwsTopologyGetTaskData()->active_goals &= (~goal))
#define ADD_ACTIVE_PROCEDURE(proc)      (TwsTopologyGetTaskData()->active_procedures |= (proc))
#define REMOVE_ACTIVE_PROCEDURE(proc)   (TwsTopologyGetTaskData()->active_procedures &= (~proc))

/*! The is currently a link between the ordering of entries in this table
 * and the ordering of entries in the tws_topology_goal enum.
 * \todo find a way (XMACRO?) to ensure this cannot be broken and/or compile time
 * assert if it does get broken. */
const tws_topology_goal_entry_t goals[] =
{
    SCRIPT_GOAL(tws_topology_goal_pair_peer, tws_topology_procedure_pair_peer_script,
         &pair_peer_script, tws_topology_goal_none),

    GOAL(tws_topology_goal_find_role, tws_topology_procedure_find_role,
         &proc_find_role_fns, tws_topology_goal_none),

    GOAL_TIMEOUT(tws_topology_goal_secondary_connect_peer, tws_topology_procedure_sec_connect_peer,
         &proc_sec_connect_peer_fns, tws_topology_goal_none, TWSTOP_RULE_EVENT_FAILED_PEER_CONNECT),

    GOAL(tws_topology_goal_primary_connect_peer_profiles, tws_topology_procedure_pri_connect_peer_profiles,
         &proc_pri_connect_peer_profiles_fns, tws_topology_goal_primary_disconnect_peer_profiles),

    GOAL(tws_topology_goal_primary_disconnect_peer_profiles, tws_topology_procedure_disconnect_peer_profiles,
         &proc_disconnect_peer_profiles_fns, tws_topology_goal_primary_connect_peer_profiles),

    GOAL(tws_topology_goal_primary_connectable_peer, tws_topology_procedure_pri_connectable_peer,
         &proc_pri_connectable_peer_fns, tws_topology_goal_none),

    SCRIPT_GOAL_CANCEL(tws_topology_goal_no_role_idle, tws_topology_procedure_no_role_idle,
         &no_role_idle_script, tws_topology_goal_none),

    GOAL(tws_topology_goal_set_role, tws_topology_procedure_set_role,
         &proc_set_role_fns, tws_topology_goal_none),

    GOAL(tws_topology_goal_connect_handset, tws_topology_procedure_connect_handset,
         &proc_connect_handset_fns, tws_topology_goal_none),

    GOAL(tws_topology_goal_disconnect_handset, tws_topology_procedure_disconnect_handset,
         &proc_disconnect_handset_fns, tws_topology_goal_none),

    GOAL(tws_topology_goal_connectable_handset, tws_topology_procedure_connectable_handset,
         &proc_connectable_handset_fns, tws_topology_goal_none),

    SCRIPT_GOAL_SUCCESS(tws_topology_goal_become_primary, tws_topology_procedure_become_primary,
                &primary_role_script, tws_topology_goal_none, TWSTOP_RULE_EVENT_ROLE_SWITCH),

    SCRIPT_GOAL_SUCCESS(tws_topology_goal_become_secondary, tws_topology_procedure_become_secondary,
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

};

/*! \brief Clear a goal from the list of current goals. 
    \param[in] goal Type of goal to clear from the list.

    \note Clearing a goal may be due to successful completion
    or successful cancellation.
*/
static void TwsTopology_ClearGoal(tws_topology_goal goal)
{
    twsTopologyTaskData* td = TwsTopologyGetTaskData();
    DEBUG_LOG("TwsTopology_ClearGoal goal 0x%x", goal);
    REMOVE_ACTIVE_GOAL(goal);
}

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

static void twsTopology_GoalProcStartCfm(tws_topology_procedure proc, proc_result_t result)
{
    DEBUG_LOG("twsTopology_GoalProcStartCfm proc 0x%x", proc);

    UNUSED(result);

    /* record the procedure as active */
    ADD_ACTIVE_PROCEDURE(proc);
}

/*! \brief Handle completion of a goal.
    
    Remove the goal and associated procedure from the lists tracking
    active ones.
    May generate events into the rules engine based on the completion
    result of the goal.
*/
static void twsTopology_GoalProcComplete(tws_topology_procedure proc, proc_result_t result)
{
    tws_topology_goal completed_goal = twsTopology_FindGoalForProcedure(proc);
    int index = twsTopology_GetGoalIndexFromName(completed_goal);

    DEBUG_LOG("twsTopology_GoalProcComplete proc 0x%x for goal 0x%x", proc, completed_goal);

    REMOVE_ACTIVE_PROCEDURE(proc);
    
    /* goals were cleared before considering further events that might need
     * generating based on the result, this has since moved to afterwards, but
     * strongly suspect this will break.... */
//    TwsTopology_ClearGoal(completed_goal);

    /* generate any events off the completion status of the goal */
    if ((proc_result_success == result) && (goals[index].success_event))
    {
        DEBUG_LOG("twsTopology_GoalProcComplete generating success event 0x%x", goals[index].success_event);
        twsTopology_RulesSetEvent(goals[index].success_event);
    }
    if ((proc_result_timeout == result) && (goals[index].timeout_event))
    {
        DEBUG_LOG("twsTopology_GoalProcComplete generating timeout event 0x%x", goals[index].timeout_event);
        twsTopology_RulesSetEvent(goals[index].timeout_event);
    }
    if ((proc_result_failed == result) && (goals[index].failed_event))
    {
        DEBUG_LOG("twsTopology_GoalProcComplete generating failed event 0x%x", goals[index].failed_event);
        twsTopology_RulesSetEvent(goals[index].failed_event);
    }

    /* clear the goal from list of active goals, this may cause further
     * goals to be delivered from the pending_goal_queue */
    TwsTopology_ClearGoal(completed_goal);
}

static void twsTopology_GoalProcCancelCfm(tws_topology_procedure proc, proc_result_t result)
{
    DEBUG_LOG("twsTopology_GoalProcCancelCfm proc 0x%x", proc);

    UNUSED(result);

    REMOVE_ACTIVE_PROCEDURE(proc);
    
    TwsTopology_ClearGoal(twsTopology_FindGoalForProcedure(proc));
}

/*! \brief Find the index in the goals table for a given goal. */
static int twsTopology_GetGoalIndexFromName(tws_topology_goal goal)
{
    int index = 0;

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

/*! \brief Determine if a goal is scripted. */
static bool twsTopology_IsScriptedGoal(tws_topology_goal goal)
{
    int index = twsTopology_GetGoalIndexFromName(goal);
    return (goals[index].proc_script != NULL);
}

/*! \brief Determine if a goal is a type that will cancel in-progress goals. */
static bool twsTopology_GoalCancelsActive(tws_topology_goal goal)
{
    int index = twsTopology_GetGoalIndexFromName(goal);
    return (goals[index].contention == goal_contention_queue_cancel_run);
}

/*! return TRUE if goal is already active. */
static bool twsTopology_GoalAlreadyActive(tws_topology_goal goal)
{
    return TwsTopologyGetTaskData()->active_goals & goal;
}

#if 0
static void twsTopology_HandleExclusiveGoals(tws_topology_goal goal)
{
    twsTopologyTaskData *td = TwsTopologyGetTaskData();
    int index = twsTopology_GetGoalIndexFromName(goal);
    tws_topology_goal exclusive_goal = goals[index].exclusive_goal;

    /* does the goal have an exclusive goal specified and that exclusive goal
     * is currently active? */
    if (    (exclusive_goal != tws_topology_goal_none)
         && (td->active_goals & goals[index].exclusive_goal))
    {
        /* call cleanup on the exclusive goal procedure (synchronous) and remove
         * the goal from active list */
        int exclusive_goal_index = twsTopology_GetGoalIndexFromName(exclusive_goal);
        if (goals[exclusive_goal_index].proc_fns->proc_cleanup_fn)
        {
            goals[exclusive_goal_index].proc_fns->proc_cleanup_fn();
        }
        else
        {
            DEBUG_LOG("twsTopology_HandleExclusiveGoals cleanup expected for goal %x but no cleaup()");
            Panic();
        }
        REMOVE_ACTIVE_GOAL(goals[index].exclusive_goal);
    }
}
#endif

/*! \brief Is the specified goal already running? */
static bool twsTopology_HandleAlreadyActiveGoal(tws_topology_goal goal, Message goal_data)
{
    int index = twsTopology_GetGoalIndexFromName(goal);
    if (goals[index].proc_fns->proc_update_fn)
    {
        goals[index].proc_fns->proc_update_fn(goal_data);
        return TRUE;
    }
    return FALSE;
}

static void twsTopology_CancelActiveGoals(void)
{
    twsTopologyTaskData* td = TwsTopologyGetTaskData();
    int index = twsTopology_GetGoalIndexFromName(td->active_goals);
    int count = 0;

    /* cancel anything waiting on the pending queue */
    count = MessageFlushTask(&td->pending_goal_queue);

    DEBUG_LOG("twsTopology_CancelActiveGoals %u cancelled", count);

    /* cancel anything active */
    if (twsTopology_IsScriptedGoal(goals[index].goal))
    {
        DEBUG_LOG("twsTopology_CancelActiveGoals cancelling scripted goal 0x%x", goals[index].goal);
        /* start scripted goal */
        TwsTopology_ProcScriptEngineCancel(TwsTopologyGetTask(),
                                           twsTopology_GoalProcCancelCfm);
    }
    else
    {
        DEBUG_LOG("twsTopology_CancelActiveGoals cancelling singular goal 0x%x", goals[index].goal);
        /* start single goal */
        goals[index].proc_fns->proc_cancel_fn(twsTopology_GoalProcCancelCfm);
    }
}

/*! \brief Put goal onto queue to be re-delivered once no goals are active.
    
    Active goals may become cleared either by normal completion or due to
    cancellation.
*/
static void twsTopology_QueueGoal(MessageId rule_id, Message goal_data, size_t goal_data_size)
{
    twsTopologyTaskData* td = TwsTopologyGetTaskData();

    DEBUG_LOG("twsTopology_QueueGoal rule_id 0x%x", rule_id);

    if (goal_data)
    {
        void* message = PanicUnlessMalloc(goal_data_size);
        memcpy(message, goal_data, goal_data_size);
        MessageSendConditionallyOnTask(&td->pending_goal_queue, rule_id, message, (Task*)&td->active_goals);
    }
    else
    {
        MessageSendConditionallyOnTask(&td->pending_goal_queue, rule_id, NULL, (Task*)&td->active_goals);
    }
}

/*! \brief Start the procedure for achieving a goal.
 
    Handles starting either of the two types of procedure definitions
    we have, either singlular or scripted.
*/
static void twsTopology_StartGoal(tws_topology_goal goal, Message goal_data)
{
    int index = twsTopology_GetGoalIndexFromName(goal);

    DEBUG_LOG("twsTopology_StartGoal goal 0x%x", goal);

    ADD_ACTIVE_GOAL(goal);

    if (twsTopology_IsScriptedGoal(goal))
    {
        /* start scripted goal */
        TwsTopology_ProcScriptEngineStart(TwsTopologyGetTask(),
                goals[index].proc_script,
                goals[index].proc,
                twsTopology_GoalProcStartCfm,
                twsTopology_GoalProcComplete);
    }
    else
    {
        /* start single goal */
        goals[index].proc_fns->proc_start_fn(TwsTopologyGetTask(),
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
    the new goal will be queued and re-delivered once thre are no active
    goals.
*/
static void TwsTopology_AddGoal(tws_topology_goal goal, MessageId rule_id, Message goal_data, size_t goal_data_size)
{
    twsTopologyTaskData* td = TwsTopologyGetTaskData();

    DEBUG_LOG("TwsTopology_AddGoal goal 0x%x existing goals 0x%x", goal, td->active_goals);

    /* handle cleanup of any exclusive goal for the new goal */
    /*! \todo not currently supported and may be retired in favour of standard cancellation */
//    twsTopology_HandleExclusiveGoals(goal);

    /* if the new goal is already active then try and update the procedure */
#if 0
    /*! \todo not currently supported */
    if (twsTopology_GoalAlreadyActive(goal))
    {
        if (!twsTopology_HandleAlreadyActiveGoal(goal, goal_data))
        {
            DEBUG_LOG("TwsTopology_AddGoal new goal %x when already running but it doesn't support Update()", goal);
            Panic();
        }
    }
    else
#endif
    {
        /* handle contention with existing active goals */
        if (td->active_goals)
        {
            /* optionally cancel any active goals, if this type of goal requires it */
            if (twsTopology_GoalCancelsActive(goal))
            {
                twsTopology_CancelActiveGoals();
            }

            /* cancel may have synchronously cleared the active goals, if not
             * then queue it, otherwise start it immediately */
            if (td->active_goals)
            {
                /* always queue the goals */
                twsTopology_QueueGoal(rule_id, goal_data, goal_data_size);
            }
            else
            {
                /* no goals active, just start this new one */
                twsTopology_StartGoal(goal, goal_data);
            }
        }
        else
        {
            /* no goals active, just start this new one */
            twsTopology_StartGoal(goal, goal_data);
        }
    }
}

bool TwsTopology_IsGoalActive(tws_topology_goal goal)
{
    return (TwsTopologyGetTaskData()->active_goals & goal);
}

void TwsTopology_HandleGoalDecision(Task task, MessageId id, Message message)
{
    UNUSED(task);

    DEBUG_LOG("TwsTopology_HandleGoalDecision id 0x%x", id);

    switch (id)
    {
        case TWSTOP_PRIMARY_GOAL_PAIR_PEER:
            TwsTopology_AddGoal(tws_topology_goal_pair_peer, id, NULL, 0);
            break;

        case TWSTOP_PRIMARY_GOAL_SET_PRIMARY_ADDRESS:
            TwsTopology_AddGoal(tws_topology_goal_set_address, id, PROC_SET_ADDRESS_TYPE_DATA_PRIMARY, sizeof(SET_ADDRESS_TYPE_T));
            break;

        case TWSTOP_PRIMARY_GOAL_SET_PRIMARY_ADDRESS_FIND_ROLE:
            TwsTopology_AddGoal(tws_topology_goal_set_primary_address_and_find_role, id, NULL, 0);
            break;

        case TWSTOP_PRIMARY_GOAL_FIND_ROLE:
            TwsTopology_AddGoal(tws_topology_goal_find_role, id, PROC_FIND_ROLE_TIMEOUT_DATA_TIMEOUT, sizeof(FIND_ROLE_PARAMS_T));
            break;

        case TWSTOP_PRIMARY_GOAL_CANCEL_FIND_ROLE:
            TwsTopology_AddGoal(tws_topology_goal_cancel_find_role, id, NULL, 0);
            break;

        case TWSTOP_SECONDARY_GOAL_FIND_ROLE:
            TwsTopology_AddGoal(tws_topology_goal_find_role, id, PROC_FIND_ROLE_TIMEOUT_DATA_TIMEOUT, sizeof(FIND_ROLE_PARAMS_T));
            break;

        case TWSTOP_SECONDARY_GOAL_CONNECT_PEER:
            TwsTopology_AddGoal(tws_topology_goal_secondary_connect_peer, id, NULL, 0);
            break;

        case TWSTOP_SECONDARY_GOAL_NO_ROLE_IDLE:
        case TWSTOP_DFU_GOAL_NO_ROLE_IDLE:
            TwsTopology_AddGoal(tws_topology_goal_no_role_idle, id, NULL, 0);
            break;

        case TWSTOP_SECONDARY_GOAL_NO_ROLE_FIND_ROLE:
        case TWSTOP_DFU_GOAL_NO_ROLE_FIND_ROLE:
            TwsTopology_AddGoal(tws_topology_goal_no_role_find_role, id, NULL, 0);
            break;

        case TWSTOP_PRIMARY_GOAL_NO_ROLE_IDLE:
            TwsTopology_AddGoal(tws_topology_goal_no_role_idle, id, NULL, 0);
            break;

        case TWSTOP_PRIMARY_GOAL_BECOME_PRIMARY:
            TwsTopology_AddGoal(tws_topology_goal_become_primary, id, NULL, 0);
            break;

        case TWSTOP_PRIMARY_GOAL_BECOME_SECONDARY:
            TwsTopology_AddGoal(tws_topology_goal_become_secondary, id, NULL, 0);
            break;

        case TWSTOP_PRIMARY_GOAL_BECOME_ACTING_PRIMARY:
            TwsTopology_AddGoal(tws_topology_goal_become_acting_primary, id, NULL, 0);
            break;

        case TWSTOP_PRIMARY_GOAL_ROLE_SWITCH_TO_SECONDARY:
            TwsTopology_AddGoal(tws_topology_goal_role_switch_to_secondary, id, NULL, 0);
            break;

        case TWSTOP_PRIMARY_GOAL_PRIMARY_FIND_ROLE:
            TwsTopology_AddGoal(tws_topology_goal_primary_find_role, id, PROC_FIND_ROLE_TIMEOUT_DATA_CONTINUOUS, sizeof(FIND_ROLE_PARAMS_T));
            break;

        case TWSTOP_PRIMARY_GOAL_CONNECT_PEER_PROFILES:
            TwsTopology_AddGoal(tws_topology_goal_primary_connect_peer_profiles, id, message, sizeof(TWSTOP_PRIMARY_GOAL_CONNECT_PEER_PROFILES_T));
            break;

        case TWSTOP_PRIMARY_GOAL_CONNECTABLE_PEER:
            TwsTopology_AddGoal(tws_topology_goal_primary_connectable_peer, id, message, sizeof(TWSTOP_PRIMARY_GOAL_CONNECTABLE_PEER_T));
            break;

        case TWSTOP_PRIMARY_GOAL_DISCONNECT_PEER_PROFILES:
            TwsTopology_AddGoal(tws_topology_goal_primary_disconnect_peer_profiles, id, message, sizeof(TWSTOP_PRIMARY_GOAL_DISCONNECT_PEER_PROFILES_T));
            break;

        case TWSTOP_PRIMARY_GOAL_CONNECTABLE_HANDSET:
            TwsTopology_AddGoal(tws_topology_goal_connectable_handset, id, message, sizeof(TWSTOP_PRIMARY_GOAL_CONNECTABLE_HANDSET_T));
            break;
        
        case TWSTOP_PRIMARY_GOAL_CONNECT_HANDSET:
            TwsTopology_AddGoal(tws_topology_goal_connect_handset, id, message, sizeof(TWSTOP_PRIMARY_GOAL_CONNECT_HANDSET_T));
            break;

        case TWSTOP_PRIMARY_GOAL_DISCONNECT_PEER_FIND_ROLE:
            TwsTopology_AddGoal(tws_topology_goal_disconnect_peer_find_role, id, NULL, 0);
            break;

#if 0
        case TWSTOP_PRIMARY_GOAL_DISCONNECT_HANDSET:
            TwsTopology_AddGoal(tws_topology_goal_disconnect_handset, id, message);
            break;
#endif

        case TWSTOP_PRIMARY_GOAL_DFU_ROLE:
        case TWSTOP_SECONDARY_GOAL_DFU_ROLE:
            TwsTopology_AddGoal(tws_topology_goal_dfu_role, id, message, 0);
            break;

        case TWSTOP_DFU_GOAL_SECONDARY:
            TwsTopology_AddGoal(tws_topology_goal_dfu_secondary, id, message, 0);
            break;

        case TWSTOP_DFU_GOAL_PRIMARY:
            TwsTopology_AddGoal(tws_topology_goal_dfu_primary, id, message, 0);
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
