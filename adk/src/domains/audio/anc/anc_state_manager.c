/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       anc_state_manager.c
\brief      State manager implementation for Active Noise Cancellation (ANC) which handles transitions
            between init, powerOff, powerOn, enable, disable and tuning states.
*/


#ifdef ENABLE_ANC
#include "anc_state_manager.h"
#include "anc_config.h"
#include "kymera.h"
#include "microphones.h"
#include <panic.h>
#include <task_list.h>
#include <logging.h>

#define DEBUG_ASSERT(x, y) {if (!(x)) {DEBUG_LOGF(y);Panic();}}

/* ANC state manager data */
typedef struct 
{
    unsigned requested_enabled:1;
    unsigned actual_enabled:1;
    unsigned power_on:1;
    unsigned persist_anc_mode:1;
    unsigned persist_anc_enabled:1;
    unsigned unused:11;
    anc_state_manager_t state;
    anc_mode_t current_mode;
    anc_mode_t requested_mode;
    uint8 num_modes;
} anc_state_manager_data_t;

static anc_state_manager_data_t anc_data;

/* ANC State Manager Events which are triggered during state transitions */
typedef enum
{
    anc_state_manager_event_initialise,
    anc_state_manager_event_power_on,
    anc_state_manager_event_power_off,
    anc_state_manager_event_enable,
    anc_state_manager_event_disable,
    anc_state_manager_event_set_mode_1,
    anc_state_manager_event_set_mode_2,
    anc_state_manager_event_set_mode_3,
    anc_state_manager_event_set_mode_4,
    anc_state_manager_event_set_mode_5,
    anc_state_manager_event_set_mode_6,
    anc_state_manager_event_set_mode_7,
    anc_state_manager_event_set_mode_8,
    anc_state_manager_event_set_mode_9,
    anc_state_manager_event_set_mode_10,
    anc_state_manager_event_activate_tuning_mode,
    anc_state_manager_event_deactivate_tuning_mode
} anc_state_manager_event_id_t;

static anc_mode_t getModeFromSetModeEvent(anc_state_manager_event_id_t event)
{
    anc_mode_t mode = anc_mode_1;
    
    switch(event)
    {
        case anc_state_manager_event_set_mode_2:
            mode = anc_mode_2;
            break;
        case anc_state_manager_event_set_mode_3:
            mode = anc_mode_3;
            break;
        case anc_state_manager_event_set_mode_4:
            mode = anc_mode_4;
            break;
        case anc_state_manager_event_set_mode_5:
            mode = anc_mode_5;
            break;
        case anc_state_manager_event_set_mode_6:
            mode = anc_mode_6;
            break;
        case anc_state_manager_event_set_mode_7:
            mode = anc_mode_7;
            break;
        case anc_state_manager_event_set_mode_8:
            mode = anc_mode_8;
            break;
        case anc_state_manager_event_set_mode_9:
            mode = anc_mode_9;
            break;
        case anc_state_manager_event_set_mode_10:
            mode = anc_mode_10;
            break;
        case anc_state_manager_event_set_mode_1:
        default:
            break;
    }
    return mode;
}

static anc_state_manager_event_id_t getSetModeEventFromMode(anc_mode_t mode)
{
    anc_state_manager_event_id_t state_event = anc_state_manager_event_set_mode_1;
    
    switch(mode)
    {
        case anc_mode_2:
            state_event = anc_state_manager_event_set_mode_2;
            break;
        case anc_mode_3:
            state_event = anc_state_manager_event_set_mode_3;
            break;
        case anc_mode_4:
            state_event = anc_state_manager_event_set_mode_4;
            break;
        case anc_mode_5:
            state_event = anc_state_manager_event_set_mode_5;
            break;
        case anc_mode_6:
            state_event = anc_state_manager_event_set_mode_6;
            break;
        case anc_mode_7:
            state_event = anc_state_manager_event_set_mode_7;
            break;
        case anc_mode_8:
            state_event = anc_state_manager_event_set_mode_8;
            break;
        case anc_mode_9:
            state_event = anc_state_manager_event_set_mode_9;
            break;
        case anc_mode_10:
            state_event = anc_state_manager_event_set_mode_10;
            break;
        case anc_mode_1:
        default:
            break;
    }
    return state_event;
}

