/**
* Copyright (c) 2014 - 2017 Qualcomm Technologies International, Ltd.
* \file  cvc_send.c
* \ingroup  capabilities
*
*  CVC send
*
*/

/****************************************************************************
Include Files
*/
#include "capabilities.h"
#include "cvc_send_cap_c.h"
#include "mem_utils/dynloader.h"
#include "mem_utils/exported_constants.h"
#include "mem_utils/exported_constant_files.h"
#include "../lib/audio_proc/iir_resamplev2_util.h"
#include "cvc_processing_c.h"

/****************************************************************************

Local Definitions
*/

/* Reference inputs 0-2 */
#define CVC_SEND_NUM_INPUTS_MASK    0x7
#define CVC_SEND_NUM_OUTPUTS_MASK   0x7

/* CVC send terminals */
#define CVC_SEND_IN_TERMINAL_AECREF 0
#define CVC_SEND_IN_TERMINAL_MIC1   1

#define CVC_SEND_OUT_TERMINAL_VOICE 0
#define CVC_SEND_OUT_TERMINAL_VA    1

/* Voice quality metric error code */
#define CVC_SEND_VOICE_QUALITY_METRIC_ERROR_CODE 0xFF

/* bit field for kicking VA channels */
#define TOUCHED_CVC_VA_SOURCES     0x01E 

/****************************************************************************
Private Constant Definitions
*/

#ifdef CAPABILITY_DOWNLOAD_BUILD
#define CVCHS1MIC_SEND_NB_CAP_ID           CAP_ID_DOWNLOAD_CVCHS1MIC_SEND_NB
#define CVCHS1MIC_SEND_WB_CAP_ID           CAP_ID_DOWNLOAD_CVCHS1MIC_SEND_WB
#define CVCHS1MIC_SEND_UWB_CAP_ID          CAP_ID_DOWNLOAD_CVCHS1MIC_SEND_UWB
#define CVCHS1MIC_SEND_SWB_CAP_ID          CAP_ID_DOWNLOAD_CVCHS1MIC_SEND_SWB
#define CVCHS1MIC_SEND_FB_CAP_ID           CAP_ID_DOWNLOAD_CVCHS1MIC_SEND_FB
#define CVCHS2MIC_MONO_SEND_NB_CAP_ID      CAP_ID_DOWNLOAD_CVCHS2MIC_MONO_SEND_NB
#define CVCHS2MIC_MONO_SEND_WB_CAP_ID      CAP_ID_DOWNLOAD_CVCHS2MIC_MONO_SEND_WB
#define CVCHS2MIC_WAKEON_WB_CAP_ID         CAP_ID_DOWNLOAD_CVCHS2MIC_WAKEON_WB
#define CVCHS2MIC_BARGEIN_WB_CAP_ID        CAP_ID_DOWNLOAD_CVCHS2MIC_BARGEIN_WB

#define CVCHS2MIC_MONO_SEND_UWB_CAP_ID     CAP_ID_DOWNLOAD_CVCHS2MIC_MONO_SEND_UWB
#define CVCHS2MIC_MONO_SEND_SWB_CAP_ID     CAP_ID_DOWNLOAD_CVCHS2MIC_MONO_SEND_SWB
#define CVCHS2MIC_MONO_SEND_FB_CAP_ID      CAP_ID_DOWNLOAD_CVCHS2MIC_MONO_SEND_FB
#define CVCHS2MIC_BINAURAL_SEND_NB_CAP_ID  CAP_ID_DOWNLOAD_CVCHS2MIC_BINAURAL_SEND_NB
#define CVCHS2MIC_BINAURAL_SEND_WB_CAP_ID  CAP_ID_DOWNLOAD_CVCHS2MIC_BINAURAL_SEND_WB
#define CVCHS2MIC_BINAURAL_SEND_UWB_CAP_ID CAP_ID_DOWNLOAD_CVCHS2MIC_BINAURAL_SEND_UWB
#define CVCHS2MIC_BINAURAL_SEND_SWB_CAP_ID CAP_ID_DOWNLOAD_CVCHS2MIC_BINAURAL_SEND_SWB
#define CVCHS2MIC_BINAURAL_SEND_FB_CAP_ID  CAP_ID_DOWNLOAD_CVCHS2MIC_BINAURAL_SEND_FB
#define CVCHF1MIC_SEND_NB_CAP_ID           CAP_ID_DOWNLOAD_CVCHF1MIC_SEND_NB
#define CVCHF1MIC_SEND_WB_CAP_ID           CAP_ID_DOWNLOAD_CVCHF1MIC_SEND_WB
#define CVCHF1MIC_SEND_UWB_CAP_ID          CAP_ID_DOWNLOAD_CVCHF1MIC_SEND_UWB
#define CVCHF1MIC_SEND_SWB_CAP_ID          CAP_ID_DOWNLOAD_CVCHF1MIC_SEND_SWB
#define CVCHF1MIC_SEND_FB_CAP_ID           CAP_ID_DOWNLOAD_CVCHF1MIC_SEND_FB
#define CVCHF2MIC_SEND_NB_CAP_ID           CAP_ID_DOWNLOAD_CVCHF2MIC_SEND_NB
#define CVCHF2MIC_SEND_WB_CAP_ID           CAP_ID_DOWNLOAD_CVCHF2MIC_SEND_WB
#define CVCHF2MIC_SEND_UWB_CAP_ID          CAP_ID_DOWNLOAD_CVCHF2MIC_SEND_UWB
#define CVCHF2MIC_SEND_SWB_CAP_ID          CAP_ID_DOWNLOAD_CVCHF2MIC_SEND_SWB
#define CVCHF2MIC_SEND_FB_CAP_ID           CAP_ID_DOWNLOAD_CVCHF2MIC_SEND_FB
#define CVCSPKR1MIC_SEND_NB_CAP_ID         CAP_ID_DOWNLOAD_CVCSPKR1MIC_SEND_NB
#define CVCSPKR1MIC_SEND_WB_CAP_ID         CAP_ID_DOWNLOAD_CVCSPKR1MIC_SEND_WB
#define CVCSPKR1MIC_SEND_UWB_CAP_ID        CAP_ID_DOWNLOAD_CVCSPKR1MIC_SEND_UWB
#define CVCSPKR1MIC_SEND_SWB_CAP_ID        CAP_ID_DOWNLOAD_CVCSPKR1MIC_SEND_SWB
#define CVCSPKR1MIC_SEND_FB_CAP_ID         CAP_ID_DOWNLOAD_CVCSPKR1MIC_SEND_FB
#define CVCSPKR2MIC_SEND_NB_CAP_ID         CAP_ID_DOWNLOAD_CVCSPKR2MIC_SEND_NB
#define CVCSPKR2MIC_SEND_WB_CAP_ID         CAP_ID_DOWNLOAD_CVCSPKR2MIC_SEND_WB
#define CVCSPKR2MIC_SEND_UWB_CAP_ID        CAP_ID_DOWNLOAD_CVCSPKR2MIC_SEND_UWB
#define CVCSPKR2MIC_SEND_SWB_CAP_ID        CAP_ID_DOWNLOAD_CVCSPKR2MIC_SEND_SWB
#define CVCSPKR2MIC_SEND_FB_CAP_ID         CAP_ID_DOWNLOAD_CVCSPKR2MIC_SEND_FB
#define CVCSPKR3MIC_SEND_NB_CAP_ID         CAP_ID_DOWNLOAD_CVCSPKR3MIC_SEND_NB
#define CVCSPKR3MIC_SEND_WB_CAP_ID         CAP_ID_DOWNLOAD_CVCSPKR3MIC_SEND_WB
#define CVCSPKR3MIC_SEND_UWB_CAP_ID        CAP_ID_DOWNLOAD_CVCSPKR3MIC_SEND_UWB
#define CVCSPKR3MIC_SEND_SWB_CAP_ID        CAP_ID_DOWNLOAD_CVCSPKR3MIC_SEND_SWB
#define CVCSPKR3MIC_SEND_FB_CAP_ID         CAP_ID_DOWNLOAD_CVCSPKR3MIC_SEND_FB
#define CVCSPKR3MIC_CIRC_SEND_NB_CAP_ID    CAP_ID_DOWNLOAD_CVCSPKR3MIC_CIRC_SEND_NB
#define CVCSPKR3MIC_CIRC_SEND_WB_CAP_ID    CAP_ID_DOWNLOAD_CVCSPKR3MIC_CIRC_SEND_WB
#define CVCSPKR3MIC_CIRC_SEND_UWB_CAP_ID   CAP_ID_DOWNLOAD_CVCSPKR3MIC_CIRC_SEND_UWB
#define CVCSPKR3MIC_CIRC_SEND_SWB_CAP_ID   CAP_ID_DOWNLOAD_CVCSPKR3MIC_CIRC_SEND_SWB
#define CVCSPKR3MIC_CIRC_SEND_FB_CAP_ID    CAP_ID_DOWNLOAD_CVCSPKR3MIC_CIRC_SEND_FB

