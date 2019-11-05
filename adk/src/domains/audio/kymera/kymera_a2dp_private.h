/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       kymera_a2dp_private.h
\brief      Private (internal) kymera A2DP header file.

*/

#ifndef KYMERA_A2DP_PRIVATE_H
#define KYMERA_A2DP_PRIVATE_H

#include <a2dp.h>
#include <source.h>
#include <chain.h>

/*!@{ \name Useful gains in kymera operators format */
#define GAIN_HALF (-6 * KYMERA_DB_SCALE)
#define GAIN_FULL (0)
#define GAIN_MIN (-90 * KYMERA_DB_SCALE)
/*!@} */

#define MIXER_GAIN_RAMP_SAMPLES 24000

/*! \brief Start A2DP master.

    \param codec_settings The A2DP codec settings to use.
    \param volume_in_db The initial volume to use.

    \return TRUE if start is completed, else FALSE.

    In TWS legacy, the 'master' is the device that is connected to the handset
    and forwards media to the 'slave' device.
    In TWS shadowing, both devices are 'master' and no media is forwarded.

    The same API is used here to simplify the code in higher layers.
 */
bool appKymeraA2dpStartMaster(const a2dp_codec_settings *codec_settings, int16 volume_in_db);

/*! \brief Start A2DP forwarding.

    \param codec_settings The A2DP codec settings to use.

    In TWS legacy, this function starts the master forwarding media to the slave.
    In TWS shadowing, this function is not be used.
 */
void appKymeraA2dpStartForwarding(const a2dp_codec_settings *codec_settings);

/*! \brief Stop A2DP forwarding.

    In TWS legacy, this function stops the master forwarding media to the slave.
    In TWS shadowing, this function is not be used.
 */
void appKymeraA2dpStopForwarding(void);

/*! \brief Stop A2DP operation.

    Common function to master and slave. */
void appKymeraA2dpCommonStop(Source source);

/*! \brief Start A2DP slave.

    \param codec_settings The A2DP codec settings to use.
    \param volume_in_db The initial volume to use.

    In TWS legacy, the 'master' is the device that is connected to the handset
    and forwards media to the 'slave' device.
    In TWS shadowing, both devices are 'master' and no media is forwarded,
    therefore this function is not used.
 */
void appKymeraA2dpStartSlave(a2dp_codec_settings *codec_settings, int16 volume_in_db);


/*! \brief Helper function to unpack a2dp codec settings into individual variables.

    \param codec_settings The A2DP codec settings to unpack.
    \param seid [out] The stream endpoint id.
    \param source [out] The media source.
    \param rate [out] The media sample rate in Hz.
    \param cp_enabled [out] Content protection enabled.
    \param mtu [out] Media channel L2CAP mtu.
 */
void appKymeraGetA2dpCodecSettingsCore(const a2dp_codec_settings *codec_settings,
                                              uint8 *seid, Source *source, uint32 *rate,
                                              bool *cp_enabled, uint16 *mtu);

/*! \brief Helper function to configure the RTP decoder.

    \param op The operator id of the RTP decoder.
    \param codec_type The codec type to configure.
    \param mode working mode to configure.
    \param rate The sample rate to configure in Hz.
    \param cp_enabled Content protection enabled.
    \param buffer_size The size of the buffer to use in words. If zero the buffer size will not be configured.
 */
void appKymeraConfigureRtpDecoder(Operator op, rtp_codec_type_t codec_type, rtp_working_mode_t mode, uint32 rate, bool cp_header_enabled, unsigned buffer_size);

/*! \brief Helper function to initially configure the l/r mixer in the A2DP input chain.

    \param chain The chain containing the left/right mixer.
    \param rate The sample rate to configure in Hz.
    \param stereo_lr_mix If TRUE the mixer will output a 50%/50% mix of the
            incoming stereo channels. If FALSE the mixer will output 100% left/right
            based on the is_left parameter.
    \param is_left Earbud is left/right.
 */
void appKymeraConfigureLeftRightMixer(kymera_chain_handle_t chain, uint32 rate, bool stereo_lr_mix, bool is_left);

/*! \brief Helper function to set/change the l/r mixer mode in the A2DP input chain.
    \param stereo_lr_mix If TRUE the mixer will output a 50%/50% mix of the
            incoming stereo channels. If FALSE the mixer will output 100% left/right
            based on the is_left parameter.
    \param is_left If TRUE, the earbud is left and 100% left channel media will
            be output by the mixer when stereo_lr_mix is FALSE.
            If FALSE, the earbud if right and 100% right channel media will be
            output by the mixer when stereo_lr_mix is FALSE.

 */
void appKymeraSetLeftRightMixerMode(kymera_chain_handle_t chain, bool stereo_lr_mix, bool is_left);

#endif /* KYMERA_A2DP_PRIVATE_H */
