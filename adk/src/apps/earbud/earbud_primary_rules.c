/*!
\copyright  Copyright (c) 2005 - 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       earbud_primary_rules.c
\brief	    Earbud application rules run when in a Primary earbud role.
*/

#include "earbud_primary_rules.h"
#include "earbud_config.h"
#include "earbud_rules_config.h"

#include <earbud_config.h>
#include <earbud_init.h>
#include <earbud_log.h>
#include <kymera_config.h>
#include <earbud_sm.h>
#include <connection_manager.h>
#include <earbud_test.h>

#include <domain_message.h>
#include <av.h>
#include <phy_state.h>
#include <bt_device.h>
#include <bredr_scan_manager.h>
#include <hfp_profile.h>
#include <scofwd_profile.h>
#include <scofwd_profile_config.h>
#include <state_proxy.h>
#include <rules_engine.h>
#include <peer_signalling.h>

#include <bdaddr.h>
#include <panic.h>
#include <system_clock.h>

#pragma unitsuppress Unused

/*! \{
    Macros for diagnostic output that can be suppressed. */
#define PRIMARY_RULE_LOG         DEBUG_LOG
/*! \} */

/* Forward declaration for use in RULE_ACTION_RUN_PARAM macro below */
static rule_action_t PrimaryRulesCopyRunParams(const void* param, size_t size_param);

/*! \brief Macro used by rules to return RUN action with parameters to return.
    Copies the parameters/data into conn_rules where the rules engine can uses
    it when building the action message.
*/
#define RULE_ACTION_RUN_PARAM(x)   PrimaryRulesCopyRunParams(&(x), sizeof(x))

/*! Get pointer to the connection rules task data structure. */
#define PrimaryRulesGetTaskData()           (&primary_rules_task_data)

/*!< Connection rules. */
PrimaryRulesTaskData primary_rules_task_data;

/*! \{
    Rule function prototypes, so we can build the rule tables below. */
DEFINE_RULE(ruleAutoHandsetPair);
DEFINE_RULE(ruleForwardLinkKeys);

DEFINE_RULE(ruleOutOfEarA2dpActive);
DEFINE_RULE(ruleInEarCancelAudioPause);
DEFINE_RULE(ruleInEarA2dpRestart);
DEFINE_RULE(ruleOutOfEarScoActive);
DEFINE_RULE(ruleInEarScoTransferToEarbud);
DEFINE_RULE(ruleOutOfEarLedsEnable);
DEFINE_RULE(ruleInEarLedsDisable);
DEFINE_RULE(ruleInCaseEnterDfu);
DEFINE_RULE(ruleOutOfCaseAllowHandsetConnect);
DEFINE_RULE(ruleInCaseRejectHandsetConnect);

DEFINE_RULE(ruleDfuAllowHandsetConnect);
DEFINE_RULE(ruleCheckUpgradable);

DEFINE_RULE(ruleInCaseScoTransferToHandset);
DEFINE_RULE(ruleSelectMicrophone);
DEFINE_RULE(ruleScoForwardingControl);

 DEFINE_RULE(rulePairingConnectTwsPlusA2dp);
 DEFINE_RULE(rulePairingConnectTwsPlusHfp);

DEFINE_RULE(ruleInEarAncEnable);
DEFINE_RULE(ruleOutOfEarAncDisable);

DEFINE_RULE(ruleOutOfCaseAncTuning);
DEFINE_RULE(ruleInCaseAncTuning);

DEFINE_RULE(ruleSelectRemoteAudioMix);
DEFINE_RULE(ruleSelectLocalAudioMix);

DEFINE_RULE(ruleNotifyPeerMruHandset);
DEFINE_RULE(ruleNotifyDFUAbort);
/*! \} */