/******************************************************************************
DESCRIPTION
    This function is responsible for persisting any of the ANC session data
    that is required.
*/
static void setSessionData(void)
{
    anc_writeable_config_def_t *write_data = NULL;

    if(ancConfigManagerGetWriteableConfig(ANC_WRITEABLE_CONFIG_BLK_ID, (void **)&write_data, 0))
    {
        if (anc_data.persist_anc_enabled)
        {
            DEBUG_LOGF("setSessionData: Persisting ANC enabled state %d\n", anc_data.requested_enabled);
            write_data->initial_anc_state =  anc_data.requested_enabled;
        }
    
        if (anc_data.persist_anc_mode)
        {
            DEBUG_LOGF("setSessionData: Persisting ANC mode %d\n", anc_data.requested_mode);
            write_data->initial_anc_mode = anc_data.requested_mode;
        }
    
        ancConfigManagerUpdateWriteableConfig(ANC_WRITEABLE_CONFIG_BLK_ID);
    }
}


/******************************************************************************
DESCRIPTION
    Handle the transition into a new state. This function is responsible for
    generating the state related system events.
*/
static void changeState(anc_state_manager_t new_state)
{
    DEBUG_LOGF("changeState: ANC State %d -> %d\n", anc_data.state, new_state);

    if ((new_state == anc_state_manager_power_off) && (anc_data.state != anc_state_manager_uninitialised))
    {
        /* When we power off from an on state persist any state required */
        setSessionData();
    }
    /* Update state */
    anc_data.state = new_state;
}

/******************************************************************************
DESCRIPTION
    Setup Enter tuning mode by disabling ANC and changes state to tuning mode.
*/
static void setupEnterTuningMode(void)
{
    DEBUG_LOGF("setupTuningMode: KymeraAnc_EnterTuning \n");

    if(AncIsEnabled())
    {
       AncEnable(FALSE);
    }
        
    KymeraAnc_EnterTuning();
    
    changeState(anc_state_manager_tuning_mode_active);
}

/****************************************************************************
DESCRIPTION
    The function returns the number of modes configured.

RETURNS
    total modes in anc_modes_t

*/
static uint8 getNumberOfModes(void)
{
    return anc_data.num_modes;
}

/******************************************************************************
DESCRIPTION
    Update the state of the ANC VM library. This is the 'actual' state, as opposed
    to the 'requested' state and therefore the 'actual' state variables should
    only ever be updated in this function.
    
    RETURNS
    Bool indicating if updating lib state was successful or not.

*/  
static bool updateLibState(bool enable, anc_mode_t mode)
{
    bool retry_later = TRUE;

    /* Check to see if we are changing mode */
    if (anc_data.current_mode != mode)
    {
        /* Set ANC Mode */
        if (!AncSetMode(mode) || (anc_data.requested_mode >= getNumberOfModes()))
        {
            DEBUG_LOGF("updateLibState: ANC Set Mode failed %d \n", mode + 1);
            retry_later = FALSE;
            /* fallback to previous success mode set */
            anc_data.requested_mode = anc_data.current_mode;
        }
        else
        {
            /* Update mode state */
            DEBUG_LOGF("updateLibState: ANC Set Mode %d\n", mode + 1);
            anc_data.current_mode = mode;
        }
     }

     /* Determine state to update in VM lib */
     if (anc_data.actual_enabled != enable)
     {
         if (!AncEnable(enable))
         {
             /* If this does fail in a release build then we will continue
                 and updating the ANC state will be tried again next time
                 an event causes a state change. */
             DEBUG_LOGF("updateLibState: ANC Enable failed %d\n", enable);
             retry_later = FALSE;
         }

         /* Update enabled state */
         DEBUG_LOGF("updateLibState: ANC Enable %d\n", enable);
         anc_data.actual_enabled = enable;
     }    
    
     return retry_later;
}

