/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       anc_state_manager.h
\defgroup   anc_state_manager anc 
\ingroup    audio_domain
\brief      State manager for Active Noise Cancellation (ANC).

Responsibilities:
  Handles state transitions between init, enable and disable states.
  The ANC audio domain is used by \ref audio_curation.
*/

#ifndef ANC_STATE_MANAGER_H_
#define ANC_STATE_MANAGER_H_

/*\{*/
#ifdef ENABLE_ANC
#include <anc.h>
        
/*! \brief ANC state manager defines the various states handled in ANC. */
typedef enum
{
    anc_state_manager_uninitialised,
    anc_state_manager_power_off,
    anc_state_manager_enabled,
    anc_state_manager_disabled,
    anc_state_manager_tuning_mode_active
} anc_state_manager_t;
#endif

/*!
    \brief Initialisation of ANC feature, reads microphone configuration  
           and default mode.
    \param init_task Unused
    \return TRUE always.
*/
#ifdef ENABLE_ANC
bool AncStateManager_Init(Task init_task);
#else
#define AncStateManager_Init(x) (FALSE)
#endif

/*!
    \brief ANC specific handling due to the device Powering On.
*/   
#ifdef ENABLE_ANC
void AncStateManager_PowerOn(void);
#else
#define AncStateManager_PowerOn() ((void)(0))
#endif

/*!
    \brief ANC specific handling due to the device Powering Off.
*/  
#ifdef ENABLE_ANC
void AncStateManager_PowerOff(void);
#else
#define AncStateManager_PowerOff() ((void)(0))
#endif

/*!
    \brief Enable ANC functionality.  
*/   
#ifdef ENABLE_ANC
void AncStateManager_Enable(void);
#else
#define AncStateManager_Enable() ((void)(0))
#endif

/*! 
    \brief Disable ANC functionality.
 */   
#ifdef ENABLE_ANC
void AncStateManager_Disable(void);
#else
#define AncStateManager_Disable() ((void)(0))
#endif

/*!
    \brief Set the operating mode of ANC to configured mode_n. 
    \param mode To be set from existing avalaible modes 0 to 9.
*/
#ifdef ENABLE_ANC
void AncStateManager_SetMode(anc_mode_t mode);
#else
#define AncStateManager_SetMode(x) ((void)(0 * (x)))
#endif

/*! 
    \brief Get the Anc mode configured.
    \return mode which is set (from available mode 0 to 9).
 */
#ifdef ENABLE_ANC
anc_mode_t AncStateManager_GetMode(void);
#else
#define AncStateManager_GetMode() (0)
#endif

/*! 
    \brief Checks if ANC is due to be enabled.
    \return TRUE if it is enabled else FALSE.
 */
#ifdef ENABLE_ANC
bool AncStateManager_IsEnabled (void);
#else
#define AncStateManager_IsEnabled() (FALSE)
#endif

/*! 
    \brief Checks the next mode of ANC.
    \param mode     Change to the next mode which is to be configured.
    \return anc_mode_t   The next mode to be set is returned from available modes 0 to 9.
 */
#ifdef ENABLE_ANC
anc_mode_t AncStateManager_GetNextMode(anc_mode_t mode);
#else
#define AncStateManager_GetNextMode(x) (0)
#endif

/*! 
    \brief Checks whether tuning mode is currently active.
    \return TRUE if it is active, else FALSE.
 */
#ifdef ENABLE_ANC
bool AncStateManager_IsTuningModeActive(void);
#else
#define AncStateManager_IsTuningModeActive() (FALSE)
#endif

/*! 
    \brief Cycles through next mode and sets it.
 */
#ifdef ENABLE_ANC
void AncStateManager_SetNextMode(void);
#else
#define AncStateManager_SetNextMode() ((void)(0))
#endif

/*! 
    \brief Enters ANC tuning mode.
 */
#ifdef ENABLE_ANC
void AncStateManager_EnterTuningMode(void);
#else
#define AncStateManager_EnterTuningMode() ((void)(0))
#endif

/*! 
    \brief Exits the ANC tuning mode.
 */
#ifdef ENABLE_ANC
void AncStateManager_ExitTuningMode(void);
#else
#define AncStateManager_ExitTuningMode() ((void)(0))
#endif

/*! 
    \brief  Configure ANC settings when a Peer connects.
 */
#ifdef ANC_PEER_SUPPORT
void AncStateManager_SychroniseStateWithPeer(void);
#else
#define AncStateManager_SychroniseStateWithPeer() ((void)(0))
#endif

/*! 
    \brief Get the next state of ANC.
    \return MessageId  Corresponding ANC message id either on/off is retrieved.
 */
#ifdef ANC_PEER_SUPPORT
MessageId AncStateManager_GetNextState(void);
#else
#define AncStateManager_GetNextState() (0)
#endif

/*! 
    \brief Get user event for the corresponding Anc mode.
    \param mode   current mode set.
    \return MessageId   Retrieve the message id for modes 0 to 9 set.
 */
#ifdef ANC_PEER_SUPPORT
MessageId AncStateManager_GetUsrEventFromMode(anc_mode_t mode);
#else
#define AncStateManager_GetUsrEventFromMode(x) (0)
#endif

/*! 
    \brief Test hook for unit tests to reset the ANC state.
    \param state  Reset the particular state
 */
#ifdef ANC_TEST_BUILD

#ifdef ENABLE_ANC
void AncStateManager_ResetStateMachine(anc_state_manager_t state);
#else
#define AncStateManager_ResetStateMachine(x) ((void)(0))
#endif

#endif /* ANC_TEST_BUILD*/

/*\}*/
#endif /* ANC_STATE_MANAGER_H_ */
