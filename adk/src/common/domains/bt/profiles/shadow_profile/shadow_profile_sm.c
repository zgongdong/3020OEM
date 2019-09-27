/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      State machine transitions and logic for shadow_profile.
*/

#ifdef INCLUDE_SHADOWING

#include "bt_device.h"
#include "link_policy.h"

#include "a2dp_profile.h"
#include "hfp_profile.h"
#include "kymera.h"
#include "kymera_adaptation.h"
#include "source_param_types.h"
#include "voice_sources.h"

#include "shadow_profile_signalling.h"
#include "shadow_profile_private.h"
#include "shadow_profile_sdm_prim.h"
#include "shadow_profile_sm.h"
#include "shadow_profile_audio_source.h"

/*
    State transition functions
*/

/*! \brief Enter INITIALISING state */
static void shadowProfile_EnterInitialising(void)
{
    /* Register with the firmware to receive MESSAGE_BLUESTACK_SDM_PRIM messages */
    MessageSdmTask(ShadowProfile_GetTask());

    /* Register with the SDM service */
    ShadowProfile_ShadowRegisterReq();
}

/*! \brief Exit INITIALISING state */
static void shadowProfile_ExitInitialising(void)
{
    shadow_profile_task_data_t *sp = ShadowProfile_Get();

    /* Send init confirmation to init module */
    MessageSend(sp->init_task, SHADOW_PROFILE_INIT_CFM, NULL);
}

/*! \brief Enter PEER_CONNECTED state */
static void shadowProfile_EnterPeerConnected(void)
{
    bdaddr bd_addr;

    SHADOW_LOG("shadowProfile_EnterPeerConnected");

    if (ShadowProfile_IsPrimary())
    {
        PanicFalse(appDeviceGetSecondaryBdAddr(&bd_addr));
    }
    else
    {
        PanicFalse(appDeviceGetPrimaryBdAddr(&bd_addr));
    }
    appLinkPolicyUpdatePowerTable(&bd_addr);
}

/*! \brief Enter ACL_CONNECTING state */
static void shadowProfile_EnterAclConnecting(void)
{
    SHADOW_LOG("shadowProfile_EnterAclConnecting");

    /* Should never reach this state as Secondary */
    assert(ShadowProfile_IsPrimary());

    /* Send SDM prim to create shadow ACL connection */
    ShadowProfile_ShadowConnectReq(LINK_TYPE_ACL);
}

/*! \brief Exit ACL_CONNECTING state */
static void shadowProfile_ExitAclConnecting(void)
{
    SHADOW_LOG("shadowProfile_ExitAclConnecting");
}

/*! \brief Enter ACL_CONNECTED parent state */
static void shadowProfile_EnterAclConnected(void)
{
    SHADOW_LOG("shadowProfile_EnterAclConnected");

    /* Required if the handset has connected A2DP before the secondary connects */
    ShadowProfile_SendA2dpStreamContextToSecondary();

    ShadowProfile_SendAclConnectInd();
}

/*! \brief Exit ACL_CONNECTED parent state */
static void shadowProfile_ExitAclConnected(void)
{
    SHADOW_LOG("shadowProfile_ExitAclConnected");
    ShadowProfile_SendAclDisconnectInd();
}

/*! \brief Enter ACL_DISCONNECTING state */
static void shadowProfile_EnterAclDisconnecting(void)
{
    shadow_profile_task_data_t *sp = ShadowProfile_Get();

    SHADOW_LOG("shadowProfile_EnterAclDisconnecting");

    /* Should never reach this state as Secondary
       Well, actually we can because when going into the case the upper layers
       put this into Secondary role before the shadow ACL is disconnected. */
    /*assert(ShadowProfile_IsPrimary());*/

    /* Send SDM prim to disconnect the shadow ACL */
    ShadowProfile_ShadowDisconnectReq(sp->acl.conn_handle, HCI_SUCCESS);
}

/*! \brief Exit ACL_DISCONNECTING state */
static void shadowProfile_ExitAclDisconnecting(void)
{
    SHADOW_LOG("shadowProfile_ExitAclDisconnecting");
}

