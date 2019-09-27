/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Interface to shadow ACL & eSCO connections.
*/

#include <stdlib.h>

#include <vm.h>
#include <message.h>

#include "av.h"
#include "connection_manager.h"
#include "hfp_profile.h"
#include "kymera.h"
#include "kymera_adaptation.h"
#include "peer_signalling.h"
#include "volume_messages.h"
#include "telephony_messages.h"
#include "a2dp_profile_sync.h"
#include "a2dp_profile_audio.h"
#include "audio_sync.h"

#ifdef INCLUDE_SHADOWING

#include "shadow_profile.h"
#include "shadow_profile_signalling.h"
#include "shadow_profile_typedef.h"
#include "shadow_profile_marshal_typedef.h"
#include "shadow_profile_private.h"
#include "shadow_profile_sdm_prim.h"
#include "shadow_profile_sm.h"
#include "shadow_profile_audio_source.h"

shadow_profile_task_data_t shadow_profile;

/*
    Helper functions
*/

/*! \brief Reset shadow SCO connection state */
void ShadowProfile_ResetEscoConnectionState(void)
{
    shadow_profile_task_data_t *sp = ShadowProfile_Get();
    sp->esco.conn_handle = SHADOW_PROFILE_CONNECTION_HANDLE_INVALID;
    sp->esco.sink = (Sink)0;
    sp->esco.codec_mode = hfp_codec_mode_none;
    sp->esco.wesco = 0;
    sp->esco.volume = appHfpGetVolume();
}

/* \brief Set the local SCO audio volume */
void ShadowProfile_SetScoVolume(uint8 volume)
{
    shadow_profile_task_data_t *sp = ShadowProfile_Get();

    SHADOW_LOGF("shadowProfile_SetLocalVolume vol %u old_vol %u", volume, sp->esco.volume);

    assert(!ShadowProfile_IsPrimary());

    if (volume != sp->esco.volume)
    {
        sp->esco.volume = volume;
        Volume_SendVoiceSourceVolumeUpdateRequest(voice_source_hfp_1, event_origin_peer, sp->esco.volume);
    }
}

/*\! brief Set the local SCO codec params */
void ShadowProfile_SetScoCodec(hfp_codec_mode_t codec_mode)
{
    shadow_profile_task_data_t *sp = ShadowProfile_Get();

    SHADOW_LOGF("ShadowProfile_SetScoCodec codec_mode 0x%x", codec_mode);

    /* \todo Store the params as hfp params? That may actually be the best way w.r.t.
             handover as well? */
    sp->esco.codec_mode = codec_mode;
}

/*
    External notification helpers
*/

void ShadowProfile_SendAclConnectInd(void)
{
    shadow_profile_task_data_t *sp = ShadowProfile_Get();
    MESSAGE_MAKE(ind, SHADOW_PROFILE_CONNECT_IND_T);
    BdaddrTpFromBredrBdaddr(&ind->tpaddr, &sp->acl.bd_addr);
    TaskList_MessageSend(sp->client_tasks, SHADOW_PROFILE_CONNECT_IND, ind);
}

void ShadowProfile_SendAclDisconnectInd(void)
{
    shadow_profile_task_data_t *sp = ShadowProfile_Get();
    MESSAGE_MAKE(ind, SHADOW_PROFILE_DISCONNECT_IND_T);
    BdaddrTpFromBredrBdaddr(&ind->tpaddr, &sp->acl.bd_addr);
    /* \todo propagate disconnect reason */
    ind->reason = hci_error_unspecified;
    TaskList_MessageSend(sp->client_tasks, SHADOW_PROFILE_DISCONNECT_IND, ind);
}

void ShadowProfile_SendScoConnectInd(void)
{
    shadow_profile_task_data_t *sp = ShadowProfile_Get();
    MESSAGE_MAKE(ind, SHADOW_PROFILE_ESCO_CONNECT_IND_T);
    BdaddrTpFromBredrBdaddr(&ind->tpaddr, &sp->acl.bd_addr);
    TaskList_MessageSend(sp->client_tasks, SHADOW_PROFILE_ESCO_CONNECT_IND, ind);
}

