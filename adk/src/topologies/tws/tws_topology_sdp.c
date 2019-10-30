/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Functions related to setting up SDP records for TWS
*/

#include "tws_topology.h"
#include "tws_topology_sdp.h"
#include "tws_topology_private.h"
#include "tws_topology_sm.h"
#include "earbud_init.h"

#include "sdp.h"
#include "bt_device.h"
#include <panic.h>
#include <connection.h>
#include <logging.h>

#include <key_sync.h>

/* TODO these have been placed here to keep this module independent
 put them in the structure of whatever ultimately owns this functionality */
static uint32 tws_sink_service_handle;
static bdaddr search_addr = {0};
static int sdp_search_attempts = 0;


void TwsTopology_RegisterServiceRecord(Task response_task)
{
    DEBUG_LOG("TwsTopology_RegisterServiceRecord");

    /* Unregister service record if there's one already registered */
    if(tws_sink_service_handle)
    {
        ConnectionUnregisterServiceRecord(response_task, tws_sink_service_handle);
    }
    else
    {
        bdaddr bd_addr = {0, 0, 0};

        /* Make copy of service secord */
        uint8 *record = PanicUnlessMalloc(appSdpGetTwsSinkServiceRecordSize());
        memcpy(record, appSdpGetTwsSinkServiceRecord(), appSdpGetTwsSinkServiceRecordSize());

        /* Find address of peer device */
        appDeviceGetSecondaryBdAddr(&bd_addr);

        /* Write peer device address into service record */
        appSdpSetTwsSinkServiceRecordPeerBdAddr(record, &bd_addr);

        DEBUG_LOGF("TwsTopology_RegisterServiceRecord, bd_addr %04x,%02x,%06lx", bd_addr.nap, bd_addr.uap, bd_addr.lap);

        /* Register service record */
        ConnectionRegisterServiceRecord(response_task, appSdpGetTwsSinkServiceRecordSize(), record);
    }
}


void TwsTopology_HandleClSdpRegisterCfm(Task task, const CL_SDP_REGISTER_CFM_T *cfm)
{
    UNUSED(task);

    DEBUG_LOGF("TwsTopology_HandleClSdpRegisterCfm, status %d", cfm->status);

    switch (TwsTopology_GetState())
    {
        case TWS_TOPOLOGY_STATE_SETTING_SDP:
            if (cfm->status == sds_status_success)
            {  /* Registration of service record was sucessful, so store service handle
                 * for use later on */
                PanicNotZero(tws_sink_service_handle);
                tws_sink_service_handle = cfm->service_handle;

                /* Inform main task that pairing is initialised */
                MessageSend(appInitGetInitTask(), TWS_TOPOLOGY_INIT_CFM, NULL);

                /* Move to 'idle' state */
                TwsTopology_SetState(TWS_TOPOLOGY_STATE_NO_ROLE);
            }
            else
            {
                Panic();
            }
            break;

        default:
            Panic();
            break;
    }
}

void TwsTopology_HandleClSdpUnregisterCfm(Task task,
                                          const CL_SDP_UNREGISTER_CFM_T *cfm)
{
    DEBUG_LOGF("TwsTopology_HandleClSdpUnregisterCfm, status %d", cfm->status);

    if (cfm->status == sds_status_success)
    {
        if (tws_sink_service_handle == cfm->service_handle)
        {
            tws_sink_service_handle = 0;

            /* Re-register service record with new peer device address */
            TwsTopology_RegisterServiceRecord(task);
        }
    }
    else if (cfm->status == sds_status_pending)
    {
        /* Wait for final confirmation message */
    }
    else
    {
        Panic();
    }
}


void TwsTopology_SearchForTwsPlusSdp(bdaddr bd_addr)
{
    DEBUG_LOG("TwsTopology_SearchForTwsPlusSdp");

    search_addr = bd_addr;

    /* Perform SDP search */
    ConnectionSdpServiceSearchAttributeRequest(TwsTopologyGetTask(), &bd_addr, 0x32,
                                               appSdpGetTwsSourceServiceSearchRequestSize(), appSdpGetTwsSourceServiceSearchRequest(),
                                               appSdpGetTwsSourceAttributeSearchRequestSize(), appSdpGetTwsSourceAttributeSearchRequest());
}


void TwsTopology_HandleClSdpServiceSearchAttributeCfm(const CL_SDP_SERVICE_SEARCH_ATTRIBUTE_CFM_T *cfm)
{
    DEBUG_LOGF("TwsTopology_HandleClSdpServiceSearchAttributeCfm, status %d", cfm->status);

    if (BdaddrIsSame(&search_addr, &cfm->bd_addr))
    {
        if (cfm->status == sdp_response_success)
        {
            /* Received response, so extract TWS version from attribute */
            uint16 tws_version = DEVICE_TWS_STANDARD;
            appSdpFindTwsVersion(cfm->attributes, cfm->attributes + cfm->size_attributes, &tws_version);
            if (appDeviceSetTwsVersion(&cfm->bd_addr, tws_version))
            {
                KeySync_Sync();
            }
            DEBUG_LOGF("TwsTopology_HandleClSdpServiceSearchAttributeCfm, TWS+ device %x,%x,%lx, version %d",
                         cfm->bd_addr.nap, cfm->bd_addr.uap, cfm->bd_addr.lap, tws_version);

            sdp_search_attempts = 0;
        }
        else if (cfm->status == sdp_no_response_data)
        {
            DEBUG_LOGF("pairing_HandleClSdpServiceSearchAttributeCfm, standard device %x,%x,%lx",
                         cfm->bd_addr.nap, cfm->bd_addr.uap, cfm->bd_addr.lap);

            /* No response data, so handset doesn't have UUID and/or version attribute, therefore
               treat as standard handset */
            if (appDeviceSetTwsVersion(&cfm->bd_addr, DEVICE_TWS_STANDARD))
            {
                KeySync_Sync(); 
            }
        }
        else
        {
            /* Increment counter, try again if haven't hit limit */
            sdp_search_attempts += 1;
            if (sdp_search_attempts < 5)
            {
                /* Send message to retry after 1 second */
/*TODO put this back once this search has found it's forever home in the meantime just set search attempts to 0
               MessageSendLater(&thePairing->task, PAIRING_INTERNAL_HANDSET_SDP_SEARCH, NULL, D_SEC(1));
*/
                sdp_search_attempts = 0; /* TODO remove this once the retry mechanism is re-instated */
            }
            else
            {
                /* SDP search failed even after retries */
                sdp_search_attempts = 0;
            }
        }
    }
}


