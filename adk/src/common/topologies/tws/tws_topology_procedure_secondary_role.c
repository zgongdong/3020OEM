/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      
*/

#include "tws_topology_procedure_script_engine.h"

#include "tws_topology_procedure_prohibit_bt.h"
#include "tws_topology_procedure_set_address.h"
#include "tws_topology_procedure_set_role.h"
#include "tws_topology_procedure_permit_bt.h"
#include "tws_topology_procedure_clean_connections.h"

#define SECONDARY_ROLE_SCRIPT(ENTRY) \
    ENTRY(proc_prohibit_bt_fns, NO_DATA), \
    ENTRY(proc_clean_connections_fns, NO_DATA), \
    ENTRY(proc_set_address_fns, PROC_SET_ADDRESS_TYPE_DATA_SECONDARY), \
    ENTRY(proc_set_role_fns, PROC_SET_ROLE_TYPE_DATA_SECONDARY), \
    ENTRY(proc_permit_bt_fns, NO_DATA), 

/* Define the secondary_role_script */
DEFINE_TOPOLOGY_SCRIPT(secondary_role, SECONDARY_ROLE_SCRIPT);