/******************************************************************************
DESCRIPTION
    Update session data retrieved from config

RETURNS
    Bool indicating if updating config was successful or not.
*/
static bool getSessionData(void)
{
    anc_writeable_config_def_t *write_data = NULL;

    ancConfigManagerGetReadOnlyConfig(ANC_WRITEABLE_CONFIG_BLK_ID, (const void **)&write_data);

    /* Extract session data */
    anc_data.requested_enabled = write_data->initial_anc_state;
    anc_data.persist_anc_enabled = write_data->persist_initial_state;
    anc_data.requested_mode = write_data->initial_anc_mode;
    anc_data.persist_anc_mode = write_data->persist_initial_mode;
    
    ancConfigManagerReleaseConfig(ANC_WRITEABLE_CONFIG_BLK_ID);

    return TRUE;
}

/******************************************************************************
DESCRIPTION
    Read the configuration from the ANC Mic params.
*/
static bool readMicConfigParams(anc_mic_params_t *anc_mic_params)
{
    anc_readonly_config_def_t *read_data = NULL;

    if (ancConfigManagerGetReadOnlyConfig(ANC_READONLY_CONFIG_BLK_ID, (const void **)&read_data))
    {
        microphone_number_t feedForwardLeftMic = read_data->anc_mic_params_r_config.feed_forward_left_mic;
        microphone_number_t feedForwardRightMic = read_data->anc_mic_params_r_config.feed_forward_right_mic;
        microphone_number_t feedBackLeftMic = read_data->anc_mic_params_r_config.feed_back_left_mic;
        microphone_number_t feedBackRightMic = read_data->anc_mic_params_r_config.feed_back_right_mic;

        memset(anc_mic_params, 0, sizeof(anc_mic_params_t));

        if (feedForwardLeftMic)
        {
            anc_mic_params->enabled_mics |= feed_forward_left;
            anc_mic_params->feed_forward_left = *Microphones_GetMicrophoneConfig(feedForwardLeftMic);
        }

        if (feedForwardRightMic)
        {
            anc_mic_params->enabled_mics |= feed_forward_right;
            anc_mic_params->feed_forward_right = *Microphones_GetMicrophoneConfig(feedForwardRightMic);
        }

        if (feedBackLeftMic)
        {
            anc_mic_params->enabled_mics |= feed_back_left;
            anc_mic_params->feed_back_left = *Microphones_GetMicrophoneConfig(feedBackLeftMic);
        }

        if (feedBackRightMic)
        {
            anc_mic_params->enabled_mics |= feed_back_right;
            anc_mic_params->feed_back_right = *Microphones_GetMicrophoneConfig(feedBackRightMic);
        }

        ancConfigManagerReleaseConfig(ANC_READONLY_CONFIG_BLK_ID);

        return TRUE;
    }
    DEBUG_LOGF("readMicConfigParams: Failed to read ANC Config Block\n");
    return FALSE;
}

/****************************************************************************    
DESCRIPTION
    Read the number of configured Anc modes.
*/
static uint8 readNumModes(void)
{
    anc_readonly_config_def_t *read_data = NULL;
    uint8 num_modes = 0;

    /* Read the existing Config data */
    if (ancConfigManagerGetReadOnlyConfig(ANC_READONLY_CONFIG_BLK_ID, (const void **)&read_data))
    {
        num_modes = read_data->num_anc_modes;
        ancConfigManagerReleaseConfig(ANC_READONLY_CONFIG_BLK_ID);
    }
    return num_modes;
}

anc_mode_t AncStateManager_GetMode(void)
{
    return (anc_data.requested_mode);
}

/******************************************************************************
DESCRIPTION
    This function reads the ANC configuration and initialises the ANC library
    returns TRUE on success FALSE otherwise 
*/ 
static bool configureAndInit(void)
{
    anc_mic_params_t anc_mic_params;
    bool init_success = FALSE;

    if(readMicConfigParams(&anc_mic_params) && getSessionData())
    {
        if(AncInit(&anc_mic_params, AncStateManager_GetMode(), 0))
        {
            /* Update local state to indicate successful initialisation of ANC */
            anc_data.current_mode = anc_data.requested_mode;
            anc_data.actual_enabled = FALSE;
            anc_data.num_modes = readNumModes();
            changeState(anc_state_manager_power_off);
            init_success = TRUE;
        }
    }
    return init_success;
}