void ShadowProfile_SendScoDisconnectInd(void)
{
    shadow_profile_task_data_t *sp = ShadowProfile_Get();
    MESSAGE_MAKE(ind, SHADOW_PROFILE_ESCO_DISCONNECT_IND_T);
    BdaddrTpFromBredrBdaddr(&ind->tpaddr, &sp->acl.bd_addr);
    /* \todo propagate disconnect reason */
    ind->reason = hci_error_unspecified;
    TaskList_MessageSend(sp->client_tasks, SHADOW_PROFILE_ESCO_DISCONNECT_IND, ind);
}

void ShadowProfile_SendA2dpStreamActiveInd(void)
{
    shadow_profile_task_data_t *sp = ShadowProfile_Get();
    TaskList_MessageSendId(sp->client_tasks, SHADOW_PROFILE_A2DP_STREAM_ACTIVE_IND);
}

void ShadowProfile_SendA2dpStreamInactiveInd(void)
{
    shadow_profile_task_data_t *sp = ShadowProfile_Get();
    TaskList_MessageSendId(sp->client_tasks, SHADOW_PROFILE_A2DP_STREAM_INACTIVE_IND);
}

/*
    Message handling functions
*/

/*! \brief Handle CON_MANAGER_CONNECTION_IND

    Both Primary & Secondary may receive this as it could be either
    a peer earbud or a handset connecting.
*/
static void shadowProfile_HandleConManagerConnectionInd(const CON_MANAGER_CONNECTION_IND_T *ind)
{
    UNUSED(ind);
}

/*! \brief Inspect profile and internal state and decide the target state. */
static void ShadowProfile_SetTargetStateFromProfileState(void)
{
    shadow_profile_state_t target = SHADOW_PROFILE_STATE_DISCONNECTED;

    if (appPeerSigIsConnected())
    {
        target = SHADOW_PROFILE_STATE_PEER_CONNECTED;

        if (appDeviceIsHandsetAnyProfileConnected())
        {
            target = SHADOW_PROFILE_STATE_ACL_CONNECTED;

            /* SCO has higher priority than A2DP */
            if (appDeviceIsHandsetScoActive())
            {
                target = SHADOW_PROFILE_STATE_ESCO_CONNECTED;
            }
            else if (ShadowProfile_GetA2dpState()->state == AUDIO_SYNC_STATE_ACTIVE)
            {
                target = SHADOW_PROFILE_STATE_A2DP_CONNECTED;
            }
        }
    }
    ShadowProfile_SetTargetState(target);
}

/*!  \brief Handle an APP_HFP_CONNECTED_IND.

    Only Primary should receive this, because the Handset must always
    be connected to the Primary.
*/
static void shadowProfile_HandleAppHfpConnectedInd(const APP_HFP_CONNECTED_IND_T *ind)
{
    SHADOW_LOGF("shadowProfile_HandleAppHfpConnectedInd state 0x%x handset %u",
                ShadowProfile_GetState(), appDeviceIsHandset(&ind->bd_addr));

    assert(ShadowProfile_IsPrimary());

    ShadowProfile_SetTargetStateFromProfileState();
}

/*! \brief Handle APP_HFP_DISCONNECTED_IND

    Only Primary should receive this, because the Handset must always
    be connected to the Primary.
*/
static void shadowProfile_HandleAppHfpDisconnectedInd(const APP_HFP_DISCONNECTED_IND_T *ind)
{
    UNUSED(ind);
    ShadowProfile_SetTargetStateFromProfileState();
}

/*! \brief Handle AV_A2DP_CONNECTED_IND

    Only Primary should receive this, because the Handset must always
    be connected to the Primary.
*/
static void shadowProfile_HandleAvA2dpConnectedInd(const AV_A2DP_CONNECTED_IND_T *ind)
{
    SHADOW_LOGF("shadowProfile_HandleAvA2dpConnectedInd state 0x%x", ShadowProfile_GetState());

    assert(ShadowProfile_IsPrimary());

    appA2dpSyncRegister(ind->av_instance, ShadowProfile_GetSyncIf());

    /* Target state is updated on AUDIO_SYNC_STATE_IND */
}

