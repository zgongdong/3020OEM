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

#include <domain_message.h>
#include <av.h>
#include <phy_state.h>
#include <bt_device.h>
#include <bredr_scan_manager.h>
#include <hfp_profile.h>
#include <scofwd_profile.h>
#include <state_proxy.h>
#include <rules_engine.h>

#include <bdaddr.h>
#include <panic.h>
#include <system_clock.h>

#pragma unitsuppress Unused

/*! \{
    Macros for diagnostic output that can be suppressed.
    Allows debug of the rules module at two levels. */
#define PRIMARY_RULES_LOG(x)       //DEBUG_LOG(x)
#define PRIMARY_RULES_LOGF(x, ...) //DEBUG_LOGF(x, __VA_ARGS__)

#define PRIMARY_RULE_LOG(x)         DEBUG_LOG(x)
#define PRIMARY_RULE_LOGF(x, ...)   DEBUG_LOGF(x, __VA_ARGS__)
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
 DEFINE_RULE(rulePeerPair);
DEFINE_RULE(ruleAutoHandsetPair);
DEFINE_RULE(ruleDecideRole);
DEFINE_RULE(ruleForwardLinkKeys);

 DEFINE_RULE(rulePeerEventConnectHandset);
 DEFINE_RULE(ruleSyncConnectPeer);
 DEFINE_RULE(ruleSyncDisconnectPeer);
 DEFINE_RULE(ruleSyncDisconnectHandset);
 DEFINE_RULE(ruleDisconnectPeer);
 DEFINE_RULE(rulePairingConnectPeerHandset);

 DEFINE_RULE(ruleUserConnectHandset);
 DEFINE_RULE(ruleUserConnectPeerHandset);
 DEFINE_RULE(ruleUserConnectPeer);

 DEFINE_RULE(ruleOutOfCaseConnectHandset);
 DEFINE_RULE(ruleOutOfCaseConnectPeer);

 DEFINE_RULE(ruleLinkLossConnectHandset);
 DEFINE_RULE(ruleLinkLossConnectPeerHandset);

 DEFINE_RULE(ruleUpdateMruHandset);
 DEFINE_RULE(ruleSendStatusToHandset);
DEFINE_RULE(ruleOutOfEarA2dpActive);
DEFINE_RULE(ruleInEarCancelAudioPause);
DEFINE_RULE(ruleInEarA2dpRestart);
DEFINE_RULE(ruleOutOfEarScoActive);
DEFINE_RULE(ruleInEarScoTransferToEarbud);
DEFINE_RULE(ruleOutOfEarLedsEnable);
DEFINE_RULE(ruleInEarLedsDisable);
 DEFINE_RULE(ruleInCaseDisconnectHandset);
 DEFINE_RULE(ruleInCaseDisconnectPeer);
 DEFINE_RULE(rulePeerInCaseDisconnectPeer);
DEFINE_RULE(ruleInCaseEnterDfu);
DEFINE_RULE(ruleOutOfCaseAllowHandsetConnect);
DEFINE_RULE(ruleInCaseRejectHandsetConnect);

DEFINE_RULE(ruleDfuAllowHandsetConnect);
DEFINE_RULE(ruleCheckUpgradable);

DEFINE_RULE(rulePageScanUpdate);

DEFINE_RULE(ruleInCaseScoTransferToHandset);
DEFINE_RULE(ruleSelectMicrophone);
DEFINE_RULE(ruleScoForwardingControl);

 DEFINE_RULE(ruleBothConnectedDisconnect);

 DEFINE_RULE(rulePairingConnectTwsPlusA2dp);
 DEFINE_RULE(rulePairingConnectTwsPlusHfp);

DEFINE_RULE(ruleInEarAncEnable);
DEFINE_RULE(ruleOutOfEarAncDisable);

 DEFINE_RULE(ruleHandoverDisconnectHandset);
 DEFINE_RULE(ruleHandoverConnectHandset);
 DEFINE_RULE(ruleHandoverConnectHandsetAndPlay);

DEFINE_RULE(ruleOutOfCaseAncTuning);
DEFINE_RULE(ruleInCaseAncTuning);

DEFINE_RULE(ruleBleConnectionUpdate);
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

/*! @brief Rule to determine if Earbud should start automatic peer pairing
    This rule determins if automatic peer pairing should start, it is triggered
    by the startup event.
    @startuml

    start
        if (IsPairedWithPeer()) then (no)
            :Start peer pairing;
            end
        else (yes)
            :Already paired;
            stop
    @enduml
*/
static rule_action_t rulePeerPair(void)
{
    if (!BtDevice_IsPairedWithPeer())
    {
        PRIMARY_RULE_LOG("ruleStartupPeerPaired, run");
        return rule_action_run;
    }
    else
    {
        PRIMARY_RULE_LOG("ruleStartupPeerPaired, done");
        return rule_action_complete;
    }
}

/*! @brief Rule to determine if this Earbud is the primary.
 *
 * \note This is a temporary rule to emulate a simple role
 * selection for use in determining which rule set to use.
 * It will be removed once we have the full role selection
 * implementation.
 *
 * Decision on Primary is determined by the following truth table.
 * (X=Don't care).
 *
 * A = this Earbud out of case
 * B = peer Earbud out of case
 * C = this Earbud is left
 *
 * A | B | C | Is this Earbud (A) Primary
 * - | - | - | --------------------------
 * 1 | 0 | X | 1
 * 0 | 1 | X | 0
 * 0 | 0 | X | 0
 * 1 | 1 | 1 | 1
 * 1 | 1 | 0 | 0
 */
static rule_action_t ruleDecideRole(void)
{
    const bool primary_role = TRUE;
    const bool secondary_role = FALSE;

    if (appSmIsOutOfCase())
    {
        PRIMARY_RULE_LOG("ruleDecideRole out of the case -> DECIDE ROLE");
        return RULE_ACTION_RUN_PARAM(primary_role);
    }
    else if(appSmIsInCase() && appSmIsDfuPending())
    {
    	   return rule_action_ignore;
    }
    else
    {
        PRIMARY_RULE_LOG("ruleDecideRole in the case -> SECONDARY ROLE");
        return RULE_ACTION_RUN_PARAM(secondary_role);
    }
}

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