/*! \brief Set of rules to run on Earbud startup. */
const rule_entry_t primary_rules_set[] =
{
    /*! \{
        Rules that should always run on any event */
    RULE_WITH_FLAGS(RULE_EVENT_CHECK_DFU,              ruleCheckUpgradable,         CONN_RULES_DFU_ALLOW,
                    rule_flag_always_evaluate),
    /*! \} */

    RULE(RULE_EVENT_ROLE_SWITCH,                    ruleAutoHandsetPair,            CONN_RULES_HANDSET_PAIR),

    /*! \{
        Rules that control handset connection permitted/denied */
    RULE(RULE_EVENT_OUT_CASE,                   ruleOutOfCaseAllowHandsetConnect,   CONN_RULES_ALLOW_HANDSET_CONNECT),
    /*! \} */

    /*! \{
        Rule to enable automatic handset pairing when taken out of the case. */
    RULE(RULE_EVENT_OUT_CASE,                   ruleAutoHandsetPair,                CONN_RULES_HANDSET_PAIR),
    /*! \} */

    /*! \{
        Rules to synchronise link keys.
        \todo will be moving into TWS topology */
    RULE(RULE_EVENT_PEER_UPDATE_LINKKEYS,       ruleForwardLinkKeys,                CONN_RULES_PEER_SEND_LINK_KEYS),
    RULE(RULE_EVENT_PEER_CONNECTED,             ruleForwardLinkKeys,                CONN_RULES_PEER_SEND_LINK_KEYS),
    /*! \} */

    /*** Below here are more Earbud product behaviour type rules, rather than connection/topology type rules
     *   These are likely to stay in the application rule set.
     ****/

    /*! \{ */
    RULE(RULE_EVENT_DFU_CONNECT,                ruleDfuAllowHandsetConnect,         CONN_RULES_ALLOW_HANDSET_CONNECT),
    /*! \} */
    /*! \{
        Rules to drive ANC tuning. */
    RULE(RULE_EVENT_OUT_CASE,                   ruleOutOfCaseAncTuning,             CONN_RULES_ANC_TUNING_STOP),
    RULE(RULE_EVENT_IN_CASE,                    ruleInCaseAncTuning,                CONN_RULES_ANC_TUNING_START),
    RULE(RULE_EVENT_OUT_EAR,                    ruleOutOfEarAncDisable,             CONN_RULES_ANC_DISABLE),
    RULE(RULE_EVENT_IN_EAR,                     ruleInEarAncEnable,                 CONN_RULES_ANC_ENABLE),
    /*! \} */
    /*! \{
        Rules to start DFU when going in the case. */
    RULE(RULE_EVENT_IN_CASE,                    ruleInCaseEnterDfu,                 CONN_RULES_ENTER_DFU),
    /*! \} */

    RULE(RULE_EVENT_IN_CASE,                    ruleInCaseRejectHandsetConnect,     CONN_RULES_REJECT_HANDSET_CONNECT),
    /*! \{
        Rules to control audio pauses when in/out of the ear. */
    RULE(RULE_EVENT_OUT_EAR,                    ruleOutOfEarA2dpActive,             CONN_RULES_A2DP_TIMEOUT),
    RULE(RULE_EVENT_PEER_OUT_EAR,               ruleOutOfEarA2dpActive,             CONN_RULES_A2DP_TIMEOUT),
    RULE(RULE_EVENT_IN_EAR,                     ruleInEarCancelAudioPause,          CONN_RULES_A2DP_TIMEOUT_CANCEL),
    RULE(RULE_EVENT_IN_CASE,                    ruleInEarCancelAudioPause,          CONN_RULES_A2DP_TIMEOUT_CANCEL),
    RULE(RULE_EVENT_PEER_IN_EAR,                ruleInEarCancelAudioPause,          CONN_RULES_A2DP_TIMEOUT_CANCEL),
    RULE(RULE_EVENT_PEER_IN_CASE,               ruleInEarCancelAudioPause,          CONN_RULES_A2DP_TIMEOUT_CANCEL),
    RULE(RULE_EVENT_IN_EAR,                     ruleInEarA2dpRestart,               CONN_RULES_MEDIA_PLAY),
    RULE(RULE_EVENT_PEER_IN_EAR,                ruleInEarA2dpRestart,               CONN_RULES_MEDIA_PLAY),
    RULE(RULE_EVENT_OUT_EAR,                    ruleOutOfEarScoActive,              CONN_RULES_SCO_TIMEOUT),
    RULE(RULE_EVENT_PEER_OUT_EAR,               ruleOutOfEarScoActive,              CONN_RULES_SCO_TIMEOUT),
    /*! \} */
    /*! \{
        Rules to LED on/off when in/out of the ear. */
    RULE(RULE_EVENT_OUT_EAR,                    ruleOutOfEarLedsEnable,             CONN_RULES_LED_ENABLE),
    RULE(RULE_EVENT_IN_EAR,                     ruleInEarLedsDisable,               CONN_RULES_LED_DISABLE),
    /*! \} */
    /*! \{
        Rules to control SCO audio transfer. */
    RULE(RULE_EVENT_IN_EAR,                     ruleInEarScoTransferToEarbud,       CONN_RULES_SCO_TRANSFER_TO_EARBUD),
    RULE(RULE_EVENT_PEER_IN_EAR,                ruleInEarScoTransferToEarbud,       CONN_RULES_SCO_TRANSFER_TO_EARBUD),
    RULE(RULE_EVENT_PEER_IN_CASE,               ruleInCaseScoTransferToHandset,     CONN_RULES_SCO_TRANSFER_TO_HANDSET),
    /*! \} */
#ifdef INCLUDE_SCOFWD
    /*! \{
        Rules to control SCO forwarding. */
    RULE(RULE_EVENT_PEER_IN_EAR,                ruleScoForwardingControl,           CONN_RULES_SCO_FORWARDING_CONTROL),
    RULE(RULE_EVENT_PEER_OUT_EAR,               ruleScoForwardingControl,           CONN_RULES_SCO_FORWARDING_CONTROL),
    RULE(RULE_EVENT_SCO_ACTIVE,                 ruleScoForwardingControl,           CONN_RULES_SCO_FORWARDING_CONTROL),
    /*! \} */
#endif /* INCLUDE_SCOFWD */
    /*! \{
        Rules to control MIC forwarding. */
    RULE(RULE_EVENT_OUT_EAR,                    ruleSelectMicrophone,               CONN_RULES_SELECT_MIC),
    RULE(RULE_EVENT_IN_EAR,                     ruleSelectMicrophone,               CONN_RULES_SELECT_MIC),
    RULE(RULE_EVENT_PEER_IN_EAR,                ruleSelectMicrophone,               CONN_RULES_SELECT_MIC),
    RULE(RULE_EVENT_PEER_OUT_EAR,               ruleSelectMicrophone,               CONN_RULES_SELECT_MIC),
    /*! \} */

    /*! \{
        Rules to control local/remote audio mix. */
    RULE(RULE_EVENT_PEER_CONNECTED,             ruleSelectRemoteAudioMix,           CONN_RULES_SET_REMOTE_AUDIO_MIX),
    RULE(RULE_EVENT_OUT_EAR,                    ruleSelectRemoteAudioMix,           CONN_RULES_SET_REMOTE_AUDIO_MIX),
    RULE(RULE_EVENT_IN_EAR,                     ruleSelectRemoteAudioMix,           CONN_RULES_SET_REMOTE_AUDIO_MIX),
    RULE(RULE_EVENT_IN_CASE,                    ruleSelectRemoteAudioMix,           CONN_RULES_SET_REMOTE_AUDIO_MIX),
    RULE(RULE_EVENT_OUT_CASE,                   ruleSelectRemoteAudioMix,           CONN_RULES_SET_REMOTE_AUDIO_MIX),
    RULE(RULE_EVENT_PEER_CONNECTED ,            ruleSelectLocalAudioMix,            CONN_RULES_SET_LOCAL_AUDIO_MIX),
    RULE(RULE_EVENT_PEER_DISCONNECTED,          ruleSelectLocalAudioMix,            CONN_RULES_SET_LOCAL_AUDIO_MIX),
    RULE(RULE_EVENT_PEER_OUT_EAR,               ruleSelectLocalAudioMix,            CONN_RULES_SET_LOCAL_AUDIO_MIX),
    RULE(RULE_EVENT_PEER_IN_EAR,                ruleSelectLocalAudioMix,            CONN_RULES_SET_LOCAL_AUDIO_MIX),
    RULE(RULE_EVENT_PEER_IN_CASE,               ruleSelectLocalAudioMix,            CONN_RULES_SET_LOCAL_AUDIO_MIX),
    RULE(RULE_EVENT_PEER_OUT_CASE,              ruleSelectLocalAudioMix,            CONN_RULES_SET_LOCAL_AUDIO_MIX),
    /*! \} */

    /*! \{
        Rules to notify peer of MRU handset. */
    RULE(RULE_EVENT_HANDSET_A2DP_CONNECTED,     ruleNotifyPeerMruHandset,           CONN_RULES_NOTIFY_PEER_MRU_HANDSET),
    RULE(RULE_EVENT_HANDSET_HFP_CONNECTED,      ruleNotifyPeerMruHandset,           CONN_RULES_NOTIFY_PEER_MRU_HANDSET),
    RULE(RULE_EVENT_PEER_CONNECTED,             ruleNotifyPeerMruHandset,           CONN_RULES_NOTIFY_PEER_MRU_HANDSET),
    /*! \} */

    /*! \{
        Rules to notify peer out case/in ear to abort DFU. */
    RULE(RULE_EVENT_PEER_OUT_CASE,              ruleNotifyDFUAbort,                 CONN_RULES_DFU_ABORT),
    RULE(RULE_EVENT_PEER_IN_EAR,                ruleNotifyDFUAbort,                 CONN_RULES_DFU_ABORT),
    /*! \} */

};

