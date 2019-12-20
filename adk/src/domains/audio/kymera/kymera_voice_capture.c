/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       kymera_voice_capture.c
\brief      Kymera module to handle voice capture APIs
*/

#include "kymera_voice_capture.h"
#include "kymera.h"
#include "kymera_private.h"
#include "chains/chain_voice_capture_1mic_sbc.h"
#include "chains/chain_voice_capture_2mic_sbc.h"
#include "kymera_aec.h"

#define AUDIO_FRAME_VA_DATA_LENGTH (9)
#define MIC_SAMPLE_RATE (16000)

typedef enum
{
    KYMERA_INTERNAL_VOICE_CAPTURE_START,
    KYMERA_INTERNAL_VOICE_CAPTURE_STOP
} internal_message_ids;

typedef struct
{
    Task client;
    va_audio_voice_capture_params_t params;
} KYMERA_INTERNAL_VOICE_CAPTURE_START_T;

typedef struct
{
    Task client;
} KYMERA_INTERNAL_VOICE_CAPTURE_STOP_T;

static void kymera_VoiceCaptureMessageHandler(Task task, MessageId id, Message msg);
static const TaskData msgHandler = { kymera_VoiceCaptureMessageHandler };

static kymera_chain_handle_t voice_capture_chain;

static const chain_config_t * kymera_GetChainConfig(void)
{
#ifdef  KYMERA_VOICE_CAPTURE_USE_1MIC_CVC
    return &chain_voice_capture_1mic_sbc_config;
#else
    return &chain_voice_capture_2mic_sbc_config;
#endif
}

static void kymera_ConfigureSbcEncoder(sbc_encoder_params_t *sbc_params)
{
    Operator sbc = ChainGetOperatorByRole(voice_capture_chain, OPR_SBC_ENCODER);
    PanicFalse(sbc);
    OperatorsSbcEncoderSetEncodingParams(sbc, sbc_params);
}

static void kymera_ConfigureCvc(void)
{
    Operator cvc = ChainGetOperatorByRole(voice_capture_chain, OPR_CVC_SEND);
    PanicFalse(cvc);
    OperatorsStandardSetUCID(cvc, UCID_CVC_SEND_VA);
}

static void kymera_ConfigureVoiceCaptureChain(va_audio_voice_capture_params_t *params)
{
    kymera_ConfigureSbcEncoder(&params->encoder_params.sbc);
    kymera_ConfigureCvc();
}

static void kymera_CreateVoiceCaptureChain(va_audio_voice_capture_params_t *params)
{
    PanicFalse(voice_capture_chain == NULL);
    voice_capture_chain = PanicNull(ChainCreate(kymera_GetChainConfig()));
    appKymeraConfigureDspPowerMode(FALSE);
    OperatorsFrameworkSetKickPeriod(KICK_PERIOD_VOICE);
    kymera_ConfigureVoiceCaptureChain(params);
    ChainConnect(voice_capture_chain);
}

static void kymera_DestroyVoiceCaptureChain(void)
{
    kymera_chain_handle_t temp = voice_capture_chain;
    voice_capture_chain = NULL;
    PanicFalse(temp != NULL);
    appKymeraConfigureDspPowerMode(FALSE);
    ChainDestroy(temp);
}

static void kymera_ConnectVoiceCaptureChainToMics(void)
{
    Source mic1 = NULL, mic2 = NULL;
    Sink mic1_input = ChainGetInput(voice_capture_chain, EPR_VOICE_CAPTURE_MIC1_IN);
    Sink mic2_input = ChainGetInput(voice_capture_chain, EPR_VOICE_CAPTURE_MIC2_IN);

    mic1 = Kymera_GetMicrophoneSource(microphone_1, NULL, MIC_SAMPLE_RATE, high_priority_user);
    if (mic2_input)
    {
        mic2 = Kymera_GetMicrophoneSource(microphone_2, mic1, MIC_SAMPLE_RATE, high_priority_user);
    }

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
    if (mic2_input)
    {
        PanicNull(StreamConnect(mic2, mic2_input));
    }
#endif
}

static void kymera_DisconnectVoiceCaptureChainFromMics(void)
{
    Sink mic2_input = ChainGetInput(voice_capture_chain, EPR_VOICE_CAPTURE_MIC2_IN);

#ifdef INCLUDE_KYMERA_AEC
    Kymera_DisconnectAudioInputFromAec();
#else
    StreamDisconnect(NULL, ChainGetInput(voice_capture_chain, EPR_VOICE_CAPTURE_MIC1_IN));
    if (mic2_input)
    {
        StreamDisconnect(NULL, mic2_input);
    }
#endif

    Kymera_CloseMicrophone(microphone_1, high_priority_user);
    if (mic2_input)
    {
        Kymera_CloseMicrophone(microphone_2, high_priority_user);
    }
}