/*! \brief Handle APP_HFP_VOLUME_IND

    Only Primary should receive this, because the Handset HFP must always
    be connected to the Primary.
*/
static void shadowProfile_HandleAppHfpVolumeInd(const APP_HFP_VOLUME_IND_T *ind)
{
    assert(ShadowProfile_IsPrimary());

    if (ShadowProfile_IsEscoConnected())
    {
        SHADOW_LOGF("shadowProfile_HandleAppHfpVolumeInd volume %u", ind->volume);

        ShadowProfile_SendHfpVolumeToSecondary(ind->volume);
    }
    else
    {
        SHADOW_LOG("shadowProfile_HandleAppHfpVolumeInd. Not handled. SCO not connected");
    }
}

/*! \brief Handle TELEPHONY_INCOMING_CALL

    Happens when a call is incoming, but before the SCO channel has been
    created. Use it as a prompt to create the shadow ACL connection.

    Only Primary should receive this, because the Handset must always
    be connected to the Primary.
*/
static void shadowProfile_HandleTelephonyIncomingCall(void)
{
}

/*! \brief Handle TELEPHONY_CALL_ENDED

    Happens when a call is ending, after the SCO channel has disconnected.
    Use it as a prompt to disconnect the shadow ACL connection.

    Only Primary should receive this, because the Handset must always
    be connected to the Primary.
*/
static void shadowProfile_HandleTelephonyCallEnded(void)
{
}

/*! \brief Handle APP_HFP_SCO_CONNECTED_IND

    Only Primary should receive this, because the Handset must always
    be connected to the Primary.
*/
static void shadowProfile_HandleAppHfpScoConnectedInd(void)
{
    SHADOW_LOG("shadowProfile_HandleAppHfpScoConnectedInd");

    assert(ShadowProfile_IsPrimary());

    ShadowProfile_SetTargetStateFromProfileState();
}

/*! \brief Handle APP_HFP_SCO_DISCONNECTED_IND

    Only Primary should receive this, because the Handset must always
    be connected to the Primary.
*/
static void shadowProfile_HandleAppHfpScoDisconnectedInd(void)
{
    SHADOW_LOG("shadowProfile_HandleAppHfpScoDisconnectedInd");

    assert(ShadowProfile_IsPrimary());

    ShadowProfile_SetTargetStateFromProfileState();
}

/*! \brief Handle PEER_SIG_CONNECTION_IND

    Both Primary and Secondary will receive this when the peer signalling
    channel is connected and disconnected.
*/
static void shadowProfile_HandlePeerSignallingConnectionInd(const PEER_SIG_CONNECTION_IND_T *ind)
{
    UNUSED(ind);

    if (ShadowProfile_IsPrimary())
    {
        ShadowProfile_SetTargetStateFromProfileState();
    }
}

/*! \brief Handle AV_A2DP_DISCONNECTED_IND_T
    \param ind The message.
*/
static void shadowProfile_HandleAvA2dpDisconnectedInd(const AV_A2DP_DISCONNECTED_IND_T *ind)
{
    appA2dpSyncUnregister(ind->av_instance, ShadowProfile_GetSyncIf());
    ShadowProfile_SetTargetStateFromProfileState();
}

/*! \brief Handle AV_AVRCP_CONNECTED_IND */
static void shadowProfile_HandleAvAvrcpConnectedInd(void)
{
    ShadowProfile_SetTargetStateFromProfileState();
}

/*! \brief Handle AV_AVRCP_DISCONNECTED_IND */
static void shadowProfile_HandleAvAvrcpDisconnectedInd(void)
{
    ShadowProfile_SetTargetStateFromProfileState();
}