/*! \brief Types of event that can cause connect rules to run. */
typedef enum
{
    RULE_CONNECT_USER,              /*!< User initiated connection */
    RULE_CONNECT_PAIRING,           /*!< Connect on startup */
    RULE_CONNECT_PEER_SYNC,         /*!< Peer sync complete initiated connection */
    RULE_CONNECT_PEER_EVENT,        /*!< Peer sync complete initiated connection */
    RULE_CONNECT_OUT_OF_CASE,       /*!< Out of case initiated connection */
    RULE_CONNECT_LINK_LOSS,         /*!< Link loss recovery initiated connection */
    RULE_CONNECT_PEER_OUT_OF_CASE,  /*!< Peer out of case initiated connection */
} ruleConnectReason;

/*****************************************************************************
 * RULES FUNCTIONS
 *****************************************************************************/
/*! @brief Rule to determine if Earbud should start automatic handset pairing
    @startuml

    start
        if (IsInCase()) then (yes)
            :Earbud is in case, do nothing;
            end
        endif
        if (IsPairedWithHandset()) then (yes)
            :Already paired with handset, do nothing;
            end
        endif
        if (IsPeerPairing()) then (yes)
            :Peer is already pairing, do nothing;
            end
        endif
        if (IsPeerPairWithHandset()) then (yes)
            :Peer is already paired with handset, do nothing;
            end
        endif
        if (IsPeerInCase()) then (yes)
            :Start pairing, peer is in case;
            stop
        endif

        :Both Earbuds are out of case;
        if (IsPeerLeftEarbud) then (yes)
            stop
        else (no)
            end
        endif
    @enduml
*/
/* DONE READY AS PRIMARY RULE */
static rule_action_t ruleAutoHandsetPair(void)
{
    /* NOTE: Ordering of these checks is important */

    if (appSmIsInCase())
    {
        PRIMARY_RULE_LOG("ruleAutoHandsetPair, ignore, we're in the case");
        return rule_action_ignore;
    }

    if (BtDevice_IsPairedWithHandset())
    {
        PRIMARY_RULE_LOG("ruleAutoHandsetPair, complete, already paired with handset");
        return rule_action_complete;
    }

    if (appTestHandsetPairingBlocked)
    {
        PRIMARY_RULE_LOG("ruleAutoHandsetPair, ignore, blocked by test command");
        return rule_action_ignore;
    }

    if (StateProxy_IsPeerPairing())
    {
        PRIMARY_RULE_LOG("ruleAutoHandsetPair, defer, peer is already in pairing mode");
        return rule_action_defer;
    }

    if (appSmIsPairing())
    {
        PRIMARY_RULE_LOG("ruleAutoHandsetPair, ignore, already in pairing mode");
        return rule_action_ignore;
    }

    if (StateProxy_HasPeerHandsetPairing())
    {
        PRIMARY_RULE_LOG("ruleAutoHandsetPair, complete, peer is already paired with handset");
        return rule_action_complete;
    }

    if (!appSmIsPrimary())
    {
        PRIMARY_RULE_LOG("ruleAutoHandsetPair, ignore, not a primary Earbud");
        return rule_action_ignore;
    }

    PRIMARY_RULE_LOG("ruleAutoHandsetPair, run, primary out of the case with no handset pairing");
    return rule_action_run;
}

