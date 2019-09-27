/****************************************************************************
Copyright (c) 2004 - 2018 Qualcomm Technologies International, Ltd.

FILE NAME
    sink_audio_routing.c

DESCRIPTION
    Module responsible for routing audio
*/

#include "src/apps/common/audio_routing/sink_audio_routing.h"

#include "src/domains/peripheral/src/legacy/audio_analogue_config.h"
#include "src/domains/bt/src/legacy/sink_private_data.h"
#include "src/framework/sink_main_task.h"
#include "src/domains/audio/src/legacy/audio_input_route.h"
#include "src/apps/sink/sink_init.h"
#include "src/apps/sink/sink_statemanager.h"
#include "src/domains/peripheral/src/legacy/sink_pio.h"
#include "src/domains/audio/src/legacy/sink_tones.h"
#include "src/domains/audio/src/legacy/sink_volume.h"
#include "src/domains/audio/src/legacy/sink_volume_data.h"
#include "src/domains/audio/src/legacy/sink_mute.h"
#include "src/domains/audio/src/legacy/sink_microphones.h"
#include "src/domains/audio/src/legacy/sink_microphones_config.h"

#include "sink_display.h"
#include "src/domains/audio/src/legacy/audio_gating.h"
#include "src/apps/common/audio_routing/sink_audio_routing_data.h"
#include "src/apps/common/audio_routing/sink_audio_routing_config.h"
#include "sink_audio_source_usb.h"
#include "src/domains/bt/src/legacy/sink_attributes.h"
#include "src/domains/bt/src/legacy/sink_devicemanager.h"
#include "src/apps/sink/sink_debug.h"
#include "src/domains/audio/src/legacy/sink_anc.h"
#include "src/domains/bt/src/bredr/profiles/hfp/legacy/sink_hfp_data.h"
#include "sink_usb.h"
#include "src/app_modes/tws/src/legacy/sink_peer_config.h"
#include "src/app_modes/tws/src/legacy/sink_peer_a2dp.h"
#include "src/app_modes/tws/src/legacy/sink_peer_commands.h"
#include "src/domains/bt/src/bredr/profiles/hfp/legacy/sink_callmanager.h"

#include "src/framework/sink_malloc_debug.h"
#include "src/domains/bt/src/bredr/profiles/a2dp/legacy/sink_a2dp_data.h"

#include "src/domains/bt/src/bredr/profiles/avrcp/legacy/avrcp_connection.h"
#include "src/domains/bt/src/bredr/profiles/avrcp/legacy/sink_avrcp_data.h"
#include "src/app_modes/ba/src/legacy/sink_ba.h"
#include "src/domains/power/src/legacy/sink_powermanager.h"
#include "src/domains/config/src/legacy/sink_config_store.h"
#include "src/domains/bt/src/bredr/profiles/hfp/legacy/sink_hfp_audio.h"
#include "audio_sources_audio_interface.h"
#include "src/domains/bt/src/bredr/profiles/hfp/legacy/sink_voice_source_hfp.h"
#include "src/domains/audio/src/legacy/audio_source_priority.h"
#include "audio_sources.h"
#include "telephony.h"
#include "voice_sources.h"
#include "src/applets/media_player/src/media_player.h"
#include "src/domains/bt/src/bredr/profiles/a2dp/legacy/audio_source_a2dp_util.h"

#include "voice_source_usb.h"
#include "usb_audio.h"

#include <connection.h>
#include <a2dp.h>
#include <stdlib.h>
#include <audio.h>
#include <audio_plugin_if.h>
#include <audio_config.h>
#include <sink.h>
#include <bdaddr.h>
#include <vm.h>
#include <byte_utils.h>

#ifdef DEBUG_AUDIO
#define AUD_DEBUG(x) DEBUG(x)
#else
#define AUD_DEBUG(x)
#endif

static audio_source_t getAudioSourceFromEventUsrSelectAudioSource(const MessageId EventUsrSelectAudioSource);
static audio_source_t audioGetAudioSourceToRoute(void);
static void audioRouteVoiceSource(void);
static bool handleAudioActiveDelayRequest(void);
static bool isAudioRoutingPermitted(audio_source_t source);

static void initAudioSourceRegistries(void)
{
    VoiceSources_SourceRegistryInit();
    VoiceSources_VolumeRegistryInit();
    VoiceSources_VolumeControlRegistryInit();
    VoiceSources_ObserverRegistryInit();

    AudioSources_SourceRegistryInit();
    AudioSources_MediaControlRegistryInit();
    AudioSources_VolumeControlRegistryInit();
    AudioSources_VolumeRegistryInit();
    AudioSources_ObserverRegistryInit();
}

