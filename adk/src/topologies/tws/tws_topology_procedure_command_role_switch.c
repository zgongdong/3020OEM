/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Procedure for a Primary to command the Secondary becomes Primary.
*/

#include "tws_topology_procedure_command_role_switch.h"
#include "tws_topology_procedures.h"
#include "tws_topology_peer_sig.h"
#include "tws_topology_marshal_typedef.h"

#include <logging.h>

#include <marshal.h>
#include <message.h>


void TwsTopology_ProcedureCommandRoleSwitchStart(Task result_task,
                                                 twstop_proc_start_cfm_func_t proc_start_cfm_fn,
                                                 twstop_proc_complete_func_t proc_complete_fn,
                                                 Message goal_data);
void TwsTopology_ProcedureCommandRoleSwitchCancel(twstop_proc_cancel_cfm_func_t proc_cancel_fn);

const tws_topology_procedure_fns_t proc_command_role_switch_fns = {
    TwsTopology_ProcedureCommandRoleSwitchStart,
    TwsTopology_ProcedureCommandRoleSwitchCancel,
    NULL,
};


typedef struct
{
    TaskData task;
    twstop_proc_complete_func_t complete;
} twsTopProcCommandRoleSwitchTaskData;

static void twsTopology_ProcCommandRoleSwitchHandleMessage(Task task, MessageId id, Message message);


twsTopProcCommandRoleSwitchTaskData twstop_proc_command_role_switch 
                    = {.task = { .handler = twsTopology_ProcCommandRoleSwitchHandleMessage} };

#define TwsTopProcCommandRoleSwitchGetTaskData()    (&twstop_proc_command_role_switch)
#define TwsTopProcCommandRoleSwitchGetTask()        (&twstop_proc_command_role_switch.task)


static void TwsTopology_ProcedureCommandRoleSwitchCommonCleanUp(void)
{
    TwsTopProcCommandRoleSwitchGetTaskData()->complete = NULL;
    TwsTopology_UnregisterPeerSigClient(TwsTopProcCommandRoleSwitchGetTask());
}


void TwsTopology_ProcedureCommandRoleSwitchStart(Task result_task,
                                                 twstop_proc_start_cfm_func_t proc_start_cfm_fn,
                                                 twstop_proc_complete_func_t proc_complete_fn,
                                                 Message goal_data)
{
    UNUSED(result_task);
    UNUSED(goal_data);

    DEBUG_LOG("TwsTopology_ProcedureCommandRoleSwitchStart");

    TwsTopProcCommandRoleSwitchGetTaskData()->complete = proc_complete_fn;
    TwsTopology_RegisterPeerSigClient(TwsTopProcCommandRoleSwitchGetTask());

    TwsTopology_SecondaryStaticHandoverCommand();

    /* start immediately. Completion awaits the handover command confirm
       (which doesn't neccesarily mean that we have sent over the air)*/
    proc_start_cfm_fn(tws_topology_procedure_command_role_switch, proc_result_success);
}

void TwsTopology_ProcedureCommandRoleSwitchCancel(twstop_proc_cancel_cfm_func_t proc_cancel_fn)
{
    DEBUG_LOG("TwsTopology_ProcedureCommandRoleSwitchCancel");

    TwsTopology_ProcedureCommandRoleSwitchCommonCleanUp();

    TwsTopology_DelayedCancelCfmCallback(proc_cancel_fn, tws_topology_procedure_command_role_switch, proc_result_success);
}

static void TwsTopologyProcCommandRoleSwitch_HandleTxCfm(const PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM_T *tx_cfm)
{
    twstop_proc_complete_func_t complete = TwsTopProcCommandRoleSwitchGetTaskData()->complete;

    DEBUG_LOG("TwsTopologyProcCommandRoleSwitch_HandleTxCfm. Type:%d", tx_cfm->type);

    if (tx_cfm->type == MARSHAL_TYPE(tws_topology_remote_static_handover_cmd_t))
    {
        TwsTopology_DelayedCancelCfmCallback(complete,
                                             tws_topology_procedure_command_role_switch,
                                             proc_result_success);

        TwsTopology_ProcedureCommandRoleSwitchCommonCleanUp();
    }
}

static void twsTopology_ProcCommandRoleSwitchHandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);

    if (NULL == TwsTopProcCommandRoleSwitchGetTaskData()->complete)
    {
        return;
    }

    switch (id)
    {
        case PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM:
            TwsTopologyProcCommandRoleSwitch_HandleTxCfm((const PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM_T *)message);
            break;

        default:
            /* Other, incoming, peer signalling messages can be received here */
            break;
    }
}

