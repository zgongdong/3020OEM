/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      State machine transitions and logic for shadow_profile.
*/

#ifndef SHADOW_PROFILE_SM_H_
#define SHADOW_PROFILE_SM_H_

/*! \brief The bitmasks for the sub-states of the shadow_profile state machine. */
typedef enum
{
    SHADOW_PROFILE_SUB_STATE_NONE               = 0x000, /*!< Pre initialisation */
    SHADOW_PROFILE_SUB_STATE_DISCONNECTED       = 0x010, /*!< No peer connected; shadow links will not be created. */
    SHADOW_PROFILE_SUB_STATE_ACL_CONNECTED      = 0x040, /*!< Shadow ACL link connected. */
    SHADOW_PROFILE_SUB_STATE_ESCO_CONNECTED     = 0x080, /*!< Shadow eSCO link connected. */
    SHADOW_PROFILE_SUB_STATE_A2DP_CONNECTED     = 0x100, /*!< Shadow A2DP link connected. */
} shadow_profile_sub_state_t;

/*! \brief Shadow Profile States

    The main states of a shadow profile link depend on the previous state.
    For example, a shadow eSCO connection must first have the peer earbud
    connected and a shadow ACL link to the earbud before it can be created.

    The state enum values below represent this by using bitmasks to group
    states based on whether the peer is disconnected, shadow ACL is connected,
    and finally if shadow eSCO or shadow A2DP is connected.

    Shadow eSCO and shadow A2DP are mutually exclusive operations.

    Transition States:
    The state machine has stable and transition states. A transition state is
    one where it is waiting for a reply from the firmware only. Other
    messages should be blocked until the reply has been received.

    A stable state is one where it is ok to process messages from any origin,
    e.g. internal messages (see #shadow_profile_internal_msg_t).

    The transition lock is set when going into a transition state. Any
    internal messages should be sent conditional on this lock.

    Stable states
    * SHADOW_PROFILE_STATE_DISCONNECTED
    * SHADOW_PROFILE_STATE_ACL_CONNECTED
    * SHADOW_PROFILE_STATE_ESCO_CONNECTED
    * SHADOW_PROFILE_STATE_A2DP_CONNECTED

    Transition states
    * SHADOW_PROFILE_STATE_INITALISING
    * SHADOW_PROFILE_STATE_ACL_CONNECTING
    * SHADOW_PROFILE_STATE_ESCO_CONNECTING
    * SHADOW_PROFILE_STATE_ACL_DISCONNECTING
    * SHADOW_PROFILE_STATE_ESCO_DISCONNECTING
    * SHADOW_PROFILE_STATE_A2DP_DISCONNECTING

    \note The stable states are also the main sub-states of the state machine.


    Pseudo States:
    The state machine has a concept of pseudo-states that group together states
    to represent a sub-state of the overall state machine. In the enum below
    the pseudo-states are marked by the top 4 bits in the first byte of the
    enum value.

    The pseudo-states are:
    ACL_CONNECTED       (SHADOW_PROFILE_SUB_STATE_ACL_CONNECTED)
    ESCO_CONNECTED      (SHADOW_PROFILE_SUB_STATE_ESCO_CONNECTED | ACL_CONNECTED)
    A2DP_CONNECTED     (SHADOW_PROFILE_SUB_STATE_A2DP_CONNECTED | ACL_CONNECTED)

    These are mainly used for testing what the sub-state is when the state
    machine is in a transition state.

    @startuml

    state DISCONNECTED : No shadow connection
    state ACL_CONNECTING : Primary initiated shadow ACL connect in progress
    state ACL_CONNECTED : Shadow ACL connected
    state ACL_DISCONNECTING : Primary initiated shadow ACL disconnect in progress
    state ESCO_CONNECTING : Primary initiated shadow eSCO connect in progress
    state ESCO_CONNECTED : Shadow eSCO connected
    state ESCO_DISCONNECTING : Primary initiated shadow eSCO disconnect in progress
    state A2DP_CONNECTING: Primary initated shadow A2DP connect in progress
    state A2DP_CONNECTED : Shadow A2DP connected
    state A2DP_DISCONNECTING : Primary initiated shadow A2DP disconnect in progress

    [*] -d-> INITIALISING : ShadowProfile_Init /\nSDM_REGISTER_REQ

    INITIALISING -d-> DISCONNECTED : SDM_REGISTER_CFM

    DISCONNECTED --> ACL_CONNECTING : Create ACL locally
    DISCONNECTED --> ACL_CONNECTING : Link-loss retry
    DISCONNECTED --> ACL_CONNECTED : ACL created remotely

    ACL_CONNECTING --> DISCONNECTED : Fail
    ACL_CONNECTING --> ACL_CONNECTED : Success

    ACL_CONNECTED --> ESCO_CONNECTING : Create eSCO locally
    ACL_CONNECTED --> ESCO_CONNECTING : Link-loss retry
    ACL_CONNECTED --> ESCO_CONNECTED : eSCO created remotely
    ACL_CONNECTED --> ACL_DISCONNECTING : Disconnect ACL locally
    ACL_CONNECTED --> DISCONNECTED : Disconnect ACL remotely
    ACL_CONNECTED --> DISCONNECTED : ACL connection timeout

    ACL_DISCONNECTING --> DISCONNECTED : ACL disconnected

    ESCO_CONNECTING --> ACL_CONNECTED : Fail
    ESCO_CONNECTING --> ESCO_CONNECTED : Success

    ESCO_CONNECTED --> ACL_CONNECTED : Disconnect (remote or timeout)
    ESCO_CONNECTED --> ESCO_DISCONNECTING : Disconnect eSCO locally

    ESCO_DISCONNECTING --> ACL_CONNECTED : eSCO disconnected

    ACL_CONNECTED --> A2DP_CONNECTING : Create shadow A2DP local/remote
    A2DP_CONNECTING --> A2DP_CONNECTED : Success
    A2DP_CONNECTING --> ACL_CONNECTED : Fail
    A2DP_CONNECTED --> A2DP_DISCONNECTING : Local/remote disconnect
    A2DP_DISCONNECTING --> ACL_CONNECTED : A2DP disconnected


    @enduml
*/
typedef enum
{
    /*! No state - pre-initialisation */
    SHADOW_PROFILE_STATE_NONE                       = SHADOW_PROFILE_SUB_STATE_NONE,

    /*! In the process of initialising. */
    SHADOW_PROFILE_STATE_INITALISING                = SHADOW_PROFILE_STATE_NONE + 1,

        /*! Initialised but no shadow connections and peer not connected. */
        SHADOW_PROFILE_STATE_DISCONNECTED               = SHADOW_PROFILE_SUB_STATE_DISCONNECTED,

            /*! Locally initiated shadow ACL connection in progress. */
            SHADOW_PROFILE_STATE_ACL_CONNECTING         = SHADOW_PROFILE_STATE_DISCONNECTED + 1,

                /* ACL_CONNECTED sub-state */

                /*! Shadow ACL connected. */
                SHADOW_PROFILE_STATE_ACL_CONNECTED      = SHADOW_PROFILE_SUB_STATE_ACL_CONNECTED,
                /*! Locally initiated shadow eSCO connection in progress. */
                SHADOW_PROFILE_STATE_ESCO_CONNECTING    = SHADOW_PROFILE_STATE_ACL_CONNECTED + 1,

                    /* ESCO_CONNECTED sub-state */

                    /*! Shadow eSCO connected. */
                    SHADOW_PROFILE_STATE_ESCO_CONNECTED = (SHADOW_PROFILE_SUB_STATE_ESCO_CONNECTED | SHADOW_PROFILE_STATE_ACL_CONNECTED),

                /*! Locally initiated shadow eSCO disconnect in progress. */
                SHADOW_PROFILE_STATE_ESCO_DISCONNECTING  = SHADOW_PROFILE_STATE_ACL_CONNECTED + 2,

                /*! Local or remote shadow A2DP connection in progress. */
                SHADOW_PROFILE_STATE_A2DP_CONNECTING = SHADOW_PROFILE_STATE_ACL_CONNECTED + 3,

                    /* A2DP_CONNECTED sub-state */

                    /*! Shadow A2DP connected. */
                    SHADOW_PROFILE_STATE_A2DP_CONNECTED = (SHADOW_PROFILE_SUB_STATE_A2DP_CONNECTED | SHADOW_PROFILE_STATE_ACL_CONNECTED),

                /*! Local or remote shadow A2DP disconnection in progress. */
                SHADOW_PROFILE_STATE_A2DP_DISCONNECTING = SHADOW_PROFILE_STATE_ACL_CONNECTED + 4,


            /*! Locally initiated shadow ACL disconnect in progress. */
            SHADOW_PROFILE_STATE_ACL_DISCONNECTING       = SHADOW_PROFILE_STATE_DISCONNECTED + 2,

} shadow_profile_state_t;

