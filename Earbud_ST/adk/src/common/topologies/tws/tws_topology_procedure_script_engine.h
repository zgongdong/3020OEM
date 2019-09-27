/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      
*/

#ifndef TWS_TOPOLOGY_PROC_STEP_ENGINE_H
#define TWS_TOPOLOGY_PROC_STEP_ENGINE_H

#include "tws_topology_procedures.h"

extern const Message no_proc_data;
#define NO_DATA    (&no_proc_data)

typedef struct
{
    /* pointer to array of procedure function structs for the procedures
     * to be run through */
    const tws_topology_procedure_fns_t** script_procs;

    /* pointer to array of procedure data objects used as params for
     * those procedures which require them. Entries may be NULL for
     * procedures which take no parameters. */
    const Message* script_procs_data;

    /* number of entries in proc_list */
    int script_proc_list_size;
} tws_topology_proc_script_t;

bool TwsTopology_ProcScriptEngineStart(Task client_task,
                                       const tws_topology_proc_script_t* script,
                                       tws_topology_procedure proc,
                                       twstop_proc_start_cfm_func_t proc_start_cfm_fn,
                                       twstop_proc_complete_func_t proc_complete_fn);

bool TwsTopology_ProcScriptEngineCancel(Task script_client,
                                        twstop_proc_cancel_cfm_func_t proc_cancel_cfm_fn);

#endif /* TWS_TOPOLOGY_PROC_STEP_ENGINE_H */