/*! @brief Sub-rule to determine if Earbud should connect to standard handset
*/
static rule_action_t ruleConnectHandsetStandard(ruleConnectReason reason)
{
    if ((reason == RULE_CONNECT_USER) ||
        (reason == RULE_CONNECT_LINK_LOSS) ||
        (reason == RULE_CONNECT_PEER_EVENT) ||
        (reason == RULE_CONNECT_OUT_OF_CASE))
    {
        /* Check if out of case */
        if (appSmIsOutOfCase())
        {
            PRIMARY_RULE_LOG("ruleConnectHandsetStandard, run Primary Earbud with standard handset and out of case");
            return rule_action_run;
        }
    }
    PRIMARY_RULE_LOGF("ruleConnectHandsetStandard, ignore for reason %u or in-case", reason);
    return rule_action_ignore;
}

/*! @brief Rule to determine if Earbud should connect to Handset
    @startuml

    start
    if (IsInCase()) then (yes)
        :Never connect when in case;
        end
    endif

    if (IsPeerSyncComplete()) then (yes)
        if (Not IsPairedWithHandset()) then (yes)
            :Not paired with handset, don't connect;
            end
        endif
        if (IsHandsetA2dpConnected() and IsHandsetAvrcpConnected() and IsHandsetHfpConnected()) then (yes)
            :Already connected;
            end
        endif

        if (IsTwsPlusHandset()) then (yes)
            :Handset is a TWS+ device;
            if (WasConnected() or Reason is 'User', 'Start-up' or 'Out of Case') then (yes)
                if (not just paired) then (yes)
                    :Connect to handset;
                    end
                else
                    :Just paired, handset will connect to us;
                    stop
                endif
            else (no)
                :Wasn't connected before;
                stop
            endif
        else (no)
            if (IsPeerConnectedA2dp() or IsPeerConnectedAvrcp() or IsPeerConnectedHfp()) then (yes)
                :Peer already has profile(s) connected, don't connect;
                stop
            else (no)
                if (WasConnected() or Reason is 'User', 'Start-up' or 'Out of Case') then (yes)
                    :run RuleConnectHandsetStandard();
                    end
                else (no)
                    :Wasn't connected before;
                    stop
                endif
            endif
        endif
    else (no)
        :Not sync'ed with peer;
        if (IsPairedWithHandset() and IsHandsetTwsPlus() and WasConnected()) then (yes)
            :Connect to handset, it is TWS+ handset;
            stop
        else (no)
            :Don't connect, not TWS+ handset;
            end
        endif
    endif

    @enduml
*/
static rule_action_t ruleConnectHandset(ruleConnectReason reason,
                                        rulePostHandsetConnectAction post_connect_action)
{
    bdaddr handset_addr;
    PrimaryRulesTaskData* primary_rules = PrimaryRulesGetTaskData();
    CONN_RULES_CONNECT_HANDSET_T action = {0, post_connect_action};

    PRIMARY_RULE_LOGF("ruleConnectHandset, reason %u", reason);

    /* Don't attempt to connect if we're in the case */
    if (appSmIsInCase())
    {
        PRIMARY_RULE_LOG("ruleConnectHandset, ignore as in case");
        return rule_action_ignore;
    }

    /* Don't attempt to connect if we're pairing */
    if (appSmIsPairing())
    {
        PRIMARY_RULE_LOG("ruleConnectHandset, ignore as pairing");
        return rule_action_ignore;
    }

    if (StateProxy_IsPeerDfuInProgress())
    {
        PRIMARY_RULE_LOG("ruleConnectHandset, ignore as peer has DFU in progress");
        return rule_action_ignore;
    }

    /* Don't attempt to connect if peer is pairing */
    if (StateProxy_IsPeerPairing())
    {
        PRIMARY_RULE_LOG("ruleConnectHandset, ignore as peer is pairing");
        return rule_action_ignore;
    }

    /* If we're not paired with handset then don't connect */
    if (!appDeviceGetHandsetBdAddr(&handset_addr))
    {
        PRIMARY_RULE_LOG("ruleConnectHandset, ignore as not paired with handset");
        return rule_action_ignore;
    }

    /* If we're already connected to handset then don't connect */
    if (appDeviceIsHandsetA2dpConnected() && appDeviceIsHandsetAvrcpConnected() && appDeviceIsHandsetHfpConnected())
    {
        PRIMARY_RULE_LOG("ruleConnectHandset, ignore as already connected to handset");
        return rule_action_ignore;
    }

    /* Peer is not connected to handset, so we should connect to our handset if it's a TWS+ handset or
       it's a standard handset and our battery level is higer */

    /* Check if TWS+ handset, if so just connect, otherwise compare battery levels
     * if we have higher battery level connect to handset */
    if (appDeviceIsTwsPlusHandset(&handset_addr))
    {
        /* this call will read persistent store, so just called once and re-use
         * results */
        action.profiles = BtDevice_GetLastConnectedProfiles(&handset_addr);

        /* Always attempt to connect HFP and A2DP if user initiated connect, or out-of-case connect, or pairing connect */
        if ((reason == RULE_CONNECT_OUT_OF_CASE) || (reason == RULE_CONNECT_USER) || (reason == RULE_CONNECT_PAIRING))
            action.profiles |= DEVICE_PROFILE_HFP | DEVICE_PROFILE_A2DP;

        /* Check if device was connected before, or we connecting due to user request */
        if (action.profiles != 0 ||
                (reason == RULE_CONNECT_USER) ||
                (reason == RULE_CONNECT_PAIRING) ||
                (reason == RULE_CONNECT_OUT_OF_CASE))
        {
            if (/*primary_rules->allow_connect_after_pairing ||*/ !BtDevice_IsJustPaired(&handset_addr))
            {
                PRIMARY_RULE_LOGF("ruleConnectHandset, run as TWS+ handset for profiles:%u", action.profiles);
                /*! \todo generate command to secondary to connect to TWS+ handset? */
                return RULE_ACTION_RUN_PARAM(action);
            }
            else
            {
                PRIMARY_RULE_LOG("ruleConnectHandset, ignore as just paired with TWS+ handset");
                return rule_action_ignore;
            }
        }
        else
        {
            PRIMARY_RULE_LOG("ruleConnectHandset, ignore as TWS+ handset but wasn't connected before");
            return rule_action_ignore;
        }
    }
    else
    {
        /* Check if device was connected before, or we connecting due to user request or startup */
        if (BtDevice_GetWasConnected(&handset_addr) ||
                (reason == RULE_CONNECT_USER) ||
                (reason == RULE_CONNECT_PAIRING) ||
                (reason == RULE_CONNECT_OUT_OF_CASE) ||
                (reason == RULE_CONNECT_PEER_EVENT))
        {
            action.profiles = BtDevice_GetLastConnectedProfiles(&handset_addr);

            /* Always attempt to connect HFP and A2DP if user initiated connect, or out-of-case connect, or pairing connect */
            if ((reason == RULE_CONNECT_OUT_OF_CASE) || (reason == RULE_CONNECT_USER) || (reason == RULE_CONNECT_PAIRING) || (reason == RULE_CONNECT_PEER_EVENT))
                action.profiles |= DEVICE_PROFILE_HFP | DEVICE_PROFILE_A2DP;

            PRIMARY_RULE_LOG("ruleConnectHandset, calling ruleConnectHandsetStandard()");
            if (ruleConnectHandsetStandard(reason) == rule_action_run)
            {
                PRIMARY_RULE_LOG("ruleConnectHandset, run as standard handset we were connected to before");
                return RULE_ACTION_RUN_PARAM(action);
            }
            else
            {
                PRIMARY_RULE_LOGF("ruleConnectHandset, ignore, standard handset but not connected before or reason %u", reason);
                return rule_action_ignore;
            }
        }
        else
        {
            PRIMARY_RULE_LOGF("ruleConnectHandset, ignore as standard handset but wasn't connected before or reason %u", reason);
            return rule_action_ignore;
        }
    }
}

