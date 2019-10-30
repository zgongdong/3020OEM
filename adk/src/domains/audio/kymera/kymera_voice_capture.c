/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       kymera_voice_capture.c
\brief      Kymera module to handle voice capture APIs
*/

#ifdef INCLUDE_KYMERA_VOICE_CAPTURE

#include "kymera.h"
#include "kymera_private.h"
#include "kymera_voice_capture.h"
#include "chains/chain_voice_capture.h"
#include "kymera_aec.h"

#define AUDIO_FRAME_VA_DATA_LENGTH (9)
#define MIC_SAMPLE_RATE (16000)
#define UCID_CVC_SEND_VA (1)
#define MIC_1 (microphone_1)
#define MIC_2 (microphone_2)

typedef enum
{
    KYMERA_INTERNAL_VOICE_CAPTURE_START,
    KYMERA_INTERNAL_VOICE_CAPTURE_STOP
} internal_message_ids;

static void appKymera_VoiceCaptureMessageHandler(Task task, MessageId id, Message msg);
static const TaskData msgHandler = { appKymera_VoiceCaptureMessageHandler };

static kymera_chain_handle_t voice_capture_chain;

static void appKymera_ConfigureSbcEncoder(sbc_encoder_params_t *sbc_params)
{
    Operator sbc = ChainGetOperatorByRole(voice_capture_chain, OPR_SBC_ENCODER);
    PanicFalse(sbc);
    OperatorsSbcEncoderSetEncodingParams(sbc, sbc_params);
}

static void appKymera_ConfigureCvc(void)
{
    Operator cvc = ChainGetOperatorByRole(voice_capture_chain, OPR_CVC_SEND);
    PanicFalse(cvc);
    OperatorsStandardSetUCID(cvc, UCID_CVC_SEND_VA);
}

static void appKymera_ConfigureVoiceCaptureChain(voice_capture_params_t *params)
{
    appKymera_ConfigureSbcEncoder(&params->encoder_params.sbc);
    appKymera_ConfigureCvc();
}

static void appKymera_CreateVoiceCaptureChain(voice_capture_params_t *params)
{
    PanicFalse(voice_capture_chain == NULL);
    voice_capture_chain = PanicNull(ChainCreate(&chain_voice_capture_config));
    appKymera_ConfigureVoiceCaptureChain(params);
    ChainConnect(voice_capture_chain);
}

static void appKymera_DestroyVoiceCaptureChain(void)
{
    PanicFalse(voice_capture_chain != NULL);
    ChainDestroy(voice_capture_chain);
    voice_capture_chain = NULL;
}

static void appKymera_ConnectVoiceCaptureChainToMics(void)
{
    Source mic1 = Kymera_GetMicrophoneSource(MIC_1, NULL, MIC_SAMPLE_RATE, high_priority_user);
    Source mic2 = Kymera_GetMicrophoneSource(MIC_2, mic1, MIC_SAMPLE_RATE, high_priority_user);
    Sink mic1_input = ChainGetInput(voice_capture_chain, EPR_VOICE_CAPTURE_MIC1_IN);
    Sink mic2_input = ChainGetInput(voice_capture_chain, EPR_VOICE_CAPTURE_MIC2_IN);

#ifdef INCLUDE_KYMERA_AEC
    aec_connect_audio_input_t connect_params = {0};
    connect_params.mic_sample_rate = MIC_SAMPLE_RATE;
    connect_params.reference_output = ChainGetInput(voice_capture_chain, EPR_VOICE_CAPTURE_AEC_IN);
    connect_params.mic_input_1 = mic1;
    connect_params.mic_input_2 = mic2;
    connect_params.mic_output_1 = mic1_input;
    connect_params.mic_output_2 = mic2_input;
    Kymera_ConnectAudioInputToAec(&connect_params);
#else
    PanicNull(StreamConnect(mic1, mic1_input));
    PanicNull(StreamConnect(mic2, mic2_input));
#endif
}

static void appKymera_DisconnectVoiceCaptureChainFromMics(void)
{
#ifdef INCLUDE_KYMERA_AEC
    Kymera_DisconnectAudioInputFromAec();
#else
    StreamDisconnect(NULL, ChainGetInput(voice_capture_chain, EPR_VOICE_CAPTURE_MIC1_IN));
    StreamDisconnect(NULL, ChainGetInput(voice_capture_chain, EPR_VOICE_CAPTURE_MIC2_IN));
#endif

    Kymera_CloseMicrophone(MIC_1, high_priority_user);
    Kymera_CloseMicrophone(MIC_2, high_priority_user);
}