/******************************************************************************
DESCRIPTION
    Event handler for the Uninitialised State

RETURNS
    Bool indicating if processing event was successful or not.
*/ 
static bool handleUninitialisedEvent(anc_state_manager_event_id_t event)
{
    bool init_success = FALSE;

    switch (event)
    {
        case anc_state_manager_event_initialise:
        {
            if(configureAndInit())
            {
                init_success = TRUE;
            }
            else
            {
                DEBUG_ASSERT(init_success, "handleUninitialisedEvent: ANC Failed to Initialise\n");
                /* indicate error by Led */
            }
        }
        break;

        default:
        {
            DEBUG_LOGF("handleUninitialisedEvent: Unhandled event [%d]\n", event);
        }
        break;
    }
    return init_success;
}

/******************************************************************************
DESCRIPTION
    Event handler for the Power Off State

RETURNS
    Bool indicating if processing event was successful or not.
*/ 
static bool handlePowerOffEvent(anc_state_manager_event_id_t event)
{
    bool event_handled = FALSE;

    DEBUG_ASSERT(!anc_data.actual_enabled, "handlePowerOffEvent: ANC actual enabled in power off state\n");

    switch (event)
    {
        case anc_state_manager_event_power_on:
        {
            anc_state_manager_t next_state = anc_state_manager_disabled;
            anc_data.power_on = TRUE;

            /* If we were previously enabled then enable on power on */
            if (anc_data.requested_enabled)
            {
                if(updateLibState(anc_data.requested_enabled, anc_data.requested_mode))
                {
                    /* Lib is enabled */
                    next_state = anc_state_manager_enabled;
                }
            }
            /* Update state */
            changeState(next_state);
            
            event_handled = TRUE;
        }
        break;

        default:
        {
            DEBUG_LOGF("handlePowerOffEvent: Unhandled event [%d]\n", event);
        }
        break;
    }
    return event_handled;
}

/******************************************************************************
DESCRIPTION
    Event handler for the Enabled State

RETURNS
    Bool indicating if processing event was successful or not.
*/
static bool handleEnabledEvent(anc_state_manager_event_id_t event)
{
    /* Assume failure until proven otherwise */
    bool event_handled = FALSE;
    anc_state_manager_t next_state = anc_state_manager_disabled;

    switch (event)
    {
        case anc_state_manager_event_power_off:
        {
            /* When powering off we need to disable ANC in the VM Lib first */
            next_state = anc_state_manager_power_off;
            anc_data.power_on = FALSE;
        }
        /* fallthrough */
        case anc_state_manager_event_disable:
        {
            /* Only update requested enabled if not due to a power off event */
            anc_data.requested_enabled = (next_state == anc_state_manager_power_off);

            /* Disable ANC */
            updateLibState(FALSE, anc_data.requested_mode);
            
            /* Update state */
            changeState(next_state);
            event_handled = TRUE;
        }
        break;

        case anc_state_manager_event_set_mode_1: /* fallthrough */
        case anc_state_manager_event_set_mode_2:
        case anc_state_manager_event_set_mode_3:
        case anc_state_manager_event_set_mode_4:
        case anc_state_manager_event_set_mode_5:
        case anc_state_manager_event_set_mode_6:
        case anc_state_manager_event_set_mode_7:
        case anc_state_manager_event_set_mode_8:
        case anc_state_manager_event_set_mode_9:
        case anc_state_manager_event_set_mode_10:            
        {
            anc_data.requested_mode = getModeFromSetModeEvent(event);           

            /* Update the ANC Mode */
            updateLibState(anc_data.requested_enabled, anc_data.requested_mode);
            
            event_handled = TRUE;
        }
        break;

        case anc_state_manager_event_activate_tuning_mode:
        {            
            setupEnterTuningMode();
            event_handled = TRUE;
        }
        break;
            
        default:
        {
            DEBUG_LOGF("handleEnabledEvent: Unhandled event [%d]\n", event);
        }
        break;
    }
    return event_handled;
}