/*!@{ \name Masks used to check for the sub-state of the state machine. */
#define SHADOW_PROFILE_STATE_MASK_ACL_CONNECTED         (SHADOW_PROFILE_STATE_ACL_CONNECTED)
#define SHADOW_PROFILE_STATE_MASK_ESCO_CONNECTED        (SHADOW_PROFILE_STATE_ESCO_CONNECTED)
#define SHADOW_PROFILE_STATE_MASK_A2DP_CONNECTED        (SHADOW_PROFILE_STATE_A2DP_CONNECTED)
/*!@} */

/*! \brief Is shadow_profile sub-state 'ACL connected' */
#define ShadowProfile_IsSubStateAclConnected(state) \
    (((state) & SHADOW_PROFILE_STATE_MASK_ACL_CONNECTED) == SHADOW_PROFILE_STATE_ACL_CONNECTED)

/*! \brief Is shadow_profile sub-state 'ESCO connected' */
#define ShadowProfile_IsSubStateEscoConnected(state) \
    (((state) & SHADOW_PROFILE_STATE_MASK_ESCO_CONNECTED) == SHADOW_PROFILE_STATE_ESCO_CONNECTED)

/*! \brief Is shadow_profile sub-state 'A2DP connected' */
#define ShadowProfile_IsSubStateA2dpConnected(state) \
    (((state) & SHADOW_PROFILE_STATE_MASK_A2DP_CONNECTED) == SHADOW_PROFILE_STATE_A2DP_CONNECTED)