/*! @brief Rule to determine if Earbud should attempt to forward handset link-key to peer
    @startuml

    start
        if (IsPairedWithPeer()) then (yes)
            :Forward any link-keys to peer;
            stop
        else (no)
            :Not paired;
            end
    @enduml
*/
static rule_action_t ruleForwardLinkKeys(void)
{
    if (BtDevice_IsPairedWithPeer())
    {
        PRIMARY_RULE_LOG("ruleForwardLinkKeys, run");
        return rule_action_run;
    }
    else
    {
        PRIMARY_RULE_LOG("ruleForwardLinkKeys, ignore as there's no peer");
        return rule_action_ignore;
    }
}

/*! @brief Rule to determine if A2DP streaming when out of ear
    Rule is triggered by the 'out of ear' event
    @startuml

    start
    if (IsAvStreaming()) then (yes)
        :Run rule, as out of ear with A2DP streaming;
        stop
    endif
    end
    @enduml
*/
static rule_action_t ruleOutOfEarA2dpActive(void)
{
    if (appAvIsStreaming())
    {
        PRIMARY_RULE_LOG("ruleOutOfEarA2dpActive, run as A2DP is active and earbud out of ear");
        return rule_action_run;
    }

    PRIMARY_RULE_LOG("ruleOutOfEarA2dpActive, ignore as A2DP not active out of ear");
    return rule_action_ignore;
}

/*! \brief In ear event on either Earbud should cancel A2DP or SCO out of ear pause/tranfer. */
static rule_action_t ruleInEarCancelAudioPause(void)
{
    PRIMARY_RULE_LOG("ruleInEarCancelAudioPause, run as always to cancel timers on in ear event");
    /* Always running looks a little odd, but it means we have a common path for handling
     * either the local earbud in ear or in case, and the same events from the peer.
     * The alternative would be phystate event handling in SM for local events and 
     * state proxy events handling in SM for peer in ear/case. */
    return rule_action_run;
}

/*! \brief Decide if we should restart A2DP on going back in the ear within timeout. */
static rule_action_t ruleInEarA2dpRestart(void)
{
    if (appSmIsA2dpRestartPending())
    {
        PRIMARY_RULE_LOG("ruleInEarA2dpRestart, run as A2DP is paused within restart timer");
        return rule_action_run;
    }
    PRIMARY_RULE_LOG("ruleInEarA2dpRestart, ignore as A2DP restart timer not running");
    return rule_action_ignore;
}

