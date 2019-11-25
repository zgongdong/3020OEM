/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Generic interface to TWS Topology procedure for enabling/disabling ble adverts.
*/

#ifndef TWS_TOPOLOGY_PROCEDURE_LE_CONNECTABLE_HANDSET_H
#define TWS_TOPOLOGY_PROCEDURE_LE_CONNECTABLE_HANDSET_H

#include "tws_topology_procedures.h"
#include "tws_topology_primary_rules.h"
#include "tws_topology_procedure_script_engine.h"
#include <message.h>


extern const TWSTOP_PRIMARY_GOAL_LE_CONNECTABLE_HANDSET_T le_enable_connectable;
#define PROC_ENABLE_LE_CONNECTABLE_PARAMS  ((Message)&le_enable_connectable)

extern const TWSTOP_PRIMARY_GOAL_LE_CONNECTABLE_HANDSET_T le_disable_connectable;
#define PROC_DISABLE_LE_CONNECTABLE_PARAMS  ((Message)&le_disable_connectable)

extern tws_topology_procedure_fns_t proc_le_connectable_fns;

#endif /* TWS_TOPOLOGY_PROCEDURE_LE_CONNECTABLE_HANDSET_H */