/****************************************************************************
NAME
      sinkAudioInit

DESCRIPTION
    Reads the PSKEY containing the audio routing information

RETURNS
      void
*/
void sinkAudioInit( void )
{
    voice_mic_params_t mic_params;
    mic_params.mic_a = sinkMicGetMicParams(sinkMicrophonesGetVoiceMicASelection());
    mic_params.mic_b = sinkMicGetMicParams(sinkMicrophonesGetVoiceMicBSelection());
    sinkAudioRoutingDataInit();
    sinkAudioSetVoiceMicParams(&mic_params);
    sinkAudioSetManualSourceSelectionEnabled(sinkAudioGetManualSourceSelectionFromConfig());

    AudioConfigSetQuality(audio_stream_voice, sinkAudioGetVoiceQuality());
    AudioConfigSetQuality(audio_stream_music, sinkAudioGetMusicQuality());
    AudioConfigSetRenderingMode(sinkAudioGetAudioRenderingMode());

    initAudioSourceRegistries();
}

/****************************************************************************
NAME
    audioSwitchToAudioSource

DESCRIPTION
    Switch audio routing to the source passed in, it may not be possible
    to actually route the audio at that point if audio sink for that source
    is not available at that time.

RETURNS
    None
*/
void audioSwitchToAudioSource(audio_source_t source)
{
    if(audioSourcePriority_isAudioSourceEnabled(source))
    {
        sinkAudioSetRequestedAudioSource(source);
        audioUpdateAudioRouting();
    }
}

/****************************************************************************
NAME
    audioIsReadyForAudioRouting

DESCRIPTION
    Fundamental checks before we continue with routing an audio source.

RETURNS
    TRUE if checks passed, FALSE otherwise.
*/
static bool audioIsReadyForAudioRouting(void)
{
    /* invalidate at-limit status */
    sinkVolumeResetMaxAndMinVolumeFlag();

    /* ensure initialisation is complete before attempting to route audio */
    if(sinkInitGetSinkInitialisingStatus())
    {
        return FALSE;
    }

    return TRUE;
}

static bool audioRouteAudioFromTWSPeer(void)
{
    if(peerIsRemotePeerInCall())
    {
        if(peerIsSingleDeviceOperationEnabled())
        {
            MediaPlayerDeselectActiveSource();
        }
        return TRUE;
    }
    if(peerIsDeviceSlaveAndStreaming() && (peerIsRemotePeerInCall() == FALSE))
    {
        uint16 peer_index;
        if(a2dpGetPeerIndex(&peer_index))
        {
            return MediaPlayerSelectSource(audioSources_GetA2dpSourceFromIndex((a2dp_index_t)peer_index));
        }
    }
    return FALSE;
}

static void audioRouteAudioSource(void)
{
    if(audioRouteAudioFromTWSPeer())
    {
        AUD_DEBUG(("AUD: Routed TWS Peer\n"));
    }
    else
    {
        audio_source_t source = audioGetAudioSourceToRoute();
        if(isAudioRoutingPermitted(source))
        {
            bool routing_success = MediaPlayerSelectSource(source);
            if((routing_success == FALSE) && AudioInputRoute_IsAudioRouted())
            {
                MediaPlayerDeselectActiveSource();
            }
        }
    }
}

void audioUpdateAudioActivePio(void)
{
    if(AudioInputRoute_IsAudioRouted())
    {
        enableAudioActivePio();
    }
    else if(!IsAudioBusy())
    {
        disableAudioActivePio();
    }
    else
    {
        // nothing to do
    }
}

void enableAudioActivePio(void)
{
    PioDrivePio(PIO_AUDIO_ACTIVE, TRUE);
}

void disableAudioActivePio(void)
{
    PioDrivePio(PIO_AUDIO_ACTIVE, FALSE);
}

void disableAudioActivePioWhenAudioNotBusy(void)
{
    MessageSendConditionallyOnTask(&theSink.task, EventSysCheckAudioAmpDrive, 0, AudioBusyPtr());
}

