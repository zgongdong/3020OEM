/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Generic interface to TWS Topology procedure handling.
*/

#ifndef TWS_TOPOLOGY_PROCEDURES_H
#define TWS_TOPOLOGY_PROCEDURES_H

#include <message.h>

/*! Types of results returned from procedures. */
typedef enum
{
    /*! Procedure succeeded. */
    proc_result_success,

    /*! Procedure failed. */
    proc_result_failed,

    /*! Procedure timed out before success or failure determined. */
    proc_result_timeout,
} proc_result_t;

/*! Definition of TWS Topology procedures. */
typedef enum
{
    /*! Procedure to pair with peer Earbud. */
    tws_topology_procedure_pair_peer = 1,

    /*! Procedure to determine Primary/Secondary role of Earbud. */
    tws_topology_procedure_find_role,

    /*! Procedure to connect BREDR ACL to Primary Earbud. */
    tws_topology_procedure_sec_connect_peer,

    /*! Procedure to connect BREDR profiles to Secondary Earbud. */
    tws_topology_procedure_pri_connect_peer_profiles,

    /*! Procedure to connect BREDR profiles to Earbud. */
    tws_topology_procedure_disconnect_peer_profiles,

    /*! Procedure to enable page scan for Secondary to establish BREDR ACL to Primary Earbud. */
    tws_topology_procedure_pri_connectable_peer,

    tws_topology_procedure_no_role_idle,
    tws_topology_procedure_set_role,

    tws_topology_procedure_connect_handset,

    tws_topology_procedure_disconnect_handset,

    tws_topology_procedure_connectable_handset,

    tws_topology_procedure_permit_bt,
    tws_topology_procedure_prohibit_bt,
    tws_topology_procedure_set_address,

    tws_topology_procedure_become_primary,
    tws_topology_procedure_become_secondary,
    tws_topology_procedure_become_acting_primary,
    tws_topology_procedure_set_primary_address_and_find_role,

    tws_topology_procedure_role_switch_to_secondary,
    tws_topology_procedure_no_role_find_role,
    tws_topology_procedure_cancel_find_role,
    tws_topology_procedure_primary_find_role,

    tws_topology_procedure_permit_connection_le,
    tws_topology_procedure_prohibit_connection_le,

    tws_topology_procedure_pair_peer_script,

    tws_topology_procedure_dfu_role,
    tws_topology_procedure_dfu_primary,
    tws_topology_procedure_dfu_secondary,

    tws_topology_procedure_allow_handset_connection,

    tws_topology_procedure_disconnect_peer_find_role,

    tws_topology_procedure_clean_connections,

    /*! Procedure to release the lock on ACL to the peer and 
        (potentially) start closing the connection */
    tws_topology_procedure_release_peer,

    tws_topology_procedure_handover,
    tws_topology_procedure_handover_entering_case,
    tws_topology_procedure_handover_entering_case_timeout,

    tws_topology_procedure_command_role_switch,
    tws_topology_procedure_wait_peer_link_drop,
    tws_topology_procedure_secondary_static_handover,
    tws_topology_procedure_primary_static_handover_in_case,
    tws_topology_procedure_event_suppress,
    tws_topology_procedure_primary_static_handover,
    tws_topology_procedure_dfu_in_case,

} tws_topology_procedure;

/*! Callback used by procedure to indicate it has started. */
typedef void (*twstop_proc_start_cfm_func_t)(tws_topology_procedure proc, proc_result_t result);

/*! Callback used by procedure to indicate it has completed. */
typedef void (*twstop_proc_complete_func_t)(tws_topology_procedure proc, proc_result_t result);

/*! Callback used by procedure to indicate cancellation has completed. */
typedef void (*twstop_proc_cancel_cfm_func_t)(tws_topology_procedure proc, proc_result_t result);

/*! Definition of common procedure start function. */
typedef void (*proc_start_t)(Task result_task,
                             twstop_proc_start_cfm_func_t start_fn,
                             twstop_proc_complete_func_t comp_fn,
                             Message goal_data);

/*! Definition of common procedure cancellation function. */
typedef void (*proc_cancel_t)(twstop_proc_cancel_cfm_func_t cancel_fn);

typedef void (*proc_update_t)(Message goal_data);

typedef struct
{
    proc_start_t    proc_start_fn;
    proc_cancel_t   proc_cancel_fn;
    proc_update_t   proc_update_fn;
} tws_topology_procedure_fns_t;

void TwsTopology_DelayedStartCfmCallback(twstop_proc_start_cfm_func_t start_fn,
                                         tws_topology_procedure proc, proc_result_t result);
void TwsTopology_DelayedCompleteCfmCallback(twstop_proc_complete_func_t comp_fn,
                                            tws_topology_procedure proc, proc_result_t result);
void TwsTopology_DelayedCancelCfmCallback(twstop_proc_cancel_cfm_func_t cancel_fn,
                                            tws_topology_procedure proc, proc_result_t result);



/*! Helper macro used in definining a topology script 

    This macro selects the FN element of an entry */
#define XDEF_TOPOLOGY_SCRIPT_FNS(fns,data) &fns

/*! Helper macro used in definining a topology script 

    This macro selects the data element of an entry */
#define XDEF_TOPOLOGY_SCRIPT_DATA(fns,data) data


/*! Macro used to define a Topology script.

    The macro is used to define a script and creates the three tables necessary.
    Three tables are used to reduce space. Combining the function pointer and data
    in the same table will lead to padding, which is undesirable (if simply avoidable).

    An example of how to use the script is shown below. It is recommended to include
    the comment so that the script can be found.

    \param name The base name of the script. _script will be appended
    \param list A macro that takes a macro as a parameter and has a list
        of tuples using that macro. See the example

    \usage

    <PRE>
        #define MY_SCRIPT(ENTRY) \
            ENTRY(proc_permit_connection_le_fns,NO_DATA), \
            ENTRY(proc_permit_bt_fns, NO_DATA), \
            ENTRY(proc_pair_peer_fns, NO_DATA),

        // Define the script pair_peer_script
        DEFINE_TOPOLOGY_SCRIPT(pair_peer,PEER_PAIR_SCRIPT);
    </PRE>
*/
#define DEFINE_TOPOLOGY_SCRIPT(name, list) \
    const tws_topology_procedure_fns_t *name##_procs[] = { \
        list(XDEF_TOPOLOGY_SCRIPT_FNS) \
        }; \
    const Message name##_procs_data[] = { \
        list(XDEF_TOPOLOGY_SCRIPT_DATA) \
        }; \
    const tws_topology_proc_script_t name##_script = { \
        name##_procs, \
        name##_procs_data, \
        ARRAY_DIM(name##_procs), \
    }


#endif /* TWS_TOPOLOGY_PROCEDURES_H */