/*! @brief Wrapper around ruleConnectHandset() that calls it with 'Peer sync' connect reason
*/
static rule_action_t rulePeerEventConnectHandset(void)
{
    PRIMARY_RULE_LOG("rulePeerEventConnectHandset");
    return ruleConnectHandset(RULE_CONNECT_PEER_EVENT, RULE_POST_HANDSET_CONNECT_ACTION_NONE);
}

/*! @brief Wrapper around ruleConnectHandset() that calls it with 'User' connect reason
*/
static rule_action_t ruleUserConnectHandset(void)
{
    PRIMARY_RULE_LOG("ruleUserConnectHandset");
    return ruleConnectHandset(RULE_CONNECT_USER, RULE_POST_HANDSET_CONNECT_ACTION_PLAY_MEDIA);
}

/*! @brief Wrapper around ruleConnectHandset() that calls it with 'Out of case' connect reason
*/
static rule_action_t ruleOutOfCaseConnectHandset(void)
{
    PRIMARY_RULE_LOG("ruleOutOfCaseConnectHandset");
    return ruleConnectHandset(RULE_CONNECT_OUT_OF_CASE, RULE_POST_HANDSET_CONNECT_ACTION_NONE);
}

/*! @brief Wrapper around ruleConnectHandset() that calls it with 'link loss' connect reason
*/
static rule_action_t ruleLinkLossConnectHandset(void)
{
    PRIMARY_RULE_LOG("ruleLinkLossConnectHandset");
    return ruleConnectHandset(RULE_CONNECT_LINK_LOSS, RULE_POST_HANDSET_CONNECT_ACTION_NONE);
}

/*! @brief Rule to determine if Earbud should connect to peer's Handset
    @startuml

    start
    if (IsInCase()) then (yes)
        :Never connect when in case;
        stop
    endif

    if (Not IsPeerSyncComplete()) then (yes)
        :Not sync'ed with peer;
        end
    endif

    if (IsPeerHandsetA2dpConnected() or IsPeerHandsetAvrcpConnected() or IsPeerHandsetHfpConnected()) then (yes)
        if (IsPeerHandsetTws()) then (yes)
            if (IsPairedWithHandset())) then (yes)
                if (Not JustPaired()) then (yes)
                    if (Reason is 'User' or 'Start-up' or 'Out of case') then (yes)
                        :Connect to peer's handset;
                        stop
                    else (no)
                        :Don't connect to peer's handset;
                        end
                    endif
                else (no)
                    :Don't connect as just paired;
                    end
                endif
            else (no)
                :Not paired with peer's handset;
                end
            endif
        else (no)
            :Peer is connected to standard handset;
            end
        endif
    else (no)
        :Don't connect as peer is not connected to handset;
        end
    endif
    @enduml
*/
static rule_action_t ruleConnectPeerHandset(ruleConnectReason reason)
{
    PrimaryRulesTaskData* primary_rules = PrimaryRulesGetTaskData();

    /* Don't attempt to connect if we're in the case */
    if (appSmIsInCase())
    {
        PRIMARY_RULE_LOG("ruleConnectHandset, ignore as in case");
        return rule_action_ignore;
    }

    /* Don't attempt to connect if we're pairing */
    if (appSmIsPairing())
    {
        PRIMARY_RULE_LOG("ruleConnectPeer, ignore as pairing");
        return rule_action_ignore;
    }

    /* Don't attempt to connect if peer is pairing */
    if (StateProxy_IsPeerPairing())
    {
        PRIMARY_RULE_LOG("ruleConnectPeerHandset, ignore as peer is pairing");
        return rule_action_ignore;
    }

    /* If connecting due to pairing or peer is connected to handset then we should also connect to this handset if it's TWS+ */
    if ((reason == RULE_CONNECT_PAIRING) ||
        StateProxy_IsPeerHandsetA2dpConnected() ||
        StateProxy_IsPeerHandsetHfpConnected())
    {
        /*  Check peer's handset is TWS+ */
        if (StateProxy_IsPeerHandsetTws())
        {
            bdaddr handset_addr;
            StateProxy_GetPeerHandsetAddr(&handset_addr);

            /* Check we paired with this handset */
            if (appDeviceIsHandset(&handset_addr))
            {
                if (/*primary_rules->allow_connect_after_pairing ||*/ !BtDevice_IsJustPaired(&handset_addr))
                {
                    if ((reason == RULE_CONNECT_USER) ||
                        (reason == RULE_CONNECT_PAIRING) ||
                        (reason == RULE_CONNECT_OUT_OF_CASE)
                        )
                    {
                        uint8 profiles = 0;

                        if (StateProxy_IsPeerHandsetA2dpConnected())
                            profiles |= DEVICE_PROFILE_A2DP;
                        if (StateProxy_IsPeerHandsetHfpConnected())
                            profiles |= DEVICE_PROFILE_HFP;
                        if (StateProxy_IsPeerHandsetAvrcpConnected())
                            profiles |= DEVICE_PROFILE_AVRCP;

                        /* Always attempt to connect HFP, A2DP and AVRCP if pairing connect */
                        if (reason == RULE_CONNECT_PAIRING)
                            profiles |= DEVICE_PROFILE_HFP | DEVICE_PROFILE_A2DP | DEVICE_PROFILE_AVRCP;

                        PRIMARY_RULE_LOGF("ruleConnectPeerHandset, run as peer is connected to TWS+ handset, profiles:%u", profiles);

                        return RULE_ACTION_RUN_PARAM(profiles);
                    }
                    else
                    {
                        PRIMARY_RULE_LOG("ruleConnectPeerHandset, ignore as peer is connected to TWS+ handset but not user or startup connect and not just paired (or allow_connect_after_pairing disabled)");
                        return rule_action_ignore;
                    }
                }
                else
                {
                    PRIMARY_RULE_LOG("ruleConnectPeerHandset, ignore as just paired with peer's TWS+ handset or allow_connect_after_pairing disabled");
                    return rule_action_ignore;
                }
            }
            else
            {
                PRIMARY_RULE_LOG("ruleConnectPeerHandset, ignore as peer is connected to TWS+ handset but we're not paired with it");
                return rule_action_ignore;
            }
        }
        else
        {
            PRIMARY_RULE_LOG("ruleConnectPeerHandset, ignore as peer is connected to standard handset");
            return rule_action_ignore;
        }
    }
    else
    {
        /* Peer is not connected to handset, don't connect as ruleConnectHandset handles this case */
        PRIMARY_RULE_LOG("ruleConnectPeerHandset, done as peer is not connected");
        return rule_action_complete;
    }
}