/****************************************************************************
NAME
    handleAudioActiveDelayRequest

DESCRIPTION
    Handle an audio active delay request.
    A delay in activating the audio may be configured for a TWS slave to reduce audio pops when
     an AG starts A2DP streaming for TWS audio.

RETURNS
    Returns TRUE when message 'EventSysEnableAudioActive' was sent to delay audio enable for a TWS slave.
    Returns FALSE when no audio enable delay request was sent.
*/
static bool handleAudioActiveDelayRequest(void)
{
    bool message_sent = FALSE ;

    if (deviceManagerIsPeerConnected() && AudioInputRoute_IsAudioRouted() &&
        !sinkAudioGetSilenceThreshold() &&
        !((stateManagerGetState() >= deviceOutgoingCallEstablish) && (stateManagerGetState() <= deviceActiveCallNoSCO)))
    {
        uint16 delay = peerGetTwsAudioActiveDelay() ;

        if (delay > 0 )
        {
            sinkCancelAndSendLater(EventSysEnableAudioActive, (uint32)delay);
            message_sent = TRUE ;
        }
    }

    if (!message_sent)
    {
        audioUpdateAudioActivePio();
    }

    return message_sent ;
}

static void updateActiveAvrcpInstanceBasedOnRequestedSource(void)
{
    if(sinkAudioGetManualSourceSelectionEnabled() && AudioInputRoute_IsAudioRouted() == FALSE
                && audioSources_IsSourceA2dp(sinkAudioGetRequestedAudioSource()))
    {
        a2dp_index_t index = audioSources_GetA2dpIndexFromSource(sinkAudioGetRequestedAudioSource());
        sinkAvrcpSetActiveConnectionFromBdaddr(getA2dpLinkBdAddr(index));
    }
}

static void audioPostRoutingAudioConfiguration(void)
{
    updateActiveAvrcpInstanceBasedOnRequestedSource();

    /* Make sure soft mute is correct */
    VolumeApplySoftMuteStates();

    peerSourceStream();

    if(AudioInputRoute_IsAudioRouted())
    {
        peerSyncVolumeIfMaster();
    }

    handleAudioActiveDelayRequest();

    baAudioPostRoutingAudioConfiguration();
}

static bool activeFeaturesOverrideRouting(void)
{
    return (sinkAncIsTuningModeActive());
}

static bool voiceSourceRequiresCall(voice_source_t source)
{
    bool require_call = FALSE;
    
    if ((source == voice_source_hfp_1) || (source == voice_source_hfp_2))
    {
        require_call = TRUE;
    }
    
    return require_call;
}

/****************************************************************************/
static voice_source_t audioGetVoiceSourceToRoute(void)
{
    voice_source_t source = voice_source_hfp_1;

    while((source != voice_source_none) && (voiceSources_IsRoutable(source, voiceSourceRequiresCall(source)) == FALSE))
    {
        switch (source)
        {
            case voice_source_hfp_1:
                source = voice_source_hfp_2;
                break;
            case voice_source_hfp_2:
                source = voice_source_usb;
                break;
            case voice_source_usb:
                source = voice_source_none;
                break;
            default:
                source = voice_source_none;
                break;
        }
    }

    AUD_DEBUG(("Voice source to route is %d\n", source));
    return source;
}

static void audioRouteVoiceSource(void)
{
    voice_source_t source = audioGetVoiceSourceToRoute();
    bool routing_success = TelephonyRouteSource(source, voiceSourceRequiresCall(source));

    if(!routing_success && AudioInputRoute_IsVoiceRouted())
    {
        Telephony_UnRouteCurrentSource();
        AUD_DEBUG(("AUD: Routed Voice Source = %d\n", (HfpLinkPriorityFromAudioSink(AudioInputRoute_GetRoutedVoiceSink())-1)));
    }
}

/****************************************************************************
NAME
    audioUpdateAudioRouting

DESCRIPTION
    Handle the routing of audio sources based on current status and other
    priority features like ANC tuning, TWS and others.

RETURNS
    None
*/
void audioUpdateAudioRouting(void)
{
    AUD_DEBUG(("AUD: Deliver EventSysCheckAudioRouting\n"));

    if(!audioIsReadyForAudioRouting())
    {
        AUD_DEBUG(("AUD: Not ready for routing\n"));
        return;
    }

    if(activeFeaturesOverrideRouting())
    {
        MediaPlayerDeselectActiveSource();
        Telephony_UnRouteCurrentSource();
        AUD_DEBUG(("AUD: Disconnected due to active features\n"));
        return;
    }

    audioRouteVoiceSource();
    audioRouteAudioSource();

    audioPostRoutingAudioConfiguration();
}

/****************************************************************************
NAME
    audioRoutingProcessUpdateMsg

DESCRIPTION
    Single call which looks after the input audio source routing, triggered by
    a queued event.

RETURNS
    None.
*/
void audioRoutingProcessUpdateMsg(void)
{
    audioUpdateAudioRouting();
}

