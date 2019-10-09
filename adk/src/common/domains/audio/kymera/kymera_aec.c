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
#define UCID                       (UCID_AEC_WB_VA)
#define MIC_1 (microphone_1)
#define MIC_2 (microphone_2)

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

static kymera_chain_handle_t appKymera_GetAecChain(void)
{
    return aec_chain;
}

static void appKymera_SetAecChain(kymera_chain_handle_t chain)
{
    aec_chain = chain;
}

static uint32 appKymera_GetOutputSampleRate(void)
{
    uint32 sample_rate = DEFAULT_OUTPUT_SAMPLE_RATE;
    kymeraTaskData *theKymera = KymeraGetTaskData();

    if (theKymera->output_rate)
    {
        sample_rate = theKymera->output_rate;
    }
    return sample_rate;
}

static uint32 appKymera_GetMicSampleRate(void)
{
    uint32 sample_rate = DEFAULT_MIC_SAMPLE_RATE;

    if (aec_config.mic_sample_rate)
    {
        sample_rate = aec_config.mic_sample_rate;
    }
    return sample_rate;
}

static void appKymera_SetMicSampleRate(uint32 sample_rate)
{
    aec_config.mic_sample_rate = sample_rate;
}

static Operator appKymera_GetAecOperator(void)
{
    Operator aec = ChainGetOperatorByRole(appKymera_GetAecChain(), OPR_AEC);
    if (!aec)
    {
        Panic();
    }
    return aec;
}

static Sink appKymera_GetAecInput(chain_endpoint_role_t input_role)
{
    return ChainGetInput(appKymera_GetAecChain(), input_role);
}

static Source appKymera_GetAecOutput(chain_endpoint_role_t output_role)
{
    return ChainGetOutput(appKymera_GetAecChain(), output_role);
}

static bool appKymera_IsAudioInputConnected(void)
{
    return (aec_config.aec_connections & audio_input_connected) != 0;
}

static bool appKymera_IsAudioOutputConnected(void)
{
    return (aec_config.aec_connections & audio_output_connected) != 0;
}

static bool appKymera_IsAecConnected(void)
{
    return appKymera_IsAudioInputConnected() || appKymera_IsAudioOutputConnected();
}

static void appKymera_AddAecConnection(aec_connections_t connection)
{
    aec_config.aec_connections |= connection;
}

static void appKymera_RemoveAecConnection(aec_connections_t connection)
{
    aec_config.aec_connections &= ~connection;
}

static kymera_operator_ucid_t appKymera_GetAecUcid(void)
{
    return UCID;
}

static void appKymera_ConfigureAecChain(void)
{
    Operator aec = appKymera_GetAecOperator();

    DEBUG_LOG("appKymera_ConfigureAecChain: Output sample rate %u, mic sample rate %u", appKymera_GetOutputSampleRate(), appKymera_GetMicSampleRate());
    OperatorsStandardSetUCID(aec, appKymera_GetAecUcid());
    OperatorsAecSetSampleRate(aec, appKymera_GetOutputSampleRate(), appKymera_GetMicSampleRate());
}

static void appKymera_CreateAecChain(void)
{
    DEBUG_LOG("appKymera_CreateAecChain");
    PanicNotNull(appKymera_GetAecChain());
    appKymera_SetAecChain(PanicNull(ChainCreate(&chain_aec_config)));
    appKymera_ConfigureAecChain();
    ChainConnect(appKymera_GetAecChain());
    ChainStart(appKymera_GetAecChain());
}

static void appKymera_DestroyAecChain(void)
{
    DEBUG_LOG("appKymera_DestroyAecChain");
    PanicNull(appKymera_GetAecChain());
    ChainStop(appKymera_GetAecChain());
    ChainDestroy(appKymera_GetAecChain());
    appKymera_SetAecChain(NULL);
}

static void appKymera_ConnectAudioOutput(const aec_connect_audio_output_t *params)
{
    DEBUG_LOG("appKymera_ConnectAudioOutput: Connect audio output to AEC");
    appKymera_AddAecConnection(audio_output_connected);
    PanicNull(StreamConnect(appKymera_GetAecOutput(EPR_AEC_SPEAKER1_OUT), params->speaker_output_1));
    PanicNull(StreamConnect(params->input_1, appKymera_GetAecInput(EPR_AEC_INPUT1)));
}

