/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Logical input switch implementation.
*/

#include "logical_input_switch.h"
#include "logical_input_switch_marshal_defs.h"

#include <bt_device.h>
#include <peer_signalling.h>
#include <state_proxy.h>
#include <ui.h>

#include <bdaddr.h>
#include <logging.h>
#include <panic.h>
#include <stdlib.h>

static void logicalInputSwitch_HandleMessage(Task task, MessageId id, Message message);

static TaskData lis_task = { .handler=logicalInputSwitch_HandleMessage };

static bool reroute_logical_inputs_to_peer = FALSE;

static uint16 lowest_valid_logical_input = 0x0;
static uint16 highest_valid_logical_input = 0xFFFF;

static inline bool logicalInputSwitch_IsValidLogicalInput(uint16 logical_input)
{
    return logical_input >= lowest_valid_logical_input && logical_input <= highest_valid_logical_input;
}

static inline void logicalInputSwitch_PassLogicalInputToLocalUi(uint16 logical_input)
{
    MessageSend(Ui_GetUiTask(), logical_input, NULL);
}

static void logicalInputSwitch_SendLogicalInputToPeer(uint16 logical_input)
{
    logical_input_ind_t* msg = PanicUnlessMalloc(sizeof(logical_input_ind_t));
    msg->logical_input = logical_input;

    DEBUG_LOG("logicalInputSwitch_SendLogicalInputToPeer %04X", msg->logical_input);

    appPeerSigMarshalledMsgChannelTx(LogicalInputSwitch_GetTask(),
                                     PEER_SIG_MSG_CHANNEL_LOGICAL_INPUT_SWITCH,
                                     msg,
                                     MARSHAL_TYPE_logical_input_ind_t);
}

static inline void logicalInputSwitch_HandleMarshalledMsgChannelTxCfm(PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM_T* cfm)
{
    DEBUG_LOG("logicalInputSwitch_HandleMarshalledMsgChannelTxCfm %d", cfm->status);
}

static void logicalInputSwitch_HandleMarshalledMsgChannelRxInd(PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND_T* ind)
{
    logical_input_ind_t *msg;
    PanicNull(ind);
    msg = (logical_input_ind_t *)ind->msg;
    PanicNull(msg);
    logicalInputSwitch_PassLogicalInputToLocalUi(msg->logical_input);
    free(msg);
}

static void logicalInputSwitch_HandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch(id)
    {
    case PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND:
        logicalInputSwitch_HandleMarshalledMsgChannelRxInd((PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND_T*)message);
        break;

    case PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM:
        logicalInputSwitch_HandleMarshalledMsgChannelTxCfm((PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM_T*)message);
        break;

    default:
        if (logicalInputSwitch_IsValidLogicalInput(id))
        {
            if (reroute_logical_inputs_to_peer)
            {
                logicalInputSwitch_SendLogicalInputToPeer(id);
            }
            else
            {
                logicalInputSwitch_PassLogicalInputToLocalUi(id);
            }
        }
        break;
    }
}

Task LogicalInputSwitch_GetTask(void)
{
    return &lis_task;
}

void LogicalInputSwitch_SetRerouteToPeer(bool reroute_enabled)
{
    reroute_logical_inputs_to_peer = reroute_enabled;
}

void LogicalInputSwitch_SetLogicalInputIdRange(uint16 lowest, uint16 highest)
{
    lowest_valid_logical_input = lowest;
    highest_valid_logical_input = highest;
}

bool LogicalInputSwitch_Init(Task init_task)
{
    LogicalInputSwitch_SetRerouteToPeer(FALSE);

    LogicalInputSwitch_SetLogicalInputIdRange(0x0,0xFFFF);

    appPeerSigMarshalledMsgChannelTaskRegister(LogicalInputSwitch_GetTask(),
                                               PEER_SIG_MSG_CHANNEL_LOGICAL_INPUT_SWITCH,
                                               logical_input_switch_marshal_type_descriptors,
                                               NUMBER_OF_MARSHAL_OBJECT_TYPES);

    UNUSED(init_task);
    return TRUE;
}