#define CVCSPKR3MIC_CIRC_VA_SEND_WB_CAP_ID      CAP_ID_DOWNLOAD_CVCSPKR3MIC_CIRC_VA_SEND_WB
#define CVCSPKR3MIC_CIRC_VA4B_SEND_WB_CAP_ID    CAP_ID_DOWNLOAD_CVCSPKR3MIC_CIRC_VA4B_SEND_WB

#define CVCSPKR4MIC_SEND_NB_CAP_ID         CAP_ID_DOWNLOAD_CVCSPKR4MIC_SEND_NB
#define CVCSPKR4MIC_SEND_WB_CAP_ID         CAP_ID_DOWNLOAD_CVCSPKR4MIC_SEND_WB
#define CVCSPKR4MIC_SEND_UWB_CAP_ID        CAP_ID_DOWNLOAD_CVCSPKR4MIC_SEND_UWB
#define CVCSPKR4MIC_SEND_SWB_CAP_ID        CAP_ID_DOWNLOAD_CVCSPKR4MIC_SEND_SWB
#define CVCSPKR4MIC_SEND_FB_CAP_ID         CAP_ID_DOWNLOAD_CVCSPKR4MIC_SEND_FB
#define CVCSPKR4MIC_CIRC_SEND_UWB_CAP_ID   CAP_ID_DOWNLOAD_CVCSPKR4MIC_CIRC_SEND_UWB
#define CVCSPKR4MIC_CIRC_SEND_SWB_CAP_ID   CAP_ID_DOWNLOAD_CVCSPKR4MIC_CIRC_SEND_SWB
#define CVCSPKR4MIC_CIRC_SEND_FB_CAP_ID    CAP_ID_DOWNLOAD_CVCSPKR4MIC_CIRC_SEND_FB
#else
#define CVCHS1MIC_SEND_NB_CAP_ID           CAP_ID_CVCHS1MIC_SEND_NB
#define CVCHS1MIC_SEND_WB_CAP_ID           CAP_ID_CVCHS1MIC_SEND_WB
#define CVCHS1MIC_SEND_UWB_CAP_ID          CAP_ID_CVCHS1MIC_SEND_UWB
#define CVCHS1MIC_SEND_SWB_CAP_ID          CAP_ID_CVCHS1MIC_SEND_SWB
#define CVCHS1MIC_SEND_FB_CAP_ID           CAP_ID_CVCHS1MIC_SEND_FB
#define CVCHS2MIC_MONO_SEND_NB_CAP_ID      CAP_ID_CVCHS2MIC_MONO_SEND_NB
#define CVCHS2MIC_MONO_SEND_WB_CAP_ID      CAP_ID_CVCHS2MIC_MONO_SEND_WB
#define CVCHS2MIC_WAKEON_WB_CAP_ID         CAP_ID_CVCHS2MIC_WAKEON_WB
#define CVCHS2MIC_BARGEIN_WB_CAP_ID        CAP_ID_CVCHS2MIC_BARGEIN_WB

#define CVCHS2MIC_MONO_SEND_UWB_CAP_ID     CAP_ID_CVCHS2MIC_MONO_SEND_UWB
#define CVCHS2MIC_MONO_SEND_SWB_CAP_ID     CAP_ID_CVCHS2MIC_MONO_SEND_SWB
#define CVCHS2MIC_MONO_SEND_FB_CAP_ID      CAP_ID_CVCHS2MIC_MONO_SEND_FB
#define CVCHS2MIC_BINAURAL_SEND_NB_CAP_ID  CAP_ID_CVCHS2MIC_BINAURAL_SEND_NB
#define CVCHS2MIC_BINAURAL_SEND_WB_CAP_ID  CAP_ID_CVCHS2MIC_BINAURAL_SEND_WB
#define CVCHS2MIC_BINAURAL_SEND_UWB_CAP_ID CAP_ID_CVCHS2MIC_BINAURAL_SEND_UWB
#define CVCHS2MIC_BINAURAL_SEND_SWB_CAP_ID CAP_ID_CVCHS2MIC_BINAURAL_SEND_SWB
#define CVCHS2MIC_BINAURAL_SEND_FB_CAP_ID  CAP_ID_CVCHS2MIC_BINAURAL_SEND_FB
#define CVCHF1MIC_SEND_NB_CAP_ID           CAP_ID_CVCHF1MIC_SEND_NB
#define CVCHF1MIC_SEND_WB_CAP_ID           CAP_ID_CVCHF1MIC_SEND_WB
#define CVCHF1MIC_SEND_UWB_CAP_ID          CAP_ID_CVCHF1MIC_SEND_UWB
#define CVCHF1MIC_SEND_SWB_CAP_ID          CAP_ID_CVCHF1MIC_SEND_SWB
#define CVCHF1MIC_SEND_FB_CAP_ID           CAP_ID_CVCHF1MIC_SEND_FB
#define CVCHF2MIC_SEND_NB_CAP_ID           CAP_ID_CVCHF2MIC_SEND_NB
#define CVCHF2MIC_SEND_WB_CAP_ID           CAP_ID_CVCHF2MIC_SEND_WB
#define CVCHF2MIC_SEND_UWB_CAP_ID          CAP_ID_CVCHF2MIC_SEND_UWB
#define CVCHF2MIC_SEND_SWB_CAP_ID          CAP_ID_CVCHF2MIC_SEND_SWB
#define CVCHF2MIC_SEND_FB_CAP_ID           CAP_ID_CVCHF2MIC_SEND_FB
#define CVCSPKR1MIC_SEND_NB_CAP_ID         CAP_ID_CVCSPKR1MIC_SEND_NB
#define CVCSPKR1MIC_SEND_WB_CAP_ID         CAP_ID_CVCSPKR1MIC_SEND_WB
#define CVCSPKR1MIC_SEND_UWB_CAP_ID        CAP_ID_CVCSPKR1MIC_SEND_UWB
#define CVCSPKR1MIC_SEND_SWB_CAP_ID        CAP_ID_CVCSPKR1MIC_SEND_SWB
#define CVCSPKR1MIC_SEND_FB_CAP_ID         CAP_ID_CVCSPKR1MIC_SEND_FB
#define CVCSPKR2MIC_SEND_NB_CAP_ID         CAP_ID_CVCSPKR2MIC_SEND_NB
#define CVCSPKR2MIC_SEND_WB_CAP_ID         CAP_ID_CVCSPKR2MIC_SEND_WB
#define CVCSPKR2MIC_SEND_UWB_CAP_ID        CAP_ID_CVCSPKR2MIC_SEND_UWB
#define CVCSPKR2MIC_SEND_SWB_CAP_ID        CAP_ID_CVCSPKR2MIC_SEND_SWB
#define CVCSPKR2MIC_SEND_FB_CAP_ID         CAP_ID_CVCSPKR2MIC_SEND_FB
#define CVCSPKR3MIC_SEND_NB_CAP_ID         CAP_ID_CVCSPKR3MIC_SEND_NB
#define CVCSPKR3MIC_SEND_WB_CAP_ID         CAP_ID_CVCSPKR3MIC_SEND_WB
#define CVCSPKR3MIC_SEND_UWB_CAP_ID        CAP_ID_CVCSPKR3MIC_SEND_UWB
#define CVCSPKR3MIC_SEND_SWB_CAP_ID        CAP_ID_CVCSPKR3MIC_SEND_SWB
#define CVCSPKR3MIC_SEND_FB_CAP_ID         CAP_ID_CVCSPKR3MIC_SEND_FB
#define CVCSPKR3MIC_CIRC_SEND_NB_CAP_ID    CAP_ID_CVCSPKR3MIC_CIRC_SEND_NB
#define CVCSPKR3MIC_CIRC_SEND_WB_CAP_ID    CAP_ID_CVCSPKR3MIC_CIRC_SEND_WB
#define CVCSPKR3MIC_CIRC_SEND_UWB_CAP_ID   CAP_ID_CVCSPKR3MIC_CIRC_SEND_UWB
#define CVCSPKR3MIC_CIRC_SEND_SWB_CAP_ID   CAP_ID_CVCSPKR3MIC_CIRC_SEND_SWB
#define CVCSPKR3MIC_CIRC_SEND_FB_CAP_ID    CAP_ID_CVCSPKR3MIC_CIRC_SEND_FB

#define CVCSPKR3MIC_CIRC_VA_SEND_WB_CAP_ID    CAP_ID_CVCSPKR3MIC_CIRC_VA_SEND_WB
#define CVCSPKR3MIC_CIRC_VA4B_SEND_WB_CAP_ID    CAP_ID_CVCSPKR3MIC_CIRC_VA4B_SEND_WB

