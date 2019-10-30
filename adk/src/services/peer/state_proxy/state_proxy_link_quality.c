/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Link quality measurement
*/

#include <connection.h>
#include <message.h>
#include <peer_signalling.h>
#include <logging.h>
#include <stdlib.h>

#include "state_proxy.h"
#include "state_proxy_private.h"
#include "state_proxy_link_quality.h"
#include "state_proxy_connection.h"
#include "state_proxy_marshal_defs.h"
#include "state_proxy_client_msgs.h"

#ifdef ENABLE_LINK_QUALITY_LOG
#define LINK_QUALITY_LOG DEBUG_LOG
#else
#define LINK_QUALITY_LOG(...)
#endif

/* Link quality measurement interval */
#define STATE_PROXY_LINK_QUALITY_INTERVAL_MS 500

static void stateProxy_SendIntervalTimerMessage(void)
{
    MessageSendLater(stateProxy_GetTask(), STATE_PROXY_INTERNAL_TIMER_LINK_QUALITY,
                     NULL, STATE_PROXY_LINK_QUALITY_INTERVAL_MS);
}

static void stateProxy_StartNextMeasurement(bool restart)
{
    state_proxy_data_t *data = stateProxy_GetLocalData();
    unsigned index;

    /* Start measurements from the first active connection */
    for (index = restart ? 0 : (stateProxy_GetLinkQualityIndex() + 1);
         index < ARRAY_DIM(data->connection);
         index++)
    {
        state_proxy_connection_t *conn = &data->connection[index];
        if (!BdaddrTpIsEmpty(&conn->device))
        {
            stateProxy_SetLinkQualityIndex(index);
            ConnectionGetRssiBdaddr(stateProxy_GetTask(), &conn->device);
        }
    }
}

static void stateProxy_NotifyLinkQualityClients(state_proxy_source source, const tp_bdaddr *bd_addr)
{
    state_proxy_data_t *data = (source == state_proxy_source_local) ?
                                    stateProxy_GetLocalData() :
                                    stateProxy_GetRemoteData();
    const state_proxy_connection_t *conn = stateProxy_GetConnection(data, bd_addr);

    if (conn)
    {
        /* notify event specific clients */
        stateProxy_MsgStateProxyEventClients(source,
                                             state_proxy_event_type_link_quality,
                                             conn);

        stateProxy_MarshalToConnectedPeer(MARSHAL_TYPE(STATE_PROXY_LINK_QUALITY_T), conn, sizeof(*conn));
    }
}


void stateProxy_LinkQualityKick(void)
{
    bool enable = FALSE;
    if (stateProxy_AnyClientsRegisteredForEvent(state_proxy_event_type_link_quality))
    {
        if (stateProxy_AnyLocalConnections())
        {
            enable = TRUE;
        }
    }
    if (enable && !stateProxy_IsMeasuringLinkQuality())
    {
        stateProxy_SendIntervalTimerMessage();
        stateProxy_SetMesauringLinkQuality(TRUE);
        stateProxy_StartNextMeasurement(TRUE);
    }
    else if (!enable && stateProxy_IsMeasuringLinkQuality())
    {
        MessageCancelAll(stateProxy_GetTask(), STATE_PROXY_INTERNAL_TIMER_LINK_QUALITY);
        stateProxy_SetMesauringLinkQuality(FALSE);
    }
}

void stateProxy_HandleIntervalTimerLinkQuality(void)
{
    stateProxy_SetMesauringLinkQuality(FALSE);
    stateProxy_LinkQualityKick();
}

void stateProxy_HandleClDmRssiBdaddrCfm(const CL_DM_RSSI_BDADDR_CFM_T *cfm)
{
    if (cfm->status == hci_success)
    {
        if (stateProxy_ConnectionUpdateRssi(stateProxy_GetLocalData(), &cfm->tpaddr, cfm->rssi))
        {
            LINK_QUALITY_LOG("stateProxy_HandleClDmRssiBdaddrCfm %d 0x%x", cfm->rssi, cfm->tpaddr.taddr.addr.lap);
            ConnectionGetLinkQualityBdaddr(stateProxy_GetTask(), &cfm->tpaddr.taddr.addr);
        }
    }
    /* Ignore failure, almost certainly means the link has disconnected */
}

void stateProxy_HandleClDmLinkQualityBdaddrCfm(const CL_DM_LINK_QUALITY_BDADDR_CFM_T *cfm)
{
    if (cfm->status == hci_success)
    {
        tp_bdaddr bd_addr;
        BdaddrTpFromBredrBdaddr(&bd_addr, &cfm->bdaddr);
        if (stateProxy_ConnectionUpdateLinkQuality(stateProxy_GetLocalData(), &bd_addr, cfm->link_quality))
        {
            LINK_QUALITY_LOG("stateProxy_HandleClDmLinkQualityBdaddrCfm %d 0x%x", cfm->link_quality, cfm->bdaddr.lap);
            stateProxy_StartNextMeasurement(FALSE);
            stateProxy_NotifyLinkQualityClients(state_proxy_source_local, &bd_addr);
        }
    }
    /* Ignore failure, almost certainly means the link has disconnected */
}

void stateProxy_HandleRemoteLinkQuality(const STATE_PROXY_LINK_QUALITY_T *msg)
{
    stateProxy_ConnectionUpdateRssi(stateProxy_GetRemoteData(), &msg->device, msg->rssi);
    stateProxy_ConnectionUpdateLinkQuality(stateProxy_GetRemoteData(), &msg->device, msg->link_quality);
    stateProxy_NotifyLinkQualityClients(state_proxy_source_remote, &msg->device);
}