/*! @brief Rule to determine if SCO active when out of ear
    Rule is triggered by the 'out of ear' event
    @startuml

    start
    if (IsScoActive()) then (yes)
        :Run rule, as out of ear with SCO active;
        stop
    endif
    end
    @enduml
*/
static rule_action_t ruleOutOfEarScoActive(void)
{
    if (ScoFwdIsSending() && StateProxy_IsPeerInEar())
    {
        PRIMARY_RULE_LOG("ruleOutOfEarScoActive, ignore as we have SCO forwarding running and peer is in ear");
        return rule_action_ignore;
    }

    if (appHfpIsScoActive() && appSmIsOutOfEar())
    {
        PRIMARY_RULE_LOG("ruleOutOfEarScoActive, run as SCO is active and earbud out of ear");
        return rule_action_run;
    }

    PRIMARY_RULE_LOG("ruleOutOfEarScoActive, ignore as SCO not active out of ear");
    return rule_action_ignore;
}


static rule_action_t ruleInEarScoTransferToEarbud(void)
{
    /* Check HFP state first, so if no active call we avoid unnecessary peer sync */
    if (!appHfpIsCallActive())
    {
        PRIMARY_RULE_LOG("ruleInEarScoTransferToEarbud, ignore as this earbud has no active call");
        return rule_action_ignore;
    }

    /* May already have SCO audio if kept while out of ear in order to service slave
     * for SCO forwarding */
    if (appHfpIsScoActive())
    {
        PRIMARY_RULE_LOG("ruleInEarScoTransferToEarbud, ignore as this earbud already has SCO");
        return rule_action_ignore;
    }

    /* For TWS+ transfer the audio the local earbud is in Ear.
     * For TWS Standard, transfer the audio if local earbud or peer is in Ear. */
    if (appSmIsInEar() || (!appDeviceIsTwsPlusHandset(appHfpGetAgBdAddr()) && StateProxy_IsPeerInEar()))
    {
        PRIMARY_RULE_LOG("ruleInEarScoTransferToEarbud, run as call is active and an earbud is in ear");
        return rule_action_run;
    }

    PRIMARY_RULE_LOG("ruleInEarScoTransferToEarbud, ignore as SCO not active or both earbuds out of the ear");
    return rule_action_ignore;
}

/*! @brief Rule to determine if LED should be enabled when out of ear
    Rule is triggered by the 'out of ear' event
    @startuml

    start
    if (Not IsLedsInEarEnabled()) then (yes)
        :Run rule, as out of ear and LEDs were disabled in ear;
        stop
    endif
    end
    @enduml
*/
static rule_action_t ruleOutOfEarLedsEnable(void)
{
    if (!appConfigInEarLedsEnabled())
    {
        PRIMARY_RULE_LOG("ruleOutOfEarLedsEnable, run as out of ear");
        return rule_action_run;
    }
    else
    {
        PRIMARY_RULE_LOG("ruleOutOfEarLedsEnable, ignore as out of ear but in ear LEDs enabled");
        return rule_action_ignore;
    }
}

/*! @brief Rule to determine if LED should be disabled when in ear
    Rule is triggered by the 'in ear' event
    @startuml

    start
    if (Not IsLedsInEarEnabled()) then (yes)
        :Run rule, as in ear and LEDs are disabled in ear;
        stop
    endif
    end
    @enduml
*/
static rule_action_t ruleInEarLedsDisable(void)
{
    if (!appConfigInEarLedsEnabled())
    {
        PRIMARY_RULE_LOG("ruleInEarLedsDisable, run as in ear");
        return rule_action_run;
    }
    else
    {
        PRIMARY_RULE_LOG("ruleInEarLedsDisable, ignore as in ear but in ear LEDs enabled");
        return rule_action_ignore;
    }
}

static rule_action_t ruleInEarAncEnable(void)
{
    PRIMARY_RULE_LOG("ruleInEarAncEnable, run");
    return rule_action_run;
}

static rule_action_t ruleOutOfEarAncDisable(void)
{
    PRIMARY_RULE_LOG("ruleOutOfEarAncDisable, run");
    return rule_action_run;
}

/*! @brief Determine if a handset disconnect should be allowed */
static bool handsetDisconnectAllowed(void)
{
//    return appDeviceIsHandsetConnected() && !appSmIsDfuPending() && !ScoFwdIsSending();
    return appDeviceIsHandsetAnyProfileConnected() && !appSmIsDfuPending() && !ScoFwdIsSending();
}

/*! @brief Rule to connect A2DP to TWS+ handset if peer Earbud has connected A2DP for the first time */
static rule_action_t rulePairingConnectTwsPlusA2dp(void)
{
    bdaddr handset_addr;
    bdaddr peer_handset_addr;
    PrimaryRulesTaskData* primary_rules = PrimaryRulesGetTaskData();

    if (!appSmIsOutOfCase())
    {
        PRIMARY_RULE_LOG("rulePairingConnectTwsPlusA2dp, ignore, not out of case");
        return rule_action_ignore;
    }

    appDeviceGetHandsetBdAddr(&handset_addr);
    StateProxy_GetPeerHandsetAddr(&peer_handset_addr);

    if (!appDeviceIsTwsPlusHandset(&handset_addr))
    {
        PRIMARY_RULE_LOG("rulePairingConnectTwsPlusA2dp, ignore, not TWS+ handset");
        return rule_action_ignore;
    }

    if (!BdaddrIsSame(&handset_addr, &peer_handset_addr))
    {
        PRIMARY_RULE_LOG("rulePairingConnectTwsPlusA2dp, ignore, not same handset as peer");
        return rule_action_ignore;
    }

    if (appDeviceIsHandsetA2dpConnected())
    {
        PRIMARY_RULE_LOG("rulePairingConnectTwsPlusA2dp, ignore, A2DP already connected");
        return rule_action_ignore;
    }

    uint8 profiles = DEVICE_PROFILE_A2DP;
    PRIMARY_RULE_LOG("rulePairingConnectTwsPlusA2dp, connect A2DP");
    return RULE_ACTION_RUN_PARAM(profiles);
}

