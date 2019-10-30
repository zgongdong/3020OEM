/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Procedure for a Primary to participate in static handover when going in the case.
*/

#include "tws_topology_procedure_script_engine.h"

#include "tws_topology_procedure_command_role_switch.h"
#include "tws_topology_procedure_clean_connections.h"
#include "tws_topology_procedure_connectable_handset.h"
#include "tws_topology_procedure_pri_connectable_peer.h"
#include "tws_topology_procedure_cancel_find_role.h"
#include "tws_topology_procedure_set_role.h"
#include "tws_topology_procedure_event_suppress.h"

#define PRIMARY_STATIC_HANDOVER_IN_CASE(ENTRY) \
    ENTRY(proc_event_suppress_fns, PROC_EVENT_SUPPRESS_DATA_SUPPRESS), \
    ENTRY(proc_command_role_switch_fns, NO_DATA), \
    ENTRY(proc_clean_connections_fns, NO_DATA), \
    ENTRY(proc_connectable_handset_fns, PROC_CONNECTABLE_HANDSET_DATA_DISABLE), \
    ENTRY(proc_pri_connectable_peer_fns, PROC_PRI_CONNECTABLE_PEER_DATA_DISABLE), \
    ENTRY(proc_cancel_find_role_fns, NO_DATA), \
    ENTRY(proc_set_role_fns, PROC_SET_ROLE_TYPE_DATA_NONE), \
    ENTRY(proc_event_suppress_fns, PROC_EVENT_SUPPRESS_DATA_EXPOSE_IN_CASE)

/* Define the primary_static_handover_in_case_script */
DEFINE_TOPOLOGY_SCRIPT(primary_static_handover_in_case, PRIMARY_STATIC_HANDOVER_IN_CASE);
