/*!
\copyright  Copyright (c) 2005 - 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       state_proxy_phy_state.c
\brief      Handling of physical state events in the state proxy.
*/

/* local includes */
#include "state_proxy.h"
#include "state_proxy_private.h"
#include "state_proxy_marshal_defs.h"
#include "state_proxy_phy_state.h"

/* framework includes */
#include <phy_state.h>

/* system includes */
#include <panic.h>
#include <logging.h>
#include <stdlib.h>
#include <peer_signalling.h>

/* forward declarations */
static void stateProxy_UpdatePhyState(state_proxy_data_t* data, phy_state_event event);

/*! \brief Get phy state for initial state message. */
void stateProxy_GetInitialPhyState(void)
{
    state_proxy_task_data_t *proxy = stateProxy_GetTaskData();
    phyState phy_state = appPhyStateGetState();
    switch (phy_state)
    {
        case PHY_STATE_IN_CASE:
            proxy->local_state->flags.in_case = TRUE;
            proxy->local_state->flags.in_ear = FALSE;
            break;
        case PHY_STATE_OUT_OF_EAR:
            /* fall-through */
        case PHY_STATE_OUT_OF_EAR_AT_REST:
            proxy->local_state->flags.in_case = FALSE;
            proxy->local_state->flags.in_ear = FALSE;
            break;
        case PHY_STATE_IN_EAR:
            proxy->local_state->flags.in_case = FALSE;
            proxy->local_state->flags.in_ear = TRUE;
            break;
        default:
            break;
    }
}

static void stateProxy_HandlePhyStateChangedIndImpl(const PHY_STATE_CHANGED_IND_T* ind,
                                                    state_proxy_source source)
{
    DEBUG_LOG("stateProxy_HandlePhyStateChangedInd %d state %u", source, ind->new_state);

    /* update local state */
    stateProxy_UpdatePhyState(stateProxy_GetData(source), ind->event);

    /* notify event specific clients */
    stateProxy_MsgStateProxyEventClients(source,
                                         state_proxy_event_type_phystate,
                                         ind);

    if (source == state_proxy_source_local)
    {
        stateProxy_MarshalToConnectedPeer(MARSHAL_TYPE(PHY_STATE_CHANGED_IND_T), ind, sizeof(*ind));
    }
}

void stateProxy_HandlePhyStateChangedInd(const PHY_STATE_CHANGED_IND_T* ind)
{
    stateProxy_HandlePhyStateChangedIndImpl(ind, state_proxy_source_local);
}

void stateProxy_HandleRemotePhyStateChangedInd(const PHY_STATE_CHANGED_IND_T* ind)
{
    stateProxy_HandlePhyStateChangedIndImpl(ind, state_proxy_source_remote);
}

/*! \brief Helper function to update phystate flags for a local or remote dataset.
    \param[in] data Pointer to local or remote dataset.
    \param[in] event Phy state event.
*/
static void stateProxy_UpdatePhyState(state_proxy_data_t* data, phy_state_event event)
{
    /*! \todo just save the event instead? */
    switch (event)
    {
        case phy_state_event_in_case:
            data->flags.in_case = TRUE;
            break;
        case phy_state_event_out_of_case:
            data->flags.in_case = FALSE;
            break;
        case phy_state_event_in_ear:
            data->flags.in_ear = TRUE;
            break;
        case phy_state_event_out_of_ear:
            data->flags.in_ear = FALSE;
            break;
        case phy_state_event_in_motion:
            data->flags.in_motion = FALSE;
            break;
        case phy_state_event_not_in_motion:
            data->flags.in_motion = FALSE;
            break;
    }
}

