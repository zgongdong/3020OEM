/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Common procedure handling functions.
*/

#include "tws_topology_procedures.h"

#include <message.h>
#include <panic.h>

#define MAKE_DELAY_CFM_MESSAGE(TYPE)  TYPE##_T *message = PanicUnlessNew(TYPE##_T);

static void twsTopology_ProcDelayCfmHandleMessage(Task task, MessageId id, Message message);

typedef struct
{
    TaskData task;
} twsTopProcDelayCfmTaskData;

const twsTopProcDelayCfmTaskData tws_topology_proc_delay_cfm = {twsTopology_ProcDelayCfmHandleMessage};

#define TwsTopProcDelayCfmGetTask() (&tws_topology_proc_delay_cfm.task)

typedef enum
{
    DELAY_START_CFM,
    DELAY_COMPLETE_CFM,
    DELAY_CANCEL_CFM,
} delayCfmMessages;

typedef struct
{
    twstop_proc_start_cfm_func_t start_fn;
    tws_topology_procedure proc;
    proc_result_t result;
} DELAY_START_CFM_T;

typedef struct
{
    twstop_proc_complete_func_t comp_fn;
    tws_topology_procedure proc;
    proc_result_t result;
} DELAY_COMPLETE_CFM_T;

typedef struct
{
    twstop_proc_cancel_cfm_func_t cancel_fn;
    tws_topology_procedure proc;
    proc_result_t result;
} DELAY_CANCEL_CFM_T;

void TwsTopology_DelayedStartCfmCallback(twstop_proc_start_cfm_func_t start_fn,
                                         tws_topology_procedure proc, proc_result_t result)
{
    MAKE_DELAY_CFM_MESSAGE(DELAY_START_CFM);
    message->start_fn = start_fn;
    message->proc = proc;
    message->result = result;
    MessageSend(TwsTopProcDelayCfmGetTask(), DELAY_START_CFM, message);
}

void TwsTopology_DelayedCompleteCfmCallback(twstop_proc_complete_func_t comp_fn,
                                            tws_topology_procedure proc, proc_result_t result)
{
    MAKE_DELAY_CFM_MESSAGE(DELAY_COMPLETE_CFM);
    message->comp_fn = comp_fn;
    message->proc = proc;
    message->result = result;
    MessageSend(TwsTopProcDelayCfmGetTask(), DELAY_COMPLETE_CFM, message);
}

void TwsTopology_DelayedCancelCfmCallback(twstop_proc_cancel_cfm_func_t cancel_fn,
                                            tws_topology_procedure proc, proc_result_t result)
{
    MAKE_DELAY_CFM_MESSAGE(DELAY_CANCEL_CFM);
    message->cancel_fn = cancel_fn;
    message->proc = proc;
    message->result = result;
    MessageSend(TwsTopProcDelayCfmGetTask(), DELAY_CANCEL_CFM, message);
}

static void twsTopology_ProcDelayCfmHandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch (id)
    {
        case DELAY_START_CFM:
        {
            DELAY_START_CFM_T* cfm = (DELAY_START_CFM_T*)message;
            cfm->start_fn(cfm->proc, cfm->result);
        }
        break;
        case DELAY_COMPLETE_CFM:
        {
            DELAY_COMPLETE_CFM_T* cfm = (DELAY_COMPLETE_CFM_T*)message;
            cfm->comp_fn(cfm->proc, cfm->result);
        }
        break;
        case DELAY_CANCEL_CFM:
        {
            DELAY_CANCEL_CFM_T* cfm = (DELAY_CANCEL_CFM_T*)message;
            cfm->cancel_fn(cfm->proc, cfm->result);
        }
        break;
    }
}
