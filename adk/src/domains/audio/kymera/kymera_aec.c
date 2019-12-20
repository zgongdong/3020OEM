/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       kymera_aec.c
\brief      Kymera module to connect/manage to AEC chain
*/

#include "kymera_aec.h"

#ifdef INCLUDE_KYMERA_AEC

#include "kymera_private.h"
#include "chains/chain_aec.h"

#define DEFAULT_OUTPUT_SAMPLE_RATE (48000)
#define DEFAULT_MIC_SAMPLE_RATE    (8000)

typedef enum
{
    audio_output_connected = (1 << 0),
    audio_input_connected  = (1 << 1)
} aec_connections_t;

static struct
{
    uint32            mic_sample_rate;
    aec_connections_t aec_connections;
} aec_config;

static kymera_chain_handle_t aec_chain;

static kymera_chain_handle_t kymera_GetAecChain(void)
{
    return aec_chain;
}

static void kymera_SetAecChain(kymera_chain_handle_t chain)
{
    aec_chain = chain;
}

static uint32 kymera_GetOutputSampleRate(void)
{
    uint32 sample_rate = DEFAULT_OUTPUT_SAMPLE_RATE;
    kymeraTaskData *theKymera = KymeraGetTaskData();

    if (theKymera->output_rate)
    {
        sample_rate = theKymera->output_rate;
    }
    return sample_rate;
}

static uint32 kymera_GetMicSampleRate(void)
{
    uint32 sample_rate = DEFAULT_MIC_SAMPLE_RATE;

    if (aec_config.mic_sample_rate)
    {
        sample_rate = aec_config.mic_sample_rate;
    }
    return sample_rate;
}

static void kymera_SetMicSampleRate(uint32 sample_rate)
{
    aec_config.mic_sample_rate = sample_rate;
}

static Operator kymera_GetAecOperator(void)
{
    Operator aec = ChainGetOperatorByRole(kymera_GetAecChain(), OPR_AEC);
    if (!aec)
    {
        Panic();
    }
    return aec;
}

static Sink kymera_GetAecInput(chain_endpoint_role_t input_role)
{
    return ChainGetInput(kymera_GetAecChain(), input_role);
}

static Source kymera_GetAecOutput(chain_endpoint_role_t output_role)
{
    return ChainGetOutput(kymera_GetAecChain(), output_role);
}

static bool kymera_IsAudioInputConnected(void)
{
    return (aec_config.aec_connections & audio_input_connected) != 0;
}

static bool kymera_IsAudioOutputConnected(void)
{
    return (aec_config.aec_connections & audio_output_connected) != 0;
}

static bool kymera_IsAecConnected(void)
{
    return kymera_IsAudioInputConnected() || kymera_IsAudioOutputConnected();
}

static void kymera_AddAecConnection(aec_connections_t connection)
{
    aec_config.aec_connections |= connection;
}

static void kymera_RemoveAecConnection(aec_connections_t connection)
{
    aec_config.aec_connections &= ~connection;
}

static kymera_operator_ucid_t kymera_GetAecUcid(void)
{
    return UCID_AEC_WB_VA;
}

static void kymera_ConfigureAecChain(void)
{
    Operator aec = kymera_GetAecOperator();

    DEBUG_LOG("kymera_ConfigureAecChain: Output sample rate %u, mic sample rate %u", kymera_GetOutputSampleRate(), kymera_GetMicSampleRate());
    OperatorsStandardSetUCID(aec, kymera_GetAecUcid());
    OperatorsAecSetSampleRate(aec, kymera_GetOutputSampleRate(), kymera_GetMicSampleRate());
}

static void kymera_CreateAecChain(void)
{
    DEBUG_LOG("kymera_CreateAecChain");
    PanicNotNull(kymera_GetAecChain());
    kymera_SetAecChain(PanicNull(ChainCreate(&chain_aec_config)));
    kymera_ConfigureAecChain();
    ChainConnect(kymera_GetAecChain());
    ChainStart(kymera_GetAecChain());
}

static void kymera_DestroyAecChain(void)
{
    DEBUG_LOG("kymera_DestroyAecChain");
    PanicNull(kymera_GetAecChain());
    ChainStop(kymera_GetAecChain());
    ChainDestroy(kymera_GetAecChain());
    kymera_SetAecChain(NULL);
}