static Source appKymera_GetVoiceCaptureOutput(void)
{
    Source capture_output = ChainGetOutput(voice_capture_chain, EPR_VOICE_CAPTURE_OUT);

    PanicFalse(SourceMapInit(capture_output, STREAM_TIMESTAMPED, AUDIO_FRAME_VA_DATA_LENGTH));

    return capture_output;
}

static void appKymera_DisconnectVoiceCaptureOutput(void)
{
    Source capture_output = ChainGetOutput(voice_capture_chain, EPR_VOICE_CAPTURE_OUT);

    SourceUnmap(capture_output);
    StreamDisconnect(capture_output, NULL);
}

static void appKymera_StartVoiceCaptureChain(void)
{
    ChainStart(voice_capture_chain);
}

static void appKymera_StopVoiceCaptureChain(void)
{
    ChainStop(voice_capture_chain);
}

static void appKymera_SendStartCaptureCfm(Task receiver, Source capture_source)
{
    MESSAGE_MAKE(message, KYMERA_START_VOICE_CAPTURE_CFM_T);
    message->status = FALSE;
    message->capture_source = capture_source;

    if (capture_source)
    {
        message->status = TRUE;
    }
    MessageSend(receiver, KYMERA_START_VOICE_CAPTURE_CFM, message);
}

static void appKymera_SendStopCaptureCfm(Task receiver, bool status)
{
    MESSAGE_MAKE(message, KYMERA_STOP_VOICE_CAPTURE_CFM_T);
    message->status = status;
    MessageSend(receiver, KYMERA_STOP_VOICE_CAPTURE_CFM, message);
}

static void appKymera_StartVoiceCapture(const KYMERA_INTERNAL_VOICE_CAPTURE_START_T *req)
{
    if (voice_capture_chain == NULL)
    {
        Source capture_source = NULL;
        OperatorsFrameworkSetKickPeriod(KICK_PERIOD_VOICE);
        appKymera_CreateVoiceCaptureChain(&req->params);
        appKymera_ConnectVoiceCaptureChainToMics();
        capture_source = appKymera_GetVoiceCaptureOutput();
        appKymera_StartVoiceCaptureChain();
        appKymera_SendStartCaptureCfm(req->client, capture_source);

        DEBUG_LOG("appKymera_StartVoiceCapture: Audio capture has started");
    }
    else
    {
        appKymera_SendStartCaptureCfm(req->client, NULL);
    }
}

static void appKymera_StopVoiceCapture(const KYMERA_INTERNAL_VOICE_CAPTURE_STOP_T *req)
{
    if (voice_capture_chain)
    {
        appKymera_DisconnectVoiceCaptureChainFromMics();
        appKymera_StopVoiceCaptureChain();
        appKymera_DisconnectVoiceCaptureOutput();
        appKymera_DestroyVoiceCaptureChain();
        appKymera_SendStopCaptureCfm(req->client, TRUE);

        DEBUG_LOG("appKymera_StopVoiceCapture: Audio capture has stopped");
    }
    else
    {
        appKymera_SendStopCaptureCfm(req->client, FALSE);
    }
}

static void appKymera_VoiceCaptureMessageHandler(Task task, MessageId id, Message msg)
{
    UNUSED(task);
    switch (id)
    {
        case KYMERA_INTERNAL_VOICE_CAPTURE_START:
            appKymera_StartVoiceCapture(msg);
        break;

        case KYMERA_INTERNAL_VOICE_CAPTURE_STOP:
            appKymera_StopVoiceCapture(msg);
        break;
    }
}

void AppKymera_StartVoiceCapture(Task client, const voice_capture_params_t *params)
{
    MESSAGE_MAKE(message, KYMERA_INTERNAL_VOICE_CAPTURE_START_T);
    kymeraTaskData *theKymera = KymeraGetTaskData();

    PanicFalse(client && params);
    message->client = client;
    message->params = *params;
    MessageSendConditionally((Task) &msgHandler, KYMERA_INTERNAL_VOICE_CAPTURE_START, message, &theKymera->lock);
}

void AppKymera_StopVoiceCapture(Task client)
{
    MESSAGE_MAKE(message, KYMERA_INTERNAL_VOICE_CAPTURE_STOP_T);
    kymeraTaskData *theKymera = KymeraGetTaskData();

    PanicNull(client);
    message->client = client;
    MessageSendConditionally((Task) &msgHandler, KYMERA_INTERNAL_VOICE_CAPTURE_STOP, message, &theKymera->lock);
}

bool appKymera_IsVoiceCaptureActive(void)
{
    return (voice_capture_chain != NULL);
}

#endif /* INCLUDE_KYMERA_VOICE_CAPTURE */
