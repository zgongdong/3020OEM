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
    tws_topology_procedure_pair_peer                        = 1UL << 0,

    /*! Procedure to determine Primary/Secondary role of Earbud. */
    tws_topology_procedure_find_role                        = 1UL << 1,

    /*! Procedure to connect BREDR ACL to Primary Earbud. */
    tws_topology_procedure_sec_connect_peer                 = 1UL << 2,

    /*! Procedure to connect BREDR profiles to Secondary Earbud. */
    tws_topology_procedure_pri_connect_peer_profiles        = 1UL << 3,

    /*! Procedure to connect BREDR profiles to Earbud. */
    tws_topology_procedure_disconnect_peer_profiles         = 1UL << 4,

    /*! Procedure to enable page scan for Secondary to establish BREDR ACL to Primary Earbud. */
    tws_topology_procedure_pri_connectable_peer             = 1UL << 5,

    tws_topology_procedure_no_role_idle                     = 1UL << 6,
    tws_topology_procedure_set_role                         = 1UL << 7,

    tws_topology_procedure_connect_handset                  = 1UL << 8,

    tws_topology_procedure_disconnect_handset               = 1UL << 9,

    tws_topology_procedure_connectable_handset              = 1UL << 10,

    tws_topology_procedure_permit_bt                        = 1UL << 11,
    tws_topology_procedure_prohibit_bt                      = 1UL << 12,
    tws_topology_procedure_set_address                      = 1UL << 13,

    tws_topology_procedure_become_primary                   = 1UL << 14,
    tws_topology_procedure_become_secondary                 = 1UL << 15,
    tws_topology_procedure_become_acting_primary            = 1UL << 16,
    tws_topology_procedure_set_primary_address_and_find_role= 1UL << 17,

    tws_topology_procedure_role_switch_to_secondary         = 1UL << 18,
    tws_topology_procedure_no_role_find_role                = 1UL << 19,
    tws_topology_procedure_cancel_find_role                 = 1UL << 20,
    tws_topology_procedure_primary_find_role                = 1UL << 21,

    tws_topology_procedure_permit_connection_le             = 1UL << 22,
    tws_topology_procedure_prohibit_connection_le           = 1UL << 23,

    tws_topology_procedure_pair_peer_script                 = 1UL << 24,

    tws_topology_procedure_dfu_role                         = 1UL << 25,
    tws_topology_procedure_dfu_primary                      = 1UL << 26,
    tws_topology_procedure_dfu_secondary                    = 1UL << 27,

    tws_topology_procedure_allow_handset_connection         = 1UL << 28,

    tws_topology_procedure_disconnect_peer_find_role        = 1UL << 29,

    tws_topology_procedure_clean_connections                = 1UL << 30,
    /* Note that << 31 doesn't work. Presumably enums are signed */

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
