/*
Copyright (c) 2004 - 2018 Qualcomm Technologies International, Ltd.

FILE NAME
    sink_audio_routing.h

DESCRIPTION
    Module responsible for routing audio
*/

#ifndef _SINK_AUDIO_ROUTING_H_
#define _SINK_AUDIO_ROUTING_H_

#include <sink.h>
#include "audio_sources_list.h"


/****************************************************************************
NAME
    audioSwitchToAudioSource

DESCRIPTION
	Switch audio routing to the source passed in, it may not be possible
    to actually route the audio at that point until the audio sink becomes
    available.

RETURNS
    None
*/
void audioSwitchToAudioSource(audio_source_t source);

/****************************************************************************
NAME
    audioUpdateAudioRouting

DESCRIPTION
	Handle the routing of audio sources based on current status and other
	priority features like ANC, TWS and others.

RETURNS
    None
*/
void audioUpdateAudioRouting(void);

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
void audioUpdateAudioRoutingAfterDisconnect(void);

/****************************************************************************
NAME
    audioA2dpStartStream

DESCRIPTION
    Start the A2DP Audio stream request from the application.

RETURNS
    None
*/
void audioA2dpStartStream(void);
/*************************************************************************
NAME
    processEventUsrSelectAudioSource

DESCRIPTION
    Function to handle the user's source selection. Follow up calls and
    configuration settings status will determine the outcome and proceed
    by indicating the event or not.

INPUTS
    EventUsrSelectAudioSource Source selection user events

RETURNS
	TRUE if routing was successful and VM isn't in deviceLimbo state.
	FALSE if routing was not possible or VM is in deviceLimbo state.

*/
bool processEventUsrSelectAudioSource(const MessageId EventUsrSelectAudioSource);

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
void processEventUsrWiredAudioConnected(const MessageId id);

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
void processEventWiredAudioDisconnected(const MessageId id);

/****************************************************************************
NAME
      sinkAudioInit

DESCRIPTION
    Reads the PSKEY containing the audio routing information

RETURNS
      void
*/
void sinkAudioInit( void );


/****************************************************************************
NAME
    audioRoutingProcessUpdateMsg

DESCRIPTION
    Single call which looks after the input audio source routing, triggered by
    a queued event.

RETURNS
    None.
*/
void audioRoutingProcessUpdateMsg(void);

/****************************************************************************
NAME
    audioUpdateAudioActivePio

DESCRIPTION
    Single call which looks after updating the audio active Pio considering the
    current audio routing and whether the audio is currently busy.

RETURNS
    None.
*/
void audioUpdateAudioActivePio(void);

/****************************************************************************
NAME
    enableAudioActivePio

DESCRIPTION
    Enable audio using the audio active Pio.

RETURNS
    None.
*/
void enableAudioActivePio(void);


/****************************************************************************
NAME
    disableAudioActivePio

DESCRIPTION
    Disable audio using the audio active Pio.

RETURNS
    None.
*/
void disableAudioActivePio(void);

/****************************************************************************
NAME
    disableAudioActivePioWhenAudioNotBusy

DESCRIPTION
    Disable audio using the audio amplifier pio when audio is not busy.

RETURNS
    None.
*/
void disableAudioActivePioWhenAudioNotBusy(void);

bool sinkAudioRoutingIsInputEnabled(audio_source_t id);


/****************************************************************************
NAME
    audioShutDownForUpgrade

RETURNS
    void
*/
void audioShutDownForUpgrade(Task task, MessageId id);

/****************************************************************************
NAME
    audioRouteIsScoActive

DESCRIPTION
    Checks for any sco being present

RETURNS
    true if sco routed, false if no sco routable
*/
bool audioRouteIsScoActive(void);

#endif /* _SINK_AUDIO_ROUTING_H_ */

