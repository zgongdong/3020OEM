/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Public interface to shadow ACL & eSCO connections.

This module manages the shadow connections used in TWS-Shadowing devices.

It is responsible for:

* Creating and managing the shadow ACL / eSCO / A2DP connection., e.g.
    dealing with link-loss of the shadow connection(s).
* Only actively creating shadow links when a peer earbud and a handset
    are connected.
* Acting in either a Primary or Secondary role. e.g. only the Primary can
    actively connect or disconnect a shadow connection.
* Interfacing with the voice/audio sources which will create the necessary SCO or
    A2DP audio chains to render a shadowed eSCO or A2DP link.
* Notifying registered modules about the state of the shadow links.
* Taking part in the handover process by:
  * Being able to veto it.
  * Being able to switch roles from Primary -> Secondary and vice-versa.
  * Making the shadow eSCO or A2DP Sink available to the handover process so
    it can be moved into a 'real' HFP or A2DP profile instance and vice-versa.

In a shadow ACL / eSCO / A2DP connection there is always a Primary device
and a Secondary device. This module puts all the responsibility for connecting
/ disconnecting and managing the lifetime of the connection on the Primary
device.

As an example, if there is a link-loss scenario both Primary & Secondary will
receive a SDM_SHADOW_LINK_DISCONNECT_IND with a link-loss reason. But then the
behaviour is different:

Primary:
* Actively try to re-connect the shadow connection(s)

Secondary:
* Do nothing until it receives notification of a successful shadow connection.

The shadow_profile module listens to notifications from other bluetooth profile
modules in order to know when to create the shadow connection(s).

Both shadow eSCO (HFP) and A2DP connections first require a shadow ACL
connection to be created.

The shadow eSCO connection is created when the handset HFP SCO channel is
created. Usually this will be when a call is incoming or outgoing.

The shadow A2DP is created when the A2DP media starts to stream.

Dependencies
To create a shadow ACL connection there must be:
* An ACL connection to the Handset.
* An ACL connection to the peer earbud.

To create a shadow eSCO connection there must be:
* A shadow ACL connection.
* A HFP SCO connection to the Handset.

To create a shadow A2DP connection there must be:
* A shadow ACL connection
* An A2DP media connection to the Handset (the module only shadows the A2DP media
  channel L2CAP channel).

The handset must be master of the ACL connection to the primary earbud and the
primary earbud must be master of the ACL connection to the secondary earbud.
The link policy module and topology set the correct ACL connection roles.

The Bluetooth firmware automatically disconnects shadow links when the
dependencies (above) are not met, e.g. on link loss. Therefore the shadow profile
does not initiate shadow link disconnection.

*/

#ifndef SHADOW_PROFILE_H_
#define SHADOW_PROFILE_H_

#include <message.h>

#include "connection_manager.h"
#include "domain_message.h"

/*! \brief Events sent by shadow_profile to other modules. */
typedef enum
{
    /*! Module initialisation complete */
    SHADOW_PROFILE_INIT_CFM = SHADOW_PROFILE_MESSAGE_BASE,

    /*! Shadow ACL connect indication */
    SHADOW_PROFILE_CONNECT_IND,

    /*! Shadow ACL disconnect indication */
    SHADOW_PROFILE_DISCONNECT_IND,

    /*! Shadow eSCO connect indication */
    SHADOW_PROFILE_ESCO_CONNECT_IND,

    /*! Shadow eSCO disconnect indication */
    SHADOW_PROFILE_ESCO_DISCONNECT_IND,

    /*! Shadow A2DP stream is active indication */
    SHADOW_PROFILE_A2DP_STREAM_ACTIVE_IND,

    /*! Shadow A2DP stream is inactive indication */
    SHADOW_PROFILE_A2DP_STREAM_INACTIVE_IND,

} shadow_profile_msg_t;

/*! Status codes used by shadow_profile */
typedef enum
{
    shadow_profile_status_connected = 0,
    shadow_profile_status_disconnected
} shadow_profile_status_t;

/*! \brief Shadow ACL connect indication. */
typedef CON_MANAGER_TP_CONNECT_IND_T SHADOW_PROFILE_CONNECT_IND_T;

/*! \brief Shadow ACL disconnect indication. */
typedef CON_MANAGER_TP_DISCONNECT_IND_T SHADOW_PROFILE_DISCONNECT_IND_T;

/*! \brief Shadow ACL connect indication. */
typedef CON_MANAGER_TP_CONNECT_IND_T SHADOW_PROFILE_ESCO_CONNECT_IND_T;

/*! \brief Shadow ACL disconnect indication. */
typedef CON_MANAGER_TP_DISCONNECT_IND_T SHADOW_PROFILE_ESCO_DISCONNECT_IND_T;


#define ShadowProfile_IsConnected() (FALSE)

#define ShadowProfile_IsEscoActive() (FALSE)

#define ShadowProfile_ClientRegister(task) UNUSED(task)

#define ShadowProfile_ClientUnregister(task) UNUSED(task)

#define ShadowProfile_Connect() /* Nothing to do */

#define ShadowProfile_Disconnect() /* Nothing to do */

#define ShadowProfile_PeerConnected() /* Nothing to do */

#define ShadowProfile_PeerDisconnected() /* Nothing to do */

#define ShadowProfile_SetRole(primary) UNUSED(primary)

#define ShadowProfile_GetScoSink() ((Sink)NULL)


#endif /* SHADOW_PROFILE_H_ */
