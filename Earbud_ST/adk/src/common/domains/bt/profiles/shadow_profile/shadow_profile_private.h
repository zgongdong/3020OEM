/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Private functions and helpers for the shadow_profile module.
*/

#ifndef SHADOW_PROFILE_PRIVATE_H_
#define SHADOW_PROFILE_PRIVATE_H_

#include <logging.h>
#include <panic.h>

#include <task_list.h>

#ifdef INCLUDE_SHADOWING

#ifdef L2CA_DISCONNECT_LINK_TRANSFERRED
#undef L2CA_DISCONNECT_LINK_TRANSFERRED
#endif
#include <app/bluestack/sdm_prim.h>
#ifdef L2CA_DISCONNECT_LINK_TRANSFERRED
#undef L2CA_DISCONNECT_LINK_TRANSFERRED
#endif

#define L2CA_DISCONNECT_LINK_TRANSFERRED 0xFF

#include "kymera_adaptation_voice_protected.h"
#include "audio_sync.h"

#include "shadow_profile.h"
#include "shadow_profile_config.h"
#include "shadow_profile_sm.h"


/*! \{
    Macros for diagnostic output that can be suppressed.
    Allows debug of this module at two levels. */
#define SHADOW_LOG    DEBUG_LOG
#define SHADOW_LOGF   DEBUG_LOGF
/*! \} */

/*! Code assertion that can be checked at run time. This will cause a panic. */
#define assert(x) PanicFalse(x)

/*! An invalid shadow ACL or eSCO connection handle */
#define SHADOW_PROFILE_CONNECTION_HANDLE_INVALID ((uint16)0xFFFF)

/*! Delay before kicking the SM */
#define SHADOW_PROFILE_KICK_LATER_DELAY D_SEC(1)

/*! Messages used internally only in shadow_profile.

    These messages should usually be sent conditionally on the shadow_profile
    state machine transition lock.

    This ensures that they will only be delivered when the state machine is
    in a stable state (e.g. not waiting for a connect request to complete).
*/
typedef enum
{
    /*! Trigger kicking the state machine */
    SHADOW_INTERNAL_DELAYED_KICK,

    /*! Message sent conditional on lock - cleared by audio when started */
    SHADOW_INTERNAL_AUDIO_STARTED_IND,

    SHADOW_INTERNAL_MAX
} shadow_profile_internal_msg_t;

/*! \brief State related to the shadow ACL connection  */
typedef struct
{
    /*! The shadow ACL connection handle */
    uint16 conn_handle;

    /*! The shadow ACL's BD_ADDR */
    bdaddr bd_addr;

} shadow_profile_acl_t;

/*! \brief State related to the shadow eSCO connection  */
typedef struct
{
    /*! The shadow eSCO connection handle */
    uint16 conn_handle;

    /*! The shadow eSCO Sink */
    Sink sink;

    /*! The shadow eSCO wesco param */
    uint8 wesco;

    /*! The shadow eSCO codec mode (forwarded from Primary) */
    hfp_codec_mode_t codec_mode;

    /*! The shadow eSCO volume (forwarded from Primary) */
    uint8 volume;
} shadow_profile_esco_t;

/*! \brief State related to the shadow A2DP connection  */
typedef struct
{
    /*! The L2CAP cid of the shadowed A2DP media channel.
        The cid is used internally when primary to determine if an A2DP media
        channel is connected. When set to L2CA_CID_INVALID there is no A2DP media
        channel connected. */
    l2ca_cid_t cid;

    /*! The L2CAP MTU. */
    uint16 mtu;

    /*! The primary earbud's active stream endpoint ID */
    uint8 seid;

    /*! The configured sample rate */
    uint32 sample_rate;

    /*! Content protection is enabled/disabled */
    bool content_protection;

    /* State of the A2DP sync */
    audio_sync_state_t state;

} shadow_profile_a2dp_t;