/*! @brief Rule to connect HFP to TWS+ handset if peer Earbud has connected HFP for the first time */
static rule_action_t rulePairingConnectTwsPlusHfp(void)
{
    bdaddr handset_addr;
    bdaddr peer_handset_addr;
    PrimaryRulesTaskData* primary_rules = PrimaryRulesGetTaskData();

    if (!appSmIsOutOfCase())
    {
        PRIMARY_RULE_LOG("rulePairingConnectTwsPlusHfp, ignore, not out of case");
        return rule_action_ignore;
    }

    appDeviceGetHandsetBdAddr(&handset_addr);
    StateProxy_GetPeerHandsetAddr(&peer_handset_addr);

    if (!appDeviceIsTwsPlusHandset(&handset_addr))
    {
        PRIMARY_RULE_LOG("rulePairingConnectTwsPlusHfp, ignore, not TWS+ handset");
        return rule_action_ignore;
    }

    if (!BdaddrIsSame(&handset_addr, &peer_handset_addr))
    {
        PRIMARY_RULE_LOG("rulePairingConnectTwsPlusHfp, ignore, not same handset as peer");
        return rule_action_ignore;
    }

    if (appDeviceIsHandsetHfpConnected())
    {
        PRIMARY_RULE_LOG("rulePairingConnectTwsPlusHfp, ignore, HFP already connected");
        return rule_action_ignore;
    }

    uint8 profiles = DEVICE_PROFILE_HFP;
    PRIMARY_RULE_LOG("rulePairingConnectTwsPlusA2dp, connect HFP");
    return RULE_ACTION_RUN_PARAM(profiles);
}

/*! @brief Rule to determine if Earbud should start DFU  when put in case
    Rule is triggered by the 'in case' event
    @startuml

    start
    if (IsInCase() and DfuUpgradePending()) then (yes)
        :DFU upgrade can start as it was pending and now in case;
        stop
    endif
    end
    @enduml
*/
static rule_action_t ruleInCaseEnterDfu(void)
{
#ifdef INCLUDE_DFU
    if (appSmIsInCase() && appSmIsDfuPending())
    {
        PRIMARY_RULE_LOG("ruleInCaseCheckDfu, run as still in case & DFU pending/active");
        return rule_action_run;
    }
    else
    {
        PRIMARY_RULE_LOG("ruleInCaseCheckDfu, ignore as not in case or no DFU pending");
        return rule_action_ignore;
    }
#else
    return rule_action_ignore;
#endif
}


static rule_action_t ruleDfuAllowHandsetConnect(void)
{
#ifdef INCLUDE_DFU
    bdaddr handset_addr;

    /* If we're already connected to handset then don't connect */
    if (appDeviceIsHandsetConnected())
    {
        PRIMARY_RULE_LOG("ruleDfuAllowHandsetConnect, ignore as already connected to handset");
        return rule_action_ignore;
    }
    /* This rule has been run when entering the special DFU mode.
       This should only be entered when we restart during an update, or if
       the user has requested it (only possible if appConfigDfuOnlyFromUiInCase()
       is TRUE */
    if (appConfigDfuOnlyFromUiInCase())
    {
        PRIMARY_RULE_LOG("ruleDfuAllowHandsetConnect - run as use in case DFU");
        return rule_action_run;
    }
    if (appSmIsInDfuMode())
    {
        PRIMARY_RULE_LOG("ruleDfuAllowHandsetConnect, run as in DFU mode");
        return rule_action_run;
    }
    PRIMARY_RULE_LOG("ruleDfuAllowHandsetConnect, ignore as not DFU");
#endif /* INCLUDE_DFU */

    return rule_action_ignore;
}