/*! @brief Wrapper around ruleUserConnectPeerHandset() that calls it with 'User' connect reason
*/
static rule_action_t ruleUserConnectPeerHandset(void)
{
    PRIMARY_RULE_LOG("ruleUserConnectPeerHandset");
    return ruleConnectPeerHandset(RULE_CONNECT_USER);
}

/*! @brief Wrapper around ruleOutOfCaseConnectPeerHandset() that calls it with 'Out of case' connect reason
*/
static rule_action_t ruleOutOfCaseConnectPeerHandset(void)
{
    PRIMARY_RULE_LOG("ruleOutOfCaseConnectPeerHandset");
    return ruleConnectPeerHandset(RULE_CONNECT_OUT_OF_CASE);
}


/*! @brief Wrapper around ruleLinkLossConnectPeerHandset() that calls it with 'link loss' connect reason
*/
static rule_action_t ruleLinkLossConnectPeerHandset(void)
{
    PRIMARY_RULE_LOG("ruleLinkLossConnectPeerHandset");
    return ruleConnectPeerHandset(RULE_CONNECT_LINK_LOSS);
}

static rule_action_t rulePairingConnectPeerHandset(void)
{
    PRIMARY_RULE_LOG("rulePairingConnectPeerHandset");
    return ruleConnectPeerHandset(RULE_CONNECT_PAIRING);
}

/*! @brief Rule to determine if Earbud should connect A2DP & AVRCP to peer Earbud
    @startuml

    start
    if (IsPeerA2dpConnected()) then (yes)
        :Already connected;
        end
    endif

    if (IsPeerSyncComplete()) then (yes)
        if (IsPeerInCase()) then (yes)
            :Peer is in case, so don't connect to it;
            end
        endif

        if (IsPeerHandsetA2dpConnected() or IsPeerHandsetHfpConnected()) then (yes)
            if (IsPeerHandsetTws()) then (yes)
                :Don't need to connect to peer, as peer is connected to TWS+ handset;
                end
            else (no)
                :Don't need to connect, peer will connect to us;
                end
            endif
        else (no)
            :Peer is not connected to handset yet;
            if (IsPairedWithHandset()) then (yes)
                if (Not IsTwsHandset()) then (yes)
                    if (IsHandsetA2dpConnected() or IsHandsetHfpConnected()) then (yes)
                        :Connect to peer as  connected to standard handset, peer won't be connected;
                        stop
                    else (no)
                        :Run RuleConnectHandsetStandard() to determine if we're going to connect to handset;
                        if (RuleConnectHandsetStandard()) then (yes)
                            :Will connect to handset, so should also connect to peer;
                            stop
                        else (no)
                            :Won't connect to handset, so don't connect to peer;
                            end
                        endif
                    endif
                else (no)
                    :Don't connect to peer, as connected to TWS+ handset;
                    end
                endif
            else (no)
                :Don't connect to peer, as not paired with handset;
                end
            endif
        endif
    else (no)
        :Not sync'ed with peer;
        end
    endif

    @enduml
*/
static rule_action_t ruleConnectPeer(ruleConnectReason reason)
{
    bdaddr handset_addr;

    /* Don't run rule if we're connected to peer */
    if (appDeviceIsPeerA2dpConnected() && (appDeviceIsPeerScoFwdConnected() || appDeviceIsPeerShadowConnected()))
    {
        PRIMARY_RULE_LOG("ruleConnectPeer, ignore as already connected to peer");
        return rule_action_ignore;
    }

    /* Don't attempt to connect if we're pairing */
    if (appSmIsPairing())
    {
        PRIMARY_RULE_LOG("ruleConnectPeer, ignore as pairing");
        return rule_action_ignore;
    }

    /* Check if peer is in case */
    if (StateProxy_IsPeerInCase())
    {
        PRIMARY_RULE_LOG("ruleConnectPeer, ignore as peer is in case");
        return rule_action_ignore;
    }

    /* Don't attempt to connect if peer is pairing */
    if (StateProxy_IsPeerPairing())
    {
        PRIMARY_RULE_LOG("ruleConnectPeer, ignore as peer is pairing");
        return rule_action_ignore;
    }

    /* Check if peer is connected to handset */
    if (StateProxy_IsPeerHandsetA2dpConnected() || StateProxy_IsPeerHandsetHfpConnected())
    {
        /* Don't connect to peer if handset is TWS+  */
        if (StateProxy_IsPeerHandsetTws())
        {
            PRIMARY_RULE_LOG("ruleConnectPeer, ignore as peer is connected to TWS+ handset");
            return rule_action_ignore;
        }
        else
        {
            PRIMARY_RULE_LOG("ruleConnectPeer, ignore as peer is connected to standard handset and peer will connect to us");
            return rule_action_ignore;
        }
    }
    else
    {
        /* Peer is not connected to handset yet */
        /* Get handset address */
        if (appDeviceGetHandsetBdAddr(&handset_addr))
        {
            /* Check if the handset we would connect to is a standard handset */
            if (!appDeviceIsTwsPlusHandset(&handset_addr))
            {
                bdaddr peer_addr;
                uint8 profiles;

                appDeviceGetPeerBdAddr(&peer_addr);
                profiles = BtDevice_GetLastConnectedProfiles(&peer_addr);

                /* Always attempt to connect A2DP and SCOFWD if user initiated connect,
                 * out-of-case connect or sync connect */
                if ((reason == RULE_CONNECT_OUT_OF_CASE) ||
                        (reason == RULE_CONNECT_USER) ||
                        (reason == RULE_CONNECT_PEER_SYNC))
                {
#ifndef INCLUDE_SHADOWING
                    profiles |= DEVICE_PROFILE_A2DP;
#ifdef INCLUDE_SCOFWD
                    profiles |= DEVICE_PROFILE_SCOFWD;
#endif
#endif
                }

                /* Check if we're already connected to handset */
                if (appDeviceIsHandsetA2dpConnected() || appDeviceIsHandsetHfpConnected())
                {
                    PRIMARY_RULE_LOG("ruleConnectPeer, run as connected to standard handset, peer won't be connected");
                    return RULE_ACTION_RUN_PARAM(profiles);
                }
                else
                {
                    /* Not connected to handset, if we are going to connect to standard handset, we should also connect to peer */
                    PRIMARY_RULE_LOG("ruleConnectPeer, calling ruleConnectHandsetStandard() to determine if we're going to connect to handset");
                    if (ruleConnectHandsetStandard(reason) == rule_action_run)
                    {
                        PRIMARY_RULE_LOG("ruleConnectPeer, run as connected/ing to standard handset");
                        return RULE_ACTION_RUN_PARAM(profiles);
                    }
                    else
                    {
                        PRIMARY_RULE_LOG("ruleConnectPeer, ignore as not connected/ing to standard handset");
                        return rule_action_ignore;
                    }
                }
            }
            else
            {
                PRIMARY_RULE_LOG("ruleConnectPeer, ignore as connected/ing to TWS+ handset");
                return rule_action_ignore;
            }
        }
        else
        {
            PRIMARY_RULE_LOG("ruleConnectPeer, ignore as no handset, so no need to connect to peer");
            return rule_action_ignore;
        }
    }
}

