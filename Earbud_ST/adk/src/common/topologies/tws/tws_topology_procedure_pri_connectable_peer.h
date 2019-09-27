/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      
\brief      Interface to procedure to enable/disable BREDR connectable (pagescan) for peer connectivity.
*/

#ifndef TWS_TOPOLOGY_PROC_PRI_CONNECTABLE_PEER_H
#define TWS_TOPOLOGY_PROC_PRI_CONNECTABLE_PEER_H

#include "tws_topology_procedures.h"

extern tws_topology_procedure_fns_t proc_pri_connectable_peer_fns;

typedef struct
{
    bool enable;
} PRI_CONNECTABLE_PEER_PARAMS_T; 

/*! Parameter definition for connectable enable */
extern const PRI_CONNECTABLE_PEER_PARAMS_T proc_connectable_peer_enable;
#define PROC_PRI_CONNECTABLE_PEER_DATA_ENABLE  ((Message)&proc_connectable_peer_enable)

/*! Parameter definition for connectable disable */
extern const PRI_CONNECTABLE_PEER_PARAMS_T proc_pri_connectable_peer_disable;
#define PROC_PRI_CONNECTABLE_PEER_DATA_DISABLE   ((Message)&proc_pri_connectable_peer_disable)

#endif /* TWS_TOPOLOGY_PROC_PRI_CONNECTABLE_PEER_H */