/****************************************************************************
NAME
    audioUpdateAudioRoutingAfterDisconnect

DESCRIPTION
    Handle the routing of audio sources based on current status and other
    priority features after certain delay (in ms).  The delay is configurable
    by the user through sink configurator tool

RETURNS
    None
 */
void audioUpdateAudioRoutingAfterDisconnect(void)
{
    sinkCancelAndSendLater(EventSysCheckAudioRouting, (uint32)sinkAudioGetSourceDisconnectDelay());
}

static bool isAudioRoutingBlockedByCallIndication(void)
{
    AUD_DEBUG(("isAudioRoutingBlockedByCallIndication = %d\n",
                (sinkCallManagerIsCallIndicationActive() && !sinkAudioGetMixingOfVoiceAndAudioEnabled())));

    return (sinkCallManagerIsCallIndicationActive() && !sinkAudioGetMixingOfVoiceAndAudioEnabled());
}

static bool isAudioRoutingBlockedByRoutedVoice(audio_source_t source)
{
    UNUSED(source);
    return (AudioInputRoute_IsVoiceRouted());
}

static bool isAudioRoutingPermitted(audio_source_t source)
{
    return (((isAudioRoutingBlockedByRoutedVoice(source)
            || isAudioRoutingBlockedByCallIndication()) == FALSE) &&(audio_source_none != source));
}

static bool a2dpMediaStartIndex(a2dp_index_t index)
{
    if(sinkA2dpGetStreamState(index) == a2dp_stream_open
                    && sinkA2dpIsA2dpLinkSuspended(index) == FALSE
                    && sinkA2dpGetRoleType(index) == a2dp_sink)
    {
        if(A2dpSignallingGetState(getA2dpLinkDataDeviceId(index)) == a2dp_signalling_connected)
        {
            A2dpMediaStartRequest(getA2dpLinkDataDeviceId(index), getA2dpLinkDataStreamId(index));
        }
        return TRUE;
    }
    return FALSE;
}

/****************************************************************************/
void audioA2dpStartStream(void)
{
    if (sinkA2dpIsStreamingAllowed())
    {
        if(!a2dpMediaStartIndex(a2dp_primary))
        {
            a2dpMediaStartIndex(a2dp_secondary);
        }
    }
}

/*************************************************************************
NAME
    getAudioSourceFromEventUsrSelectAudioSource

DESCRIPTION
    Maps a user event to the relevant audio source.

INPUTS
    EventUsrSelectAudioSource  User event related to audio sources.

RETURNS
    The matching audio source.

*/
static audio_source_t getAudioSourceFromEventUsrSelectAudioSource(const MessageId EventUsrSelectAudioSource)
{
    audio_source_t audio_source = audio_source_none;

    switch(EventUsrSelectAudioSource)
    {
        case EventUsrSelectAudioSourceAnalog:
            audio_source = audio_source_ANALOG;
            break;

        case EventUsrSelectAudioSourceSpdif:
            audio_source = audio_source_SPDIF;
            break;

        case EventUsrSelectAudioSourceI2S:
            audio_source = audio_source_I2S;
            break;

        case EventUsrSelectAudioSourceUSB:
            audio_source = audio_source_USB;
            break;

        case EventUsrSelectAudioSourceA2DP1:
            audio_source = audio_source_a2dp_1;
            break;

        case EventUsrSelectAudioSourceA2DP2:
            audio_source = audio_source_a2dp_2;
            break;

        case EventUsrSelectAudioSourceNextRoutable:
            audio_source = audioSourcePriority_getNextRoutable(sinkAudioGetRequestedAudioSource());
            break;

        case EventUsrSelectAudioSourceNext:
            audio_source = audioSourcePriority_getNext(sinkAudioGetRequestedAudioSource());
            break;

        default:
            break;
    }

    if((audio_source == audio_source_none) && !audioSourcePriority_isAudioSourceEnabled(audio_source))
    {
        audio_source = AudioSources_GetRoutedAudioSource();
    }

    return audio_source;
}

