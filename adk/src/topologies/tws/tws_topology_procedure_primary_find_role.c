/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Procedure to start finding a role when already in a Primary (acting) role
*/

#include "tws_topology_procedure_script_engine.h"

#include "tws_topology_procedure_find_role.h"

#include <logging.h>

const tws_topology_procedure_fns_t* primary_find_role_procs[] = {
    &proc_find_role_fns,
};

const Message primary_find_role_procs_data[] = {
    PROC_FIND_ROLE_TIMEOUT_DATA_CONTINUOUS,
};

const tws_topology_proc_script_t primary_find_role_script = {
    primary_find_role_procs,
    primary_find_role_procs_data,
    ARRAY_DIM(primary_find_role_procs),
};