/*! @brief Wrapper around ruleConnectPeer() that calls it with 'Peer sync' connect reason
*/
static rule_action_t ruleSyncConnectPeer(void)
{
    PRIMARY_RULE_LOG("ruleSyncConnectPeer");
    return ruleConnectPeer(RULE_CONNECT_PEER_SYNC);
}

/*! @brief Wrapper around ruleConnectPeer() that calls it with 'User' connect reason
*/
static rule_action_t ruleUserConnectPeer(void)
{
    PRIMARY_RULE_LOG("ruleUserConnectPeer");
    return ruleConnectPeer(RULE_CONNECT_USER);
}

/*! @brief Wrapper around ruleConnectPeer() that calls it with 'Out of case' connect reason
*/
static rule_action_t ruleOutOfCaseConnectPeer(void)
{
    PRIMARY_RULE_LOG("ruleOutOfCaseConnectPeer");
    return ruleConnectPeer(RULE_CONNECT_OUT_OF_CASE);
}

/*! @brief Rule to determine if most recently used handset should be updated
    @startuml

    start
    if (Not IsPeerSyncComplete()) then (yes)
        :Peer sync not completed;
        end
    endif

    if (IsPeerHandsetA2dpConnected() or IsPeerHandsetHfpConnected()) then (yes)
        if (IsPairedPeerHandset()) then (yes)
            :Update MRU handzset as peer is connected to handset;
            stop
        else (no)
            :Do nothing as not paired to peer's handset;
            end
        endif
    else
        :Do nothing as peer is not connected to handset;
        end
    endif
    @enduml
*/
/*! \todo run on local or remote handset connection
    May result in command to secondary to update MRU handset */
static rule_action_t ruleUpdateMruHandset(void)
{
    /* If peer is connected to handset then we should mark this handset as most recently used */
    if (StateProxy_IsPeerHandsetA2dpConnected() || StateProxy_IsPeerHandsetHfpConnected())
    {
        /* Check we paired with this handset */
        bdaddr handset_addr;
        StateProxy_GetPeerHandsetAddr(&handset_addr);
        if (appDeviceIsHandset(&handset_addr))
        {
            PRIMARY_RULE_LOG("ruleUpdateMruHandset, run as peer is connected to handset");
            return rule_action_run;
        }
        else
        {
            PRIMARY_RULE_LOG("ruleUpdateMruHandset, ignore as not paired with peer's handset");
            return rule_action_ignore;
        }
    }
    else
    {
        /* Peer is not connected to handset */
        PRIMARY_RULE_LOG("ruleUpdateMruHandset, ignore as peer is not connected");
        return rule_action_ignore;
    }

}