#define CVCSPKR4MIC_SEND_NB_CAP_ID         CAP_ID_CVCSPKR4MIC_SEND_NB
#define CVCSPKR4MIC_SEND_WB_CAP_ID         CAP_ID_CVCSPKR4MIC_SEND_WB
#define CVCSPKR4MIC_SEND_UWB_CAP_ID        CAP_ID_CVCSPKR4MIC_SEND_UWB
#define CVCSPKR4MIC_SEND_SWB_CAP_ID        CAP_ID_CVCSPKR4MIC_SEND_SWB
#define CVCSPKR4MIC_SEND_FB_CAP_ID         CAP_ID_CVCSPKR4MIC_SEND_FB
#define CVCSPKR4MIC_CIRC_SEND_UWB_CAP_ID   CAP_ID_CVCSPKR4MIC_CIRC_SEND_UWB
#define CVCSPKR4MIC_CIRC_SEND_SWB_CAP_ID   CAP_ID_CVCSPKR4MIC_CIRC_SEND_SWB
#define CVCSPKR4MIC_CIRC_SEND_FB_CAP_ID    CAP_ID_CVCSPKR4MIC_CIRC_SEND_FB

#endif

/* Message handlers */

/** The cvc send capability function handler table */
const handler_lookup_struct cvc_send_handler_table =
{
    cvc_send_create,          /* OPCMD_CREATE */
    cvc_send_destroy,         /* OPCMD_DESTROY */
    base_op_start,            /* OPCMD_START */
    base_op_stop,             /* OPCMD_STOP */
    base_op_reset,            /* OPCMD_RESET */
    cvc_send_connect,         /* OPCMD_CONNECT */
    cvc_send_disconnect,      /* OPCMD_DISCONNECT */
    cvc_send_buffer_details,  /* OPCMD_BUFFER_DETAILS */
    base_op_get_data_format,  /* OPCMD_DATA_FORMAT */
    cvc_send_get_sched_info   /* OPCMD_GET_SCHED_INFO */
};

/* Null terminated operator message handler table */

const opmsg_handler_lookup_table_entry cvc_send_opmsg_handler_table[] =
    {{OPMSG_COMMON_ID_GET_CAPABILITY_VERSION,           base_op_opmsg_get_capability_version},
    {OPMSG_COMMON_ID_SET_CONTROL,                       cvc_send_opmsg_obpm_set_control},
    {OPMSG_COMMON_ID_GET_PARAMS,                        cvc_send_opmsg_obpm_get_params},
    {OPMSG_COMMON_ID_GET_DEFAULTS,                      cvc_send_opmsg_obpm_get_defaults},
    {OPMSG_COMMON_ID_SET_PARAMS,                        cvc_send_opmsg_obpm_set_params},
    {OPMSG_COMMON_ID_GET_STATUS,                        cvc_send_opmsg_obpm_get_status},

    {OPMSG_COMMON_ID_SET_UCID,                          cvc_send_opmsg_set_ucid},
    {OPMSG_COMMON_ID_GET_LOGICAL_PS_ID,                 cvc_send_opmsg_get_ps_id},
    {OPMSG_COMMON_GET_VOICE_QUALITY,                    cvc_send_opmsg_get_voice_quality},

    {0, NULL}};


