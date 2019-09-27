/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      
*/

#include "tws_topology_procedure_script_engine.h"
#include "tws_topology_procedures.h"
#include "tws_topology_goals.h"

#include <logging.h>

#include <panic.h>

#pragma unitsuppress Unused

#define NEXT_SCRIPT_STEP                (TwsTopProcScriptGetTaskData()->next_step)
#define SCRIPT_SIZE(script)             (script->script_proc_list_size)
#define ALL_PROCEDURES_COMPLETE(script) (NEXT_SCRIPT_STEP == SCRIPT_SIZE(script))

#define SCRIPT_PROCEDURES(script)       (((script)->script_procs))
#define SCRIPT_PROCEDURES_DATA(script)  (((script)->script_procs_data))

#define NEXT_PROCEDURE(script)          ((SCRIPT_PROCEDURES(script)[NEXT_SCRIPT_STEP]))
#define NEXT_PROCEDURE_DATA(script)     ((SCRIPT_PROCEDURES_DATA(script)[NEXT_SCRIPT_STEP]))

const Message no_proc_data;

typedef enum
{
    proc_script_engine_idle,
    proc_script_engine_active,
    proc_script_engine_cancelling,
} proc_script_engine_state_t;

typedef struct
{
    TaskData script_engine_task;

    proc_script_engine_state_t state;
    int next_step;

    const tws_topology_proc_script_t* script;
    tws_topology_procedure proc;
    Task script_client;
    
    twstop_proc_start_cfm_func_t start_fn;
    twstop_proc_complete_func_t complete_fn;
    twstop_proc_cancel_cfm_func_t cancel_fn;
} twsTopProcScriptTaskData;

twsTopProcScriptTaskData tws_topology_proc_script_engine;

#define TwsTopProcScriptGetTaskData()   (&tws_topology_proc_script_engine)
//#define TwsTopProcScriptGetTask()       (&tws_topology_proc_script_engine.script_engine_task)

static void twsTopology_ProcScriptEngineStartCfm(tws_topology_procedure proc, proc_result_t result);
static void twsTopology_ProcScriptEngineCompleteCfm(tws_topology_procedure proc, proc_result_t result);
static void twsTopology_ProcScriptEngineCancelCfm(tws_topology_procedure proc, proc_result_t result);

static void twsTopology_ProcScriptEngineReset(void)
{
    twsTopProcScriptTaskData* td = TwsTopProcScriptGetTaskData();
    td->state = proc_script_engine_idle;
    td->next_step = 0;
    td->script = NULL;
}

static void twsTopology_ProcScriptEngineNextStep(void)
{
    twsTopProcScriptTaskData* td = TwsTopProcScriptGetTaskData();

    if (NEXT_SCRIPT_STEP < SCRIPT_SIZE(td->script))
    {
        DEBUG_LOG("twsTopology_ProcScriptEngineNextStep calling start %p", NEXT_PROCEDURE(td->script)->proc_start_fn);
        NEXT_PROCEDURE(td->script)->proc_start_fn(td->script_client,
                                                 twsTopology_ProcScriptEngineStartCfm,
                                                 twsTopology_ProcScriptEngineCompleteCfm,
                                                 NEXT_PROCEDURE_DATA(td->script));
    }
    else
    {
        DEBUG_LOG("twsTopology_ProcScriptEngineNextStep script %p success", td->script);

        /* script finished with no failures, inform client */
        twsTopology_ProcScriptEngineReset();
        td->complete_fn(td->proc, proc_result_success);
    }
}

/*! \todo Future use expected to track in-progress procedures for smarter goal resolution */
static void twsTopology_ProcScriptEngineStartCfm(tws_topology_procedure proc, proc_result_t result)
{
    twsTopProcScriptTaskData* td = TwsTopProcScriptGetTaskData();

    DEBUG_LOG("twsTopology_ProcScriptEngineStartCfm script %p proc %x, result %u", td->script, proc, result);
    
    if (result != proc_result_success)
    {
        DEBUG_LOG("twsTopology_ProcScriptEngineStartCfm step %u for script %p failed to start",
                   td->next_step, td->script);
        Panic();
    }
}

static void twsTopology_ProcScriptEngineCompleteCfm(tws_topology_procedure proc, proc_result_t result)
{
    twsTopProcScriptTaskData* td = TwsTopProcScriptGetTaskData();
    DEBUG_LOG("twsTopology_ProcScriptEngineCompleteCfm proc %x, result %u", proc, result);

    if (result != proc_result_success)
    {
        DEBUG_LOG("twsTopology_ProcScriptEngineCompleteCfm step %u failed to complete", td->next_step);
        Panic();
    }

    /* as long as engine is still active, process next step in the script */
    if (td->state == proc_script_engine_active)
    {
        NEXT_SCRIPT_STEP++;
        twsTopology_ProcScriptEngineNextStep();
    }
}

static void twsTopology_ProcScriptEngineCancelCfm(tws_topology_procedure proc, proc_result_t result)
{
    twsTopProcScriptTaskData* td = TwsTopProcScriptGetTaskData();
    DEBUG_LOG("twsTopology_ProcScriptEngineCancelCfm proc %x, result %u", proc, result);
    
    PanicFalse(td->state == proc_script_engine_cancelling);
    
    /* clean up the script engine */
    twsTopology_ProcScriptEngineReset();

    /* callback with result of cancellation */
    td->cancel_fn(td->proc, result);
}

bool TwsTopology_ProcScriptEngineStart(Task client_task,
                                       const tws_topology_proc_script_t* script,
                                       tws_topology_procedure proc,
                                       twstop_proc_start_cfm_func_t proc_start_cfm_fn,
                                       twstop_proc_complete_func_t proc_complete_fn)
{
    twsTopProcScriptTaskData* td = TwsTopProcScriptGetTaskData();

    DEBUG_LOG("TwsTopology_ProcScriptEngineStart script %x proc %x", script, proc);

    PanicFalse(td->state == proc_script_engine_idle);

    td->start_fn = proc_start_cfm_fn;
    td->complete_fn = proc_complete_fn;
    td->proc = proc;
    td->script_client = client_task;

    td->state = proc_script_engine_active;
    td->next_step = 0;
    td->script = script;

    twsTopology_ProcScriptEngineNextStep();

    return TRUE;
}

bool TwsTopology_ProcScriptEngineCancel(Task script_client,
                                        twstop_proc_cancel_cfm_func_t proc_cancel_cfm_fn)
{
    twsTopProcScriptTaskData* td = TwsTopProcScriptGetTaskData();

    DEBUG_LOG("TwsTopology_ProcScriptEngineCancel client %x script %p step %u",
                        script_client, td->script, td->next_step);

    PanicFalse(td->script_client == script_client);

    /* can only cancel a script in progress */
    if (td->state != proc_script_engine_active)
    {
        return FALSE;
    }

    /* prevent any further start of script entries */
    td->state = proc_script_engine_cancelling;

    /* remember the cancel comfirm callback */
    td->cancel_fn = proc_cancel_cfm_fn;

    /* cancel the current step */
    NEXT_PROCEDURE(td->script)->proc_cancel_fn(twsTopology_ProcScriptEngineCancelCfm);

    return TRUE;
}