/*! \brief Enter ESCO_CONNECTING state */
static void shadowProfile_EnterEscoConnecting(void)
{
    source_defined_params_t source_params;
    SHADOW_LOG("shadowProfile_EnterEscoConnecting");

    /* Should never reach this state as Secondary */
    assert(ShadowProfile_IsPrimary());

    /* The local HFP SCO should already have been setup and started at this
       point, so we can read the codec params from the hfp voice source. */
    if (VoiceSources_GetConnectParameters(voice_source_hfp_1, &source_params))
    {
        shadow_profile_esco_t *esco = ShadowProfile_GetScoState();
        voice_connect_parameters_t *voice_params = (voice_connect_parameters_t *)source_params.data;
        assert(source_params.data_length == sizeof(voice_connect_parameters_t));

        ShadowProfile_SendHfpCodecToSecondary(voice_params->codec_mode);

        /* Store parameters locally so state is known on primary->secondary transition */
        esco->sink = voice_params->audio_sink;
        esco->codec_mode = voice_params->codec_mode;
        esco->wesco = voice_params->wesco;

        VoiceSources_ReleaseConnectParameters(voice_source_hfp_1, &source_params);
    }

    /* Request creation of shadow eSCO link */
    ShadowProfile_ShadowConnectReq(LINK_TYPE_ESCO);
}

/*! \brief Exit ESCO_CONNECTING state */
static void shadowProfile_ExitEscoConnecting(void)
{
    SHADOW_LOG("shadowProfile_ExitEscoConnecting");
}

/*! \brief Enter ESCO_CONNECTED parent state */
static void shadowProfile_EnterScoConnected(void)
{
    SHADOW_LOG("shadowProfile_EnterScoConnected");

    if (!ShadowProfile_IsPrimary())
    {
        source_defined_params_t source_params;
        shadow_profile_task_data_t *sp = ShadowProfile_Get();

        /* Suspend AV streaming */
        appAvStreamingSuspend(AV_SUSPEND_REASON_SCOFWD);

        /* Create the SCO audio chain.
           Note: Use the hfp connect params as defaults & override with the
                 shadowing params where necessary. */
        if (VoiceSources_GetConnectParameters(voice_source_hfp_1, &source_params))
        {
            connect_parameters_t connect_params = {.source_type = source_type_voice, .source_params = source_params};
            voice_connect_parameters_t *voice_params = (voice_connect_parameters_t *)source_params.data;
            assert(source_params.data_length == sizeof(voice_connect_parameters_t));

            /* Override some of the values with shadowing values */
            voice_params->audio_sink = sp->esco.sink;
            voice_params->codec_mode = sp->esco.codec_mode;
            voice_params->wesco = sp->esco.wesco;
            voice_params->allow_scofwd = FALSE;
            voice_params->allow_micfwd = FALSE;
            voice_params->pre_start_delay = 0;
            voice_params->volume.value = sp->esco.volume;

            KymeraAdaptation_Connect(&connect_params);
            VoiceSources_ReleaseConnectParameters(voice_source_hfp_1, &source_params);
        }
    }

    /* Notify clients that the shadow SCO connection has connected */
    ShadowProfile_SendScoConnectInd();
}

/*! \brief Exit ESCO_CONNECTED parent state */
static void shadowProfile_ExitScoConnected(void)
{
    SHADOW_LOG("shadowProfile_ExitScoConnected");

    if (!ShadowProfile_IsPrimary())
    {
        source_defined_params_t source_params;

        /* SCO has gone, stop Kymera */
        if (VoiceSources_GetDisconnectParameters(voice_source_hfp_1, &source_params))
        {
            disconnect_parameters_t disconnect_params = {.source_type = source_type_voice, .source_params = source_params};
            KymeraAdaptation_Disconnect(&disconnect_params);
            VoiceSources_ReleaseDisconnectParameters(voice_source_hfp_1, &source_params);
        }

        /* Resume AV streaming */
        appAvStreamingResume(AV_SUSPEND_REASON_SCOFWD);
    }

    /* Notify clients that the shadow SCO connection has disconnected */
    ShadowProfile_SendScoDisconnectInd();
}

