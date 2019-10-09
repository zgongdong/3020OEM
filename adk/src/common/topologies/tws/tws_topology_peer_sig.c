/*!
\copyright  Copyright (c) 2015 - 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Implementation of TWS Topology use of peer signalling marshalled message channel.
*/

#include "tws_topology.h"
#include "tws_topology_goals.h"
#include "tws_topology_peer_sig.h"
#include "tws_topology_private.h"
#include "tws_topology_typedef.h"
#include "tws_topology_marshal_typedef.h"
#include "tws_topology_rule_events.h"

#include <peer_signalling.h>
#include <task_list.h>
#include <logging.h>
#include <timestamp_event.h>

#include <message.h>
#include <panic.h>
#include <marshal.h>

void TwsTopology_RegisterPeerSigClient(Task task)
{
    twsTopologyTaskData* td = TwsTopologyGetTaskData();
    task_list_t* list = NULL;

    list = TaskList_GetFlexibleBaseTaskList((task_list_flexible_t*)&td->peer_sig_msg_client_list);
    TaskList_AddTask(list, task);
}

void TwsTopology_UnregisterPeerSigClient(Task task)
{
    twsTopologyTaskData* td = TwsTopologyGetTaskData();
    task_list_t* list = NULL;

    list = TaskList_GetFlexibleBaseTaskList((task_list_flexible_t*)&td->peer_sig_msg_client_list);
    PanicFalse(TaskList_RemoveTask(list, task));
}

void TwsTopology_HandleMarshalledMsgChannelRxInd(PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND_T* ind)
{
    switch (ind->type)
    {
        case MARSHAL_TYPE(tws_topology_remote_rule_event_t):
            {    
                tws_topology_remote_rule_event_t* req = (tws_topology_remote_rule_event_t*)ind->msg;

                DEBUG_LOG("TwsTopology_HandleMarshalledMsgChannelRxInd tws_topology_remote_rule_event_t event 0x%x", req->event);
                TimestampEvent(TIMESTAMP_EVENT_ROLE_SWAP_COMMAND_RECEIVED);
                twsTopology_RulesSetEvent(req->event);
            }
            break;

#ifdef TOPOLOGY_PEER_SIG_RX_HANDLING_ENABLE
        case MARSHAL_TYPE(tws_topology_NEW_PEER_SIG_MSG:
            TWSTOP_PEER_SIG_MSG_RX_T* message = PanicUnlessMalloc(sizeof(TWSTOP_PEER_SIG_MSG_RX_T));

            DEBUG_LOG("TwsTopology_HandleMarshalledMsgChannelRxInd tws_topology_remote_goal_cfm_t");

            memcpy(&message->msg.NEW_PEER_SIG_MSG_TYPE, ind->msg, sizeof(tws_topology_NEW_PEER_SIG_MSG_TYPE));
            message->type = MARSHAL_TYPE(tws_topology_NEW_PEER_SIG_MSG_TYPE);
            TaskList_MessageSend(TaskList_GetFlexibleBaseTaskList((task_list_flexible_t*)&td->peer_sig_msg_client_list), TWSTOP_PEER_SIG_MSG_RX, message);
            break;
#endif

        default:
            break;
    }
}

void TwsTopology_HandleMarshalledMsgChannelTxCfm(PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM_T* cfm)
{
    DEBUG_LOG("TwsTopology_HandleMarshalledMsgChannelTxCfm sts:%u type:%d", cfm->status, cfm->type);
}

void TwsTopology_SecondaryRoleSwitchCommand(void)
{
    tws_topology_remote_rule_event_t* req = PanicUnlessMalloc(sizeof(tws_topology_remote_rule_event_t));
    req->event = TWSTOP_RULE_EVENT_FORCED_ROLE_SWITCH;
    appPeerSigMarshalledMsgChannelTx(TwsTopologyGetTask(), PEER_SIG_MSG_CHANNEL_TOPOLOGY, req, MARSHAL_TYPE(tws_topology_remote_rule_event_t));
}

