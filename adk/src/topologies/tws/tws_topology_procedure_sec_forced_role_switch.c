/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Procedure for a Secondary to become Primary on command from Primary.
*/

#include "tws_topology_procedure_script_engine.h"

#include "tws_topology_procedure_find_role.h"
#include "tws_topology_procedure_wait_peer_link_drop.h"
#include "tws_topology_procedure_prohibit_bt.h"
#include "tws_topology_procedure_set_address.h"
#include "tws_topology_procedure_set_role.h"
#include "tws_topology_procedure_permit_bt.h"

#define SECONDARY_STATIC_HANDOVER_SCRIPT(ENTRY) \
    ENTRY(proc_wait_peer_link_drop_fns, PROC_PEER_LINK_DROP_DATA_DEFAULT_TIMEOUT), \
    ENTRY(proc_prohibit_bt_fns, NO_DATA), \
    ENTRY(proc_set_address_fns, PROC_SET_ADDRESS_TYPE_DATA_PRIMARY), \
    ENTRY(proc_permit_bt_fns, NO_DATA), \
    ENTRY(proc_set_role_fns, PROC_SET_ROLE_TYPE_DATA_PRIMARY), \
    ENTRY(proc_find_role_fns, PROC_FIND_ROLE_TIMEOUT_DATA_CONTINUOUS)


/* Define the secondary_static_handover_script */
DEFINE_TOPOLOGY_SCRIPT(secondary_static_handover, SECONDARY_STATIC_HANDOVER_SCRIPT);