/*! \brief Handle AUDIO_SYNC_CONNECT_IND_T
    \param ind The message.
*/
static void ShadowProfile_HandleAudioSyncConnectInd(const AUDIO_SYNC_CONNECT_IND_T *ind)
{
    ShadowProfile_StoreAudioSourceParameters(ind->source_id);
    ShadowProfile_SendA2dpStreamContextToSecondary();

    MESSAGE_MAKE(rsp, AUDIO_SYNC_CONNECT_RES_T);
    rsp->sync_id = ind->sync_id;
    MessageSend(ind->task, AUDIO_SYNC_CONNECT_RES, rsp);

    ShadowProfile_GetA2dpState()->state = AUDIO_SYNC_STATE_CONNECTED;
    ShadowProfile_SetTargetStateFromProfileState();
}

/*! \brief Handle AUDIO_SYNC_ACTIVATE_IND_T
    \param ind The message.
*/
static void ShadowProfile_HandleAudioSyncActivateInd(const AUDIO_SYNC_ACTIVATE_IND_T *ind)
{
    ShadowProfile_StoreAudioSourceParameters(ind->source_id);

    ShadowProfile_GetA2dpState()->state = AUDIO_SYNC_STATE_ACTIVE;
    ShadowProfile_SetTargetStateFromProfileState();

    {
        MESSAGE_MAKE(rsp, AUDIO_SYNC_ACTIVATE_RES_T);
        rsp->sync_id = ind->sync_id;
        MessageSendConditionally(ind->task, AUDIO_SYNC_ACTIVATE_RES, rsp,
                                 ShadowProfile_GetA2dpStartLockAddr());
    }
}

/*! \brief Handle AUDIO_SYNC_STATE_IND_T
    \param ind The message.

    The only state of interest here is disconnected, since other states are
    indicated in other sync messages.
*/
static void ShadowProfile_HandleAudioSyncStateInd(const AUDIO_SYNC_STATE_IND_T *ind)
{
    SHADOW_LOGF("ShadowProfile_HandleAudioSyncStateInd state:%u", ind->state);

    switch (ind->state)
    {
        case AUDIO_SYNC_STATE_DISCONNECTED:
            ShadowProfile_ClearAudioSourceParameters(ind->source_id);
            ShadowProfile_SendA2dpStreamContextToSecondary();
        break;
        case AUDIO_SYNC_STATE_CONNECTED:
        break;
        case AUDIO_SYNC_STATE_ACTIVE:
        break;
    }

    ShadowProfile_GetA2dpState()->state = ind->state;
    ShadowProfile_SetTargetStateFromProfileState();
}

/*! \brief Handle AUDIO_SYNC_CODEC_RECONFIGURED_IND_T
    \param ind The message.
*/
static void ShadowProfile_HandleAudioSyncReconfiguredInd(const AUDIO_SYNC_CODEC_RECONFIGURED_IND_T *ind)
{
    ShadowProfile_StoreAudioSourceParameters(ind->source_id);
    ShadowProfile_SendA2dpStreamContextToSecondary();
}

