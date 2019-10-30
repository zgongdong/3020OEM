/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Procedure to start finding a role when not in the Primary role.
*/

#include "tws_topology_procedure_script_engine.h"

#include "tws_topology_procedure_prohibit_bt.h"
#include "tws_topology_procedure_prohibit_connection_le.h"
#include "tws_topology_procedure_set_address.h"
#include "tws_topology_procedure_set_role.h"
#include "tws_topology_procedure_permit_bt.h"
#include "tws_topology_procedure_permit_connection_le.h"
#include "tws_topology_procedure_find_role.h"
#include "tws_topology_procedure_clean_connections.h"
#include "tws_topology_procedure_event_suppress.h"

#include <logging.h>

#define NO_ROLE_FIND_ROLE_SCRIPT(ENTRY) \
    ENTRY(proc_event_suppress_fns, PROC_EVENT_SUPPRESS_DATA_SUPPRESS), \
    ENTRY(proc_prohibit_bt_fns, NO_DATA), \
    ENTRY(proc_prohibit_connection_le_fns, NO_DATA), \
    ENTRY(proc_clean_connections_fns, NO_DATA), \
    ENTRY(proc_set_address_fns ,PROC_SET_ADDRESS_TYPE_DATA_PRIMARY), \
    ENTRY(proc_set_role_fns, PROC_SET_ROLE_TYPE_DATA_NONE), \
    ENTRY(proc_permit_bt_fns, NO_DATA), \
    ENTRY(proc_permit_connection_le_fns, NO_DATA), \
    ENTRY(proc_event_suppress_fns, PROC_EVENT_SUPPRESS_DATA_EXPOSE_OUT_CASE), \
    ENTRY(proc_find_role_fns, PROC_FIND_ROLE_TIMEOUT_DATA_TIMEOUT)


/* Define the no_role_find_role_script */
DEFINE_TOPOLOGY_SCRIPT(no_role_find_role, NO_ROLE_FIND_ROLE_SCRIPT);