/*! \brief Enter SHADOW_PROFILE_STATE_A2DP_CONNECTED sub-state */
static void shadowProfile_EnterA2dpConnected(void)
{
    SHADOW_LOG("shadowProfile_EnterA2dpConnected");
    ShadowProfile_SendA2dpStreamActiveInd();
}

/*! \brief Exit SHADOW_PROFILE_STATE_A2DP_CONNECTED sub-state */
static void shadowProfile_ExitA2dpConnected(void)
{
    SHADOW_LOG("shadowProfile_ExitA2dpConnected");
    ShadowProfile_SendA2dpStreamInactiveInd();
}

/*! \brief Enter ESCO_DISCONNECTING state */
static void shadowProfile_EnterEscoDisconnecting(void)
{
    shadow_profile_task_data_t *sp = ShadowProfile_Get();

    SHADOW_LOG("shadowProfile_EnterEscoDisconnecting");

    /* Should never reach this state as Secondary */
    assert(ShadowProfile_IsPrimary());

    /* Send SDM prim to disconnect the shadow eSCO */
    ShadowProfile_ShadowDisconnectReq(sp->esco.conn_handle, HCI_SUCCESS);
}

/*! \brief Exit ESCO_DISCONNECTING state */
static void shadowProfile_ExitEscoDisconnecting(void)
{
    SHADOW_LOG("shadowProfile_ExitEscoDisconnecting");
}

/* \brief Enter SHADOW_PROFILE_STATE_A2DP_CONNECTING state */
static void shadowProfile_EnterA2dpConnecting(void)
{
    shadow_profile_task_data_t *sp = ShadowProfile_Get();

    SHADOW_LOG("shadowProfile_EnterA2dpConnecting");

    if (ShadowProfile_IsPrimary())
    {
        ShadowProfile_ShadowL2capConnectReq(sp->acl.conn_handle, sp->a2dp.cid);
        ShadowProfile_SetA2dpShadowStartLock();
    }
    else
    {
        /* As secondary, start audio, wait for lock to clear, then send
        connect response to Bluestack */
        MESSAGE_MAKE(media, AV_A2DP_AUDIO_CONNECT_MESSAGE_T);
        media->audio_source = audio_source_a2dp_1;

        ShadowProfile_SetAudioStartLock();

        appAvSendStatusMessage(AV_A2DP_AUDIO_CONNECTED, media, sizeof(*media));

        MessageSendConditionally(ShadowProfile_GetTask(),
                                 SHADOW_INTERNAL_AUDIO_STARTED_IND, NULL,
                                 ShadowProfile_GetA2dpStartLockAddr());
    }
}

/* \brief Exit SHADOW_PROFILE_STATE_A2DP_CONNECTING state */
static void shadowProfile_ExitA2dpConnecting(void)
{
    SHADOW_LOG("shadowProfile_ExitA2dpConnecting");

    ShadowProfile_ClearA2dpShadowStartLock();
    ShadowProfile_ClearAudioStartLock();
}

/* \brief Enter SHADOW_PROFILE_STATE_A2DP_DISCONNECTING state */
static void shadowProfile_EnterA2dpDisconnecting(void)
{
    shadow_profile_task_data_t *sp = ShadowProfile_Get();

    SHADOW_LOG("shadowProfile_EnterA2dpDisconnecting");

    if (ShadowProfile_IsPrimary())
    {
        ShadowProfile_ShadowL2capDisconnectReq(sp->a2dp.cid);
    }
    else
    {
        MESSAGE_MAKE(media, AV_A2DP_AUDIO_CONNECT_MESSAGE_T);
        media->audio_source = audio_source_a2dp_1;

        /* send message with audio_source_a2dp as a parameter and let the service do the disconnection */
        appAvSendStatusMessage(AV_A2DP_AUDIO_DISCONNECTED, media, sizeof(*media));

        ShadowProfile_ShadowL2capDisconnectRsp(sp->a2dp.cid);

        ShadowProfile_SetState(SHADOW_PROFILE_STATE_ACL_CONNECTED);
    }
}