/*! \brief Shadow Profile internal state. */
typedef struct
{
    /*! Shadow Profile task */
    TaskData task_data;

    /*! Shadow Profile state */
    shadow_profile_state_t state;

    /*! Shadow Profile target state */
    shadow_profile_state_t target_state;

    /*! state machine lock */
    uint16 lock;

    /*! Lock set when starting a2dp shadowing. */
    uint16 a2dp_start_lock;

    /*! Current role of this instance */
    bool is_primary;

    /*! Flag whether to delay before kicking the state machine after a state change. */
    bool delay_kick;

    /*! The shadow ACL connection state */
    shadow_profile_acl_t acl;

    /*! The shadow eSCO connection state */
    shadow_profile_esco_t esco;

    /*! The shadow A2DP media connection state */
    shadow_profile_a2dp_t a2dp;

    /*! Init task to send init cfm to */
    Task init_task;

    /*! List of tasks registered for notifications from shadow_profile */
    task_list_t *client_tasks;

    /*! The interface to register with an AV instance that we want to sync to. */
    audio_sync_t    sync_if;

} shadow_profile_task_data_t;


extern shadow_profile_task_data_t shadow_profile;

#define ShadowProfile_Get() (&shadow_profile)

#define ShadowProfile_GetTask() (&ShadowProfile_Get()->task_data)

/*! \brief Get current shadow_profile state. */
#define ShadowProfile_GetState() (ShadowProfile_Get()->state)
/*! \brief Get current shadow_profile target state. */
#define ShadowProfile_GetTargetState() (ShadowProfile_Get()->target_state)

/*! \brief Is the shadow_profile in Primary role */
#define ShadowProfile_IsPrimary()   (ShadowProfile_Get()->is_primary)
/*! \brief Is the shadow_profile in Secondary role */
#define ShadowProfile_IsSecondary()   (!ShadowProfile_IsPrimary())
/*! \brief Get the module's audio sync interface */
#define ShadowProfile_GetSyncIf() (&(ShadowProfile_Get()->sync_if))
/*! \brief Get pointer to ACL state */
#define ShadowProfile_GetAclState() (&(ShadowProfile_Get()->acl))
/*! \brief Get pointer to A2DP state */
#define ShadowProfile_GetA2dpState() (&(ShadowProfile_Get()->a2dp))
/*! \brief Get pointer to eSCO state */
#define ShadowProfile_GetScoState() (&(ShadowProfile_Get()->esco))

/*! \brief Set the delay kick flag */
#define ShadowProfile_SetDelayKick() ShadowProfile_Get()->delay_kick = TRUE
/*! \brief Clear the delay kick flag */
#define ShadowProfile_ClearDelayKick() ShadowProfile_Get()->delay_kick = FALSE
/*! \brief Get the delay kick flag */
#define ShadowProfile_GetDelayKick() (ShadowProfile_Get()->delay_kick)

/*!@{ \name Shadow profile lock bit masks */
#define SHADOW_PROFILE_TRANSITION_LOCK 1U
/*!@} */

/*! \brief Set shadow_profile lock bit for transition states */
#define ShadowProfile_SetTransitionLockBit() (shadow_profile.lock |= SHADOW_PROFILE_TRANSITION_LOCK)
/*! \brief Clear shadow_profile lock bit for transition states */
#define ShadowProfile_ClearTransitionLockBit() (shadow_profile.lock &= ~SHADOW_PROFILE_TRANSITION_LOCK)
/*! \brief Get shadow_profile state machine lock. */
#define ShadowProfile_GetLock() (shadow_profile.lock)

/*!@{ \name Shadow profile A2DP shadow start lock bit masks */
#define SHADOW_PROFILE_AUDIO_START_LOCK 1U
#define SHADOW_PROFILE_A2DP_SHADOW_START_LOCK 2U
/*!@} */
/*! \brief Get shadow_profile state machine lock. */
#define ShadowProfile_GetA2dpStartLockAddr() (&(shadow_profile.a2dp_start_lock))

