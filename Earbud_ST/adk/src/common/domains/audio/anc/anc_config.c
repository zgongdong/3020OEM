/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       anc_config.c
\brief      Configuration for microphones and stubs for Active Noise Cancellation (ANC).
            Static conifguration for ANC FF, FB, HY modes.
*/


#ifdef ENABLE_ANC
#include <logging.h>
#include "anc_config.h"
#include "kymera_config.h"

/*
 * There is no config manager setup yet, so hard-code the default value as Feed Forward Mode on Analog Mic from 
 * Kymera_config for reference.
 */

#define getFeedForwardLeftMicConfig() ((appConfigAncFeedForwardMic() == NO_MIC) ? disabled : appConfigAncFeedForwardMic() + 1)
#define getFeedBackLeftMicConfig() ((appConfigAncFeedBackMic() == NO_MIC) ? disabled : appConfigAncFeedBackMic() + 1)


anc_readonly_config_def_t anc_readonly_config =
    {
        {
            .feed_forward_left_mic = getFeedForwardLeftMicConfig(),
            .feed_forward_right_mic = disabled,
            .feed_back_left_mic = getFeedBackLeftMicConfig(),
            .feed_back_right_mic = disabled,
         },       
         .num_anc_modes = 10,
    };

/* Write to persistance is not enabled for now and set to defaults */
static anc_writeable_config_def_t anc_writeable_config = {0x0};

/* Initialize to default analogue mic0 values */
static audio_mic_params mic_params = 
    {
       .bias_config = BIAS_CONFIG_MIC_BIAS_0,
       .pio = 0,
       .gain = 5,
       .is_digital = 0,
       .instance = 0,
       .channel = 0
     };

uint16 ancConfigManagerGetReadOnlyConfig(uint16 config_id, const void **data)
{
    UNUSED(config_id);
    *data = &anc_readonly_config;
    DEBUG_LOGF("ancConfigManagerGetReadOnlyConfig\n");
    return (uint16) sizeof(anc_readonly_config);
}

void ancConfigManagerReleaseConfig(uint16 config_id)
{
    UNUSED(config_id);
    DEBUG_LOGF("ancConfigManagerReleaseConfig\n");
}

uint16 ancConfigManagerGetWriteableConfig(uint16 config_id, void **data, uint16 size)
{
    UNUSED(config_id);
    UNUSED(size);
    *data = &anc_writeable_config;
    DEBUG_LOGF("ancConfigManagerGetWriteableConfig\n");
    return (uint16) sizeof(anc_writeable_config);
}

void ancConfigManagerUpdateWriteableConfig(uint16 config_id)
{
    UNUSED(config_id);
    DEBUG_LOGF("ancConfigManagerUpdateWriteableConfig\n");
}

/* Default parameters for microphone 1 - Left analog MIC */
audio_mic_params ancAudioGetMic1Params(void)
{
    mic_params.bias_config = appConfigMic0Bias();
    mic_params.pio = appConfigMic0Pio();
    mic_params.gain = appConfigMic0Gain();
    mic_params.is_digital = appConfigMic0IsDigital();
    mic_params.instance = appConfigMic0AudioInstance();
    mic_params.channel = appConfigMic0AudioChannel();
  
    DEBUG_LOGF("ancAudioGetMic1Params\n");
    return mic_params;
}

/* Default parameters for microphone 2 - Right analog MIC */
audio_mic_params ancAudioGetMic2Params(void)
{
    mic_params.bias_config = appConfigMic1Bias();
    mic_params.pio = appConfigMic1Pio();
    mic_params.gain = appConfigMic1Gain();
    mic_params.is_digital = appConfigMic1IsDigital();
    mic_params.instance = appConfigMic1AudioInstance();
    mic_params.channel = appConfigMic1AudioChannel();
    
    DEBUG_LOGF("ancAudioGetMic2Params\n");
    return mic_params;
}

/* Parameters for microphone 3 */
audio_mic_params ancAudioGetMic3Params(void)
{
    mic_params.bias_config = appConfigMic2Bias();
    mic_params.pio = appConfigMic2Pio();
    mic_params.gain = appConfigMic2Gain();
    mic_params.is_digital = appConfigMic2IsDigital();
    mic_params.instance = appConfigMic2AudioInstance();
    mic_params.channel = appConfigMic2AudioChannel();
    
    DEBUG_LOGF("ancAudioGetMic3Params\n");
    return mic_params;
}

/* Parameters for microphone 4 */
audio_mic_params ancAudioGetMic4Params(void)
{
    mic_params.bias_config = appConfigMic3Bias();
    mic_params.pio = appConfigMic3Pio();
    mic_params.gain = appConfigMic3Gain();
    mic_params.is_digital = appConfigMic3IsDigital();
    mic_params.instance = appConfigMic3AudioInstance();
    mic_params.channel = appConfigMic3AudioChannel();
    
    DEBUG_LOGF("ancAudioGetMic4Params\n");
    return mic_params;
}

/* Mic5 is not used on Earbud app */
audio_mic_params ancAudioGetMic5Params(void)
{
    mic_params.bias_config = BIAS_CONFIG_MIC_BIAS_1;
    mic_params.pio = 0;
    mic_params.gain = 5;
    mic_params.is_digital = 0;
    mic_params.instance = 1;
    mic_params.channel = 0;
    
    DEBUG_LOGF("ancAudioGetMic5Params\n");
    return mic_params;
}

/* Mic6 is not used on Earbud app */
audio_mic_params ancAudioGetMic6Params(void)
{
    mic_params.bias_config = BIAS_CONFIG_MIC_BIAS_1;
    mic_params.pio = 0;
    mic_params.gain = 5;
    mic_params.is_digital = 0;
    mic_params.instance = 1;
    mic_params.channel = 1;
    
    DEBUG_LOGF("ancAudioGetMic6Params\n");
    return mic_params;
}

#ifdef ANC_PEER_SUPPORT
/* To be cleaned up once peer handling is done */
bool ancPeerProcessEvent(MessageId id)
{
      UNUSED(id);
      return TRUE;
}

bool ancPeerIsLinkMaster(void)
{      
      return TRUE;
}

void ancPeerSendAncState(void)
{

}

void ancPeerSendAncMode(void)
{

}
#endif /* ANC_PEER_SUPPORT */

#endif /* ENABLE_ANC */
