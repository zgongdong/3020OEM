/*!
\copyright  Copyright (c) 2017 - 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       kymera_anc.c
\brief      Kymera ANC code
*/

#include <audio_clock.h>
#include <audio_power.h>
#include <vmal.h>
#include <file.h>
#include <cap_id_prim.h>
#include <opmsg_prim.h>

#include "kymera_private.h"
#include "kymera_config.h"
#include "microphones.h"
#include "usb_common.h"

#define ANC_TUNING_SINK_USB_LEFT      0 /*can be any other backend device. PCM used in this tuning graph*/
#define ANC_TUNING_SINK_USB_RIGHT     1
#define ANC_TUNING_SINK_FBMON_LEFT    2 /*reserve slots for FBMON tap. Always connected.*/
#define ANC_TUNING_SINK_FBMON_RIGHT   3
#define ANC_TUNING_SINK_MIC1_LEFT     4 /* must be connected to internal ADC. Analog or digital */
#define ANC_TUNING_SINK_MIC1_RIGHT    5
#define ANC_TUNING_SINK_MIC2_LEFT     6
#define ANC_TUNING_SINK_MIC2_RIGHT    7

#define ANC_TUNING_SOURCE_USB_LEFT    0 /*can be any other backend device. USB used in the tuning graph*/
#define ANC_TUNING_SOURCE_USB_RIGHT   1
#define ANC_TUNING_SOURCE_DAC_LEFT    2 /* must be connected to internal DAC */
#define ANC_TUNING_SOURCE_DAC_RIGHT   3


/*! Macro for creating messages */
#define MAKE_KYMERA_MESSAGE(TYPE) \
    TYPE##_T *message = PanicUnlessNew(TYPE##_T);


#define ANC_TUNING_SINK_USB_LEFT      0 /*can be any other backend device. PCM used in this tuning graph*/
#define ANC_TUNING_SINK_USB_RIGHT     1
#define ANC_TUNING_SINK_FBMON_LEFT    2 /*reserve slots for FBMON tap. Always connected.*/
#define ANC_TUNING_SINK_FBMON_RIGHT   3
#define ANC_TUNING_SINK_MIC1_LEFT     4 /* must be connected to internal ADC. Analog or digital */
#define ANC_TUNING_SINK_MIC1_RIGHT    5
#define ANC_TUNING_SINK_MIC2_LEFT     6
#define ANC_TUNING_SINK_MIC2_RIGHT    7

#define ANC_TUNING_SOURCE_USB_LEFT    0 /*can be any other backend device. USB used in the tuning graph*/
#define ANC_TUNING_SOURCE_USB_RIGHT   1
#define ANC_TUNING_SOURCE_DAC_LEFT    2 /* must be connected to internal DAC */
#define ANC_TUNING_SOURCE_DAC_RIGHT   3


/*! Macro for creating messages */
#define MAKE_KYMERA_MESSAGE(TYPE) \
    TYPE##_T *message = PanicUnlessNew(TYPE##_T);

#ifdef DOWNLOAD_USB_AUDIO
#define EB_CAP_ID_USB_AUDIO_RX CAP_ID_DOWNLOAD_USB_AUDIO_RX
#define EB_CAP_ID_USB_AUDIO_TX CAP_ID_DOWNLOAD_USB_AUDIO_TX
#else
#define EB_CAP_ID_USB_AUDIO_RX CAP_ID_USB_AUDIO_RX
#define EB_CAP_ID_USB_AUDIO_TX CAP_ID_USB_AUDIO_TX
#endif
void KymeraAnc_EnterTuning(void)
{
    Usb_ClientRegister(KymeraGetTask());
    Usb_AttachtoHub();
}

void KymeraAnc_ExitTuning(void)
{
    Usb_DetachFromHub();
}

void KymeraAnc_TuningStart(uint16 usb_rate)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    DEBUG_LOG("KymeraAnc_TuningStart");
    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_ANC_TUNING_START);
    message->usb_rate = usb_rate;
    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_ANC_TUNING_START, message, &theKymera->busy_lock);
}

void KymeraAnc_TuningStop(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    DEBUG_LOG("KymeraAnc_TuningStop");
    MessageSend(&theKymera->task, KYMERA_INTERNAL_ANC_TUNING_STOP, NULL);
}

static void kymeraAnc_GetMics(microphone_number_t *mic0, microphone_number_t *mic1)
{
    *mic0 = microphone_none;
    *mic1 = microphone_none;

    if (appConfigAncPathEnable() & feed_forward_left)
    {
        *mic0 = appConfigAncFeedForwardMic();
        if (appConfigAncPathEnable() & feed_back_left)
            *mic1 = appConfigAncFeedBackMic();
    }
    else
    {
        if (appConfigAncPathEnable() & feed_back_left)
            *mic0 = appConfigAncFeedBackMic();
    }
}