/******************************************************************************
DESCRIPTION
    Event handler for the Disabled State

RETURNS
    Bool indicating if processing event was successful or not.
*/
static bool handleDisabledEvent(anc_state_manager_event_id_t event)
{
    /* Assume failure until proven otherwise */
    bool event_handled = FALSE;

    switch (event)
    {
        case anc_state_manager_event_power_off:
        {
            /* Nothing to do, just update state */
            changeState(anc_state_manager_power_off);
            anc_data.power_on = FALSE;
            event_handled = TRUE;
        }
        break;

        case anc_state_manager_event_enable:
        {
            /* Try to enable */
            anc_state_manager_t next_state = anc_state_manager_enabled;
            anc_data.requested_enabled = TRUE;

            /* Enable ANC */
            updateLibState(anc_data.requested_enabled, anc_data.requested_mode);
            
            /* Update state */
            changeState(next_state);
           
            event_handled = TRUE;
        }
        break;

        case anc_state_manager_event_set_mode_1: /* fallthrough */
        case anc_state_manager_event_set_mode_2:
        case anc_state_manager_event_set_mode_3:
        case anc_state_manager_event_set_mode_4:
        case anc_state_manager_event_set_mode_5:
        case anc_state_manager_event_set_mode_6:
        case anc_state_manager_event_set_mode_7:
        case anc_state_manager_event_set_mode_8:
        case anc_state_manager_event_set_mode_9:
        case anc_state_manager_event_set_mode_10:     
        {
            /* Update the requested ANC Mode, will get applied next time we enable */
            anc_data.requested_mode = getModeFromSetModeEvent(event);
            event_handled = TRUE;
        }
        break;
        
        case anc_state_manager_event_activate_tuning_mode:
        {
            setupEnterTuningMode();
            event_handled = TRUE;
        }
        break;

        default:
        {
            DEBUG_LOGF("handleDisabledEvent: Unhandled event [%d]\n", event);
        }
        break;
    }
    return event_handled;
}

static bool handleTuningModeActiveEvent(anc_state_manager_event_id_t event)
{
    bool event_handled = FALSE;
    
    switch(event)
    {
        case anc_state_manager_event_power_off:
        {
            DEBUG_LOGF("handleTuningModeActiveEvent: anc_state_manager_event_power_off\n");

            KymeraAnc_ExitTuning();
            changeState(anc_state_manager_power_off);
            event_handled = TRUE;
        }
        break;

        case anc_state_manager_event_deactivate_tuning_mode:
        {
            DEBUG_LOGF("handleTuningModeActiveEvent: anc_state_manager_event_deactivate_tuning_mode\n");

            KymeraAnc_ExitTuning();
            changeState(anc_state_manager_disabled);
            event_handled = TRUE;
        }
        break;
        
        default:
        break;
    }
    return event_handled;
}

/******************************************************************************
DESCRIPTION
    Entry point to the ANC State Machine.

RETURNS
    Bool indicating if processing event was successful or not.
*/
static bool ancStateManager_HandleEvent(anc_state_manager_event_id_t event)
{
    bool ret_val = FALSE;

    DEBUG_LOGF("ancStateManager_HandleEvent: ANC Handle Event %d in State %d\n", event, anc_data.state);

    switch(anc_data.state)
    {
        case anc_state_manager_uninitialised:
            ret_val = handleUninitialisedEvent(event);
        break;
        
        case anc_state_manager_power_off:
            ret_val = handlePowerOffEvent(event);
        break;

        case anc_state_manager_enabled:
            ret_val = handleEnabledEvent(event);
        break;

        case anc_state_manager_disabled:
            ret_val = handleDisabledEvent(event);
        break;

        case anc_state_manager_tuning_mode_active:
            ret_val = handleTuningModeActiveEvent(event);
        break;

        default:
            DEBUG_LOGF("ancStateManager_HandleEvent: Unhandled state [%d]\n", anc_data.state);
        break;
    }
    return ret_val;
}

/*******************************************************************************
 * All the functions from this point onwards are the ANC module API functions
 * The functions are simply responsible for injecting
 * the correct event into the ANC State Machine, which is then responsible
 * for taking the appropriate action.
 ******************************************************************************/

bool AncStateManager_Init(Task init_task)
{
    UNUSED(init_task);

    /* Initialise the ANC VM Lib */
    if(ancStateManager_HandleEvent(anc_state_manager_event_initialise))
    {
        /* Initialisation successful, go ahead with ANC power ON*/
        AncStateManager_PowerOn();
    }
    return TRUE;
}

void AncStateManager_PowerOn(void)
{
    /* Power On ANC */
    if(!ancStateManager_HandleEvent(anc_state_manager_event_power_on))
    {
        DEBUG_LOGF("AncStateManager_PowerOn: Power On ANC failed\n");
    }
}