/*************************************************************************
NAME
    processEventUsrSelectAudioSource

DESCRIPTION
    Function to handle the user's source selection. Follow up calls and
    configuration settings status will determine the outcome and proceed
    by indicating the event or not.

INPUTS
    EventUsrSelectAudioSource  Source selection user events.

RETURNS
    TRUE if routing was successful and VM isn't in deviceLimbo state.
    FALSE if routing was not possible or VM is in deviceLimbo state.

*/
bool processEventUsrSelectAudioSource(const MessageId EventUsrSelectAudioSource)
{
    bool result = TRUE;

    /* No need to attempt routing a source while in deviceLimbo state. */
    if(stateManagerGetState() == deviceLimbo)
    {
        result = FALSE;
    }
    else
    {
        if(sinkAudioGetManualSourceSelectionEnabled())
        {
            sinkAudioSetRequestedAudioSource(getAudioSourceFromEventUsrSelectAudioSource(EventUsrSelectAudioSource));

            result = audioSources_IsRoutable(sinkAudioGetRequestedAudioSource());

            peerUpdateRelaySource(sinkAudioGetRequestedAudioSource());
            audioUpdateAudioRouting();

            if(result)
            {
                sinkVolumeResetVolumeAndSourceSaveTimeout();
            }
        }
        else
        {
            result = FALSE;
        }
    }

    return result;
}

/*************************************************************************
NAME
    processEventUsrWiredAudioConnected

DESCRIPTION
    Function to handle the wired audio connection upon user events

INPUTS
    Source selection user events (i.e. EventUsrAnalogAudioConnected).

RETURNS
      void
*/
void processEventUsrWiredAudioConnected(const MessageId id)
{
    switch (id)
    {
        case EventUsrAnalogAudioConnected:
            /* Cancel any disconnect debounce timer */
            MessageCancelAll(&theSink.task, EventSysAnalogueAudioDisconnectTimeout);

            /* Start the limbo timer here to turn off the device if this feature is enabled */
            if(SinkWiredIsPowerOffOnAnalogueAudioConnected())
            {
                /* cancel existing limbo timeout and reschedule another limbo timeout */
                sinkCancelAndSendLater(EventSysLimboTimeout, D_SEC(SinkWiredGetAudioConnectedPowerOffTimeout()));
            }
            else
            {
                peerNotifyWiredSourceConnected(ANALOG_AUDIO);
                audioUpdateAudioRouting();
            }
            break;

        case EventUsrSpdifAudioConnected:
            audioUpdateAudioRouting();
        break;
    }
}

/*************************************************************************
NAME
    processEventWiredAudioDisconnected

DESCRIPTION
    Function to handle the wired audio disconnection upon events

INPUTS
    Event ID (e.g. EventUsrAnalogAudioDisconnected).

RETURNS
      void

*/
void processEventWiredAudioDisconnected(const MessageId id)
{
    switch (id)
    {
        case EventUsrAnalogAudioDisconnected:
            sinkCancelAndSendLater(
                    EventSysAnalogueAudioDisconnectTimeout,
                    SinkWiredGetAnalogueDisconnectDebouncePeriod());
        break;

        case EventSysAnalogueAudioDisconnectTimeout:
            peerNotifyWiredSourceDisconnected(ANALOG_AUDIO);
            audioUpdateAudioRoutingAfterDisconnect();
        break;

        case EventUsrSpdifAudioDisconnected:
            audioUpdateAudioRoutingAfterDisconnect();
        break;
    }
}


/*************************************************************************
NAME
    audioGetAudioSourceToRoute

DESCRIPTION
    Final call of routing an audio source.

INPUTS
    None

RETURNS
    Audio source to route.

*/
static audio_source_t audioGetAudioSourceToRoute(void)
{
    audio_source_t source_to_route = sinkAudioGetRequestedAudioSource();

    if(!sinkAudioGetManualSourceSelectionEnabled())
    {
        source_to_route = audioSourcePriority_getHighestRoutable();
    }

    AUD_DEBUG(("audioGetAudioSourceToRoute()  0x%X\n", source_to_route));
    return source_to_route;
}


bool sinkAudioRoutingIsInputEnabled(audio_source_t id)
{
    /* Returns true if the source in on the priority list */
    return audioSourcePriority_isAudioSourceEnabled(id);
}

/****************************************************************************
NAME
    audioShutDownForUpgrade

DESCRIPTION
    Disconnects any routed voice/audio sources and once the audio busy
    pointer is cleared, messages sink_upgrade to proceed

RETURNS
    void
*/
void audioShutDownForUpgrade(Task task, MessageId id)
{
    Telephony_UnRouteCurrentSource();
    MediaPlayerDeselectActiveSource();

    MessageSendConditionallyOnTask(task, id, NULL, AudioBusyPtr());
}

/****************************************************************************
NAME
    audioRouteIsScoActive

DESCRIPTION
    Checks for any sco being present

RETURNS
    true if sco routed, false if no sco routable
*/
bool audioRouteIsScoActive(void)
{
    return (bool)(sinkCallManagerGetActiveScoSink());
}