static inline void kymeraAnc_SynchroniseAncTuningMicrophoneSources(Source mic_source_0, Source mic_source_1, Source monitor_mic_source)
{
    PanicFalse(SourceSynchronise(monitor_mic_source, mic_source_0));
    if(mic_source_1)
    {
        PanicFalse(SourceSynchronise(mic_source_0, mic_source_1));
        PanicFalse(SourceSynchronise(monitor_mic_source, mic_source_1));
    }
}

void KymeraAnc_TuningCreateChain(uint16 usb_rate)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    theKymera->usb_rate = usb_rate;

    const char anc_tuning_edkcs[] = "download_anc_tuning.edkcs";
    DEBUG_LOG("KymeraAnc_TuningCreateChain, rate %u", usb_rate);

    /* Only 48KHz supported */
    PanicFalse(usb_rate == 48000);

    /* Turn on audio subsystem */
    OperatorFrameworkEnable(1);

    /* Move to ANC tuning state, this prevents A2DP and HFP from using kymera */
    appKymeraSetState(KYMERA_STATE_ANC_TUNING);

    /* Create ANC tuning operator */
    FILE_INDEX index = FileFind(FILE_ROOT, anc_tuning_edkcs, strlen(anc_tuning_edkcs));
    PanicFalse(index != FILE_NONE);
    theKymera->anc_tuning_bundle_id = PanicZero (OperatorBundleLoad(index, 0)); /* 0 is processor ID */
    theKymera->anc_tuning = (Operator)PanicFalse(VmalOperatorCreate(CAP_ID_DOWNLOAD_ANC_TUNING));

    /* Create the operators for USB Rx & Tx audio */
    uint16 usb_config[] =
    {
        OPMSG_USB_AUDIO_ID_SET_CONNECTION_CONFIG,
        0,              // data_format
        usb_rate / 25,  // sample_rate
        2,              // number_of_channels
        16,             // subframe_size
        16,             // subframe_resolution
    };

#ifdef DOWNLOAD_USB_AUDIO
    const char usb_audio_edkcs[] = "download_usb_audio.edkcs";
    index = FileFind(FILE_ROOT, usb_audio_edkcs, strlen(usb_audio_edkcs));
    PanicFalse(index != FILE_NONE);
    theKymera->usb_audio_bundle_id = PanicZero (OperatorBundleLoad(index, 0)); /* 0 is processor ID */
#endif
    theKymera->usb_rx = (Operator)PanicFalse(VmalOperatorCreate(EB_CAP_ID_USB_AUDIO_RX));

    PanicFalse(VmalOperatorMessage(theKymera->usb_rx,
                                   usb_config, SIZEOF_OPERATOR_MESSAGE(usb_config),
                                   NULL, 0));
    theKymera->usb_tx = (Operator)PanicFalse(VmalOperatorCreate(EB_CAP_ID_USB_AUDIO_TX));
    PanicFalse(VmalOperatorMessage(theKymera->usb_tx,
                                   usb_config, SIZEOF_OPERATOR_MESSAGE(usb_config),
                                   NULL, 0));

    /* Get the DAC output sinks */
    Sink DAC_L = (Sink)PanicFalse(StreamAudioSink(AUDIO_HARDWARE_CODEC, AUDIO_INSTANCE_0, AUDIO_CHANNEL_A));
    PanicFalse(SinkConfigure(DAC_L, STREAM_CODEC_OUTPUT_RATE, usb_rate));

    microphone_number_t mic0, mic1;
    kymeraAnc_GetMics(&mic0, &mic1);
    Source mic_in0 = Kymera_GetMicrophoneSource(mic0, NULL, theKymera->sco_info->rate, high_priority_user);
    Source mic_in1 = Kymera_GetMicrophoneSource(mic1, NULL, theKymera->sco_info->rate, high_priority_user);
    Source fb_mon0 = Kymera_GetMicrophoneSource(appConfigAncTuningMonitorMic(), NULL, theKymera->sco_info->rate, high_priority_user);
    PanicFalse(SourceSynchronise(fb_mon0, mic_in0));

    uint16 anc_tuning_frontend_config[3] =
    {
        OPMSG_ANC_TUNING_ID_FRONTEND_CONFIG,        // ID
        0,                                          // 0 = mono, 1 = stereo
        mic_in1 ? 1 : 0                             // 0 = 1-mic, 1 = 2-mic
    };
    PanicFalse(VmalOperatorMessage(theKymera->anc_tuning,
                                   &anc_tuning_frontend_config, SIZEOF_OPERATOR_MESSAGE(anc_tuning_frontend_config),
                                   NULL, 0));

    /* Connect microphone */
    PanicFalse(StreamConnect(mic_in0,
                             StreamSinkFromOperatorTerminal(theKymera->anc_tuning, ANC_TUNING_SINK_MIC1_LEFT)));
    if (mic_in1)
        PanicFalse(StreamConnect(mic_in1,
                                 StreamSinkFromOperatorTerminal(theKymera->anc_tuning, ANC_TUNING_SINK_MIC2_LEFT)));

    /* Connect FBMON microphone */
    PanicFalse(StreamConnect(fb_mon0,
                             StreamSinkFromOperatorTerminal(theKymera->anc_tuning, ANC_TUNING_SINK_FBMON_LEFT)));

    /* Connect speaker */
    PanicFalse(StreamConnect(StreamSourceFromOperatorTerminal(theKymera->anc_tuning, ANC_TUNING_SOURCE_DAC_LEFT),
                             DAC_L));

    /* Connect backend (USB) out */
    PanicFalse(StreamConnect(StreamSourceFromOperatorTerminal(theKymera->anc_tuning, ANC_TUNING_SOURCE_USB_LEFT),
                             StreamSinkFromOperatorTerminal(theKymera->usb_tx, 0)));
    PanicFalse(StreamConnect(StreamSourceFromOperatorTerminal(theKymera->anc_tuning, ANC_TUNING_SOURCE_USB_RIGHT),
                             StreamSinkFromOperatorTerminal(theKymera->usb_tx, 1)));

    /* Connect backend (USB) in */
    PanicFalse(StreamConnect(StreamSourceFromOperatorTerminal(theKymera->usb_rx, 0),
                             StreamSinkFromOperatorTerminal(theKymera->anc_tuning, ANC_TUNING_SINK_USB_LEFT)));
    PanicFalse(StreamConnect(StreamSourceFromOperatorTerminal(theKymera->usb_rx, 1),
                             StreamSinkFromOperatorTerminal(theKymera->anc_tuning, ANC_TUNING_SINK_USB_RIGHT)));

    /* Connect USB ISO in endpoint to USB Rx operator */
    PanicFalse(StreamConnect(StreamUsbEndPointSource(end_point_iso_in),
                             StreamSinkFromOperatorTerminal(theKymera->usb_rx, 0)));

    /* Connect USB Tx operator to USB ISO out endpoint */
    PanicFalse(StreamConnect(StreamSourceFromOperatorTerminal(theKymera->usb_tx, 0),
                             StreamUsbEndPointSink(end_point_iso_out)));

    /* Finally start the operators */
    Operator op_list[] = {theKymera->usb_rx, theKymera->anc_tuning, theKymera->usb_tx};
    PanicFalse(OperatorStartMultiple(3, op_list, NULL));

    /* Ensure audio amp is on */
    appKymeraExternalAmpControl(TRUE);

    /* Set kymera lock to prevent anything else using kymera */
    theKymera->lock = TRUE;
}