void AncStateManager_PowerOff(void)
{
    /* Power Off ANC */
    if (!ancStateManager_HandleEvent(anc_state_manager_event_power_off))
    {
        DEBUG_LOGF("AncStateManager_PowerOff: Power Off ANC failed\n");
    }
}

void AncStateManager_Enable(void)
{
    /* Enable ANC */
    if (!ancStateManager_HandleEvent(anc_state_manager_event_enable))
    {
        DEBUG_LOGF("AncStateManager_Enable: Enable ANC failed\n");
    }
}

void AncStateManager_Disable(void)
{
    /* Disable ANC */
    if (!ancStateManager_HandleEvent(anc_state_manager_event_disable))
    {
        DEBUG_LOGF("AncStateManager_Disable: Disable ANC failed\n");
    }
}

void AncStateManager_SetMode(anc_mode_t mode)
{
    anc_state_manager_event_id_t state_event = getSetModeEventFromMode (mode);

    /* Set ANC mode_n */
    if (!ancStateManager_HandleEvent(state_event))
    {
        DEBUG_LOGF("AncStateManager_SetMode: Set ANC Mode %d failed\n", mode);
    }
}

void AncStateManager_EnterTuningMode(void)
{
    if(!ancStateManager_HandleEvent(anc_state_manager_event_activate_tuning_mode))
    {
       DEBUG_LOGF("AncStateManager_EnterTuningMode: Tuning mode event failed\n");
    }
}

void AncStateManager_ExitTuningMode(void)
{
    if(!ancStateManager_HandleEvent(anc_state_manager_event_deactivate_tuning_mode))
    {
       DEBUG_LOGF("AncStateManager_ExitTuningMode: Tuning mode event failed\n");
    }
}

bool AncStateManager_IsEnabled(void)
{
    return (anc_data.state == anc_state_manager_enabled);
}

anc_mode_t AncStateManager_GetNextMode(anc_mode_t anc_mode)
{
    anc_mode++;
    if(anc_mode > getNumberOfModes())
    {
       anc_mode = anc_mode_1;
    }
    return anc_mode;
}

void AncStateManager_SetNextMode(void)
{
    anc_data.requested_mode = AncStateManager_GetNextMode(anc_data.requested_mode);
    AncStateManager_SetMode(anc_data.requested_mode);
 }

bool AncStateManager_IsTuningModeActive(void)
{
    return (anc_data.state == anc_state_manager_tuning_mode_active);
}

#ifdef ANC_PEER_SUPPORT
void AncStateManager_SychroniseStateWithPeer (void)
{
    if (ancPeerIsLinkMaster())
    {
        ancPeerSendAncState();
        ancPeerSendAncMode();
    }
}

MessageId AncStateManager_GetNextState(void)
{
    MessageId id;

    if (AncStateManager_IsEnabled())
    {
        id = EventUsrAncOff;
    }
    else
    {
        id = EventUsrAncOn;
    }

    return id;
}

MessageId AncStateManager_GetUsrEventFromMode(anc_mode_t anc_mode)
{
    MessageId id = EventUsrAncMode1;

    switch(anc_mode)
    {
        case anc_mode_1:
            id = EventUsrAncMode1;
            break;
        case anc_mode_2:
            id = EventUsrAncMode2;
            break;
        case anc_mode_3:
            id = EventUsrAncMode3;
            break;
        case anc_mode_4:
            id = EventUsrAncMode4;
            break;
        case anc_mode_5:
            id = EventUsrAncMode5;
            break;
        case anc_mode_6:
            id = EventUsrAncMode6;
            break;
        case anc_mode_7:
            id = EventUsrAncMode7;
            break;
        case anc_mode_8:
            id = EventUsrAncMode8;
            break;
        case anc_mode_9:
            id = EventUsrAncMode9;
            break;
        case anc_mode_10:
            id = EventUsrAncMode10;
            break;
        default:
            break;
    }
    return id;
}
#endif /* ANC_PEER_SUPPORT */

#ifdef ANC_TEST_BUILD
void AncStateManager_ResetStateMachine(anc_state_manager_t state)
{
    anc_data.state = state;
}
#endif /* ANC_TEST_BUILD */

#endif /* ENABLE_ANC */
