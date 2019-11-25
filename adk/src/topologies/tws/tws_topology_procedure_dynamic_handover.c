/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Dynamic handover script, which handles handover recommendation other than earbud going in case.
*/

#include "tws_topology_procedure_dynamic_handover.h"
#include "tws_topology_procedure_script_engine.h"
#include "tws_topology_procedure_handover.h"
#include "tws_topology_procedure_set_role.h"
#include "tws_topology_procedure_le_connectable_handset.h"
#include "tws_topology_procedure_disconnect_le_connections.h"

#define DYNAMIC_HANDOVER_SCRIPT(ENTRY) \
    ENTRY(proc_le_connectable_fns, PROC_DISABLE_LE_CONNECTABLE_PARAMS), \
    ENTRY(proc_disconnect_le_connections_fns, NO_DATA), \
    ENTRY(proc_handover_fns, NO_DATA), \
    ENTRY(proc_set_role_fns, PROC_SET_ROLE_TYPE_DATA_SECONDARY),\

/* Define the dynamic handover script */
DEFINE_TOPOLOGY_SCRIPT(dynamic_handover, DYNAMIC_HANDOVER_SCRIPT);