static rule_action_t ruleCheckUpgradable(void)
{
    bool allow_dfu = TRUE;
    bool block_dfu = FALSE;

    if (appSmIsOutOfCase())
    {
        if (appConfigDfuOnlyFromUiInCase())
        {
            PRIMARY_RULE_LOG("ruleCheckUpgradable, block as only allow DFU from UI (and in case)");
            return RULE_ACTION_RUN_PARAM(block_dfu);
        }
        if (ConManagerAnyTpLinkConnected(cm_transport_ble) && appConfigDfuAllowBleUpgradeOutOfCase())
        {
            PRIMARY_RULE_LOG("ruleCheckUpgradable, allow as BLE connection");
            return RULE_ACTION_RUN_PARAM(allow_dfu);
        }
        if (appDeviceIsHandsetConnected() && appConfigDfuAllowBredrUpgradeOutOfCase())
        {
            PRIMARY_RULE_LOG("ruleCheckUpgradable, allow as BREDR connection");
            return RULE_ACTION_RUN_PARAM(allow_dfu);
        }

        PRIMARY_RULE_LOG("ruleCheckUpgradable, block as out of case and not permitted");
        return RULE_ACTION_RUN_PARAM(block_dfu);
    }
    else
    {
        if (appConfigDfuOnlyFromUiInCase())
        {
            if (appSmIsDfuPending())
            {
                PRIMARY_RULE_LOG("ruleCheckUpgradable, allow as in case - DFU pending");
                return RULE_ACTION_RUN_PARAM(allow_dfu);
            }
            PRIMARY_RULE_LOG("ruleCheckUpgradable, block as only allow DFU from UI");
            return RULE_ACTION_RUN_PARAM(block_dfu);
        }

        if (!StateProxy_IsPeerInCase())
        {
            PRIMARY_RULE_LOG("ruleCheckUpgradable, block as peer is not in-case and not permitted");
            return RULE_ACTION_RUN_PARAM(block_dfu);
        }

        PRIMARY_RULE_LOG("ruleCheckUpgradable, allow as in case");
        return RULE_ACTION_RUN_PARAM(allow_dfu);
    }
}

static rule_action_t ruleOutOfCaseAllowHandsetConnect(void)
{
    PRIMARY_RULE_LOG("ruleOutOfCaseAllowHandsetConnect, run as out of case");
    return rule_action_run;
}

static rule_action_t ruleInCaseRejectHandsetConnect(void)
{
#ifdef INCLUDE_DFU
    if (appSmIsDfuPending())
    {
        PRIMARY_RULE_LOG("ruleInCaseRejectHandsetConnect, ignored as DFU pending");
        return rule_action_ignore;
    }
#endif

    PRIMARY_RULE_LOG("ruleInCaseRejectHandsetConnect, run as in case and no DFU");
    return rule_action_run;
}

static rule_action_t ruleInCaseAncTuning(void)
{
    if (appConfigAncTuningEnabled())
        return rule_action_run;
    else
        return rule_action_ignore;
}

static rule_action_t ruleOutOfCaseAncTuning(void)
{
    if (appConfigAncTuningEnabled())
        return rule_action_run;
    else
        return rule_action_ignore;
}

static rule_action_t ruleInCaseScoTransferToHandset(void)
{
    if (!appHfpIsScoActive())
    {
        PRIMARY_RULE_LOG("ruleInCaseScoTransferToHandset, ignore as no active call");
        return rule_action_ignore;
    }
    if (appSmIsInCase() && StateProxy_IsPeerInCase())
    {
        PRIMARY_RULE_LOG("ruleInCaseScoTransferToHandset, run, we have active call but both earbuds in case");
        return rule_action_run;
    }
    PRIMARY_RULE_LOG("ruleInCaseScoTransferToHandset, ignored");
    return rule_action_ignore;
}

static rule_action_t ruleSelectMicrophone(void)
{
    micSelection selected_mic = MIC_SELECTION_LOCAL;

    if (!appSmIsInEar() && StateProxy_IsPeerInEar())
    {
        selected_mic = MIC_SELECTION_REMOTE;
        PRIMARY_RULE_LOG("ruleSelectMicrophone, SCOFWD master out of ear and slave in ear use remote microphone");
        return RULE_ACTION_RUN_PARAM(selected_mic);
    }
    if (appSmIsInEar())
    {
        PRIMARY_RULE_LOG("ruleSelectMicrophone, SCOFWD master in ear, use local microphone");
        return RULE_ACTION_RUN_PARAM(selected_mic);
    }

    PRIMARY_RULE_LOG("ruleSelectMicrophone, ignore as both earbuds out of ear");
    return rule_action_ignore;
}

static rule_action_t ruleScoForwardingControl(void)
{
    const bool forwarding_enabled = TRUE;
    const bool forwarding_disabled = FALSE;

    /* Always ignore if Sco forwarding is not enabled */
    if (!appConfigScoForwardingEnabled())
    {
        PRIMARY_RULE_LOG("ruleScoForwardingControl, ignore as Sco forwarding disabled");
        return rule_action_ignore;
    }

    /* only need to consider this rule if we have SCO audio on the earbud */
    if (!appHfpIsScoActive())
    {
        PRIMARY_RULE_LOG("ruleScoForwardingControl, ignore as no active SCO");
        return rule_action_ignore;
    }
    if (!StateProxy_IsPeerInEar())
    {
        PRIMARY_RULE_LOG("ruleScoForwardingControl, run and disable forwarding as peer out of ear");
        return RULE_ACTION_RUN_PARAM(forwarding_disabled);
    }
    if (StateProxy_IsPeerInEar())
    {
        PRIMARY_RULE_LOG("ruleScoForwardingControl, run and enable forwarding as peer in ear");
        return RULE_ACTION_RUN_PARAM(forwarding_enabled);
    }

    PRIMARY_RULE_LOG("ruleScoForwardingControl, ignore");
    return rule_action_ignore;
}

