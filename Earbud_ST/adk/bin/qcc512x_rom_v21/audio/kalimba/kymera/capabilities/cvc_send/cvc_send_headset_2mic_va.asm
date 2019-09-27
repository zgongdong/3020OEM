// *****************************************************************************
// Copyright (c) 2014 - 2017 Qualcomm Technologies International, Ltd.
// %%version
//
//   
// *****************************************************************************

#if defined(CAPABILITY_DOWNLOAD_BUILD)

#include "stack.h"
#include "portability_macros.h"
#include "shared_volume_control_asm_defs.h"
#include "cvc_send_gen_asm.h"
#include "cvc_send_cap_asm.h"

#include "patch_library.h"
#include "cvc_modules.h"

#define $cvc_send.mic_config.HEADSET_MONO          MIC_CONFIG_ENDFIRE

#define $M.CVC_SEND_CAP.headset_2mic_data_va_wakeon.dyn_table_main     $M.CVC_SEND_CAP.headset_2mic_data_va_wakeon.Downloadable.DynTable_Main
#define $M.CVC_SEND_CAP.headset_2mic_data_va_bargein.dyn_table_main    $M.CVC_SEND_CAP.headset_2mic_data_va_bargein.Downloadable.DynTable_Main
#define $M.CVC_SEND_CAP.headset_2mic_data_va_wakeon.dyn_table_linker $M.CVC_SEND_CAP.headset_2mic_data_va_wakeon.Downloadable.DynTable_Linker
#define $M.CVC_SEND_CAP.headset_2mic_data_va_bargein.dyn_table_linker $M.CVC_SEND_CAP.headset_2mic_data_va_bargein.Downloadable.DynTable_Linker

#else

#define $M.CVC_SEND_CAP.headset_2mic_data_va_wakeon.dyn_table_main     $M.CVC_SEND_CAP.headset_2mic_data_va_wakeon.DynTable_Main
#define $M.CVC_SEND_CAP.headset_2mic_data_va_bargein.dyn_table_main    $M.CVC_SEND_CAP.headset_2mic_data_va_bargein.DynTable_Main
#define $M.CVC_SEND_CAP.headset_2mic_data_va_wakeon.dyn_table_linker $M.CVC_SEND_CAP.headset_2mic_data_va_wakeon.DynTable_Linker
#define $M.CVC_SEND_CAP.headset_2mic_data_va_bargein.dyn_table_linker $M.CVC_SEND_CAP.headset_2mic_data_va_bargein.DynTable_Linker

#endif



// *****************************************************************************
// MODULE:
//    void CVC_SEND_CAP_Config_headset_2mic_wakeon(CVC_SEND_OP_DATA *op_extra_data, unsigned data_variant);
//    void CVC_SEND_CAP_Config_headset_2mic_bargein(CVC_SEND_OP_DATA *op_extra_data, unsigned data_variant);
//
// DESCRIPTION:
//    CVC SEND Configuration (per capability) (C-callable)
//
// MODIFICATIONS:
//
// INPUTS:
//    r0 - Extended Data
//    r1 - data_variant
//
// OUTPUTS:
//    - cVc send configuration data
//
// TRASHED REGISTERS:
//
// CPU USAGE:
// *****************************************************************************
.MODULE $M.CVC_SEND_CAP_Config_headset_2mic_wakeon;
   .CODESEGMENT PM;
$_CVC_SEND_CAP_Config_headset_2mic_wakeon:
   r2 = $cvc_send.mic_config.HEADSET_MONO;
   r3 = $cvc_send.HEADSET;
   I3 = $M.CVC_SEND_CAP.headset_2mic_data_va_wakeon.dyn_table_main;
   I7 = $M.CVC_SEND_CAP.headset_2mic_data_va_wakeon.dyn_table_linker;
   r10 = 2;
   jump $cvc_send.dyn_config;
.ENDMODULE;

.MODULE $M.CVC_SEND_CAP_Config_headset_2mic_bargein;
   .CODESEGMENT PM;
$_CVC_SEND_CAP_Config_headset_2mic_bargein:
   r2 = $cvc_send.mic_config.HEADSET_MONO;
   r3 = $cvc_send.HEADSET;
   I3 = $M.CVC_SEND_CAP.headset_2mic_data_va_bargein.dyn_table_main;
   I7 = $M.CVC_SEND_CAP.headset_2mic_data_va_bargein.dyn_table_linker;
   r10 = 2;
   jump $cvc_send.dyn_config;
.ENDMODULE;


// *****************************************************************************
// MODULE:
//    $cvc.init.hs2mic_va
//    $cvc.event.echo_flag.hs2mic_va
//
// DESCRIPTION:
//    Echo_Flag = VAD_REF
//
// MODIFICATIONS:
//
// INPUTS:
//    - r7 - AEC_NLP data object
//    - r8 - rcv_vad flag pointer
//    - r9 - cvc data root object
//
// OUTPUTS:
//
// TRASHED REGISTERS:
//
// CPU USAGE:
//
// NOTE:
// *****************************************************************************
.MODULE $M.CVC_SEND.hs2mic_va;

   .CODESEGMENT PM;

$cvc.init.hs2mic_va:
   r0 = M[r9 + $cvc_send.data.hfk_config];
   r0 = r0 OR ($M.GEN.CVC_SEND.CONFIG.HFK.BYP_WNR | $M.GEN.CVC_SEND.CONFIG.HFK.BYP_RER);
   M[r9 + $cvc_send.data.hfk_config] = r0;

   r0 = M[r9 + $cvc_send.data.dmss_config];
   r0 = r0 OR $M.GEN.CVC_SEND.CONFIG.DMSS.BYP_SPP;
   M[r9 + $cvc_send.data.dmss_config] = r0;
   rts;

$cvc.event.echo_flag.hs2mic_va:
   // VAD_AEC
   r0 = M[r8];
   // HD_mode
   Null = r7;
   if Z jump save_echo_flag;
      r1 = M[r7 + $aec520.nlp.FLAG_HD_MODE_FIELD];
      r0 = r0 OR r1;
   save_echo_flag:
   // Echo_Flag
   M[r9 + $cvc_send.data.echo_flag] = r0;
   rts;

.ENDMODULE;