static void kymera_ConnectAudioOutput(const aec_connect_audio_output_t *params)
{
    DEBUG_LOG("kymera_ConnectAudioOutput: Connect audio output to AEC");
    kymera_AddAecConnection(audio_output_connected);
    PanicNull(StreamConnect(kymera_GetAecOutput(EPR_AEC_SPEAKER1_OUT), params->speaker_output_1));
    PanicNull(StreamConnect(params->input_1, kymera_GetAecInput(EPR_AEC_INPUT1)));
}

static void kymera_DisconnectAudioOutput(void)
{
    DEBUG_LOG("kymera_DisconnectAudioOutput: Disconnect audio output from AEC");
    kymera_RemoveAecConnection(audio_output_connected);
    StreamDisconnect(NULL, kymera_GetAecInput(EPR_AEC_INPUT1));
    StreamDisconnect(kymera_GetAecOutput(EPR_AEC_SPEAKER1_OUT), NULL);
}

static void kymera_ConnectAudioInput(const aec_connect_audio_input_t *params)
{
    DEBUG_LOG("kymera_ConnectAudioInput: Connect audio input to AEC");
    kymera_AddAecConnection(audio_input_connected);

    PanicNull(StreamConnect(kymera_GetAecOutput(EPR_AEC_REFERENCE_OUT), params->reference_output));

    PanicNull(StreamConnect(kymera_GetAecOutput(EPR_AEC_MIC1_OUT), params->mic_output_1));
    PanicNull(StreamConnect(params->mic_input_1, kymera_GetAecInput(EPR_AEC_MIC1_IN)));

    if (params->mic_output_2)
    {
        PanicNull(StreamConnect(kymera_GetAecOutput(EPR_AEC_MIC2_OUT), params->mic_output_2));
        PanicNull(StreamConnect(params->mic_input_2, kymera_GetAecInput(EPR_AEC_MIC2_IN)));
    }
}

static void kymera_DisconnectAudioInput(void)
{
    DEBUG_LOG("kymera_DisconnectAudioInput: Disconnect audio input from AEC");
    kymera_RemoveAecConnection(audio_input_connected);

    StreamDisconnect(NULL, kymera_GetAecInput(EPR_AEC_MIC1_IN));
    StreamDisconnect(NULL, kymera_GetAecInput(EPR_AEC_MIC2_IN));

    StreamDisconnect(kymera_GetAecOutput(EPR_AEC_REFERENCE_OUT), NULL);
    StreamDisconnect(kymera_GetAecOutput(EPR_AEC_MIC1_OUT), NULL);
    StreamDisconnect(kymera_GetAecOutput(EPR_AEC_MIC2_OUT), NULL);
}

void Kymera_ConnectAudioOutputToAec(const aec_connect_audio_output_t *params)
{
    PanicNotZero(kymera_IsAudioOutputConnected() || (params == NULL));
    if (kymera_GetAecChain() == NULL)
    {
        kymera_CreateAecChain();
    }
    kymera_ConfigureAecChain();
    kymera_ConnectAudioOutput(params);
}

void Kymera_DisconnectAudioOutputFromAec(void)
{
    PanicFalse(kymera_IsAudioOutputConnected());
    kymera_DisconnectAudioOutput();
    if (kymera_IsAecConnected() == FALSE)
    {
        kymera_DestroyAecChain();
    }
}

void Kymera_ConnectAudioInputToAec(const aec_connect_audio_input_t *params)
{
    PanicNotZero(kymera_IsAudioInputConnected() || (params == NULL));
    kymera_SetMicSampleRate(params->mic_sample_rate);
    if (kymera_IsAecConnected() == FALSE)
    {
        kymera_CreateAecChain();
    }
    kymera_ConfigureAecChain();
    kymera_ConnectAudioInput(params);
}

void Kymera_DisconnectAudioInputFromAec(void)
{
    PanicFalse(kymera_IsAudioInputConnected());
    kymera_SetMicSampleRate(0);
    kymera_DisconnectAudioInput();
    if (kymera_IsAecConnected() == FALSE)
    {
        kymera_DestroyAecChain();
    }
}

#endif /* INCLUDE_KYMERA_AEC */