static void appKymera_DisconnectAudioOutput(void)
{
    DEBUG_LOG("appKymera_DisconnectAudioOutput: Disconnect audio output from AEC");
    appKymera_RemoveAecConnection(audio_output_connected);
    StreamDisconnect(NULL, appKymera_GetAecInput(EPR_AEC_INPUT1));
    StreamDisconnect(appKymera_GetAecOutput(EPR_AEC_SPEAKER1_OUT), NULL);
}

static void appKymera_ConnectAudioInput(const aec_connect_audio_input_t *params)
{
    DEBUG_LOG("appKymera_ConnectAudioInput: Connect audio input to AEC");
    appKymera_AddAecConnection(audio_input_connected);

    PanicNull(StreamConnect(appKymera_GetAecOutput(EPR_AEC_REFERENCE_OUT), params->reference_output));
    PanicNull(StreamConnect(appKymera_GetAecOutput(EPR_AEC_MIC1_OUT), params->mic_output_1));
    PanicNull(StreamConnect(appKymera_GetAecOutput(EPR_AEC_MIC2_OUT), params->mic_output_2));

    Source mic1 = Kymera_GetMicrophoneSource(MIC_1, NULL, appKymera_GetMicSampleRate(), high_priority_user);
    Source mic2 = Kymera_GetMicrophoneSource(MIC_2, mic1, appKymera_GetMicSampleRate(), high_priority_user);
    PanicNull(StreamConnect(mic1, appKymera_GetAecInput(EPR_AEC_MIC1_IN)));
    PanicNull(StreamConnect(mic2, appKymera_GetAecInput(EPR_AEC_MIC2_IN)));
}

static void appKymera_DisconnectAudioInput(void)
{
    DEBUG_LOG("appKymera_DisconnectAudioInput: Disconnect audio input from AEC");
    appKymera_RemoveAecConnection(audio_input_connected);

    StreamDisconnect(NULL, appKymera_GetAecInput(EPR_AEC_MIC1_IN));
    StreamDisconnect(NULL, appKymera_GetAecInput(EPR_AEC_MIC2_IN));
    Kymera_CloseMicrophone(MIC_1, high_priority_user);
    Kymera_CloseMicrophone(MIC_2, high_priority_user);

    StreamDisconnect(appKymera_GetAecOutput(EPR_AEC_REFERENCE_OUT), NULL);
    StreamDisconnect(appKymera_GetAecOutput(EPR_AEC_MIC1_OUT), NULL);
    StreamDisconnect(appKymera_GetAecOutput(EPR_AEC_MIC2_OUT), NULL);
}

void appKymera_ConnectAudioOutputToAec(const aec_connect_audio_output_t *params)
{
    PanicNotZero(appKymera_IsAudioOutputConnected() || (params == NULL));
    if (appKymera_GetAecChain() == NULL)
    {
        appKymera_CreateAecChain();
    }
    appKymera_ConfigureAecChain();
    appKymera_ConnectAudioOutput(params);
}

void appKymera_DisconnectAudioOutputFromAec(void)
{
    PanicFalse(appKymera_IsAudioOutputConnected());
    appKymera_DisconnectAudioOutput();
    if (appKymera_IsAecConnected() == FALSE)
    {
        appKymera_DestroyAecChain();
    }
}

void appKymera_ConnectAudioInputToAec(const aec_connect_audio_input_t *params)
{
    PanicNotZero(appKymera_IsAudioInputConnected() || (params == NULL));
    appKymera_SetMicSampleRate(params->mic_sample_rate);
    if (appKymera_IsAecConnected() == FALSE)
    {
        appKymera_CreateAecChain();
    }
    appKymera_ConfigureAecChain();
    appKymera_ConnectAudioInput(params);
}

void appKymera_DisconnectAudioInputFromAec(void)
{
    PanicFalse(appKymera_IsAudioInputConnected());
    appKymera_SetMicSampleRate(0);
    appKymera_DisconnectAudioInput();
    if (appKymera_IsAecConnected() == FALSE)
    {
        appKymera_DestroyAecChain();
    }
}

#endif /* INCLUDE_KYMERA_AEC */