static void shadowProfile_MessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch (id)
    {
    /* Notifications from other bt domain modules */
    case CON_MANAGER_CONNECTION_IND:
        shadowProfile_HandleConManagerConnectionInd((const CON_MANAGER_CONNECTION_IND_T *)message);
        break;

    case APP_HFP_CONNECTED_IND:
        shadowProfile_HandleAppHfpConnectedInd((const APP_HFP_CONNECTED_IND_T *)message);
        break;

    case APP_HFP_DISCONNECTED_IND:
        shadowProfile_HandleAppHfpDisconnectedInd((const APP_HFP_DISCONNECTED_IND_T *)message);
        break;

    case APP_HFP_SCO_INCOMING_RING_IND:
        /* \todo Use this as a trigger to send a ring command to Secondary */
        break;

    case APP_HFP_SCO_INCOMING_ENDED_IND:
        /* \todo Use this as a trigger to send a stop ring command to Secondary */
        break;

    case APP_HFP_VOLUME_IND:
        shadowProfile_HandleAppHfpVolumeInd((const APP_HFP_VOLUME_IND_T *)message);
        break;

    case APP_HFP_SCO_CONNECTED_IND:
        shadowProfile_HandleAppHfpScoConnectedInd();
        break;

    case APP_HFP_SCO_DISCONNECTED_IND:
        shadowProfile_HandleAppHfpScoDisconnectedInd();
        break;

    case AV_A2DP_CONNECTED_IND:
        shadowProfile_HandleAvA2dpConnectedInd((const AV_A2DP_CONNECTED_IND_T *)message);
        break;

    case AV_A2DP_DISCONNECTED_IND:
        shadowProfile_HandleAvA2dpDisconnectedInd((const AV_A2DP_DISCONNECTED_IND_T *)message);
        break;

    case AV_AVRCP_CONNECTED_IND:
        shadowProfile_HandleAvAvrcpConnectedInd();
        break;

    case AV_AVRCP_DISCONNECTED_IND:
        shadowProfile_HandleAvAvrcpDisconnectedInd();
        break;

    case TELEPHONY_INCOMING_CALL:
        shadowProfile_HandleTelephonyIncomingCall();
        break;

    case TELEPHONY_CALL_ENDED:
        shadowProfile_HandleTelephonyCallEnded();
        break;

    /* Internal shadow_profile messages */
    case SHADOW_INTERNAL_DELAYED_KICK:
        ShadowProfile_SmKick();
        break;

    case SHADOW_INTERNAL_AUDIO_STARTED_IND:
        ShadowProfile_HandleAudioStartedInd();

    /* SDM prims from firmware */
    case MESSAGE_BLUESTACK_SDM_PRIM:
        ShadowProfile_HandleMessageBluestackSdmPrim((const SDM_UPRIM_T *)message);
        break;

    /* Peer Signalling messages */
    case PEER_SIG_CONNECTION_IND:
        shadowProfile_HandlePeerSignallingConnectionInd((const PEER_SIG_CONNECTION_IND_T *)message);
        break;

    case PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND:
        ShadowProfile_HandlePeerSignallingMessage((const PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND_T *)message);
        break;

    case PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM:
        ShadowProfile_HandlePeerSignallingMessageTxConfirm((const PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM_T *)message);
        break;

    /* Audio sync messages */
    case AUDIO_SYNC_CONNECT_IND:
        ShadowProfile_HandleAudioSyncConnectInd((const AUDIO_SYNC_CONNECT_IND_T *)message);
        break;

    case AUDIO_SYNC_ACTIVATE_IND:
        ShadowProfile_HandleAudioSyncActivateInd((const AUDIO_SYNC_ACTIVATE_IND_T *)message);
        break;

    case AUDIO_SYNC_STATE_IND:
        ShadowProfile_HandleAudioSyncStateInd((const AUDIO_SYNC_STATE_IND_T *)message);
        break;

    case AUDIO_SYNC_CODEC_RECONFIGURED_IND:
        ShadowProfile_HandleAudioSyncReconfiguredInd((const AUDIO_SYNC_CODEC_RECONFIGURED_IND_T *)message);
        break;

    default:
        SHADOW_LOGF("shadowProfile_MessageHandler: Unhandled id 0x%x", id);
        break;
    }
}

/*! \brief Send an audio_sync_msg_t internally.

    The audio_sync_msg_t messages must only be handled in a known stable
    state, so they are sent conditionally on the lock.
*/
static void shadowProfile_SyncSendAudioSyncMessage(audio_sync_t *sync_inst, MessageId id, Message message)
{
    PanicFalse(MessageCancelAll(sync_inst->task, id) <= 1);
    MessageSendConditionally(sync_inst->task, id, message, &ShadowProfile_GetLock());
}

