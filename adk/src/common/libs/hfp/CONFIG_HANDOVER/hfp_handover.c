/****************************************************************************
Copyright (c) 2019 Qualcomm Technologies International, Ltd.


FILE NAME
    hfp_handover.c

DESCRIPTION
    Implements HFP handover logic (Veto, Marshals/Unmarshals, Handover, etc).
 
NOTES
    See handover_if.h for further interface description
    
    Builds requiring this should include CONFIG_HANDOVER in the
    makefile. e.g.
        CONFIG_FEATURES:=CONFIG_HANDOVER    
 */

#include "hfp_marshal_desc.h"
#include "hfp_private.h"
#include "hfp_init.h"
#include "hfp_link_manager.h"

#include "marshal.h"
#include "bdaddr.h"
#include <panic.h>
#include <stdlib.h>
/*#include <app/bluestack/rfcomm_prim.h>*/
#include <sink.h>


#define RFC_INVALID_SERV_CHANNEL   0x00

static bool hfpVeto(void);

static bool hfpMarshal(const tp_bdaddr *tp_bd_addr,
                       uint8 *buf,
                       uint16 length,
                       uint16 *written);

static bool hfpUnmarshal(const tp_bdaddr *tp_bd_addr,
                         const uint8 *buf,
                         uint16 length,
                         uint16 *consumed);

static void hfpMarshalCommit( const bool newRole );

static void hfpMarshalAbort( void );

extern const handover_interface hfp_handover_if =  {
        &hfpVeto,
        &hfpMarshal,
        &hfpUnmarshal,
        &hfpMarshalCommit,
        &hfpMarshalAbort};


typedef struct
{
    enum
    {
        HFP_MARSHAL_STATE_BITFIELDS,        /* Bitfields of hfp_task_data */
        HFP_MARSHAL_STATE_SERVER_CHANNEL,   /* Marshal remote server channel */
        HFP_MARSHAL_STATE_LINK,             /* Marshal connection information */
    } state;

    union
    {
        marshaller_t marshaller;
        unmarshaller_t unmarshaller;
    } marshal;


    uint8 *server_channel;
    uint8 channel;
    hfp_link_data *link;

} hfp_marshal_instance_t;

static hfp_marshal_instance_t *hfp_marshal_inst = NULL;

/****************************************************************************
NAME    
    stitchLink

DESCRIPTION
    Stitch an unmarshalled HFP connection 

RETURNS
    void
*/    
static void stitchLink(hfp_link_data *unmarshalled_link, const tp_bdaddr *tp_bd_addr)
{
    uint16 conn_id;
    hfp_link_data *link = hfpGetIdleLink();

    PanicNull(link);

    unmarshalled_link->identifier.bd_addr = tp_bd_addr->taddr.addr;
    unmarshalled_link->identifier.sink = StreamRfcommSinkFromServerChannel(tp_bd_addr,
                                                                           *(hfp_marshal_inst->server_channel));
    *link = *unmarshalled_link;
    /* The sink should always be valid, do the state copy first, so its easier
       to debug by looking at appHfp state */
    PanicNull(unmarshalled_link->identifier.sink);

    conn_id = SinkGetRfcommConnId(link->identifier.sink);
    PanicFalse(VmOverrideRfcommConnContext(conn_id, (conn_context_t)&theHfp->task));
    /* Stitch the RFCOMM sink and the task */
    MessageStreamTaskFromSink(link->identifier.sink, &theHfp->task);
    /* Configure RFCOMM sink messages */
    SinkConfigure(link->identifier.sink, VM_SINK_MESSAGES, VM_MESSAGES_ALL);    

    free(unmarshalled_link);
    free(hfp_marshal_inst->server_channel);

    hfp_marshal_inst->server_channel = NULL;
}


/****************************************************************************
NAME    
    getRemoteServerChannel

DESCRIPTION
    Returns remote server channel of the specified link. If it does not exist
    or is idle or disabled then it returns RFC_INVALID_SERV_CHANNEL

RETURNS
    uint8 Remote server channel
*/
static uint8 getRemoteServerChannel(hfp_link_data *link)
{
    uint8 channel = RFC_INVALID_SERV_CHANNEL;
                            
    if (link->bitfields.ag_slc_state == hfp_slc_idle ||
        link->bitfields.ag_slc_state == hfp_slc_disabled)
    {
        /* Not connected */
    }
    else
    {
        /* Connected */
        channel = SinkGetRfcommServerChannel(link->identifier.sink);
    }

    return channel;
}

