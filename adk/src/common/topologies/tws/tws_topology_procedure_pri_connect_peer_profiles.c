/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Procedure to connect profiles between Primary and Secondary Earbuds.
*/

#include "tws_topology_procedure_pri_connect_peer_profiles.h"
#include "tws_topology_procedures.h"
#include "tws_topology_primary_rules.h"

#include <bt_device.h>
#include <peer_signalling.h>
#include <scofwd_profile.h>
#include <av.h>
#include <connection_manager.h>

#include <logging.h>

#include <message.h>

static void twsTopology_ProcPriConnectPeerProfilesHandleMessage(Task task, MessageId id, Message message);
void TwsTopology_ProcedurePriConnectPeerProfilesStart(Task result_task,
                                        twstop_proc_start_cfm_func_t proc_start_cfm_fn,
                                        twstop_proc_complete_func_t proc_complete_fn,
                                        Message goal_data);
void TwsTopology_ProcedurePriConnectPeerProfilesCancel(twstop_proc_cancel_cfm_func_t proc_cancel_fn);
void TwsTopology_ProcedurePriConnectPeerProfilesUpdate(Message goal_data);

tws_topology_procedure_fns_t proc_pri_connect_peer_profiles_fns = {
    TwsTopology_ProcedurePriConnectPeerProfilesStart,
    TwsTopology_ProcedurePriConnectPeerProfilesCancel,
    TwsTopology_ProcedurePriConnectPeerProfilesUpdate,
};

typedef struct
{
    TaskData task;
    twstop_proc_complete_func_t complete_fn;
    uint8 profiles_status;
    bool active;
} twsTopProcPriConnectPeerProfilesTaskData;

twsTopProcPriConnectPeerProfilesTaskData twstop_proc_pri_connect_peer_profiles = {twsTopology_ProcPriConnectPeerProfilesHandleMessage};

#define TwsTopProcPriConnectPeerProfilesGetTaskData()     (&twstop_proc_pri_connect_peer_profiles)
#define TwsTopProcPriConnectPeerProfilesGetTask()         (&twstop_proc_pri_connect_peer_profiles.task)

static void twsTopology_ProcedurePriConnectPeerProfilesReset(void)
{
    twsTopProcPriConnectPeerProfilesTaskData* td = TwsTopProcPriConnectPeerProfilesGetTaskData();
    td->profiles_status = 0;
    td->active = FALSE;
}

static void twsTopology_ProcedurePriConnectPeerProfilesStartProfile(uint8 profiles)
{
    twsTopProcPriConnectPeerProfilesTaskData* td = TwsTopProcPriConnectPeerProfilesGetTaskData();
    bdaddr secondary_addr;

    /* remember the profiles requested to track when complete */
    td->profiles_status |= profiles;

    appDeviceGetSecondaryBdAddr(&secondary_addr);

    /* connect the requested profiles */
    if (profiles & DEVICE_PROFILE_PEERSIG)
    {
        DEBUG_LOG("twsTopology_ProcedurePriConnectPeerProfilesStartProfile PEERSIG");
        appPeerSigConnect(TwsTopProcPriConnectPeerProfilesGetTask(), &secondary_addr);
    }
    if (profiles & DEVICE_PROFILE_SCOFWD)
    {
        DEBUG_LOG("twsTopology_ProcedurePriConnectPeerProfilesStartProfile SCOFWD");
        ScoFwdConnectPeer(TwsTopProcPriConnectPeerProfilesGetTask());
    }
    if (profiles & DEVICE_PROFILE_A2DP)
    {
        DEBUG_LOG("twsTopology_ProcedurePriConnectPeerProfilesStartProfile A2DP");
        appAvConnectPeer(&secondary_addr);
        /*! \todo temporary fix as AV does not yet support CFMs */
        td->profiles_status &= ~DEVICE_PROFILE_A2DP;
    }
}

