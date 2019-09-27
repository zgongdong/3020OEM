/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Procedure to start find role following completion of peer pairing when out of the case.
*/

#include "tws_topology_procedure_script_engine.h"

#include "tws_topology_procedure_prohibit_bt.h"
#include "tws_topology_procedure_set_address.h"
#include "tws_topology_procedure_permit_bt.h"
#include "tws_topology_procedure_find_role.h"

const tws_topology_procedure_fns_t* primary_address_find_role_procs[] = {
    &proc_prohibit_bt_fns,
    &proc_set_address_fns,
    &proc_permit_bt_fns,
    &proc_find_role_fns,
};

const Message primary_address_find_role_procs_data[] = {
    NO_DATA,
    PROC_SET_ADDRESS_TYPE_DATA_PRIMARY,
    NO_DATA,
    PROC_FIND_ROLE_TIMEOUT_DATA_TIMEOUT,
};

const tws_topology_proc_script_t primary_address_find_role_script = {
    primary_address_find_role_procs,
    primary_address_find_role_procs_data,
    ARRAY_DIM(primary_address_find_role_procs),
};