/****************************************************************************
NAME    
    hfpMarshalAbort

DESCRIPTION
    Abort the HFP Handover process, free any memory
    associated with the marshalling process.

RETURNS
    void
*/
static void hfpMarshalAbort(void)
{
    MarshalDestroy(hfp_marshal_inst->marshal.marshaller,
                   FALSE);

    free(hfp_marshal_inst);
    hfp_marshal_inst = NULL;
}

/****************************************************************************
NAME    
    hfpMarshal

DESCRIPTION
    Marshal the data associated with HFP connections

RETURNS
    bool TRUE if HFP module marshalling complete, otherwise FALSE
*/
static bool hfpMarshal(const tp_bdaddr *tp_bd_addr,
                       uint8 *buf,
                       uint16 length,
                       uint16 *written)
{
    bool marshalled = TRUE;   

    if (!hfp_marshal_inst)
    { 
        bool validLink = TRUE;
        uint8 channel = RFC_INVALID_SERV_CHANNEL;

        /* Check we have a valid link */
        hfp_link_data *link = hfpGetLinkFromBdaddr(&tp_bd_addr->taddr.addr);

        if(link)
        {
            /* check for a valid RFC channel */
            channel = getRemoteServerChannel(link);
            if (channel == RFC_INVALID_SERV_CHANNEL)
            {
                validLink = FALSE;
            }
        }
        else
        {
            validLink = FALSE;
        }
            
        if(validLink)
        {
            /* Initiating marshalling, initialize the instance */
            hfp_marshal_inst = PanicUnlessNew(hfp_marshal_instance_t);
            hfp_marshal_inst->marshal.marshaller = MarshalInit(mtd_hfp,
                                                               HFP_MARSHAL_OBJ_TYPE_COUNT);

            hfp_marshal_inst->state = HFP_MARSHAL_STATE_BITFIELDS;
            hfp_marshal_inst->link = link;
            hfp_marshal_inst->channel = channel;
        }
        else
        {
            /* Link not valid, nothing to marshal */
            *written = 0;
            return TRUE;
        }
    }
    else
    {
        /* Resume the marshalling */
    }

    MarshalSetBuffer(hfp_marshal_inst->marshal.marshaller,
                     (void *) buf,
                     length);

    do
    {
        switch (hfp_marshal_inst->state)
        {
            case HFP_MARSHAL_STATE_BITFIELDS:
                if (Marshal(hfp_marshal_inst->marshal.marshaller,
                            &theHfp->bitfields,
                            MARSHAL_TYPE(hfp_task_data_bitfields)))
                {
                    hfp_marshal_inst->state = HFP_MARSHAL_STATE_SERVER_CHANNEL;
                }
                else
                { /* Insufficient buffer */
                    marshalled = FALSE;
                }

                break;

            case HFP_MARSHAL_STATE_SERVER_CHANNEL:
            {
                if (Marshal(hfp_marshal_inst->marshal.marshaller,
                            &hfp_marshal_inst->channel,
                            MARSHAL_TYPE(uint8)))
                {
                    hfp_marshal_inst->state = HFP_MARSHAL_STATE_LINK;
                }
                else
                { /* Insufficient buffer */
                    marshalled = FALSE;
                }

                break;
            }

            case HFP_MARSHAL_STATE_LINK:
            {
                if (Marshal(hfp_marshal_inst->marshal.marshaller,
                            hfp_marshal_inst->link,
                            MARSHAL_TYPE(hfp_link_data)))
                {
                    /* Done marshalling this link */
                    *written = MarshalProduced(hfp_marshal_inst->marshal.marshaller);
                    hfpMarshalAbort();

                    return TRUE;
                }
                else
                { /* Insufficient buffer */
                    marshalled = FALSE;
                }
            }
                break;

            default: /* Unexpected state */
                Panic();
                break;
        }

        *written = MarshalProduced(hfp_marshal_inst->marshal.marshaller);
    } while (marshalled && *written <= length);

    return marshalled;
}

