/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       kymera_config.h
\brief      Configuration related definitions for Kymera audio.
*/

#ifndef KYMERA_CONFIG_H
#define KYMERA_CONFIG_H

#include "microphones.h"

/*! \brief Fixed tone volume in dB */
#define KYMERA_CONFIG_TONE_VOLUME               (-20)

/*! \brief Fixed prompt volume in dB */
#define KYMERA_CONFIG_PROMPT_VOLUME             (-10)

/*! \brief Defining USB as downloadable capability for Aura2.1 variant */
#if defined(HAVE_STR_ROM_2_0_1)
#define DOWNLOAD_USB_AUDIO
#endif
/*! @{ Which microphones to use for SCO */
    /*! microphone to use for the first SCO mic */
#define appConfigScoMic1()                      (microphone_1)
#define appConfigScoMic2()                      (microphone_none)     /* Don't use microphone for SCO 2nd mic (CVC 1-mic) */
/*! @} */

//!@{ @name ANC configuration */
#define appConfigAncPathEnable()                (feed_forward_mode_left_only)
#define appConfigAncFeedForwardMic()            (microphone_1)
#define appConfigAncFeedBackMic()               (microphone_none)
#define appConfigAncMicGainStepSize()           (5)
#define appConfigAncSidetoneGain()              (10)
#define appConfigAncMode()                      (anc_mode_1)
//!@}

/*! Enable ANC tuning functionality */
#define appConfigAncTuningEnabled()             (FALSE)

/*! ANC tuning monitor microphone */
#define appConfigAncTuningMonitorMic()          (microphone_none)

/*! Time to play to be applied on this earbud, based on the Wesco
    value specified when creating the connection.
    A value of 0 will disable TTP.  */
#define appConfigScoChainTTP(wesco)     (wesco * 0)

/*! The time before the TTP at which a packet should be transmitted.
    This is only relevant for TWS forwarding topology.  */
#define appConfigTwsTimeBeforeTx() MAX(70000, TWS_STANDARD_LATENCY_US-200000)

/*! The last time before the TTP at which a packet may be transmitted */
#define appConfigTwsDeadline()      MAX(35000, TWS_STANDARD_LATENCY_US-250000)

/*! @{ Define the hardware settings for the left audio */
/*! Define which channel the 'left' audio channel comes out of. */
#define appConfigLeftAudioChannel()              (AUDIO_CHANNEL_A)

/*! Define the type of Audio Hardware for the 'left' audio channel. */
#define appConfigLeftAudioHardware()             (AUDIO_HARDWARE_CODEC)

/*! Define the instance for the 'left' audio channel comes. */
#define appConfigLeftAudioInstance()             (AUDIO_INSTANCE_0)
/*! @} */

/*! Define whether audio should start with or without a soft volume ramp */
#define appConfigEnableSoftVolumeRampOnStart() (FALSE)

/*!@{ @name External AMP control
      @brief If required, allows the PIO/bank/masks used to control an external
             amp to be defined.
*/
#if defined(CE821_CF212) || defined(CF376_CF212) || defined(CE821_CE826) || defined(CF133)

#define appConfigExternalAmpControlRequired()    (TRUE)
#define appConfigExternalAmpControlPio()         (32)
#define appConfigExternalAmpControlPioBank()     (1)
#define appConfigExternalAmpControlEnableMask()  (0 << 0)
#define appConfigExternalAmpControlDisableMask() (1 << (appConfigExternalAmpControlPio() % 32))

#else

#define appConfigExternalAmpControlRequired()    (FALSE)
#define appConfigExternalAmpControlPio()         (0)
#define appConfigExternalAmpControlEnableMask()  (0)
#define appConfigExternalAmpControlDisableMask() (0)

#endif /* defined(CE821_CF212) or defined(CF376_CF212) or defined(CE821_CE826) */
//!@}

/*! Enable or disable voice quality measurements for TWS+. */
#define appConfigVoiceQualityMeasurementEnabled() TRUE

/*! The worst reportable voice quality */
#define appConfigVoiceQualityWorst() 0

/*! The best reportable voice quality */
#define appConfigVoiceQualityBest() 15

/*! The voice quality to report if measurement is disabled. Must be in the
    range appConfigVoiceQualityWorst() to appConfigVoiceQualityBest(). */
#define appConfigVoiceQualityWhenDisabled() appConfigVoiceQualityBest()

/*! Minimum volume gain in dB */
#define appConfigMinVolumedB() (-45)

/*! Maximum volume gain in dB */
#define appConfigMaxVolumedB() (0)

/*! This enables support for rendering a 50/50 mono mix of the left/right
    decoded aptX channels when only one earbud is in ear. This feature requires
    a stereo aptX licence since a stereo decode is performed, so it is disabled
    by default. If disabled (set to 0), with aptX, only the left/right channel
    audio will be rendered by the left/right earbud.

    SBC and AAC support stereo mixing by default.
*/
#define appConfigEnableAptxStereoMix() (0)

/*! Will give significant audio heap savings.
    Achieved by buffering the local channel of the aptX demux output in place of buffering the decoder output.
*/
#define appConfigAptxNoPcmLatencyBuffer() TRUE

/*! Will give significant audio heap savings. Only works when AAC stereo is forwarded.
    Achieved by buffering the decoder input in place of buffering the decoder output.
*/
#define appConfigAacNoPcmLatencyBuffer()  TRUE

/*! Will give significant audio heap savings at the cost of MCPS
    Achieved by re-encoding and re-decoding the latency buffer (which buffers the local channel output of the decoder)
    LOW DSP clock is not enough to run SBC forwarding when this option is enabled
*/
#define appConfigSbcNoPcmLatencyBuffer()  FALSE

#endif /* KYMERA_CONFIG_H */
