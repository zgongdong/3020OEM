/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      
*/

#include "tws_topology_procedure_script_engine.h"

#include "tws_topology_procedure_allow_handset_connect.h"
#include "tws_topology_procedure_connectable_handset.h"
#include "tws_topology_procedure_pri_connectable_peer.h"
#include "tws_topology_procedure_disconnect_handset.h"
#include "tws_topology_procedure_disconnect_peer_profiles.h"
#include "tws_topology_procedure_cancel_find_role.h"
#include "tws_topology_procedure_prohibit_bt.h"
#include "tws_topology_procedure_prohibit_connection_le.h"
#include "tws_topology_procedure_set_address.h"
#include "tws_topology_procedure_set_role.h"
#include "tws_topology_procedure_permit_bt.h"
#include "tws_topology_procedure_permit_connection_le.h"
#include "tws_topology_procedure_clean_connections.h"
#include "tws_topology_procedure_le_connectable_handset.h"

#include <logging.h>


#define NO_ROLE_IDLE_SCRIPT(ENTRY) \
    ENTRY(proc_connectable_handset_fns, PROC_CONNECTABLE_HANDSET_DATA_DISABLE), \
    ENTRY(proc_pri_connectable_peer_fns, PROC_PRI_CONNECTABLE_PEER_DATA_DISABLE), \
    ENTRY(proc_allow_handset_connect_fns, PROC_ALLOW_HANDSET_CONNECT_DATA_DISALLOW), \
    ENTRY(proc_disconnect_handset_fns, NO_DATA), \
    ENTRY(proc_disconnect_peer_profiles_fns, PROC_DISCONNECT_PEER_PROFILES_DATA_ALL), \
    ENTRY(proc_cancel_find_role_fns, NO_DATA), \
    ENTRY(proc_prohibit_bt_fns, NO_DATA), \
    ENTRY(proc_prohibit_connection_le_fns, NO_DATA), \
    ENTRY(proc_le_connectable_fns,PROC_DISABLE_LE_CONNECTABLE_PARAMS), \
    ENTRY(proc_clean_connections_fns, NO_DATA), \
    ENTRY(proc_set_address_fns, PROC_SET_ADDRESS_TYPE_DATA_PRIMARY), \
    ENTRY(proc_set_role_fns, PROC_SET_ROLE_TYPE_DATA_NONE), \
    ENTRY(proc_permit_bt_fns, NO_DATA), \
    ENTRY(proc_permit_connection_le_fns, NO_DATA)


/* Define the no_role_idle_script */
DEFINE_TOPOLOGY_SCRIPT(no_role_idle, NO_ROLE_IDLE_SCRIPT);