/****************************************************************************
CVC send capability data declarations
*/
#ifdef INSTALL_OPERATOR_CVC_HEADSET_1MIC
    const CAPABILITY_DATA cvc_send_1mic_nb_hs_cap_data =
        {
            CVCHS1MIC_SEND_NB_CAP_ID,       /* Capability ID */
            GEN_CVC_SEND_1M_HS_NB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            2, CVC_NUM_OUTPUTS,           /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCHS1MIC_SEND_NB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCHS1MIC_SEND_NB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */

    const CAPABILITY_DATA cvc_send_1mic_wb_hs_cap_data =
        {
            CVCHS1MIC_SEND_WB_CAP_ID,       /* Capability ID */
            GEN_CVC_SEND_1M_HS_WB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            2, CVC_NUM_OUTPUTS,           /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCHS1MIC_SEND_WB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCHS1MIC_SEND_WB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */

#ifdef INSTALL_OPERATOR_CVC_24K
    const CAPABILITY_DATA cvc_send_1mic_uwb_hs_cap_data =
        {
            CVCHS1MIC_SEND_UWB_CAP_ID,      /* Capability ID */
            GEN_CVC_SEND_1M_HS_UWB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            2, CVC_NUM_OUTPUTS,           /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCHS1MIC_SEND_UWB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCHS1MIC_SEND_UWB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */
#endif

#ifdef INSTALL_OPERATOR_CVC_32K
    const CAPABILITY_DATA cvc_send_1mic_swb_hs_cap_data =
        {
            CVCHS1MIC_SEND_SWB_CAP_ID,       /* Capability ID */
            GEN_CVC_SEND_1M_HS_SWB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            2, CVC_NUM_OUTPUTS,           /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCHS1MIC_SEND_SWB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCHS1MIC_SEND_SWB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */
#endif
#ifdef INSTALL_OPERATOR_CVC_48K
    const CAPABILITY_DATA cvc_send_1mic_fb_hs_cap_data =
        {
            CVCHS1MIC_SEND_FB_CAP_ID,       /* Capability ID */
            GEN_CVC_SEND_1M_HS_FB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            2, CVC_NUM_OUTPUTS,           /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCHS1MIC_SEND_FB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCHS1MIC_SEND_FB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */
#endif
#endif

#ifdef INSTALL_OPERATOR_CVC_HEADSET_2MIC_MONO
    const CAPABILITY_DATA cvc_send_2mic_hs_mono_nb_cap_data =
        {
            CVCHS2MIC_MONO_SEND_NB_CAP_ID,       /* Capability ID */
            GEN_CVC_SEND_2M_HSE_NB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            3, CVC_NUM_OUTPUTS,           /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCHS2MIC_MONO_SEND_NB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCHS2MIC_MONO_SEND_NB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */

    const CAPABILITY_DATA cvc_send_2mic_hs_mono_wb_cap_data =
        {
            CVCHS2MIC_MONO_SEND_WB_CAP_ID,       /* Capability ID */
            GEN_CVC_SEND_2M_HSE_WB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            3, CVC_NUM_OUTPUTS,           /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
     
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCHS2MIC_MONO_SEND_WB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCHS2MIC_MONO_SEND_WB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */

#ifdef INSTALL_OPERATOR_CVC_24K
    const CAPABILITY_DATA cvc_send_2mic_hs_mono_uwb_cap_data =
        {
            CVCHS2MIC_MONO_SEND_UWB_CAP_ID,       /* Capability ID */
            GEN_CVC_SEND_2M_HSE_UWB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            3, CVC_NUM_OUTPUTS,           /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCHS2MIC_MONO_SEND_UWB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCHS2MIC_MONO_SEND_UWB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */

#endif
#ifdef INSTALL_OPERATOR_CVC_32K
    const CAPABILITY_DATA cvc_send_2mic_hs_mono_swb_cap_data =
        {
            CVCHS2MIC_MONO_SEND_SWB_CAP_ID,       /* Capability ID */
            GEN_CVC_SEND_2M_HSE_SWB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            3, CVC_NUM_OUTPUTS,           /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCHS2MIC_MONO_SEND_SWB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCHS2MIC_MONO_SEND_SWB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */

#endif
#ifdef INSTALL_OPERATOR_CVC_48K
    const CAPABILITY_DATA cvc_send_2mic_hs_mono_fb_cap_data =
        {
            CVCHS2MIC_MONO_SEND_FB_CAP_ID,       /* Capability ID */
            GEN_CVC_SEND_2M_HSE_FB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            3, CVC_NUM_OUTPUTS,           /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCHS2MIC_MONO_SEND_FB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCHS2MIC_MONO_SEND_FB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */
#endif
#endif

#ifdef INSTALL_OPERATOR_CVC_HEADSET_BINAURAL
    const CAPABILITY_DATA cvc_send_2mic_hs_binaural_nb_cap_data =
        {
            CVCHS2MIC_BINAURAL_SEND_NB_CAP_ID,       /* Capability ID */
            GEN_CVC_SEND_2M_HSB_NB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            3, CVC_NUM_OUTPUTS,           /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCHS2MIC_BINAURAL_SEND_NB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCHS2MIC_BINAURAL_SEND_NB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */

    const CAPABILITY_DATA cvc_send_2mic_hs_binaural_wb_cap_data =
        {
            CVCHS2MIC_BINAURAL_SEND_WB_CAP_ID,       /* Capability ID */
            GEN_CVC_SEND_2M_HSB_WB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            3, CVC_NUM_OUTPUTS,           /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCHS2MIC_BINAURAL_SEND_WB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCHS2MIC_BINAURAL_SEND_WB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */

#ifdef INSTALL_OPERATOR_CVC_24K
    const CAPABILITY_DATA cvc_send_2mic_hs_binaural_uwb_cap_data =
        {
            CVCHS2MIC_BINAURAL_SEND_UWB_CAP_ID,       /* Capability ID */
            GEN_CVC_SEND_2M_HSB_UWB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            3, CVC_NUM_OUTPUTS,           /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCHS2MIC_BINAURAL_SEND_UWB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCHS2MIC_BINAURAL_SEND_UWB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */
#endif

#ifdef INSTALL_OPERATOR_CVC_32K
    const CAPABILITY_DATA cvc_send_2mic_hs_binaural_swb_cap_data =
        {
            CVCHS2MIC_BINAURAL_SEND_SWB_CAP_ID,       /* Capability ID */
            GEN_CVC_SEND_2M_HSB_SWB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            3, CVC_NUM_OUTPUTS,           /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCHS2MIC_BINAURAL_SEND_SWB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCHS2MIC_BINAURAL_SEND_SWB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */
#endif

#ifdef INSTALL_OPERATOR_CVC_48K
    const CAPABILITY_DATA cvc_send_2mic_hs_binaural_fb_cap_data =
        {
            CVCHS2MIC_BINAURAL_SEND_FB_CAP_ID,       /* Capability ID */
            GEN_CVC_SEND_2M_HSB_FB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            3, CVC_NUM_OUTPUTS,           /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCHS2MIC_BINAURAL_SEND_FB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCHS2MIC_BINAURAL_SEND_FB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */
#endif
#endif

#ifdef INSTALL_OPERATOR_CVC_AUTO_1MIC
    const CAPABILITY_DATA cvc_send_1mic_nb_auto_cap_data =
        {
            CVCHF1MIC_SEND_NB_CAP_ID,       /* Capability ID */
            GEN_CVC_SEND_1M_AUTO_NB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            2, CVC_NUM_OUTPUTS,           /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCHF1MIC_SEND_NB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCHF1MIC_SEND_NB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */

    const CAPABILITY_DATA cvc_send_1mic_wb_auto_cap_data =
        {
            CVCHF1MIC_SEND_WB_CAP_ID,       /* Capability ID */
            GEN_CVC_SEND_1M_AUTO_WB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            2, CVC_NUM_OUTPUTS,           /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCHF1MIC_SEND_WB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCHF1MIC_SEND_WB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */

#ifdef INSTALL_OPERATOR_CVC_24K
    const CAPABILITY_DATA cvc_send_1mic_uwb_auto_cap_data =
        {
            CVCHF1MIC_SEND_UWB_CAP_ID,       /* Capability ID */
            GEN_CVC_SEND_1M_AUTO_UWB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            2, CVC_NUM_OUTPUTS,           /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCHF1MIC_SEND_UWB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCHF1MIC_SEND_UWB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */

#endif
#ifdef INSTALL_OPERATOR_CVC_32K
    const CAPABILITY_DATA cvc_send_1mic_swb_auto_cap_data =
        {
            CVCHF1MIC_SEND_SWB_CAP_ID,       /* Capability ID */
            GEN_CVC_SEND_1M_AUTO_SWB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            2, CVC_NUM_OUTPUTS,           /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCHF1MIC_SEND_SWB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCHF1MIC_SEND_SWB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */

#endif
#ifdef INSTALL_OPERATOR_CVC_48K
    const CAPABILITY_DATA cvc_send_1mic_fb_auto_cap_data =
        {
            CVCHF1MIC_SEND_FB_CAP_ID,       /* Capability ID */
            GEN_CVC_SEND_1M_AUTO_FB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            2, CVC_NUM_OUTPUTS,           /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCHF1MIC_SEND_FB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCHF1MIC_SEND_FB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */
#endif
#endif

#ifdef INSTALL_OPERATOR_CVC_AUTO_2MIC
    const CAPABILITY_DATA cvc_send_2mic_nb_auto_cap_data =
        {
            CVCHF2MIC_SEND_NB_CAP_ID,                                                       /* Capability ID */
            GEN_CVC_SEND_2M_AUTO_NB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            3, CVC_NUM_OUTPUTS,           /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCHF2MIC_SEND_NB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCHF2MIC_SEND_NB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */

    const CAPABILITY_DATA cvc_send_2mic_wb_auto_cap_data =
        {
            CVCHF2MIC_SEND_WB_CAP_ID,                                                       /* Capability ID */
            GEN_CVC_SEND_2M_AUTO_WB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            3, CVC_NUM_OUTPUTS,           /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCHF2MIC_SEND_WB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCHF2MIC_SEND_WB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */

#ifdef INSTALL_OPERATOR_CVC_24K
    const CAPABILITY_DATA cvc_send_2mic_uwb_auto_cap_data =
        {
            CVCHF2MIC_SEND_UWB_CAP_ID,                                                       /* Capability ID */
            GEN_CVC_SEND_2M_AUTO_UWB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            3, CVC_NUM_OUTPUTS,           /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCHF2MIC_SEND_UWB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCHF2MIC_SEND_UWB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */
#endif

#ifdef INSTALL_OPERATOR_CVC_32K
    const CAPABILITY_DATA cvc_send_2mic_swb_auto_cap_data =
        {
            CVCHF2MIC_SEND_SWB_CAP_ID,                                                       /* Capability ID */
            GEN_CVC_SEND_2M_AUTO_SWB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            3, CVC_NUM_OUTPUTS,           /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCHF2MIC_SEND_SWB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCHF2MIC_SEND_SWB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */
#endif

#ifdef INSTALL_OPERATOR_CVC_48K
    const CAPABILITY_DATA cvc_send_2mic_fb_auto_cap_data =
        {
            CVCHF2MIC_SEND_FB_CAP_ID,                                                       /* Capability ID */
            GEN_CVC_SEND_2M_AUTO_FB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            3, CVC_NUM_OUTPUTS,           /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCHF2MIC_SEND_FB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCHF2MIC_SEND_FB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */
#endif
#endif

#ifdef INSTALL_OPERATOR_CVC_SPEAKER_1MIC
    const CAPABILITY_DATA cvc_send_1mic_speaker_nb_cap_data =
        {
            CVCSPKR1MIC_SEND_NB_CAP_ID,       /* Capability ID */
            GEN_CVC_SEND_1M_SPKR_NB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            2, CVC_NUM_OUTPUTS,           /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCSPKR1MIC_SEND_NB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCSPKR1MIC_SEND_NB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */

    const CAPABILITY_DATA cvc_send_1mic_speaker_wb_cap_data =
        {
            CVCSPKR1MIC_SEND_WB_CAP_ID,          /* Capability ID */
            GEN_CVC_SEND_1M_SPKR_WB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            2, CVC_NUM_OUTPUTS,           /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCSPKR1MIC_SEND_WB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCSPKR1MIC_SEND_WB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */

#ifdef INSTALL_OPERATOR_CVC_24K
    const CAPABILITY_DATA cvc_send_1mic_speaker_uwb_cap_data =
        {
            CVCSPKR1MIC_SEND_UWB_CAP_ID,       /* Capability ID */
            GEN_CVC_SEND_1M_SPKR_UWB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            2, CVC_NUM_OUTPUTS,           /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCSPKR1MIC_SEND_UWB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCSPKR1MIC_SEND_UWB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */
#endif

#ifdef INSTALL_OPERATOR_CVC_32K
    const CAPABILITY_DATA cvc_send_1mic_speaker_swb_cap_data =
        {
            CVCSPKR1MIC_SEND_SWB_CAP_ID,       /* Capability ID */
            GEN_CVC_SEND_1M_SPKR_SWB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            2, CVC_NUM_OUTPUTS,           /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCSPKR1MIC_SEND_SWB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCSPKR1MIC_SEND_SWB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */
#endif

#ifdef INSTALL_OPERATOR_CVC_48K
    const CAPABILITY_DATA cvc_send_1mic_speaker_fb_cap_data =
        {
            CVCSPKR1MIC_SEND_FB_CAP_ID,       /* Capability ID */
            GEN_CVC_SEND_1M_SPKR_FB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            2, CVC_NUM_OUTPUTS,           /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCSPKR1MIC_SEND_FB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCSPKR1MIC_SEND_FB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */
#endif
#endif

#ifdef INSTALL_OPERATOR_CVC_SPEAKER_2MIC
    const CAPABILITY_DATA cvc_send_2mic_speaker_nb_cap_data =
        {
            CVCSPKR2MIC_SEND_NB_CAP_ID,       /* Capability ID */
            GEN_CVC_SEND_2M_SPKRB_NB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            3, CVC_NUM_OUTPUTS,           /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCSPKR2MIC_SEND_NB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCSPKR2MIC_SEND_NB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */

    const CAPABILITY_DATA cvc_send_2mic_speaker_wb_cap_data =
        {
            CVCSPKR2MIC_SEND_WB_CAP_ID,       /* Capability ID */
            GEN_CVC_SEND_2M_SPKRB_WB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            3, CVC_NUM_OUTPUTS,           /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCSPKR2MIC_SEND_WB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCSPKR2MIC_SEND_WB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */

#ifdef INSTALL_OPERATOR_CVC_24K
    const CAPABILITY_DATA cvc_send_2mic_speaker_uwb_cap_data =
        {
            CVCSPKR2MIC_SEND_UWB_CAP_ID,       /* Capability ID */
            GEN_CVC_SEND_2M_SPKRB_UWB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            3, CVC_NUM_OUTPUTS,           /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCSPKR2MIC_SEND_UWB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCSPKR2MIC_SEND_UWB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */
#endif

#ifdef INSTALL_OPERATOR_CVC_32K
    const CAPABILITY_DATA cvc_send_2mic_speaker_swb_cap_data =
        {
            CVCSPKR2MIC_SEND_SWB_CAP_ID,       /* Capability ID */
            GEN_CVC_SEND_2M_SPKRB_SWB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            3, CVC_NUM_OUTPUTS,           /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCSPKR2MIC_SEND_SWB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCSPKR2MIC_SEND_SWB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */
#endif

#ifdef INSTALL_OPERATOR_CVC_48K
    const CAPABILITY_DATA cvc_send_2mic_speaker_fb_cap_data =
        {
            CVCSPKR2MIC_SEND_FB_CAP_ID,       /* Capability ID */
            GEN_CVC_SEND_2M_SPKRB_FB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            3, CVC_NUM_OUTPUTS,           /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCSPKR2MIC_SEND_FB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCSPKR2MIC_SEND_FB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */
#endif
#endif

#ifdef INSTALL_OPERATOR_CVC_SPEAKER_3MIC
    const CAPABILITY_DATA cvc_send_3mic_speaker_nb_cap_data =
        {
            CVCSPKR3MIC_SEND_NB_CAP_ID,                                                       /* Capability ID */
            GEN_CVC_SEND_3M_SPKRB_NB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            4, CVC_NUM_OUTPUTS,           /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCSPKR3MIC_SEND_NB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCSPKR3MIC_SEND_NB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */

    const CAPABILITY_DATA cvc_send_3mic_speaker_wb_cap_data =
        {
            CVCSPKR3MIC_SEND_WB_CAP_ID,                                                       /* Capability ID */
            GEN_CVC_SEND_3M_SPKRB_WB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            4, CVC_NUM_OUTPUTS,           /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCSPKR3MIC_SEND_WB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCSPKR3MIC_SEND_WB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */

#ifdef INSTALL_OPERATOR_CVC_24K
    const CAPABILITY_DATA cvc_send_3mic_speaker_uwb_cap_data =
        {
            CVCSPKR3MIC_SEND_UWB_CAP_ID,                                                       /* Capability ID */
            GEN_CVC_SEND_3M_SPKRB_UWB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            4, CVC_NUM_OUTPUTS,           /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCSPKR3MIC_SEND_UWB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCSPKR3MIC_SEND_UWB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */
#endif

#ifdef INSTALL_OPERATOR_CVC_32K
    const CAPABILITY_DATA cvc_send_3mic_speaker_swb_cap_data =
        {
            CVCSPKR3MIC_SEND_SWB_CAP_ID,                                                       /* Capability ID */
            GEN_CVC_SEND_3M_SPKRB_SWB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            4, CVC_NUM_OUTPUTS,           /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCSPKR3MIC_SEND_SWB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCSPKR3MIC_SEND_SWB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */
#endif

#ifdef INSTALL_OPERATOR_CVC_48K
    const CAPABILITY_DATA cvc_send_3mic_speaker_fb_cap_data =
        {
            CVCSPKR3MIC_SEND_FB_CAP_ID,                                                       /* Capability ID */
            GEN_CVC_SEND_3M_SPKRB_FB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            4, CVC_NUM_OUTPUTS,           /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCSPKR3MIC_SEND_FB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCSPKR3MIC_SEND_FB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */
#endif


#endif

#ifdef INSTALL_OPERATOR_CVC_SPEAKER_4MIC
    const CAPABILITY_DATA cvc_send_4mic_speaker_nb_cap_data =
        {
            CVCSPKR4MIC_SEND_NB_CAP_ID,                                                       /* Capability ID */
            GEN_CVC_SEND_4M_SPKRB_NB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            5, CVC_NUM_OUTPUTS,           /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCSPKR4MIC_SEND_NB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCSPKR4MIC_SEND_NB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */

    const CAPABILITY_DATA cvc_send_4mic_speaker_wb_cap_data =
        {
            CVCSPKR4MIC_SEND_WB_CAP_ID,                                                       /* Capability ID */
            GEN_CVC_SEND_4M_SPKRB_WB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            5, CVC_NUM_OUTPUTS,           /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCSPKR4MIC_SEND_WB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCSPKR4MIC_SEND_WB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */

#ifdef INSTALL_OPERATOR_CVC_24K
    const CAPABILITY_DATA cvc_send_4mic_speaker_uwb_cap_data =
        {
            CVCSPKR4MIC_SEND_UWB_CAP_ID,                                                       /* Capability ID */
            GEN_CVC_SEND_4M_SPKRB_UWB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            5, CVC_NUM_OUTPUTS,           /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCSPKR4MIC_SEND_UWB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_CVCSPKR4MIC_SEND_UWB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */

#endif
#ifdef INSTALL_OPERATOR_CVC_32K
    const CAPABILITY_DATA cvc_send_4mic_speaker_swb_cap_data =
        {
            CVCSPKR4MIC_SEND_SWB_CAP_ID,                                                       /* Capability ID */
            GEN_CVC_SEND_4M_SPKRB_SWB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            5, CVC_NUM_OUTPUTS,           /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCSPKR4MIC_SEND_SWB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCSPKR4MIC_SEND_SWB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */

#endif
#ifdef INSTALL_OPERATOR_CVC_48K
    const CAPABILITY_DATA cvc_send_4mic_speaker_fb_cap_data =
        {
            CVCSPKR4MIC_SEND_FB_CAP_ID,                                                       /* Capability ID */
            GEN_CVC_SEND_4M_SPKRB_FB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            5, CVC_NUM_OUTPUTS,           /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCSPKR4MIC_SEND_FB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCSPKR4MIC_SEND_FB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */
#endif


#endif

#ifdef INSTALL_OPERATOR_CVC_SPKR_CIRC_3MIC
    const CAPABILITY_DATA cvc_send_3mic_circ_speaker_wb_cap_data =
        {
            CVCSPKR3MIC_CIRC_SEND_WB_CAP_ID,                                                       /* Capability ID */
            GEN_CVC_SEND_3M_SPKRCIRC_WB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            4, CVC_NUM_OUTPUTS,           /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCSPKR3MIC_CIRC_SEND_WB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCSPKR3MIC_CIRC_SEND_WB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */

#ifdef INSTALL_OPERATOR_CVC_24K
    const CAPABILITY_DATA cvc_send_3mic_circ_speaker_uwb_cap_data =
        {
            CVCSPKR3MIC_CIRC_SEND_UWB_CAP_ID,                                                       /* Capability ID */
            GEN_CVC_SEND_3M_SPKRCIRC_UWB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            4, CVC_NUM_OUTPUTS,           /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCSPKR3MIC_CIRC_SEND_UWB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCSPKR3MIC_CIRC_SEND_UWB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */
#endif

#ifdef INSTALL_OPERATOR_CVC_32K
    const CAPABILITY_DATA cvc_send_3mic_circ_speaker_swb_cap_data =
        {
            CVCSPKR3MIC_CIRC_SEND_SWB_CAP_ID,                                                       /* Capability ID */
            GEN_CVC_SEND_3M_SPKRCIRC_SWB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            4, CVC_NUM_OUTPUTS,           /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCSPKR3MIC_CIRC_SEND_SWB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCSPKR3MIC_CIRC_SEND_SWB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */
#endif

#ifdef INSTALL_OPERATOR_CVC_48K
    const CAPABILITY_DATA cvc_send_3mic_circ_speaker_fb_cap_data =
        {
            CVCSPKR3MIC_CIRC_SEND_FB_CAP_ID,                                                       /* Capability ID */
            GEN_CVC_SEND_3M_SPKRCIRC_FB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            4, CVC_NUM_OUTPUTS,           /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCSPKR3MIC_CIRC_SEND_FB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCSPKR3MIC_CIRC_SEND_FB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */
#endif
#endif /* INSTALL_OPERATOR_CVC_SPKR_CIRC_3MIC */

#ifdef INSTALL_OPERATOR_CVC_SPKR_CIRC_4MIC
#ifdef INSTALL_OPERATOR_CVC_24K
    const CAPABILITY_DATA cvc_send_4mic_circ_speaker_uwb_cap_data =
        {
            CVCSPKR4MIC_CIRC_SEND_UWB_CAP_ID,                                                       /* Capability ID */
            GEN_CVC_SEND_4M_SPKRCIRC_UWB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            5, CVC_NUM_OUTPUTS,           /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCSPKR4MIC_CIRC_SEND_UWB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCSPKR4MIC_CIRC_SEND_UWB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */
#endif

#ifdef INSTALL_OPERATOR_CVC_32K
    const CAPABILITY_DATA cvc_send_4mic_circ_speaker_swb_cap_data =
        {
            CVCSPKR4MIC_CIRC_SEND_SWB_CAP_ID,                                                       /* Capability ID */
            GEN_CVC_SEND_4M_SPKRCIRC_SWB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            5, CVC_NUM_OUTPUTS,           /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCSPKR4MIC_CIRC_SEND_SWB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCSPKR4MIC_CIRC_SEND_SWB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */
#endif

#ifdef INSTALL_OPERATOR_CVC_48K
    const CAPABILITY_DATA cvc_send_4mic_circ_speaker_fb_cap_data =
        {
            CVCSPKR4MIC_CIRC_SEND_FB_CAP_ID,                                                       /* Capability ID */
            GEN_CVC_SEND_4M_SPKRCIRC_FB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            5, CVC_NUM_OUTPUTS,           /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCSPKR4MIC_CIRC_SEND_FB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCSPKR4MIC_CIRC_SEND_FB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */
#endif
#endif /* INSTALL_OPERATOR_CVC_SPKR_CIRC_4MIC */

#ifdef INSTALL_OPERATOR_CVC_SPKR_CIRC_3MIC_VA
    const CAPABILITY_DATA cvc_send_3mic_circ_speaker_va_wb_cap_data =
        {
            CVCSPKR3MIC_CIRC_VA_SEND_WB_CAP_ID,                                                       /* Capability ID */
            GEN_CVC_SEND_3M_SPKRCIRC_VA_WB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            4, 5,                            /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCSPKR3MIC_CIRC_VA_SEND_WB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCSPKR3MIC_CIRC_VA_SEND_WB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */
#endif

#ifdef INSTALL_OPERATOR_CVC_SPKR_CIRC_3MIC_VA4B
     const CAPABILITY_DATA cvc_send_3mic_circ_speaker_va4b_wb_cap_data =
        {
            CVCSPKR3MIC_CIRC_VA4B_SEND_WB_CAP_ID,                                                       /* Capability ID */
            GEN_CVC_SEND_3M_SPKRCIRC_VA4B_WB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            4, 5,                            /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCSPKR3MIC_CIRC_VA4B_SEND_WB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCSPKR3MIC_CIRC_VA4B_SEND_WB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */
#endif

#ifdef INSTALL_OPERATOR_CVC_HEADSET_2MIC_MONO_VA_WAKEON
    const CAPABILITY_DATA cvc_send_2mic_hs_mono_wb_va_wakeon_cap_data =
        {
            CVCHS2MIC_WAKEON_WB_CAP_ID,       /* Capability ID */
            GEN_CVC_SEND_2M_HSE_WB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            3, CVC_NUM_OUTPUTS,           /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCHS2MIC_WAKEON_WB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCHS2MIC_WAKEON_WB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */
#endif

#ifdef INSTALL_OPERATOR_CVC_HEADSET_2MIC_MONO_VA_BARGEIN
    const CAPABILITY_DATA cvc_send_2mic_hs_mono_wb_va_bargein_cap_data =
        {
            CVCHS2MIC_BARGEIN_WB_CAP_ID,       /* Capability ID */
            GEN_CVC_SEND_2M_HSE_WB_VERSION_MAJOR, CVC_SEND_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
            3, CVC_NUM_OUTPUTS,           /* Max number of sinks/inputs and sources/outputs */
            &cvc_send_handler_table,      /* Pointer to message handler function table */
            cvc_send_opmsg_handler_table, /* Pointer to operator message handler function table */
            cvc_send_process_data,        /* Pointer to data processing function */
            0,                               /* Reserved */
            sizeof(CVC_SEND_OP_DATA)      /* Size of capability-specific per-instance data */
        };
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_CVCHS2MIC_BARGEIN_WB, CVC_SEND_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_CVCHS2MIC_BARGEIN_WB, CVC_SEND_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */
#endif
    
typedef void (*cvc_send_config_function_type)(CVC_SEND_OP_DATA *op_extra_data, unsigned data_variant);

typedef struct cvc_send_config_data
{
    cvc_send_config_function_type config_func;
    unsigned cap_ids[NUM_DATA_VARIANTS];
} cvc_send_config_data;

/*
 *   cVc send capability configuration table:
 *       cap1_config_func(), cap1_id_nb, cap1_id_wb, cap1_id_uwb, cap1_id_swb, cap1_id_fb,
 *       cap2_config_func(), cap2_id_nb, cap2_id_wb, cap2_id_uwb, cap2_id_swb, cap2_id_fb,
 *       ...
 *       0;
 */
cvc_send_config_data cvc_send_caps[] = {
#ifdef INSTALL_OPERATOR_CVC_HEADSET_1MIC
    {
        &CVC_SEND_CAP_Config_headset_1mic,
        {
            CVCHS1MIC_SEND_NB_CAP_ID,  CVCHS1MIC_SEND_WB_CAP_ID,
            CVCHS1MIC_SEND_UWB_CAP_ID, CVCHS1MIC_SEND_SWB_CAP_ID, CVCHS1MIC_SEND_FB_CAP_ID
        }
    },
#endif

#ifdef INSTALL_OPERATOR_CVC_HEADSET_2MIC_MONO
    {
        &CVC_SEND_CAP_Config_headset_2mic_mono,
        {
            CVCHS2MIC_MONO_SEND_NB_CAP_ID,   CVCHS2MIC_MONO_SEND_WB_CAP_ID,
            CVCHS2MIC_MONO_SEND_UWB_CAP_ID,  CVCHS2MIC_MONO_SEND_SWB_CAP_ID,  CVCHS2MIC_MONO_SEND_FB_CAP_ID
        }
    },
#endif

#ifdef INSTALL_OPERATOR_CVC_HEADSET_BINAURAL
    {
        &CVC_SEND_CAP_Config_headset_2mic_binaural,
        {
            CVCHS2MIC_BINAURAL_SEND_NB_CAP_ID,  CVCHS2MIC_BINAURAL_SEND_WB_CAP_ID,
            CVCHS2MIC_BINAURAL_SEND_UWB_CAP_ID, CVCHS2MIC_BINAURAL_SEND_SWB_CAP_ID, CVCHS2MIC_BINAURAL_SEND_FB_CAP_ID
        }
    },
#endif

#ifdef INSTALL_OPERATOR_CVC_SPEAKER_1MIC
    {
        &CVC_SEND_CAP_Config_speaker_1mic,
        {
            CVCSPKR1MIC_SEND_NB_CAP_ID,   CVCSPKR1MIC_SEND_WB_CAP_ID,
            CVCSPKR1MIC_SEND_UWB_CAP_ID,  CVCSPKR1MIC_SEND_SWB_CAP_ID,  CVCSPKR1MIC_SEND_FB_CAP_ID
        }
    },
#endif

#ifdef INSTALL_OPERATOR_CVC_SPEAKER_2MIC
    {
        &CVC_SEND_CAP_Config_speaker_2mic,
        {
            CVCSPKR2MIC_SEND_NB_CAP_ID,   CVCSPKR2MIC_SEND_WB_CAP_ID,
            CVCSPKR2MIC_SEND_UWB_CAP_ID,  CVCSPKR2MIC_SEND_SWB_CAP_ID,  CVCSPKR2MIC_SEND_FB_CAP_ID
        }
    },
#endif

#ifdef INSTALL_OPERATOR_CVC_SPEAKER_3MIC
    {
        &CVC_SEND_CAP_Config_speaker_3mic,
        {
            CVCSPKR3MIC_SEND_NB_CAP_ID,   CVCSPKR3MIC_SEND_WB_CAP_ID,
            CVCSPKR3MIC_SEND_UWB_CAP_ID,  CVCSPKR3MIC_SEND_SWB_CAP_ID,  CVCSPKR3MIC_SEND_FB_CAP_ID
        }
    },
#endif

#ifdef INSTALL_OPERATOR_CVC_SPEAKER_4MIC
    {
        &CVC_SEND_CAP_Config_speaker_4mic,
        {
            CVCSPKR4MIC_SEND_NB_CAP_ID,   CVCSPKR4MIC_SEND_WB_CAP_ID,
            CVCSPKR4MIC_SEND_UWB_CAP_ID,  CVCSPKR4MIC_SEND_SWB_CAP_ID,  CVCSPKR4MIC_SEND_FB_CAP_ID
        }
    },
#endif

#ifdef INSTALL_OPERATOR_CVC_AUTO_1MIC
    {
        &CVC_SEND_CAP_Config_auto_1mic,
        {
            CVCHF1MIC_SEND_NB_CAP_ID,  CVCHF1MIC_SEND_WB_CAP_ID,
            CVCHF1MIC_SEND_UWB_CAP_ID, CVCHF1MIC_SEND_SWB_CAP_ID, CVCHF1MIC_SEND_FB_CAP_ID
        }
    },
#endif

#ifdef INSTALL_OPERATOR_CVC_AUTO_2MIC
    {
        &CVC_SEND_CAP_Config_auto_2mic,
        {
            CVCHF2MIC_SEND_NB_CAP_ID,  CVCHF2MIC_SEND_WB_CAP_ID,
            CVCHF2MIC_SEND_UWB_CAP_ID, CVCHF2MIC_SEND_SWB_CAP_ID, CVCHF2MIC_SEND_FB_CAP_ID
        }
    },
#endif

#ifdef INSTALL_OPERATOR_CVC_SPKR_CIRC_3MIC
    {
        &CVC_SEND_CAP_Config_spkr_circ_3mic,
        {
            0,  CVCSPKR3MIC_CIRC_SEND_WB_CAP_ID,
            0, 0, 0
        }
    }, 
#endif

#ifdef INSTALL_OPERATOR_CVC_SPKR_CIRC_3MIC_VA
    {
        &CVC_SEND_CAP_Config_spkr_circ_3mic_va,
        {
            0,  CVCSPKR3MIC_CIRC_VA_SEND_WB_CAP_ID,
            0, 0, 0
        }
    }, 
#endif

#ifdef INSTALL_OPERATOR_CVC_SPKR_CIRC_3MIC_VA4B
    {
        &CVC_SEND_CAP_Config_spkr_circ_3mic_va4b,
        {
            0,  CVCSPKR3MIC_CIRC_VA4B_SEND_WB_CAP_ID,
            0, 0, 0
        }
    }, 
#endif

#ifdef INSTALL_OPERATOR_CVC_HEADSET_2MIC_MONO_VA_WAKEON
    {
        &CVC_SEND_CAP_Config_headset_2mic_wakeon,
        {
            0,   CVCHS2MIC_WAKEON_WB_CAP_ID,
            0,  0,  0
        }
     },
#endif
       
#ifdef INSTALL_OPERATOR_CVC_HEADSET_2MIC_MONO_VA_BARGEIN
    {
        &CVC_SEND_CAP_Config_headset_2mic_bargein,
        {
            0,   CVCHS2MIC_BARGEIN_WB_CAP_ID,
            0,  0,  0
        }
     },
#endif

    {
        /* end of table */
        NULL, {0, 0, 0, 0, 0}
    }
};


/*
 * cvc_send_config(CVC_SEND_OP_DATA *op_extra_data)
 *    Search for op_extra_data->cap_id from cvc_send_caps[] table, if found
 *
 *       set the following field based on specified cap_id:
 *          op_extra_data->data_variant
 *          op_extra_data->major_config
 *          op_extra_data->num_mics
 *          op_extra_data->mic_config
 *          op_extra_data->frame_size
 *          op_extra_data->sample_rate
 *
 *       and initialize other internal fields
 */
static bool cvc_send_config(CVC_SEND_OP_DATA *op_extra_data)
{
    cvc_send_config_data *caps;
    unsigned cap_id = op_extra_data->cap_id;
    unsigned variant = 0;

    for (caps = cvc_send_caps; caps->config_func != NULL; caps++)
    {
        for (variant = DATA_VARIANT_NB; variant <= DATA_VARIANT_FB; variant++)
        {
            if (caps->cap_ids[variant] == cap_id)
            {
                caps->config_func(op_extra_data, variant);

                switch(op_extra_data->data_variant)
                {
                case DATA_VARIANT_WB:  // 16 kHz
                    op_extra_data->frame_size = 120;
                    op_extra_data->sample_rate = 16000;
                    break;
                case DATA_VARIANT_UWB: // 24 kHz
                    op_extra_data->frame_size = 120;
                    op_extra_data->sample_rate = 24000;
                    break;
                case DATA_VARIANT_SWB: // 32 kHz
                    op_extra_data->frame_size = 240;
                    op_extra_data->sample_rate = 32000;
                    break;
                case DATA_VARIANT_FB:  // 48 kHz
                    op_extra_data->frame_size = 240;
                    op_extra_data->sample_rate = 48000;
                    break;
                case DATA_VARIANT_NB:  // 8 kHz
                default:
                    op_extra_data->frame_size = 60;
                    op_extra_data->sample_rate = 8000;
                    break;
                }
                op_extra_data->ReInitFlag = 1;
                op_extra_data->Host_mode = GEN_CVC_SEND_SYSMODE_FULL;
                op_extra_data->Cur_mode = GEN_CVC_SEND_SYSMODE_STANDBY;
                op_extra_data->mute_control_ptr = &op_extra_data->Cur_Mute;

                return TRUE;
            }
        }
    }

   return FALSE;
}

static inline CVC_SEND_OP_DATA *get_instance_data(OPERATOR_DATA *op_data)
{
    return (CVC_SEND_OP_DATA *) base_op_get_instance_data(op_data);
}

/****************************************************************************
Public Function Declarations
*/


bool cvc_send_create(OPERATOR_DATA *op_data, void *message_data, unsigned *response_id, void **response_data)
{
    CVC_SEND_OP_DATA *op_extra_data = get_instance_data(op_data);
    PS_KEY_TYPE key;

    patch_fn_shared(cvc_send_wrapper);
    /* Setup Response to Creation Request.   Assume Failure*/
    if (!base_op_build_std_response_ex(op_data, STATUS_CMD_FAILED, response_data))
    {
        /* We call cvc_send_release_constants as there is a slim chance we fail on
         * the second pass through */
        cvc_send_release_constants(op_data);
        return(FALSE);
    }

    /* Initialize extended data for operator.  Assume intialized to zero*/
    op_extra_data->cap_id = base_op_get_cap_id(op_data);

    /* Capability Specific Configuration */
    if (FALSE == cvc_send_config(op_extra_data)) {
        return(TRUE);
    }

#if defined(INSTALL_OPERATOR_CREATE_PENDING) && defined(INSTALL_CAPABILITY_CONSTANT_EXPORT)
    /* Reserve (and request) any dynamic memory tables that may be in external
     * file system.
     * A negative return value indicates a fatal error */
    INT_OP_ID int_id = base_op_get_int_op_id(op_data);
    if (   !external_constant_reserve(cvclib_dataDynTable_Main, int_id)
        || !external_constant_reserve(aec520_DynamicMemDynTable_Main, int_id)
        || !external_constant_reserve(ASF100_DynamicMemDynTable_Main, int_id)
        || !external_constant_reserve(oms280_DynamicMemDynTable_Main, int_id)
        || !external_constant_reserve(filter_bank_DynamicMemDynTable_Main, int_id)
        || !external_constant_reserve(vad410_DynamicMemDynTable_Main, int_id)
        || !external_constant_reserve(op_extra_data->dyn_main, int_id))
    {
        L2_DBG_MSG("cvc_send_create failed reserving constants");
        cvc_send_release_constants(op_data);
        return TRUE;
    }

    /* Now see if these tables are available yet */
    if (   !is_external_constant_available(cvclib_dataDynTable_Main, int_id)
        || !is_external_constant_available(aec520_DynamicMemDynTable_Main, int_id)
        || !is_external_constant_available(ASF100_DynamicMemDynTable_Main, int_id)
        || !is_external_constant_available(oms280_DynamicMemDynTable_Main, int_id)
        || !is_external_constant_available(filter_bank_DynamicMemDynTable_Main, int_id)
        || !is_external_constant_available(vad410_DynamicMemDynTable_Main, int_id)
        || !is_external_constant_available(op_extra_data->dyn_main, int_id))
    {
        /* Free the response created above, before it gets overwritten with the pending data */
        pdelete(*response_data);

        /* Database isn't available yet. Arrange for a callback
         * Only need to check on one table */
        *response_id = (unsigned)op_extra_data->dyn_main;
        *response_data = (void*)(pending_operator_cb)cvc_send_create_pending_cb;

        L4_DBG_MSG("cvc_send_create - requesting callback when constants available");
        return (bool)HANDLER_INCOMPLETE;
    }
#endif

    patch_fn_shared(cvc_send_wrapper);


    /*allocate the volume control shared memory*/
    op_extra_data->shared_volume_ptr = allocate_shared_volume_cntrl();
    if(!op_extra_data->shared_volume_ptr)
    {
        cvc_send_release_constants(op_data);
        return(TRUE);
    }

    /* call the "create" assembly function */
    if(CVC_SEND_CAP_Create(op_extra_data))
    {
        /* Free all the scratch memory we reserved */
        CVC_SEND_CAP_Destroy(op_extra_data);
        release_shared_volume_cntrl(op_extra_data->shared_volume_ptr);
        op_extra_data->shared_volume_ptr = NULL;
        cvc_send_release_constants(op_data);
        return(TRUE);
    }

    if(!cvc_send_register_component((void*)op_extra_data))
    {
        /* Free all the scratch memory we reserved, exit with fail response msg. Even if it had failed
         * silently, subsequent security checks will fail in lack of a successful registration.
         */
        CVC_SEND_CAP_Destroy(op_extra_data);
        release_shared_volume_cntrl(op_extra_data->shared_volume_ptr);
        op_extra_data->shared_volume_ptr = NULL;
        cvc_send_release_constants(op_data);
        return(TRUE);
    }


    if(!cpsInitParameters(&op_extra_data->parms_def,(unsigned*)CVC_SEND_GetDefaults(op_extra_data->cap_id),(unsigned*)op_extra_data->params,sizeof(CVC_SEND_PARAMETERS)))
    {
        /* Free all the scratch memory we reserved, exit with fail response msg. Even if it had failed
         * silently, subsequent security checks will fail in lack of a successful registration.
         */
        CVC_SEND_CAP_Destroy(op_extra_data);
        release_shared_volume_cntrl(op_extra_data->shared_volume_ptr);
        op_extra_data->shared_volume_ptr = NULL;
        cvc_send_release_constants(op_data);
        return(TRUE);
    }

     /* Read state info from UCID 0 */
    key = MAP_CAPID_UCID_SBID_TO_PSKEYID(op_extra_data->cap_id,0,OPMSG_P_STORE_STATE_VARIABLE_SUB_ID);
    ps_entry_read((void*)op_data,key,PERSIST_ANY,ups_state_snd);

#if defined(BUILD_CVC_SEND_USE_IIR_RESAMPLER)
    /* We don't have a new constant table to add - only register our interest in the IIR RESAMPLER constant tables. */
    iir_resamplerv2_add_config_to_list(NULL);
#endif

    base_op_change_response_status(response_data,STATUS_OK);
    return TRUE;
}

/* ************************************* Data processing-related functions and wrappers **********************************/

void cvc_send_process_data(OPERATOR_DATA *op_data, TOUCHED_TERMINALS *touched)
{
    CVC_SEND_OP_DATA *op_extra_data = get_instance_data(op_data);
    int samples_to_process, stream_amount_data;
    int mic_index;
    int frame_size = op_extra_data->frame_size;

    patch_fn(cvc_send_process_data_patch);

    unsigned op_feature_requested = op_extra_data->op_feature_requested;

    /* Bypass processing until all streams are connected */
    if(op_feature_requested == 0)
    {
       return;
    }

    /* number of samples to process at the reference */
    if(op_feature_requested & CVC_REQUESTED_FEATURE_AEC)
    {
       samples_to_process = cbuffer_calc_amount_data_in_words(op_extra_data->input_stream[0]->cbuffer);   /* Reference */
    }
    else
    {
       samples_to_process = frame_size;
    }
    /* number of samples to process at the mics */
    for(mic_index=1; mic_index <= op_extra_data->num_mics; mic_index++)
    {
        stream_amount_data = cbuffer_calc_amount_data_in_words(op_extra_data->input_stream[mic_index]->cbuffer);
        if (stream_amount_data < samples_to_process)
        {
            samples_to_process = stream_amount_data;
        }
    }
#ifdef CVC_SEND_SUPPORT_METADATA
    patch_fn_shared(cvc_send_wrapper);

    if (op_extra_data->output_stream[CVC_SEND_OUT_TERMINAL_VA]->cbuffer != NULL) {
       if(op_extra_data->mic_metadata_buffer!= NULL)
       {
           /* if mic inputs have metadata, then limit the amount of
            * consuming to the amount of metadata available.
            */
           unsigned meta_data_available =
               buff_metadata_available_octets(op_extra_data->mic_metadata_buffer)/OCTETS_PER_SAMPLE;
   
           samples_to_process = MIN(samples_to_process, meta_data_available);
       }
    
       if(buff_has_metadata(op_extra_data->output_stream[CVC_SEND_OUT_TERMINAL_VA]->cbuffer))
       {
           /* also check the space available */
           unsigned space_available = cbuffer_calc_amount_space_in_words(op_extra_data->output_stream[CVC_SEND_OUT_TERMINAL_VA]->cbuffer);
           samples_to_process = MIN(samples_to_process, space_available);        
       }
    }
#endif /* CVC_SEND_SUPPORT_METADATA */

    /* Check for sufficient data and space */
    if(samples_to_process < frame_size)
    {
        return;
    }

    if (op_extra_data->Ovr_Control & GEN_CVC_SEND_CONTROL_MODE_OVERRIDE)
    {
        op_extra_data->Cur_mode = op_extra_data->Obpm_mode;
    }
    else if (op_extra_data->major_config!=CVC_SEND_CONFIG_HEADSET)
    {
        op_extra_data->Cur_mode = op_extra_data->Host_mode;
    }
    else if (op_extra_data->Host_mode != GEN_CVC_SEND_SYSMODE_FULL)
    {
        op_extra_data->Cur_mode = op_extra_data->Host_mode;
    }
    else
    {
        unsigned temp = op_extra_data->Cur_mode;
        if ((temp == GEN_CVC_SEND_SYSMODE_FULL) || (temp == GEN_CVC_SEND_SYSMODE_LOWVOLUME) )
        {
           /* TODO - need to redefine OFFSET_LVMODE_THRES to dB/60 */
            unsigned vol_level = 15 - (((int)op_extra_data->shared_volume_ptr->current_volume_level)/(-360));

            if (vol_level < op_extra_data->params->OFFSET_LVMODE_THRES)
            {
               op_extra_data->Cur_mode = GEN_CVC_SEND_SYSMODE_LOWVOLUME;
            }
            else
            {
               op_extra_data->Cur_mode = GEN_CVC_SEND_SYSMODE_FULL;
            }

            if (temp != op_extra_data->Cur_mode)
            {
               op_extra_data->ReInitFlag = 1;
            }
        }
        else
        {
            op_extra_data->Cur_mode = op_extra_data->Host_mode;
        }
    }

#ifdef CVC_SEND_SUPPORT_METADATA
     patch_fn_shared(cvc_send_wrapper);

    if (op_extra_data->output_stream[CVC_SEND_OUT_TERMINAL_VA]->cbuffer != NULL) {
       if(NULL != op_extra_data->mic_metadata_buffer)
       {
           /* transport metadata from input to output */
           metadata_strict_transport(op_extra_data->mic_metadata_buffer, op_extra_data->output_stream[CVC_SEND_OUT_TERMINAL_VA]->cbuffer,
                                 frame_size*OCTETS_PER_SAMPLE);
       }
       else if (buff_has_metadata(op_extra_data->output_stream[CVC_SEND_OUT_TERMINAL_VA]->cbuffer))
       {
           /* input doesn't have metadata but output does,
            * Normally this shouldn't be the case but in this
            * case we only append a balank tag to output buffer.
            * TODO: This could be done in metadata_strict_transport function.
            */
           metadata_tag *out_mtag = buff_metadata_new_tag();
           unsigned afteridx = frame_size * OCTETS_PER_SAMPLE;
           if(out_mtag != NULL)
           {
               out_mtag->length = afteridx;
           }
           buff_metadata_append(op_extra_data->output_stream[CVC_SEND_OUT_TERMINAL_VA]->cbuffer, out_mtag, 0, afteridx);
       }
    }
#endif /* CVC_SEND_SUPPORT_METADATA */

    /* call the "process" assembly function */
    CVC_SEND_CAP_Process(op_extra_data);

    /* Set touched->sources for kicking */
    
    
    if(op_feature_requested & CVC_REQUESTED_FEATURE_VOICE) 
    {
        touched->sources = TOUCHED_SOURCE_0;
    }

    if(op_feature_requested & CVC_REQUESTED_FEATURE_VA) 
    {
        /* in order to kick all the connected VA channels, we need to assign a binary number 
        such as 11110 (for 4 VA channels) or 10 (for 1 VA channel) to the touched->sources.
        
        The 0 on the last bit (least significant bit) of the binary number represents the voice channel 
        (1st output channel) whose kicking logic should be determined separately. */
        
        touched->sources |= (TOUCHED_SOURCE_1 << (op_extra_data->num_va_outputs)) - TOUCHED_SOURCE_1;
    }
}