/*! \brief Set audio start lock bit */
#define ShadowProfile_SetAudioStartLock() (shadow_profile.a2dp_start_lock |= SHADOW_PROFILE_AUDIO_START_LOCK)
/*! \brief Clear audio start lock bit */
#define ShadowProfile_ClearAudioStartLock() (shadow_profile.a2dp_start_lock &= ~SHADOW_PROFILE_AUDIO_START_LOCK)
/*! \brief Set a2dp shadow start lock bit */
#define ShadowProfile_SetA2dpShadowStartLock() (shadow_profile.a2dp_start_lock |= SHADOW_PROFILE_A2DP_SHADOW_START_LOCK)
/*! \brief Clear a2dp shadow start lock bit */
#define ShadowProfile_ClearA2dpShadowStartLock() (shadow_profile.a2dp_start_lock &= ~SHADOW_PROFILE_A2DP_SHADOW_START_LOCK)


/*! \brief Test if Shadow ACL connection handle is valid */
#define ShadowProfile_IsAclConnected() \
    (ShadowProfile_Get()->acl.conn_handle != SHADOW_PROFILE_CONNECTION_HANDLE_INVALID)

/*! \brief Test if Shadow eSCO connection handle is valid */
#define ShadowProfile_IsEscoConnected() \
    (ShadowProfile_Get()->esco.conn_handle != SHADOW_PROFILE_CONNECTION_HANDLE_INVALID)

/*! \brief Test if Shadow A2DP is connected */
#define ShadowProfile_IsA2dpConnected() \
    (ShadowProfile_Get()->a2dp.cid != L2CA_CID_INVALID)

/*! \brief Notify clients of the shadow ACL connection.

    Sends a SHADOW_PROFILE_CONNECT_IND to all clients that registered
    for notifications with #ShadowProfile_ClientRegister.
*/
void ShadowProfile_SendAclConnectInd(void);

/*! \brief Notify clients of the shadow ACL dis-connection.

    Sends a SHADOW_PROFILE_DISCONNECT_IND to all clients that registered
    for notifications with #ShadowProfile_ClientRegister.
*/
void ShadowProfile_SendAclDisconnectInd(void);

/*! \brief Notify clients of the shadow eSCO connection.

    Sends a SHADOW_PROFILE_ESCO_CONNECT_IND to all clients that registered
    for notifications with #ShadowProfile_ClientRegister.
*/
void ShadowProfile_SendScoConnectInd(void);

/*! \brief Notify clients of the shadow eSCO dis-connection.

    Sends a SHADOW_PROFILE_ESCO_DISCONNECT_IND to all clients that registered
    for notifications with #ShadowProfile_ClientRegister.
*/
void ShadowProfile_SendScoDisconnectInd(void);

/*! \brief Notify clients shadow A2DP stream is active.

    Sends a SHADOW_PROFILE_A2DP_STREAM_ACTIVE_IND to all clients that registered
    for notifications with #ShadowProfile_ClientRegister.
*/
void ShadowProfile_SendA2dpStreamActiveInd(void);

/*! \brief Notify clients shadow A2DP stream is inactive .

    Sends a SHADOW_PROFILE_A2DP_STREAM_INACTIVE_IND to all clients that registered
    for notifications with #ShadowProfile_ClientRegister.
*/
void ShadowProfile_SendA2dpStreamInactiveInd(void);

/*! \brief Reset shadow SCO connection state */
void ShadowProfile_ResetEscoConnectionState(void);

/* \brief Set the local SCO audio volume

    This is only expected to be called on the Secondary, in response to
    a forwarded volume update from the Primary.

    \param volume The HFP volume forwarded from the Primary.
*/
void ShadowProfile_SetScoVolume(uint8 volume);

/*\! brief Set the local SCO codec params

    This is only expected to be called on the Secondary, in response to
    a forwarded HFP codec from the Primary.

    \param code_mode The HFP codec mode forwarded from the Primary.
*/
void ShadowProfile_SetScoCodec(hfp_codec_mode_t codec_mode);

/*
    Test-only functions
*/

/* Destroy the shadow_profile instance for unit tests */
void ShadowProfile_Destroy(void);

#endif /* INCLUDE_SHADOWING */

#endif /* SHADOW_PROFILE_PRIVATE_H_ */