/****************************************************************************
NAME    
    connectionUnmarshal

DESCRIPTION
    Unmarshal the data associated with the HFP connections

RETURNS
    bool TRUE if HFP unmarshalling complete, otherwise FALSE
*/
static bool hfpUnmarshal(const tp_bdaddr *tp_bd_addr,
                         const uint8 *buf,
                         uint16 length,
                         uint16 *consumed)
{
    marshal_type_t type;
    bool unmarshalled = TRUE;

    if (!hfp_marshal_inst)
    { /* Initiating unmarshalling, initialize the instance */
        hfp_marshal_inst = PanicUnlessNew(hfp_marshal_instance_t);
        hfp_marshal_inst->marshal.unmarshaller = UnmarshalInit(mtd_hfp,
                                                               HFP_MARSHAL_OBJ_TYPE_COUNT);

        hfp_marshal_inst->state = HFP_MARSHAL_STATE_BITFIELDS;
        hfp_marshal_inst->server_channel = NULL;
        hfp_marshal_inst->link =  NULL;
        hfp_marshal_inst->channel = 0;
    }
    else
    {
        /* Resuming unmarshalling */
    }

    UnmarshalSetBuffer(hfp_marshal_inst->marshal.unmarshaller,
                       (void *) buf,
                       length);

    do
    {
        switch (hfp_marshal_inst->state)
        {
            case HFP_MARSHAL_STATE_BITFIELDS:
            {
                hfp_task_data_bitfields *bitfields = NULL;

                if (Unmarshal(hfp_marshal_inst->marshal.unmarshaller,
                              (void **) &bitfields,
                              &type))
                {
                    PanicFalse(type == MARSHAL_TYPE(hfp_task_data_bitfields));

                    theHfp->bitfields = *bitfields;
                    free(bitfields);

                    hfp_marshal_inst->state = HFP_MARSHAL_STATE_SERVER_CHANNEL;
                }
                else
                { /* Insufficient buffer */
                    unmarshalled = FALSE;
                }

                break;
            }

            case HFP_MARSHAL_STATE_SERVER_CHANNEL:
                if (Unmarshal(hfp_marshal_inst->marshal.unmarshaller,
                              (void **) &hfp_marshal_inst->server_channel,
                              &type))
                {
                    PanicFalse(type == MARSHAL_TYPE(uint8));

                    if (*hfp_marshal_inst->server_channel == RFC_INVALID_SERV_CHANNEL)
                    { /* Should not have marshalled an invalid server channel */
                        Panic();
                    }
                    else
                    {
                        hfp_marshal_inst->state = HFP_MARSHAL_STATE_LINK;
                    }
                }
                else
                { /* Insufficient buffer */
                    unmarshalled = FALSE;
                }

                break;

            case HFP_MARSHAL_STATE_LINK:
            {
                hfp_link_data *link = NULL;

                if (Unmarshal(hfp_marshal_inst->marshal.unmarshaller,
                              (void **) &link,
                              &type))
                {
                    PanicFalse(type == MARSHAL_TYPE(hfp_link_data));

                    stitchLink(link, tp_bd_addr);

                    /* Unmarshalling link complete */
                    *consumed = UnmarshalConsumed(hfp_marshal_inst->marshal.unmarshaller);

                    UnmarshalDestroy(hfp_marshal_inst->marshal.unmarshaller,
                                     FALSE);

                    free(hfp_marshal_inst);
                    hfp_marshal_inst = NULL;

                    return TRUE;
                }
                else
                { /* Insufficient buffer */
                    unmarshalled = FALSE;
                }

                break;
            }

            default: /* Unexpected state */
                Panic();
                break;
        }

        *consumed = UnmarshalConsumed(hfp_marshal_inst->marshal.unmarshaller);
    } while (unmarshalled && *consumed <= length);

    return unmarshalled;
}


/****************************************************************************
NAME    
    hfpVeto

DESCRIPTION
    Veto check for HFP library

    Prior to handover commencing this function is called and
    the libraries internal state is checked to determine if the
    handover should proceed.

RETURNS
    bool TRUE if the HFP Library wishes to veto the handover attempt.
*/
bool hfpVeto( void )
{
    hfp_link_data* link;

    /* Check the HFP library is initialized */
    if ( !theHfp )
    {
        return TRUE;
    }

    /* check multipoint is not active */
    if (hfpGetLinkFromPriority(hfp_secondary_link) != NULL)
    {
        return TRUE;
    }

    /* Check if an AT command response is pending from the AG.  */
    link = hfpGetLinkFromPriority(hfp_primary_link);
    if(link && link->bitfields.at_cmd_resp_pending != hfpNoCmdPending)
    {
        return TRUE;
    }

    /* Check message queue status */
    if(MessagesPendingForTask(&theHfp->task, NULL) != 0)
    {
        return TRUE;
    }

    return FALSE;
}

/****************************************************************************
NAME    
    hfpMarshalCommit

DESCRIPTION
    The HFP library commits to the specified new role (primary or
    secondary)

RETURNS
    void
*/
static void hfpMarshalCommit( const bool newRole )
{
    UNUSED(newRole);
}