bool ShadowProfile_Init(Task task)
{
    memset(&shadow_profile, 0, sizeof(shadow_profile));
    shadow_profile.task_data.handler = shadowProfile_MessageHandler;
    shadow_profile.state = SHADOW_PROFILE_SUB_STATE_NONE;
    shadow_profile.target_state = SHADOW_PROFILE_SUB_STATE_NONE;
    shadow_profile.acl.conn_handle = SHADOW_PROFILE_CONNECTION_HANDLE_INVALID;
    shadow_profile.esco.conn_handle = SHADOW_PROFILE_CONNECTION_HANDLE_INVALID;
    shadow_profile.esco.volume = appHfpGetVolume();
    shadow_profile.init_task = task;
    shadow_profile.client_tasks = TaskList_Create();
    shadow_profile.sync_if.task = &shadow_profile.task_data;
    shadow_profile.sync_if.sync_if.SendSyncMessage = shadowProfile_SyncSendAudioSyncMessage;

    ShadowProfile_SetTargetState(SHADOW_PROFILE_STATE_DISCONNECTED);

    /* Register for notifications when devices and/or profiles connect
       or disconnect. */
    ConManagerRegisterConnectionsClient(ShadowProfile_GetTask());
    appHfpStatusClientRegister(ShadowProfile_GetTask());
    appAvStatusClientRegister(ShadowProfile_GetTask());
    Telephony_RegisterForMessages(ShadowProfile_GetTask());

    /* Register a channel for peer signalling */
    appPeerSigMarshalledMsgChannelTaskRegister(ShadowProfile_GetTask(),
                                            PEER_SIG_MSG_CHANNEL_SHADOW_PROFILE,
                                            shadow_profile_marshal_type_descriptors,
                                            NUMBER_OF_SHADOW_PROFILE_MARSHAL_TYPES);

    /* Register for peer signaling notifications */
    appPeerSigClientRegister(ShadowProfile_GetTask());

    /* Now wait for SDM_REGISTER_CFM */
    return TRUE;
}

/* \brief Inform shadow profile of current device Primary/Secondary role.

    todo : A Primary <-> Secondary role switch should only be allowed
    when the state machine is in a stable state. This will be more important
    when the handover logic is implemented.
*/
void ShadowProfile_SetRole(bool primary)
{
    shadow_profile_task_data_t *sp = ShadowProfile_Get();

    if (!primary)
    {
        /* Take ownership of the A2DP source (shadow) when becoming secondary */
        AudioSources_RegisterAudioInterface(audio_source_a2dp_1, ShadowProfile_GetAudioInterface());
    }
    else
    {
        /* This code assumes AV always registers itself on becoming primary */

        /*! \todo When secondary becomes primary, the new primary needs to register
            for audio sync with the unmarshalled AV instance. */
    }

    sp->is_primary = primary;
    SHADOW_LOGF("ShadowProfile_SetRole primary %u", sp->is_primary);
}
/* \brief Get the SCO sink associated with the shadow eSCO link. */
Sink ShadowProfile_GetScoSink(void)
{
    shadow_profile_task_data_t *sp = ShadowProfile_Get();
    return sp->esco.sink;
}

void ShadowProfile_PeerConnected(void)
{
    SHADOW_LOG("ShadowProfile_PeerConnected");
    ShadowProfile_SetTargetStateFromProfileState();
}

void ShadowProfile_PeerDisconnected(void)
{
    SHADOW_LOG("ShadowProfile_PeerDisconnected");
    ShadowProfile_SetTargetStateFromProfileState();
}

void ShadowProfile_Connect(void)
{
    SHADOW_LOG("ShadowProfile_Connect not implemented");
    Panic();
}

void ShadowProfile_Disconnect(void)
{
    SHADOW_LOG("ShadowProfile_Disconnect not implemented");
    Panic();
}

void ShadowProfile_ClientRegister(Task client_task)
{
    shadow_profile_task_data_t *sp = ShadowProfile_Get();
    TaskList_AddTask(sp->client_tasks, client_task);
}

void ShadowProfile_ClientUnregister(Task client_task)
{
    shadow_profile_task_data_t *sp = ShadowProfile_Get();
    TaskList_RemoveTask(sp->client_tasks, client_task);
}

bool ShadowProfile_IsConnected(void)
{
    return (ShadowProfile_IsAclConnected() || ShadowProfile_IsEscoConnected());
}

bool ShadowProfile_IsEscoActive(void)
{
    shadow_profile_task_data_t *sp = ShadowProfile_Get();
    return (!ShadowProfile_IsPrimary() && SinkIsValid(sp->esco.sink));
}

/*
    Test only functions
*/
void ShadowProfile_Destroy(void)
{
    shadow_profile_task_data_t *sp = ShadowProfile_Get();
    TaskList_Destroy(sp->client_tasks);
}

#endif /* INCLUDE_SHADOWING */