/*! @brief Rule to select the audio mix to be rendered by the remote earbud */
static rule_action_t ruleSelectLocalAudioMix(void)
{
    bool stereo_mix = TRUE;

    if (appPeerSigIsConnected() && StateProxy_IsPeerInEar())
    {
        stereo_mix = FALSE;
    }

    PRIMARY_RULE_LOG("ruleSelectLocalAudioMix stereo_mix=%d", stereo_mix);
    return RULE_ACTION_RUN_PARAM(stereo_mix);
}

/*! @brief Rule to select the audio mix to be rendered by the local earbud */
static rule_action_t ruleSelectRemoteAudioMix(void)
{
    bool stereo_mix = TRUE;

    if (appPeerSigIsConnected())
    {
        if (appSmIsInEar())
        {
            stereo_mix = FALSE;
        }
        PRIMARY_RULE_LOG("ruleSelectRemoteAudioMix stereo_mix=%d", stereo_mix);
        return RULE_ACTION_RUN_PARAM(stereo_mix);
    }
    else
    {
        return rule_action_ignore;
    }
}

/*! @brief Rule to determine if we send MRU handset update to peer */
static rule_action_t ruleNotifyPeerMruHandset(void)
{
    rule_action_t action = rule_action_ignore;

    if (appPeerSigIsConnected() && BtDevice_IsPairedWithHandset())
    {
        action = rule_action_run;
    }

    PRIMARY_RULE_LOG("ruleNotifyPeerMruHandset result = %d", action);

    return action;
}

/*! @brief Rule to determine if we abort DFU if peer state changes */
static rule_action_t ruleNotifyDFUAbort(void)
{
    rule_action_t action = rule_action_ignore;

    if (SmGetTaskData()->dfu_in_progress)
    {
        action = rule_action_run;
    }

    PRIMARY_RULE_LOG("ruleNotifyDFUAbort result = %d", action);

    return action;
}

/*****************************************************************************
 * END RULES FUNCTIONS
 *****************************************************************************/

/*! \brief Initialise the primary rules module. */
bool PrimaryRules_Init(Task init_task)
{
    PrimaryRulesTaskData *primary_rules = PrimaryRulesGetTaskData();
    rule_set_init_params_t rule_params;

    UNUSED(init_task);

    memset(&rule_params, 0, sizeof(rule_params));
    rule_params.rules = primary_rules_set;
    rule_params.rules_count = ARRAY_DIM(primary_rules_set);
    rule_params.nop_message_id = CONN_RULES_NOP;
    primary_rules->rule_set = RulesEngine_CreateRuleSet(&rule_params);

    return TRUE;
}

rule_set_t PrimaryRules_GetRuleSet(void)
{
    PrimaryRulesTaskData *primary_rules = PrimaryRulesGetTaskData();
    return primary_rules->rule_set;
}

void PrimaryRules_SetEvent(Task client_task, rule_events_t event_mask)
{
    PrimaryRulesTaskData *primary_rules = PrimaryRulesGetTaskData();
    RulesEngine_SetEvent(primary_rules->rule_set, client_task, event_mask);
}

void PrimaryRules_ResetEvent(rule_events_t event)
{
    PrimaryRulesTaskData *primary_rules = PrimaryRulesGetTaskData();
    RulesEngine_ResetEvent(primary_rules->rule_set, event);
}

rule_events_t PrimaryRules_GetEvents(void)
{
    PrimaryRulesTaskData *primary_rules = PrimaryRulesGetTaskData();
    return RulesEngine_GetEvents(primary_rules->rule_set);
}

void PrimaryRules_SetRuleComplete(MessageId message)
{
    PrimaryRulesTaskData *primary_rules = PrimaryRulesGetTaskData();
    RulesEngine_SetRuleComplete(primary_rules->rule_set, message);
}

void PrimaryRulesSetRuleWithEventComplete(MessageId message, rule_events_t event)
{
    PrimaryRulesTaskData *primary_rules = PrimaryRulesGetTaskData();
    RulesEngine_SetRuleWithEventComplete(primary_rules->rule_set, message, event);
}

/*! \brief Copy rule param data for the engine to put into action messages.
    \param param Pointer to data to copy.
    \param size_param Size of the data in bytes.
    \return rule_action_run_with_param to indicate the rule action message needs parameters.
 */
static rule_action_t PrimaryRulesCopyRunParams(const void* param, size_t size_param)
{
    PrimaryRulesTaskData *primary_rules = PrimaryRulesGetTaskData();
    RulesEngine_CopyRunParams(primary_rules->rule_set, param, size_param);
    return rule_action_run_with_param;
}
