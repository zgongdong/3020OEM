/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Procedure to inform connection manager that connections from the handset 
            are allowed
*/

#ifndef TWS_TOPOLOGY_PROC_ALLOW_HANDSET_CONNECT_H
#define TWS_TOPOLOGY_PROC_ALLOW_HANDSET_CONNECT_H

#include "tws_topology_procedures.h"

typedef struct
{
    bool allow;
} ALLOW_HANDSET_CONNECT_PARAMS_T; 


extern const ALLOW_HANDSET_CONNECT_PARAMS_T proc_allow_handset_connect_allow;
#define PROC_ALLOW_HANDSET_CONNECT_DATA_ALLOW  ((Message)&proc_allow_handset_connect_allow)

extern const ALLOW_HANDSET_CONNECT_PARAMS_T proc_allow_handset_connect_disallow;
#define PROC_ALLOW_HANDSET_CONNECT_DATA_DISALLOW  ((Message)&proc_allow_handset_connect_disallow)

extern tws_topology_procedure_fns_t proc_allow_handset_connect_fns;

#endif /* TWS_TOPOLOGY_PROC_ALLOW_HANDSET_CONNECT_H */
