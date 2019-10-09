/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Generic interface to TWS Topology procedure handling for handover.
*/

#ifndef TWS_TOPOLOGY_PROCEDURE_HANDOVER_H
#define TWS_TOPOLOGY_PROCEDURE_HANDOVER_H

#include "tws_topology_procedures.h"
#include "tws_topology_procedure_script_engine.h"
#include <message.h>

typedef struct
{
    bool delay;
} HANDOVER_PARAMS_T;

extern const HANDOVER_PARAMS_T handover_delay;
extern const HANDOVER_PARAMS_T handover_no_delay;
#define PROC_HANDOVER_DELAY    ((Message)&handover_delay)
#define PROC_HANDOVER_NO_DELAY ((Message)&handover_no_delay)

extern tws_topology_procedure_fns_t proc_handover_fns;

#endif /* TWS_TOPOLOGY_PROCEDURE_HANDOVER_H */

