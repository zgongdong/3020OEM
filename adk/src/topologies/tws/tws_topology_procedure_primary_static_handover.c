/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Procedure for a Primary to participate in static handover and become Secondary.
*/

#include "tws_topology_procedure_script_engine.h"

#include "tws_topology_procedure_command_role_switch.h"
#include "tws_topology_procedure_clean_connections.h"
#include "tws_topology_procedure_connectable_handset.h"
#include "tws_topology_procedure_pri_connectable_peer.h"
#include "tws_topology_procedure_cancel_find_role.h"
#include "tws_topology_procedure_prohibit_bt.h"
#include "tws_topology_procedure_set_address.h"
#include "tws_topology_procedure_permit_bt.h"
#include "tws_topology_procedure_set_role.h"
#include "tws_topology_procedure_event_suppress.h"
#include "tws_topology_procedure_le_connectable_handset.h"

#define PRIMARY_STATIC_HANDOVER_SCRIPT(ENTRY) \
    ENTRY(proc_event_suppress_fns, PROC_EVENT_SUPPRESS_DATA_SUPPRESS), \
    ENTRY(proc_command_role_switch_fns, NO_DATA), \
    ENTRY(proc_clean_connections_fns, NO_DATA), \
    ENTRY(proc_connectable_handset_fns, PROC_CONNECTABLE_HANDSET_DATA_DISABLE), \
    ENTRY(proc_pri_connectable_peer_fns, PROC_PRI_CONNECTABLE_PEER_DATA_DISABLE), \
    ENTRY(proc_cancel_find_role_fns, NO_DATA), \
    ENTRY(proc_prohibit_bt_fns, NO_DATA), \
    ENTRY(proc_set_address_fns, PROC_SET_ADDRESS_TYPE_DATA_SECONDARY), \
    ENTRY(proc_permit_bt_fns, NO_DATA), \
    ENTRY(proc_le_connectable_fns, PROC_DISABLE_LE_CONNECTABLE_PARAMS), \
    ENTRY(proc_event_suppress_fns, PROC_EVENT_SUPPRESS_DATA_EXPOSE_OUT_CASE),\
    ENTRY(proc_set_role_fns, PROC_SET_ROLE_TYPE_DATA_SECONDARY)

/* Define the primary_static_handover_script */
DEFINE_TOPOLOGY_SCRIPT(primary_static_handover, PRIMARY_STATIC_HANDOVER_SCRIPT);