void TwsTopology_ProcedurePriConnectPeerProfilesStart(Task result_task,
                                                   twstop_proc_start_cfm_func_t proc_start_cfm_fn,
                                                   twstop_proc_complete_func_t proc_complete_fn,
                                                   Message goal_data)
{
    twsTopProcPriConnectPeerProfilesTaskData* td = TwsTopProcPriConnectPeerProfilesGetTaskData();
    TWSTOP_PRIMARY_GOAL_CONNECT_PEER_PROFILES_T* cpp = (TWSTOP_PRIMARY_GOAL_CONNECT_PEER_PROFILES_T*)goal_data;

    UNUSED(result_task);

    DEBUG_LOG("TwsTopology_ProcedurePriConnectPeerProfilesStart");

    /* remember completion function */
    td->complete_fn = proc_complete_fn;
    /* mark procedure active so if cleanup() requested this procedure can ignore
     * any CFM messages that arrive afterwards */
    td->active = TRUE;
    
    /* kick off connect for all requested profiles */
    twsTopology_ProcedurePriConnectPeerProfilesStartProfile(cpp->profiles);

    /* start is synchronous, use the callback to confirm now */
    proc_start_cfm_fn(tws_topology_procedure_pri_connect_peer_profiles, proc_result_success);
}

void TwsTopology_ProcedurePriConnectPeerProfilesCancel(twstop_proc_cancel_cfm_func_t proc_cancel_cfm_fn)
{
    DEBUG_LOG("TwsTopology_ProcedurePriConnectPeerProfilesCancel");
    twsTopology_ProcedurePriConnectPeerProfilesReset();
    TwsTopology_DelayedCancelCfmCallback(proc_cancel_cfm_fn, tws_topology_procedure_pri_connect_peer_profiles, proc_result_success);
}

void TwsTopology_ProcedurePriConnectPeerProfilesUpdate(Message goal_data)
{
    /* request connect for newly requested profiles */
    TWSTOP_PRIMARY_GOAL_CONNECT_PEER_PROFILES_T* cpp = (TWSTOP_PRIMARY_GOAL_CONNECT_PEER_PROFILES_T*)goal_data;
    twsTopology_ProcedurePriConnectPeerProfilesStartProfile(cpp->profiles);
}

static void twsTopology_ProcPriConnectPeerProfilesStatus(uint8 profile)
{
    twsTopProcPriConnectPeerProfilesTaskData* td = TwsTopProcPriConnectPeerProfilesGetTaskData();

    /* clear the connected profile */
    td->profiles_status &= ~profile;

    /* report start complete if all done */
    if (!td->profiles_status)
    {
        twsTopology_ProcedurePriConnectPeerProfilesReset();
        td->complete_fn(tws_topology_procedure_pri_connect_peer_profiles, proc_result_success);
    }
}

static void twsTopology_ProcPriConnectPeerProfilesHandleMessage(Task task, MessageId id, Message message)
{
    twsTopProcPriConnectPeerProfilesTaskData* td = TwsTopProcPriConnectPeerProfilesGetTaskData();
    bdaddr secondary_addr;

    UNUSED(task);
    UNUSED(message);

    /* if no longer active then ignore any CFM messages,
     * they'll be connect_cfm(cancelled) */
    if (!td->active)
    {
        return;
    }

    appDeviceGetSecondaryBdAddr(&secondary_addr);

    switch (id)
    {
        /*! \todo handle PEERSIG connect CFM */
        case PEER_SIG_CONNECT_CFM:
        {
            DEBUG_LOG("twsTopology_ProcPriConnectPeerProfilesHandleMessage PEERSIG");
            ConManagerReleaseAcl(&secondary_addr);
            twsTopology_ProcPriConnectPeerProfilesStatus(DEVICE_PROFILE_PEERSIG);
        }
        break;

        case SFWD_CONNECT_CFM:
        {
            DEBUG_LOG("twsTopology_ProcPriConnectPeerProfilesHandleMessage SCOFWD");
            ConManagerReleaseAcl(&secondary_addr);
            twsTopology_ProcPriConnectPeerProfilesStatus(DEVICE_PROFILE_SCOFWD);
        }
        break;

        /*! \todo handle AV connect CFM */

        default:
        break;
    }
}