/*! If no other bits are set than those defined in this mask, the state is steady. */
#define STEADY_STATE_MASK  (SHADOW_PROFILE_SUB_STATE_DISCONNECTED   | \
                            SHADOW_PROFILE_SUB_STATE_ACL_CONNECTED  | \
                            SHADOW_PROFILE_SUB_STATE_ESCO_CONNECTED | \
                            SHADOW_PROFILE_SUB_STATE_A2DP_CONNECTED)

/*! \brief Is shadow profile in steady state? */
#define ShadowProfile_IsSteadyState(state) (((state) | STEADY_STATE_MASK) == STEADY_STATE_MASK)


/*! \brief Tell the shadow_profile state machine to go to a new state.

    Changing state always follows the same procedure:
    \li Call the Exit function of the current state (if it exists)
    \li Call the Exit function of the current psuedo-state if leaving it.
    \li Change the current state
    \li Call the Entry function of the new psuedo-state (if necessary)
    \li Call the Entry function of the new state (if it exists)

    \param state New state to go to.
*/
void ShadowProfile_SetState(shadow_profile_state_t state);

/*! \brief Set a new target state for the state machine.

    \param state New state to target.

    The target state should be a steady state as described above.
*/
void ShadowProfile_SetTargetState(shadow_profile_state_t target_state);

/*! \brief Assess the target state vs the current state and transition to a state
    that will achieve (or be a step towards) the target state.

    The state machine will only transition if it is in a stable state.

    If the delay_kick flag is set, the kick will be deferred.
 */
void ShadowProfile_SmKick(void);

/*! \brief Handle shadow_profile error

    Some error occurred in the shadow_profile state machine.

    Currently the behaviour is to log the error and panic the device.
*/
void ShadowProfile_StateError(MessageId id, Message message);

/*! \brief Handle audio started indication */
void ShadowProfile_HandleAudioStartedInd(void);

#endif /* SHADOW_PROFILE_SM_H_ */
