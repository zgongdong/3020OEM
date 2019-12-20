/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      
*/

#ifndef TWS_TOPOLOGY_PROC_CONNECTABLE_HANDSET_T
#define TWS_TOPOLOGY_PROC_CONNECTABLE_HANDSET_T

#include "tws_topology_procedures.h"

extern const tws_topology_procedure_fns_t proc_connectable_handset_fns;

typedef struct
{
    bool enable;
} CONNECTABLE_HANDSET_PARAMS_T; 

/*! Parameter definition for connectable enable */
extern const CONNECTABLE_HANDSET_PARAMS_T proc_connectable_handset_enable;
#define PROC_CONNECTABLE_HANDSET_DATA_ENABLE  ((Message)&proc_connectable_handset_enable)

/*! Parameter definition for connectable disable */
extern const CONNECTABLE_HANDSET_PARAMS_T proc_connectable_handset_disable;
#define PROC_CONNECTABLE_HANDSET_DATA_DISABLE  ((Message)&proc_connectable_handset_disable)
#endif /* TWS_TOPOLOGY_PROC_CONNECTABLE_HANDSET_T */
