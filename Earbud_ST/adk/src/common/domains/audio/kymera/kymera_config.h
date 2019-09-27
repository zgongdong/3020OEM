/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       kymera_config.h
\brief      Configuration related definitions for Kymera audio.
*/

#ifndef KYMERA_CONFIG_H
#define KYMERA_CONFIG_H

#include "scofwd_profile_config.h"

/*! \brief Fixed tone volume in dB */
#define KYMERA_CONFIG_TONE_VOLUME               (-20)

/*! \brief Fixed prompt volume in dB */
#define KYMERA_CONFIG_PROMPT_VOLUME             (-10)

/*! \brief Defining USB as downloadable capability for Aura2.1 variant */
#if defined(HAVE_STR_ROM_2_0_1)
#define DOWNLOAD_USB_AUDIO
#endif

//!@{ @name Parameters for microphone 0 - Left analog MIC */
#define appConfigMic0Bias()                     (BIAS_CONFIG_MIC_BIAS_0)
#define appConfigMic0BiasVoltage()              (3) /* 1.9v */
#define appConfigMic0PreAmp()                   (TRUE)
#define appConfigMic0Pio()                      (0x13)
#define appConfigMic0Gain()                     (0x5)
#define appConfigMic0IsDigital()                (FALSE)
#define appConfigMic0AudioInstance()            (AUDIO_INSTANCE_0)
#define appConfigMic0AudioChannel()             (AUDIO_CHANNEL_A)
//!@}

//!@{ @name Parameters for microphone 1 - Right analog MIC */
#define appConfigMic1Bias()                     (BIAS_CONFIG_MIC_BIAS_0)
#define appConfigMic1BiasVoltage()              (3) /* 1.9v */
#define appConfigMic1PreAmp()                   (TRUE)
#define appConfigMic1Pio()                      (0x16)
#define appConfigMic1Gain()                     (0x5)
#define appConfigMic1IsDigital()                (FALSE)
#define appConfigMic1AudioInstance()            (AUDIO_INSTANCE_0)
#define appConfigMic1AudioChannel()             (AUDIO_CHANNEL_B)
//!@}

#if defined(CORVUS_PG806)

//!@{ @name Parameters for microphone 2 - HBL_L_FB Analog MIC connected to digital MIC ADC */
#define appConfigMic2Bias()                     (BIAS_CONFIG_MIC_BIAS_0)
#define appConfigMic2BiasVoltage()              (3)
#define appConfigMic2PreAmp()                   (FALSE)
#define appConfigMic2Pio()                      (0)
#define appConfigMic2Gain()                     (0)
#define appConfigMic2IsDigital()                (TRUE)
#define appConfigMic2AudioInstance()            (AUDIO_INSTANCE_1)
#define appConfigMic2AudioChannel()             (AUDIO_CHANNEL_B)
//!@}

//!@{ @name Parameters microphone 3 - HBL_L_FF Analog MIC connected to digital MIC ADC */
#define appConfigMic3Bias()                     (BIAS_CONFIG_MIC_BIAS_0)
#define appConfigMic3BiasVoltage()              (3)
#define appConfigMic3PreAmp()                   (FALSE)
#define appConfigMic3Pio()                      (0)
#define appConfigMic3Gain()                     (0)
#define appConfigMic3IsDigital()                (TRUE)
#define appConfigMic3AudioInstance()            (AUDIO_INSTANCE_1)
#define appConfigMic3AudioChannel()             (AUDIO_CHANNEL_A)
//!@}

/*! @{ Which microphones to use for SCO */
    /*! microphone to use for the first SCO mic */
#define appConfigScoMic1()                      (0)
    /*! microphone to use for the second SCO mic. This should be defined as
        NO_MIC if using 1-mic CVC */
#define appConfigScoMic2()                      (NO_MIC)     /* Don't use microphone for SCO 2nd mic (CVC 1-mic) */
/*! @} */

//!@{ @name ANC configuration */
#define appConfigAncPathEnable()                (feed_forward_mode_left_only)
#define appConfigAncFeedForwardMic()            (3)         /* Use microphone 3 for ANC feed-forward */
#define appConfigAncFeedBackMic()               (NO_MIC)    /* Don't use microphone for ANC feed-back */
#define appConfigAncMicGainStepSize()           (5)
#define appConfigAncSidetoneGain()              (10)
#define appConfigAncMode()                      (anc_mode_1)
//!@}


/*! Enable ANC tuning functionality */
#define appConfigAncTuningEnabled()             (TRUE)

/*! ANC tuning monitor microphone */
#define appConfigAncTuningMonitorMic()          (2)

#else

//!@{ @name Parameters for microphone 2 */
#define appConfigMic2Bias()                     (BIAS_CONFIG_PIO)
#define appConfigMic2BiasVoltage()              (0)
#define appConfigMic2PreAmp()                   (FALSE)
#define appConfigMic2Pio()                      (4)
#define appConfigMic2Gain()                     (0)
#define appConfigMic2IsDigital()                (TRUE)
#define appConfigMic2AudioInstance()            (AUDIO_INSTANCE_1)
#define appConfigMic2AudioChannel()             (AUDIO_CHANNEL_A)
//!@}

//!@{ @name Parameters microphone 3 */
#define appConfigMic3Bias()                     (BIAS_CONFIG_PIO)
#define appConfigMic3BiasVoltage()              (0)
#define appConfigMic3PreAmp()                   (FALSE)
#define appConfigMic3Pio()                      (4)
#define appConfigMic3Gain()                     (0)
#define appConfigMic3IsDigital()                (TRUE)
#define appConfigMic3AudioInstance()            (AUDIO_INSTANCE_1)
#define appConfigMic3AudioChannel()             (AUDIO_CHANNEL_B)
//!@}

/*! @{ Which microphones to use for SCO */
    /*! microphone to use for the first SCO mic */
#define appConfigScoMic1()                      (0)          /* Use microphone 0 for SCO 1st mic */
    /*! microphone to use for the second SCO mic. This should be defined as
        NO_MIC if using 1-mic CVC */
#define appConfigScoMic2()                      (NO_MIC)     /* Don't use microphone for SCO 2nd mic (CVC 1-mic) */
/*! @} */

//!@{ @name ANC configuration */
#define appConfigAncPathEnable()                (feed_forward_mode_left_only)
#define appConfigAncFeedForwardMic()            (0)         /* Use microphone 0 for ANC feed-forward */
#define appConfigAncFeedBackMic()               (NO_MIC)    /* Don't use microphone for ANC feed-back */
#define appConfigAncMicGainStepSize()           (5)
#define appConfigAncSidetoneGain()              (10)
#define appConfigAncMode()                      (anc_mode_1)
//!@}

/*! Enable ANC tuning functionality */
#define appConfigAncTuningEnabled()             (FALSE)

/*! ANC tuning monitor microphone */
#define appConfigAncTuningMonitorMic()          (NO_MIC)

#endif

/*! Time to play to be applied on this earbud, based on the Wesco
    value specified when creating the connection.
    A value of 0 will disable TTP.  */
#define appConfigScoChainTTP(wesco)     (wesco * 0)

/*! The time before the TTP at which a packet should be transmitted */
#define appConfigTwsTimeBeforeTx()  MAX(70000, TWS_STANDARD_LATENCY_US-200000)

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

#endif /* KYMERA_CONFIG_H */
