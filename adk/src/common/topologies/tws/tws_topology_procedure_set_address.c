/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      
*/

#include "tws_topology_procedure_set_address.h"
#include "tws_topology_procedure_prohibit_bt.h"
#include "tws_topology_procedure_permit_bt.h"
#include "tws_topology_procedures.h"

#include <bt_device.h>

#include <logging.h>

#include <message.h>
#include <bdaddr.h>
#include <panic.h>

const SET_ADDRESS_TYPE_T proc_set_address_primary = {TRUE};
const SET_ADDRESS_TYPE_T proc_set_address_secondary = {FALSE};

void TwsTopology_ProcedureSetAddressStart(Task result_task,
                                        twstop_proc_start_cfm_func_t proc_start_cfm_fn,
                                        twstop_proc_complete_func_t proc_complete_fn,
                                        Message goal_data);
void TwsTopology_ProcedureSetAddressCancel(twstop_proc_cancel_cfm_func_t proc_cancel_cfm_fn);

tws_topology_procedure_fns_t proc_set_address_fns = {
    TwsTopology_ProcedureSetAddressStart,
    TwsTopology_ProcedureSetAddressCancel,
    NULL,
};

/* Define components of script set_primary_address_script */
#define SET_PRIMARY_ADDRESS_SCRIPT(ENTRY) \
    ENTRY(proc_prohibit_bt_fns, NO_DATA), \
    ENTRY(proc_set_address_fns, PROC_SET_ADDRESS_TYPE_DATA_PRIMARY), \
    ENTRY(proc_permit_bt_fns, NO_DATA),
/* Create the script set_primary_address_script */
DEFINE_TOPOLOGY_SCRIPT(set_primary_address, SET_PRIMARY_ADDRESS_SCRIPT);

/*! The max number of times to attempt to override the BD_ADDR */
#define OVERRIDE_BD_ADDR_MAX_ATTEMPTS 1000

void TwsTopology_ProcedureSetAddressStart(Task result_task,
                                          twstop_proc_start_cfm_func_t proc_start_cfm_fn,
                                          twstop_proc_complete_func_t proc_complete_fn,
                                          Message goal_data)
{
    SET_ADDRESS_TYPE_T* type = (SET_ADDRESS_TYPE_T*)goal_data;
    bdaddr addr;
    unsigned attempts_remaining = OVERRIDE_BD_ADDR_MAX_ATTEMPTS;

    DEBUG_LOG("TwsTopology_ProcedureSetAddressStart");

    UNUSED(result_task);

    /* procedure is entirely synchronous, but play the game expected by the topology
     * goals engine, indicate the process started, do the switch, them indicate success
     * or failure on completion */

    proc_start_cfm_fn(tws_topology_procedure_set_address, proc_result_success);

    /* start the procedure */
    if (type->primary)
    {
        appDeviceGetPrimaryBdAddr(&addr);
        DEBUG_LOG("TwsTopology_ProcedureSetAddressStart, SWITCHING TO PRIMARY ADDRESS %04x,%02x,%06lx",
                   addr.nap, addr.uap, addr.lap);
    }
    else
    {
        appDeviceGetSecondaryBdAddr(&addr);
        DEBUG_LOG("TwsTopology_ProcedureSetAddressStart, SWITCHING TO SECONDARY ADDRESS %04x,%02x,%06lx",
                   addr.nap, addr.uap, addr.lap);
    }

    /* To override the BD_ADDR requires the BTSS to be idle. Occasionally the
       VmOverrideBdaddr trap can be called before BTSS has become completely idle,
       resulting in the trap returning false. In these scenarios, retry the
       address override. */
    while (attempts_remaining && !VmOverrideBdaddr(&addr))
    {
        attempts_remaining--;
    }
    if (attempts_remaining)
    {
        DEBUG_LOG("TwsTopology_ProcedureSetAddressStart (%u) SUCCESS", OVERRIDE_BD_ADDR_MAX_ATTEMPTS - attempts_remaining);
        /* update my address in BT device database if required */
        BtDevice_SetMyAddress(&addr);
        TwsTopology_DelayedCompleteCfmCallback(proc_complete_fn, tws_topology_procedure_set_address, proc_result_success);
    }
    else
    {
        DEBUG_LOG("TwsTopology_ProcedureSetAddressStart FAILED");
        TwsTopology_DelayedCompleteCfmCallback(proc_complete_fn, tws_topology_procedure_set_address, proc_result_failed);
    }
}

void TwsTopology_ProcedureSetAddressCancel(twstop_proc_cancel_cfm_func_t proc_cancel_cfm_fn)
{
    DEBUG_LOG("TwsTopology_ProcedureSetAddressCancel");
    TwsTopology_DelayedCancelCfmCallback(proc_cancel_cfm_fn, tws_topology_procedure_set_address, proc_result_success);
}