/*! @brief Rule to determine if Earbud should send status to handset over HFP and/or AVRCP
    @startuml

    start
    if (IsPairedHandset() and IsTwsPlusHandset()) then (yes)
        if (IsHandsetHfpConnected() or IsHandsetAvrcpConnected()) then (yes)
            :HFP and/or AVRCP connected, send status update;
            stop
        endif
    endif

    :Not connected with AVRCP or HFP to handset;
    end
    @enduml
*/
static rule_action_t ruleSendStatusToHandset(void)
{
    bdaddr handset_addr;

    if (appDeviceGetHandsetBdAddr(&handset_addr) && appDeviceIsTwsPlusHandset(&handset_addr))
    {
        if (appDeviceIsHandsetHfpConnected() || appDeviceIsHandsetAvrcpConnected())
        {
            PRIMARY_RULE_LOG("ruleSendStatusToHandset, run as TWS+ handset");
            return rule_action_run;
        }
    }

    PRIMARY_RULE_LOG("ruleSendStatusToHandset, ignore as not connected to TWS+ handset");
    return rule_action_ignore;
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

/*! @brief Rule to determine if Earbud should disconnect from handset when put in case
    Rule is triggered by the 'in case' event
    @startuml

    start
    if (IsInCase() and IsHandsetConnected() and Not DfuUpgradePending()) then (yes)
        :Disconnect from handset as now in case;
        stop
    endif
    end
    @enduml
*/
static rule_action_t ruleInCaseDisconnectHandset(void)
{
    if(appSmIsInCase() && appSmIsDfuPending())
    {
        return rule_action_ignore;
    }
    else if (appSmIsInCase() && handsetDisconnectAllowed())
    {
        PRIMARY_RULE_LOG("ruleInCaseDisconnectHandset, run as in case and handset connected");
        // Try to handover the handset connection to the other earbud
        bool handover = TRUE;
        return RULE_ACTION_RUN_PARAM(handover);
    }
    else
    {
        PRIMARY_RULE_LOG("ruleInCaseDisconnectHandset, ignore as not in case or handset not connected or master of active call forwarding");
        return rule_action_ignore;
    }
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

/*! @brief Rule to decide if one Earbud should disconnect from handset, if both connected.
 */
static rule_action_t ruleBothConnectedDisconnect(void)
{
      bdaddr handset_addr;
      bdaddr peer_handset_addr;

    appDeviceGetHandsetBdAddr(&handset_addr);
    StateProxy_GetPeerHandsetAddr(&peer_handset_addr);

    /* if both earbuds connected to the same TWS Standard handset */
    if (   appDeviceIsHandsetConnected()
        &&
           (StateProxy_IsPeerHandsetA2dpConnected()   ||
            StateProxy_IsPeerHandsetHfpConnected()    ||
            StateProxy_IsPeerHandsetAvrcpConnected())
        &&
            (!appDeviceIsTwsPlusHandset(&handset_addr))
        &&
            (BdaddrIsSame(&handset_addr, &peer_handset_addr)))
    {
        /* Score each earbud, to determine which should disconnect.
         * Weight an active SCO or active A2DP stream higher, so that even if
         * only 1 profile is connected, but there is active audio this will
         * count higher than all profiles being connected. */
        int this_earbud_score = (appDeviceIsHandsetA2dpConnected() ? 1 : 0) +
                                (appDeviceIsHandsetAvrcpConnected() ? 1 : 0) +
                                (appDeviceIsHandsetHfpConnected() ? 1 : 0) +
                                (appHfpIsScoActive() ? 3 : 0) +
                                (appDeviceIsHandsetA2dpStreaming() ? 3 : 0);
        int other_earbud_score = (StateProxy_IsPeerHandsetA2dpConnected() ? 1 : 0) +
                                (StateProxy_IsPeerHandsetAvrcpConnected() ? 1 : 0) +
                                (StateProxy_IsPeerHandsetHfpConnected() ? 1 : 0) +
                                (StateProxy_IsPeerScoActive() ? 3 : 0) +
                                (StateProxy_IsPeerHandsetA2dpStreaming() ? 3 : 0);

        /* disconnect lowest scoring earbud */
        if (this_earbud_score < other_earbud_score)
        {
            PRIMARY_RULE_LOGF("ruleBothConnectedDisconnect, run as lower score: this %u other %u",
                        this_earbud_score, other_earbud_score);
            return rule_action_run;
        }
        else if (this_earbud_score > other_earbud_score)
        {
            PRIMARY_RULE_LOGF("ruleBothConnectedDisconnect, ignore as higher score: this %u other %u",
                    this_earbud_score, other_earbud_score);
            /* \todo needs to become run and send command to secondary to disconnect */
            return rule_action_ignore;
        }
        else
        {
            /* equal scores, disconnect the left */
            if (appConfigIsLeft())
            {
                PRIMARY_RULE_LOGF("ruleBothConnectedDisconnect, run, same score and we're left: this %u other %u",
                        this_earbud_score, other_earbud_score);
                return rule_action_run;
            }
            else
            {
                PRIMARY_RULE_LOGF("ruleBothConnectedDisconnect, ignore, same score and we're right: this %u other %u",
                        this_earbud_score, other_earbud_score);
                /* \todo needs to become run and send command to secondary to disconnect */
                return rule_action_ignore;
            }
        }
    }

    PRIMARY_RULE_LOG("ruleBothConnectedDisconnect, ignore, both earbuds not connected to the same TWS Standard handset");
    return rule_action_ignore;
}

/*! @brief Rule to determine if Earbud should disconnect A2DP/AVRCP/SCOFWD from peer when put in case
    Rule is triggered by the 'in case' event
    @startuml

    start
    if (IsInCase() and IsPeerA2dpConnected() and IsPeerAvrcpConnectedForAv()) then (yes)
        :Disconnect from peer as now in case;
        stop
    endif
    end
    @enduml
*/
static rule_action_t ruleInCaseDisconnectPeer(void)
{
    if(appSmIsInCase() && appSmIsDfuPending())
    {
        return rule_action_ignore;
    }
    else if (appSmIsInCase() && (appDeviceIsPeerA2dpConnected() ||
                            appDeviceIsPeerAvrcpConnectedForAv() ||
                            appDeviceIsPeerScoFwdConnected() ||
                            appDeviceIsPeerShadowConnected()))
    {
        if (ScoFwdIsSending())
        {
            PRIMARY_RULE_LOG("ruleInCaseDisconnectPeer, ignore as master of active SCO forwarding");
            return rule_action_ignore;
        }

        PRIMARY_RULE_LOG("ruleInCaseDisconnectPeer, run as in case and peer connected");
        return rule_action_run;
    }
    else
    {
        PRIMARY_RULE_LOG("ruleInCaseDisconnectPeer, ignore as not in case or peer not connected");
        return rule_action_ignore;
    }
}

static rule_action_t rulePeerInCaseDisconnectPeer(void)
{
    if(appSmIsInCase() && appSmIsDfuPending())
    {
        return rule_action_ignore;
    }
    else if (StateProxy_IsPeerInCase() && (appDeviceIsPeerA2dpConnected() ||
                                      appDeviceIsPeerAvrcpConnectedForAv() ||
                                      appDeviceIsPeerScoFwdConnected() ||
                                      appDeviceIsPeerShadowConnected()))
    {
        PRIMARY_RULE_LOG("rulePeerInCaseDisconnectPeer, run as peer in the case and it is connected");
        return rule_action_run;
    }
    return rule_action_ignore;
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

        PRIMARY_RULE_LOG("ruleCheckUpgradable, allow as in case");
        return RULE_ACTION_RUN_PARAM(allow_dfu);
    }
}


/*! @brief Rule to determine if Earbud should disconnect A2DP/AVRCP from peer Earbud
    @startuml

    start
    if (Not IsPeerA2dpConnected() and Not IsPeerAvrcoConnectedForAv()) then (yes)
        :Not connected, do nothing;
        stop
    endif

    if (Not IsHandsetPaired()) then (yes)
        :Not paired with handset, disconnect from peer;
        stop
    endif

    if (IsHandsetA2dpConnected()) then (yes)
        if (IsTwsPlusHandset()) then (yes)
            :Connected to TWS+ handset, no need for A2DP/AVCRP to peer;
            stop
        else
            :Connected to standard handset, still require A2DP/AVRCP to peer;
            end
        endif
    else
        :Not connected with A2DP to handset;
        end
    endif
    @enduml
*/
static rule_action_t ruleDisconnectPeer(void)
{
    bdaddr handset_addr;

    /* Don't run rule if we're not connected to peer */
    if (!appDeviceIsPeerA2dpConnected() &&
        !appDeviceIsPeerAvrcpConnectedForAv() &&
        !appDeviceIsPeerScoFwdConnected() &&
        !appDeviceIsPeerShadowConnected())
    {
        PRIMARY_RULE_LOG("ruleDisconnectPeer, ignore as not connected to peer");
        return rule_action_ignore;
    }

    /* If we're not paired with handset then disconnect */
    if (!appDeviceGetHandsetBdAddr(&handset_addr))
    {
        PRIMARY_RULE_LOG("ruleDisconnectPeer, run as not paired with handset");
        return rule_action_run;
    }

    /* If we're connected to a handset, but it's a TWS+ handset then we don't need media connction to peer */
    if (appDeviceIsHandsetA2dpConnected() || appDeviceIsHandsetHfpConnected())
    {
        if (appDeviceIsTwsPlusHandset(&handset_addr))
        {
            PRIMARY_RULE_LOG("ruleDisconnectPeer, run as connected to TWS+ handset");
            return rule_action_run;
        }
        else
        {
            PRIMARY_RULE_LOG("ruleDisconnectPeer, ignore as connected to standard handset");
            return rule_action_ignore;
        }
    }
    else
    {
        PRIMARY_RULE_LOG("ruleDisconnectPeer, run as not connected handset");
        return rule_action_run;
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

/*! Rule to disconnect the peer when peer is pairing. */
static rule_action_t ruleSyncDisconnectPeer(void)
{
    /* Don't run rule if we're not connected to peer */
    if (!appDeviceIsPeerA2dpConnected() &&
        !appDeviceIsPeerAvrcpConnectedForAv() &&
        !appDeviceIsPeerScoFwdConnected() &&
        !appDeviceIsPeerShadowConnected())
    {
        PRIMARY_RULE_LOG("ruleSyncDisconnectPeer, ignore as not connected to peer");
        return rule_action_ignore;
    }

    if (StateProxy_IsPeerPairing())
    {
        PRIMARY_RULE_LOG("ruleSyncDisconnectPeer, run as peer is pairing");
        return rule_action_run;
    }

    return rule_action_ignore;
}


/*! Rule to disconnect the handset when peer is pairing. */
static rule_action_t ruleSyncDisconnectHandset(void)
{
    /* Don't run rule if we're not connected to handset */
    if (!appDeviceIsHandsetA2dpConnected() &&
        !appDeviceIsHandsetAvrcpConnected() &&
        !appDeviceIsHandsetHfpConnected())
    {
        PRIMARY_RULE_LOG("ruleSyncDisconnectHandset, ignore as not connected to handset");
        return rule_action_ignore;
    }

    if (StateProxy_IsPeerPairing())
    {
        PRIMARY_RULE_LOG("ruleSyncDisconnectHandset, run as peer is pairing");
        return rule_action_run;
    }

    return rule_action_ignore;
}




/*! @brief Rule to determine if page scan settings should be changed.

    @startuml
        (handset1)
        (handset2)
        (earbud1)
        (earbud2)
        earbud1 <-> earbud2 : A
        earbud1 <--> handset1 : B
        earbud2 <--> handset2 : C
    @enduml
    A = link between earbuds
    B = link from earbud1 to handset1
    C = link from earbud2 to handset2
    D = earbud1 handset is TWS+
    E = earbud2 handset is TWS+
    F = earbud is connectable
    Links B and C are mutually exclusive.

    Page scan is controlled as defined in the following truth table (X=Don't care).
    Viewed from the perspective of Earbud1.

    Peer sync must be complete for the rule to run if the state of the peer handset
    affects the result of the rule.

    A | B | C | D | E | F | Page Scan On
    - | - | - | - | - | - | ------------
    X | X | X | X | X | 0 | 0
    0 | X | X | X | X | 1 | 1
    1 | 0 | 0 | X | X | 1 | 1
    1 | 0 | 1 | X | 1 | 1 | 1
    1 | 0 | 1 | X | 0 | 1 | 0
    1 | 1 | X | X | X | 1 | 0
*/
static rule_action_t rulePageScanUpdate(void)
{
    bool ps_on = FALSE;
    bool sm_connectable = appSmIsConnectable();
    bool peer = appDeviceIsPeerConnected();
    bool handset = appDeviceIsHandsetAnyProfileConnected();
    bool peer_handset = StateProxy_IsPeerHandsetA2dpConnected() || StateProxy_IsPeerHandsetAvrcpConnected() || StateProxy_IsPeerHandsetHfpConnected();
    bool peer_handset_tws = StateProxy_IsPeerHandsetTws();

    if (!sm_connectable)
    {
        // Not connectable
        ps_on = FALSE;
    }
    else if (!peer)
    {
        // No peer
        ps_on = TRUE;
    }
    else if (handset)
    {
        // Peer, handset
        ps_on = FALSE;
    }
    else if (peer_handset)
    {
        if (peer_handset_tws)
        {
            // Peer, no handset, tws peer handset
            ps_on = TRUE;
        }
        else
        {
            // Peer, no handset, standard peer handset
            ps_on = FALSE;
        }
    }
    else
    {
        // Peer, no handset, no peer handset
        ps_on = TRUE;
    }

#ifndef DISABLE_LOG
    /* Reduce log utilisation by this frequently run rule by compressing the
       booleans into nibbles in a word. When printed in hex, this will display
       as a binary bitfield. */
    uint32 log_val = (!!sm_connectable   << 20) |
                     (!!peer             << 16) |
                     (!!handset          << 12) |
                     (!!peer_handset     << 4)  |
                     (!!peer_handset_tws << 0);
    PRIMARY_RULE_LOGF("rulePageScanUpdate, (sm_connectable,peer,handset,peer_handset,peer_handset_tws)=%06x, ps=%d", log_val, ps_on);
#endif

    if (ps_on && !BredrScanManager_IsPageScanEnabledForClient(SmGetTask()))
    {
        /* need to enable page scan and it is not already enabled for the SM user */
        PRIMARY_RULE_LOG("rulePageScanUpdate, run, need to enable page scan");

        /* using CONN_RULES_PAGE_SCAN_UPDATE message which take a bool parameter,
         * use RULE_ACTION_RUN_PARAM macro to prime the message data and indicate
         * to the rules engine it should return it in the message to the client task */
        return RULE_ACTION_RUN_PARAM(ps_on);
    }
    else if (!ps_on && BredrScanManager_IsPageScanEnabledForClient(SmGetTask()))
    {
        /* need to disable page scan and it is currently enabled for SM user */
        PRIMARY_RULE_LOG("rulePageScanUpdate, run, need to disable page scan");
        return RULE_ACTION_RUN_PARAM(ps_on);
    }

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


/*! @brief Rule to validate whether handover should be initiated */
static rule_action_t ruleHandoverDisconnectHandset(void)
{
    bdaddr addr;
    if (appDeviceIsHandsetConnected() &&
        appDeviceGetHandsetBdAddr(&addr) &&
        !appDeviceIsTwsPlusHandset(&addr))
    {
        bool allowed = !appSmIsDfuPending();
        return allowed ? RULE_ACTION_RUN_PARAM(allowed) : rule_action_defer;
    }
    return rule_action_ignore;
}

/*! @brief Rule to validate whether handover should cause handset to be connected */
static rule_action_t ruleHandoverConnectHandset(void)
{
    return ruleConnectHandset(RULE_CONNECT_USER, RULE_POST_HANDSET_CONNECT_ACTION_NONE);
}

/*! @brief Rule to validate whether handover should cause handset to be connected
    then media played */
static rule_action_t ruleHandoverConnectHandsetAndPlay(void)
{
    return ruleConnectHandset(RULE_CONNECT_USER, RULE_POST_HANDSET_CONNECT_ACTION_PLAY_MEDIA);
}


/*! @brief Rule to determine if BLE connection settings should be changed

    \todo include UML documentation.

*/
static rule_action_t ruleBleConnectionUpdate(void)
{
    bool peer = appDeviceIsPeerConnected();
    bool peer_handset = StateProxy_IsPeerHandsetA2dpConnected() || StateProxy_IsPeerHandsetAvrcpConnected() || StateProxy_IsPeerHandsetHfpConnected();
    bool peer_handset_paired = StateProxy_HasPeerHandsetPairing();
    bool peer_ble_advertising = StateProxy_IsPeerAdvertising();
    bool peer_ble_connected = StateProxy_IsPeerBleConnected();
    bool left = appConfigIsLeft();
    bool peer_dfu = StateProxy_IsPeerDfuInProgress();
    bool in_case = appSmIsInCase();
    bool peer_in_case = StateProxy_IsPeerInCase();
    bool ble_connectable = appSmStateAreNewBleConnectionsAllowed(appGetState());
    bool paired_with_peer = BtDevice_IsPairedWithPeer();
    bool paired_with_handset = BtDevice_IsPairedWithHandset();

    bool has_ble_connection = ConManagerAnyTpLinkConnected(cm_transport_ble);
    bool is_ble_connecting = appSmIsBleAdvertising();

    bool connectable;

        /* Use our own status to decide if we should be connectable */
    connectable = paired_with_peer && paired_with_handset && ble_connectable && !has_ble_connection;
    DEBUG_LOG("ruleBleConnectionUpdate Paired(peer:%d, handset:%d). BLE(allowed:%d,allowed_out_case:%d,has:%d,is_trying:%d)",
                    paired_with_peer,paired_with_handset,
                    ble_connectable, appConfigBleAllowedOutOfCase(),
                    has_ble_connection, is_ble_connecting);

    /* Now take the peer status into account */
    if (connectable && peer)
    {
        if (peer_handset || peer_dfu || (peer_ble_advertising && ! peer_in_case) || peer_ble_connected)
        {
            DEBUG_LOG("ruleBleConnectionUpdate Peer has handset connection:%d, DFU:%d, BLE-adv:%d (out of case), or BLE-connection. We don't want to do BLE",
                        peer_handset, peer_dfu, peer_ble_advertising, peer_ble_connected);

            connectable = FALSE;
        }
        else if (!peer_handset_paired)
        {
            DEBUG_LOG("ruleBleConnectionUpdate Peer has no handset.");
        }
        else if (in_case)
        {
            /* Do nothing. We want both to be connectable, unless DFU/BLE
               which were eliminated above */
            DEBUG_LOG("ruleBleConnectionUpdate In case, ignore excuses not to advertise");
        }
        else if (!peer_in_case)
        {
            uint16 our_battery;
            uint16 peer_battery;

            StateProxy_GetLocalAndRemoteBatteryLevels(&our_battery,&peer_battery);
            if (our_battery < peer_battery)
            {
                DEBUG_LOG("ruleBleConnectionUpdate Peer (out of case) has stronger battery.");
                connectable = FALSE;
            }
            else if (our_battery == peer_battery)
            {
                if (!left)
                {
                    DEBUG_LOG("ruleBleConnectionUpdate we have same battery, and are right handset. Don't do BLE.");
                    connectable = FALSE;
                }
            }
        }
    }
    else if (connectable)
    {
        DEBUG_LOG("ruleBleConnectionUpdate No peer connection, just using local values");
    }

    if (connectable == is_ble_connecting)
    {
        PRIMARY_RULE_LOG("ruleBleConnectionUpdate, IGNORE - no change");
        return rule_action_ignore;
    }

    if (connectable)
    {
        PRIMARY_RULE_LOG("ruleBleConnectionUpdate, run, need to allow new BLE connections");
    }
    else
    {
        PRIMARY_RULE_LOG("ruleBleConnectionUpdate, run, need to disallow new BLE connections");
    }
    return RULE_ACTION_RUN_PARAM(connectable);

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
