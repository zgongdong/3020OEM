/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Procedure for resuming a DFU after a scheduled restart

            Already using the DFU rules, this procedure switches to the primary
            address and allows connecting to the peer and handset.
*/

#include "tws_topology_procedure_dfu_secondary_after_boot.h"

#include "tws_topology_procedure_set_address.h"
#include "tws_topology_procedure_permit_bt.h"
#include "tws_topology_procedure_pri_connectable_peer.h"


/*! Definition of script for restarted DFU as secondary
 */
#define DFU_SECONDARY_AFTER_BOOT_SCRIPT(ENTRY) \
    ENTRY(proc_set_address_fns, PROC_SET_ADDRESS_TYPE_DATA_SECONDARY), \
    ENTRY(proc_permit_bt_fns, NO_DATA), \
    ENTRY(proc_pri_connectable_peer_fns, PROC_PRI_CONNECTABLE_PEER_DATA_ENABLE)

/* Define the dfu_secondary_after_boot_script */
DEFINE_TOPOLOGY_SCRIPT(dfu_secondary_after_boot, DFU_SECONDARY_AFTER_BOOT_SCRIPT);