static Source kymera_GetVoiceCaptureOutput(void)
{
    Source capture_output = ChainGetOutput(voice_capture_chain, EPR_VOICE_CAPTURE_OUT);

    PanicFalse(SourceMapInit(capture_output, STREAM_TIMESTAMPED, AUDIO_FRAME_VA_DATA_LENGTH));

    return capture_output;
}

static void kymera_DisconnectVoiceCaptureOutput(void)
{
    Source capture_output = ChainGetOutput(voice_capture_chain, EPR_VOICE_CAPTURE_OUT);

    SourceUnmap(capture_output);
    StreamDisconnect(capture_output, NULL);
}

static void kymera_StartVoiceCaptureChain(void)
{
    ChainStart(voice_capture_chain);
}

static void kymera_StopVoiceCaptureChain(void)
{
    ChainStop(voice_capture_chain);
}

static void kymera_SendStartCaptureCfm(Task receiver, Source capture_source)
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

static void kymera_SendStopCaptureCfm(Task receiver, bool status)
{
    MESSAGE_MAKE(message, KYMERA_STOP_VOICE_CAPTURE_CFM_T);
    message->status = status;
    MessageSend(receiver, KYMERA_STOP_VOICE_CAPTURE_CFM, message);
}

static void kymera_StartVoiceCapture(const KYMERA_INTERNAL_VOICE_CAPTURE_START_T *req)
{
    if (voice_capture_chain == NULL)
    {
        Source capture_source = NULL;
        kymera_CreateVoiceCaptureChain(&req->params);
        kymera_ConnectVoiceCaptureChainToMics();
        capture_source = kymera_GetVoiceCaptureOutput();
        kymera_StartVoiceCaptureChain();
        kymera_SendStartCaptureCfm(req->client, capture_source);

        DEBUG_LOG("kymera_StartVoiceCapture: Audio capture has started");
    }
    else
    {
        kymera_SendStartCaptureCfm(req->client, NULL);
    }
}

static void kymera_StopVoiceCapture(const KYMERA_INTERNAL_VOICE_CAPTURE_STOP_T *req)
{
    if (voice_capture_chain)
    {
        kymera_DisconnectVoiceCaptureChainFromMics();
        kymera_StopVoiceCaptureChain();
        kymera_DisconnectVoiceCaptureOutput();
        kymera_DestroyVoiceCaptureChain();
        kymera_SendStopCaptureCfm(req->client, TRUE);

        DEBUG_LOG("kymera_StopVoiceCapture: Audio capture has stopped");
    }
    else
    {
        kymera_SendStopCaptureCfm(req->client, FALSE);
    }
}

static void kymera_VoiceCaptureMessageHandler(Task task, MessageId id, Message msg)
{
    UNUSED(task);
    switch (id)
    {
        case KYMERA_INTERNAL_VOICE_CAPTURE_START:
            kymera_StartVoiceCapture(msg);
        break;

        case KYMERA_INTERNAL_VOICE_CAPTURE_STOP:
            kymera_StopVoiceCapture(msg);
        break;
    }
}

void Kymera_StartVoiceCapture(Task client, const va_audio_voice_capture_params_t *params)
{
    MESSAGE_MAKE(message, KYMERA_INTERNAL_VOICE_CAPTURE_START_T);
    kymeraTaskData *theKymera = KymeraGetTaskData();

    PanicFalse(client && params);
    message->client = client;
    message->params = *params;
    MessageSendConditionally((Task) &msgHandler, KYMERA_INTERNAL_VOICE_CAPTURE_START, message, &theKymera->lock);
}

void Kymera_StopVoiceCapture(Task client)
{
    MESSAGE_MAKE(message, KYMERA_INTERNAL_VOICE_CAPTURE_STOP_T);
    kymeraTaskData *theKymera = KymeraGetTaskData();

    PanicNull(client);
    message->client = client;
    MessageSendConditionally((Task) &msgHandler, KYMERA_INTERNAL_VOICE_CAPTURE_STOP, message, &theKymera->lock);
}

bool Kymera_IsVoiceCaptureActive(void)
{
    return (voice_capture_chain != NULL);
}
