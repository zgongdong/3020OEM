/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Interface to procedure to suppress or expose events injections into topology rules engines.
*/

#ifndef TWS_TOPOLOGY_PROC_EVENT_SUPPRESS_H
#define TWS_TOPOLOGY_PROC_EVENT_SUPPRESS_H

#include "tws_topology.h"
#include "tws_topology_procedures.h"

extern tws_topology_procedure_fns_t proc_event_suppress_fns;

typedef struct
{
    bool suppress;
    bool expected_in_case;
} EVENT_SUPPRESS_TYPE_T;

extern const EVENT_SUPPRESS_TYPE_T proc_event_suppress;
#define PROC_EVENT_SUPPRESS_DATA_SUPPRESS        ((Message)&proc_event_suppress)

extern const EVENT_SUPPRESS_TYPE_T proc_event_expose_in_case;
#define PROC_EVENT_SUPPRESS_DATA_EXPOSE_IN_CASE  ((Message)&proc_event_expose_in_case)

extern const EVENT_SUPPRESS_TYPE_T proc_event_expose_out_case;
#define PROC_EVENT_SUPPRESS_DATA_EXPOSE_OUT_CASE ((Message)&proc_event_expose_out_case)

#endif /* TWS_TOPOLOGY_PROC_EVENT_SUPPRESS_H */