/* \brief Exit SHADOW_PROFILE_STATE_A2DP_DISCONNECTING state */
static void shadowProfile_ExitA2dpDisconnecting(void)
{
    SHADOW_LOG("shadowProfile_ExitA2dpDisconnecting");
}

void ShadowProfile_SetState(shadow_profile_state_t state)
{
    shadow_profile_task_data_t *sp = ShadowProfile_Get();
    shadow_profile_state_t old_state = sp->state;

    /* It is not valid to re-enter the same state */
    assert(old_state != state);

    SHADOW_LOGF("ShadowProfile_SetState state(%02x) old_state(%02x)", state, old_state);

    /* Handle state exit functions */
    switch (old_state)
    {
    case SHADOW_PROFILE_STATE_INITALISING:
        shadowProfile_ExitInitialising();
        break;
    case SHADOW_PROFILE_STATE_ACL_CONNECTING:
        shadowProfile_ExitAclConnecting();
        break;
    case SHADOW_PROFILE_STATE_ACL_DISCONNECTING:
        shadowProfile_ExitAclDisconnecting();
        break;
    case SHADOW_PROFILE_STATE_ESCO_CONNECTING:
        shadowProfile_ExitEscoConnecting();
        break;
    case SHADOW_PROFILE_STATE_ESCO_DISCONNECTING:
        shadowProfile_ExitEscoDisconnecting();
        break;
    case SHADOW_PROFILE_STATE_A2DP_CONNECTING:
        shadowProfile_ExitA2dpConnecting();
        break;
    case SHADOW_PROFILE_STATE_A2DP_DISCONNECTING:
        shadowProfile_ExitA2dpDisconnecting();
        break;
    default:
        break;
    }

    /* Check if exiting ACL connected sub-state */
    if (ShadowProfile_IsSubStateAclConnected(old_state) && !ShadowProfile_IsSubStateAclConnected(state))
        shadowProfile_ExitAclConnected();

    /* Check if exiting SCO connected sub-state */
    if (ShadowProfile_IsSubStateEscoConnected(old_state) && !ShadowProfile_IsSubStateEscoConnected(state))
        shadowProfile_ExitScoConnected();

    /* Check if exiting A2DP connected sub-state */
    if (ShadowProfile_IsSubStateA2dpConnected(old_state) && !ShadowProfile_IsSubStateA2dpConnected(state))
        shadowProfile_ExitA2dpConnected();

    /* Check if exiting a steady state */
    if (ShadowProfile_IsSteadyState(old_state) && !ShadowProfile_IsSteadyState(state))
        ShadowProfile_SetTransitionLockBit();

    /* Set new state */
    sp->state = state;

    /* Check if entering ACL connected sub-state */
    if (!ShadowProfile_IsSubStateAclConnected(old_state) && ShadowProfile_IsSubStateAclConnected(state))
        shadowProfile_EnterAclConnected();

    /* Check if entering SCO connected sub-state */
    if (!ShadowProfile_IsSubStateEscoConnected(old_state) && ShadowProfile_IsSubStateEscoConnected(state))
        shadowProfile_EnterScoConnected();

    /* Check if entering A2DP connected sub-state */
    if (!ShadowProfile_IsSubStateA2dpConnected(old_state) && ShadowProfile_IsSubStateA2dpConnected(state))
        shadowProfile_EnterA2dpConnected();

    /* Check if entering a steady state */
    if (!ShadowProfile_IsSteadyState(old_state) && ShadowProfile_IsSteadyState(state))
        ShadowProfile_ClearTransitionLockBit();

    /* Handle state entry functions */
    switch (sp->state)
    {
    case SHADOW_PROFILE_STATE_INITALISING:
        shadowProfile_EnterInitialising();
        break;
    case SHADOW_PROFILE_STATE_PEER_CONNECTED:
        shadowProfile_EnterPeerConnected();
        break;
    case SHADOW_PROFILE_STATE_ACL_CONNECTING:
        shadowProfile_EnterAclConnecting();
        break;
    case SHADOW_PROFILE_STATE_ACL_DISCONNECTING:
        shadowProfile_EnterAclDisconnecting();
        break;
    case SHADOW_PROFILE_STATE_ESCO_CONNECTING:
        shadowProfile_EnterEscoConnecting();
        break;
    case SHADOW_PROFILE_STATE_ESCO_DISCONNECTING:
        shadowProfile_EnterEscoDisconnecting();
        break;
    case SHADOW_PROFILE_STATE_A2DP_CONNECTING:
        shadowProfile_EnterA2dpConnecting();
        break;
    case SHADOW_PROFILE_STATE_A2DP_DISCONNECTING:
        shadowProfile_EnterA2dpDisconnecting();
        break;
    default:
        break;
    }

    /*  Now the state change is complete, kick the SM to transition towards
        the target state. The target state is only used in primary role. */
    if (ShadowProfile_IsPrimary())
    {
        ShadowProfile_SmKick();
    }
}