void KymeraAnc_TuningDestroyChain(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    /* Turn audio amp is off */
    appKymeraExternalAmpControl(FALSE);

    /* Stop the operators */
    Operator op_list[] = {theKymera->usb_rx, theKymera->anc_tuning, theKymera->usb_tx};
    PanicFalse(OperatorStopMultiple(3, op_list, NULL));

    /* Disconnect USB ISO in endpoint */
    StreamDisconnect(StreamUsbEndPointSource(end_point_iso_in), 0);

    /* Disconnect USB ISO out endpoint */
    StreamDisconnect(0, StreamUsbEndPointSink(end_point_iso_out));

    /* Get the DAC output sinks */
    Sink DAC_L = (Sink)PanicFalse(StreamAudioSink(AUDIO_HARDWARE_CODEC, AUDIO_INSTANCE_0, AUDIO_CHANNEL_A));

    microphone_number_t mic0, mic1;
    kymeraAnc_GetMics(&mic0, &mic1);
    Source mic_in0 = Kymera_GetMicrophoneSource(mic0, NULL, theKymera->usb_rate, high_priority_user);
    Source mic_in1 = Kymera_GetMicrophoneSource(mic1, NULL, theKymera->usb_rate, high_priority_user);
    Source fb_mon0 = Kymera_GetMicrophoneSource(appConfigAncTuningMonitorMic(), NULL, theKymera->usb_rate, high_priority_user);

    StreamDisconnect(mic_in0, 0);
    if (mic_in1)
        StreamDisconnect(mic_in1, 0);

    StreamDisconnect(fb_mon0, 0);

    /* Disconnect speaker */
    StreamDisconnect(0, DAC_L);

    /* Distroy operators */
    OperatorsDestroy(op_list, 3);

    /* Unload bundle */
    PanicFalse(OperatorBundleUnload(theKymera->anc_tuning_bundle_id));
#ifdef DOWNLOAD_USB_AUDIO
    PanicFalse(OperatorBundleUnload(theKymera->usb_audio_bundle_id));
#endif

    /* Clear kymera lock and go back to idle state to allow other uses of kymera */
    theKymera->lock = FALSE;
    appKymeraSetState(KYMERA_STATE_IDLE);

    /* Turn off audio subsystem */
    OperatorFrameworkEnable(0);
    Usb_ClientUnRegister(KymeraGetTask());
}
