/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       kymera_a2d_common.c
\brief      Kymera A2DP common functions.
*/

#include <a2dp.h>
#include <operator.h>
#include <operators.h>
#include <panic.h>
#include <opmsg_prim.h>

#include "earbud_chain_roles.h"

#include "kymera_a2dp_private.h"
#include "kymera_private.h"


void appKymeraGetA2dpCodecSettingsCore(const a2dp_codec_settings *codec_settings,
                                       uint8 *seid, Source *source, uint32 *rate,
                                       bool *cp_enabled, uint16 *mtu)
{
    if (seid)
    {
        *seid = codec_settings->seid;
    }
    if (source)
    {
        *source = StreamSourceFromSink(codec_settings->sink);
    }
    if (rate)
    {
        *rate = codec_settings->rate;
    }
    if (cp_enabled)
    {
        *cp_enabled = !!(codec_settings->codecData.content_protection);
    }
    if (mtu)
    {
        *mtu = codec_settings->codecData.packet_size;
    }
}

void appKymeraConfigureRtpDecoder(Operator op, rtp_codec_type_t codec_type, uint32 rate, bool cp_header_enabled)
{
    rtp_working_mode_t mode = (codec_type == rtp_codec_type_aptx && !cp_header_enabled) ?
                                    rtp_ttp_only : rtp_decode;
    const uint32 filter_gain = FRACTIONAL(0.997);
    const uint32 err_scale = FRACTIONAL(-0.00000001);
    /* Disable the Kymera TTP startup period, the other parameters are defaults. */
    const OPMSG_COMMON_MSG_SET_TTP_PARAMS ttp_params_msg = {
        OPMSG_COMMON_MSG_SET_TTP_PARAMS_CREATE(OPMSG_COMMON_SET_TTP_PARAMS,
            UINT32_MSW(filter_gain), UINT32_LSW(filter_gain),
            UINT32_MSW(err_scale), UINT32_LSW(err_scale),
            0)
    };

    OperatorsRtpSetCodecType(op, codec_type);
    OperatorsRtpSetWorkingMode(op, mode);
    OperatorsStandardSetTimeToPlayLatency(op, TWS_STANDARD_LATENCY_US);

    /* The RTP decoder controls the audio latency by assigning timestamps
    to the incoming audio stream. If the latency falls outside the limits (e.g.
    because the source delivers too much/little audio in a given time) the
    RTP decoder will reset its timestamp generator, returning to the target
    latency immediately. This will cause an audio glitch, but the AV sync will
    be correct and the system will operate correctly.

    Since audio is forwarded to the slave earbud, the minimum latency is the
    time at which the packetiser transmits packets to the slave device.
    If the latency were lower than this value, the packetiser would discard the audio
    frames and not transmit any audio to the slave, resulting in silence.
    */

    OperatorsStandardSetLatencyLimits(op, appConfigTwsTimeBeforeTx(), US_PER_MS*TWS_STANDARD_LATENCY_MAX_MS);
    OperatorsStandardSetBufferSizeWithFormat(op, PRE_DECODER_BUFFER_SIZE, operator_data_format_encoded);
    OperatorsRtpSetContentProtection(op, cp_header_enabled);
    /* Sending this message trashes the RTP operator sample rate */
    PanicFalse(OperatorMessage(op, ttp_params_msg._data, OPMSG_COMMON_MSG_SET_TTP_PARAMS_WORD_SIZE, NULL, 0));
    OperatorsStandardSetSampleRate(op, rate);
}

static void appKymeraGetLeftRightMixerGains(bool stereo_lr_mix, bool is_left, int *gain_l, int *gain_r)
{
    int gl, gr;

    if (stereo_lr_mix)
    {
        gl = gr = GAIN_HALF;
    }
    else
    {
        gl = is_left ? GAIN_FULL : GAIN_MIN;
        gr = is_left ? GAIN_MIN : GAIN_FULL;
    }
    *gain_l = gl;
    *gain_r = gr;
}

void appKymeraConfigureLeftRightMixer(kymera_chain_handle_t chain, uint32 rate, bool stereo_lr_mix, bool is_left)
{
    Operator mixer;

    if (GET_OP_FROM_CHAIN(mixer, chain, OPR_LEFT_RIGHT_MIXER))
    {
        int gain_l, gain_r;

        appKymeraGetLeftRightMixerGains(stereo_lr_mix, is_left, &gain_l, &gain_r);

        OperatorsConfigureMixer(mixer, rate, 1, gain_l, gain_r, GAIN_MIN, 1, 1, 0);
        OperatorsMixerSetNumberOfSamplesToRamp(mixer, MIXER_GAIN_RAMP_SAMPLES);
    }
}

void appKymeraSetLeftRightMixerMode(kymera_chain_handle_t chain, bool stereo_lr_mix, bool is_left)
{
    Operator mixer;

    if (GET_OP_FROM_CHAIN(mixer, chain, OPR_LEFT_RIGHT_MIXER))
    {
        int gain_l, gain_r;

        appKymeraGetLeftRightMixerGains(stereo_lr_mix, is_left, &gain_l, &gain_r);

        OperatorsMixerSetGains(mixer, gain_l, gain_r, GAIN_MIN);
    }
}