/*! \brief Handle shadow_profile error

    Some error occurred in the shadow_profile state machine.

    To avoid the state machine getting stuck, if instance is connected then
    drop connection and move to 'disconnecting' state.
*/
void ShadowProfile_StateError(MessageId id, Message message)
{
    UNUSED(message);
    UNUSED(id);

    SHADOW_LOGF("ShadowProfile_StateError state 0x%x id 0x%x", ShadowProfile_GetState(), id);

    Panic();
}

/*! \brief Logic to transition from current state to target state.

    \return The next state to enter in the transition to the target state.

    Generally, the logic determines the transitionary state to enter from the
    current steady state.
 */
static shadow_profile_state_t shadowProfile_SmTransition(void)
{
    switch (ShadowProfile_GetTargetState())
    {
    case SHADOW_PROFILE_STATE_DISCONNECTED:
        switch (ShadowProfile_GetState())
        {
        case SHADOW_PROFILE_STATE_NONE:
            return SHADOW_PROFILE_STATE_INITALISING;
        /* This is a special case, where there is no transition state, just enter the new state */
        case SHADOW_PROFILE_STATE_PEER_CONNECTED:
            return SHADOW_PROFILE_STATE_DISCONNECTED;
        case SHADOW_PROFILE_STATE_ACL_CONNECTED:
            return SHADOW_PROFILE_STATE_ACL_DISCONNECTING;
        case SHADOW_PROFILE_STATE_ESCO_CONNECTED:
            return SHADOW_PROFILE_STATE_ESCO_DISCONNECTING;
        case SHADOW_PROFILE_STATE_A2DP_CONNECTED:
            return SHADOW_PROFILE_STATE_A2DP_DISCONNECTING;
        default:
            break;
        }
        break;
    case SHADOW_PROFILE_STATE_PEER_CONNECTED:
        switch (ShadowProfile_GetState())
        {
        /* This is a special case, where there is no transition, just enter the new state */
        case SHADOW_PROFILE_STATE_DISCONNECTED:
            return SHADOW_PROFILE_STATE_PEER_CONNECTED;
        case SHADOW_PROFILE_STATE_ACL_CONNECTED:
            return SHADOW_PROFILE_STATE_ACL_DISCONNECTING;
        case SHADOW_PROFILE_STATE_ESCO_CONNECTED:
            return SHADOW_PROFILE_STATE_ESCO_DISCONNECTING;
        case SHADOW_PROFILE_STATE_A2DP_CONNECTED:
            return SHADOW_PROFILE_STATE_A2DP_DISCONNECTING;
        default:
            break;
        }
        break;

    case SHADOW_PROFILE_STATE_ACL_CONNECTED:
        switch (ShadowProfile_GetState())
        {
        /* This is a special case, where there is no transition, just enter the new state */
        case SHADOW_PROFILE_STATE_DISCONNECTED:
            return SHADOW_PROFILE_STATE_PEER_CONNECTED;
        case SHADOW_PROFILE_STATE_PEER_CONNECTED:
            return SHADOW_PROFILE_STATE_ACL_CONNECTING;
        case SHADOW_PROFILE_STATE_ESCO_CONNECTED:
            return SHADOW_PROFILE_STATE_ESCO_DISCONNECTING;
        case SHADOW_PROFILE_STATE_A2DP_CONNECTED:
            return SHADOW_PROFILE_STATE_A2DP_DISCONNECTING;
        default:
            break;
        }
        break;

    case SHADOW_PROFILE_STATE_ESCO_CONNECTED:
        switch (ShadowProfile_GetState())
        {
        /* This is a special case, where there is no transition, just enter the new state */
        case SHADOW_PROFILE_STATE_DISCONNECTED:
            return SHADOW_PROFILE_STATE_PEER_CONNECTED;
        case SHADOW_PROFILE_STATE_PEER_CONNECTED:
            return SHADOW_PROFILE_STATE_ACL_CONNECTING;
        case SHADOW_PROFILE_STATE_ACL_CONNECTED:
            return SHADOW_PROFILE_STATE_ESCO_CONNECTING;
        case SHADOW_PROFILE_STATE_A2DP_CONNECTED:
            return SHADOW_PROFILE_STATE_A2DP_DISCONNECTING;
        default:
            break;
        }
        break;

    case SHADOW_PROFILE_STATE_A2DP_CONNECTED:
        switch (ShadowProfile_GetState())
        {
        /* This is a special case, where there is no transition, just enter the new state */
        case SHADOW_PROFILE_STATE_DISCONNECTED:
            return SHADOW_PROFILE_STATE_PEER_CONNECTED;
        case SHADOW_PROFILE_STATE_PEER_CONNECTED:
            return SHADOW_PROFILE_STATE_ACL_CONNECTING;
        case SHADOW_PROFILE_STATE_ACL_CONNECTED:
            return SHADOW_PROFILE_STATE_A2DP_CONNECTING;
        case SHADOW_PROFILE_STATE_ESCO_CONNECTED:
            return SHADOW_PROFILE_STATE_ESCO_DISCONNECTING;
        default:
            break;
        }
        break;
    default:
        Panic();
        break;
    }
    return ShadowProfile_GetState();
}

