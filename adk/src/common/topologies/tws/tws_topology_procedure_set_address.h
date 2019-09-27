/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      
*/

#ifndef TWS_TOPOLOGY_PROC_SET_ADDRESS_H
#define TWS_TOPOLOGY_PROC_SET_ADDRESS_H

#include "tws_topology_procedure_script_engine.h"
#include "tws_topology_procedures.h"

extern tws_topology_procedure_fns_t proc_set_address_fns;

typedef struct
{
    bool primary;
} SET_ADDRESS_TYPE_T;

extern const SET_ADDRESS_TYPE_T proc_set_address_primary;
#define PROC_SET_ADDRESS_TYPE_DATA_PRIMARY  ((Message)&proc_set_address_primary)

extern const SET_ADDRESS_TYPE_T proc_set_address_secondary;
#define PROC_SET_ADDRESS_TYPE_DATA_SECONDARY  ((Message)&proc_set_address_secondary)

/* Make script available that will set primary address (including BT protection). */
extern const tws_topology_proc_script_t set_primary_address_script;

#endif /* TWS_TOPOLOGY_PROC_SET_ADDRESS_H */
