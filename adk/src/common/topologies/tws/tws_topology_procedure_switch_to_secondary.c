/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      
*/

#include "tws_topology_procedure_script_engine.h"

#include "tws_topology_procedure_disconnect_handset.h"
#include "tws_topology_procedure_disconnect_peer_profiles.h"
#include "tws_topology_procedure_prohibit_bt.h"
#include "tws_topology_procedure_prohibit_connection_le.h"
#include "tws_topology_procedure_set_address.h"
#include "tws_topology_procedure_set_role.h"
#include "tws_topology_procedure_permit_bt.h"
#include "tws_topology_procedure_permit_connection_le.h"
#include "tws_topology_procedure_sec_connect_peer.h"
#include "tws_topology_procedure_clean_connections.h"

#include <logging.h>


#define SWITCH_TO_SECONDARY_SCRIPT(ENTRY) \
    ENTRY(proc_disconnect_handset_fns, NO_DATA), \
    ENTRY(proc_disconnect_peer_profiles_fns, PROC_DISCONNECT_PEER_PROFILES_DATA_ALL), \
    ENTRY(proc_prohibit_bt_fns, NO_DATA), \
    ENTRY(proc_prohibit_connection_le_fns, NO_DATA), \
    ENTRY(proc_clean_connections_fns, NO_DATA), \
    ENTRY(proc_set_address_fns, PROC_SET_ADDRESS_TYPE_DATA_SECONDARY), \
    ENTRY(proc_set_role_fns, PROC_SET_ROLE_TYPE_DATA_SECONDARY), \
    ENTRY(proc_permit_bt_fns, NO_DATA), \
    ENTRY(proc_permit_connection_le_fns, NO_DATA), \
    ENTRY(proc_sec_connect_peer_fns, NO_DATA), 


/* Define the switch_to_secondary_script */
DEFINE_TOPOLOGY_SCRIPT(switch_to_secondary, SWITCH_TO_SECONDARY_SCRIPT);