void ShadowProfile_SmKick(void)
{
    if (ShadowProfile_GetDelayKick())
    {
        ShadowProfile_ClearDelayKick();
        MessageSendLater(ShadowProfile_GetTask(), SHADOW_INTERNAL_DELAYED_KICK,
                         NULL, SHADOW_PROFILE_KICK_LATER_DELAY);
    }
    else
    {
        shadow_profile_state_t current = ShadowProfile_GetState();

        /* Only allow when in steady state. */
        if (ShadowProfile_IsSteadyState(current))
        {
            shadow_profile_state_t next = shadowProfile_SmTransition();

            if (next != current)
            {
                ShadowProfile_SetState(next);
            }
            MessageCancelAll(ShadowProfile_GetTask(), SHADOW_INTERNAL_DELAYED_KICK);
        }
    }
}

void ShadowProfile_SetTargetState(shadow_profile_state_t target_state)
{
    SHADOW_LOG("ShadowProfile_SetTargetState 0x%x", target_state);
    ShadowProfile_Get()->target_state = target_state;
    ShadowProfile_SmKick();
}

void ShadowProfile_HandleAudioStartedInd(void)
{
    SHADOW_LOGF("ShadowProfile_HandleAudioStartedInd");

    switch (ShadowProfile_GetState())
    {
        case SHADOW_PROFILE_STATE_A2DP_CONNECTING:
        {
            ShadowProfile_ShadowL2capConnectRsp(
                    ShadowProfile_GetAclState()->conn_handle,
                    ShadowProfile_GetA2dpState()->cid);
        }
        break;
        default:
            ShadowProfile_StateError(SHADOW_INTERNAL_AUDIO_STARTED_IND, NULL);
        break;
    }
}

#endif /* INCLUDE_SHADOWING */
