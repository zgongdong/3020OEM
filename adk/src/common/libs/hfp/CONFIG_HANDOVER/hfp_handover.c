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
#include <app/bluestack/rfcomm_prim.h>
#include <panic.h>
#include <stdlib.h>


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
        HFP_MARSHAL_STATE_ADDRESS,          /* Marshal remote device address */
        HFP_MARSHAL_STATE_LINK,             /* Marshal connection information */
    } state;

    union
    {
        marshaller_t marshaller;
        unmarshaller_t unmarshaller;
    } marshal;

    uint8 link_index;

    uint8 *server_channel;
    bdaddr *bd_addr;
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
static void stitchLink(hfp_link_data *unmarshalled_link)
{
    hfp_link_data *link = hfpGetIdleLink();

    PanicNull(link);

    unmarshalled_link->identifier.bd_addr = *(hfp_marshal_inst->bd_addr);
    unmarshalled_link->identifier.sink = StreamRfcommSinkFromServerChannel(unmarshalled_link->identifier.bd_addr,
                                                                           *(hfp_marshal_inst->server_channel));

    *link = *unmarshalled_link;

    free(unmarshalled_link);
    free(hfp_marshal_inst->bd_addr);
    free(hfp_marshal_inst->server_channel);

    hfp_marshal_inst->bd_addr = NULL;
    hfp_marshal_inst->server_channel = NULL;
}

/****************************************************************************
NAME    
    getNextRemoteServerChannel

DESCRIPTION
    Returns remote server channel of connected link. If none of further links
    are connected, then it returns RFC_INVALID_SERV_CHANNEL

RETURNS
    uint8 Remote server channel
*/
static uint8 getNextRemoteServerChannel(void)
{
    uint8 channel = RFC_INVALID_SERV_CHANNEL;

    while (hfp_marshal_inst->link_index < theHfp->num_links)
    {
        hfp_link_data *link = &theHfp->links[hfp_marshal_inst->link_index];

        if (link->bitfields.ag_slc_state == hfp_slc_idle ||
            link->bitfields.ag_slc_state == hfp_slc_disabled)
        {
            /* Not connected, try next link */
        }
        else
        {
            /* Connected */
            channel = SinkGetRfcommServerChannel(link->identifier.sink);
            break;
        }
        hfp_marshal_inst->link_index++;
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
    { /* Initiating marshalling, initialize the instance */
        hfp_marshal_inst = PanicUnlessNew(hfp_marshal_instance_t);
        hfp_marshal_inst->marshal.marshaller = MarshalInit(mtd_hfp,
                                                           HFP_MARSHAL_OBJ_TYPE_COUNT);

        hfp_marshal_inst->state = HFP_MARSHAL_STATE_BITFIELDS;
        hfp_marshal_inst->link_index = 0;
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
                uint8 channel = getNextRemoteServerChannel();

                if (Marshal(hfp_marshal_inst->marshal.marshaller,
                            &channel,
                            MARSHAL_TYPE(uint8)))
                {
                    if (channel == RFC_INVALID_SERV_CHANNEL)
                    { /* All connections have been marshalled */
                        *written = MarshalProduced(hfp_marshal_inst->marshal.marshaller);
                        hfpMarshalAbort();

                        return TRUE;
                    }
                    else
                    {
                        hfp_marshal_inst->state = HFP_MARSHAL_STATE_ADDRESS;
                    }
                }
                else
                { /* Insufficient buffer */
                    marshalled = FALSE;
                }

                break;
            }

            case HFP_MARSHAL_STATE_ADDRESS:
                if (Marshal(hfp_marshal_inst->marshal.marshaller,
                            &theHfp->links[hfp_marshal_inst->link_index].identifier.bd_addr,
                            MARSHAL_TYPE(bdaddr)))
                {
                    hfp_marshal_inst->state = HFP_MARSHAL_STATE_LINK;
                }
                else
                { /* Insufficient buffer */
                    marshalled = FALSE;
                }

                break;

            case HFP_MARSHAL_STATE_LINK:
                if (Marshal(hfp_marshal_inst->marshal.marshaller,
                            &theHfp->links[hfp_marshal_inst->link_index],
                            MARSHAL_TYPE(hfp_link_data)))
                {
                    hfp_marshal_inst->link_index++;
                    hfp_marshal_inst->state = HFP_MARSHAL_STATE_SERVER_CHANNEL;
                }
                else
                { /* Insufficient buffer */
                    marshalled = FALSE;
                }

                break;

            default: /* Unexpected state */
                Panic();
                break;
        }

        *written = MarshalProduced(hfp_marshal_inst->marshal.marshaller);
    } while (marshalled && *written <= length);

    UNUSED(tp_bd_addr);

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
        hfp_marshal_inst->bd_addr = NULL;
        hfp_marshal_inst->server_channel = NULL;
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

                    if (*(uint8 *) hfp_marshal_inst->server_channel == RFC_INVALID_SERV_CHANNEL)
                    { /* No more links to be unmarshalled */
                        *consumed = UnmarshalConsumed(hfp_marshal_inst->marshal.unmarshaller);

                        UnmarshalDestroy(hfp_marshal_inst->marshal.unmarshaller,
                                         FALSE);

                        free(hfp_marshal_inst->server_channel);
                        free(hfp_marshal_inst);
                        hfp_marshal_inst = NULL;

                        return TRUE;
                    }
                    else
                    {
                        hfp_marshal_inst->state = HFP_MARSHAL_STATE_ADDRESS;
                    }
                }
                else
                { /* Insufficient buffer */
                    unmarshalled = FALSE;
                }

                break;

            case HFP_MARSHAL_STATE_ADDRESS:
                if (Unmarshal(hfp_marshal_inst->marshal.unmarshaller,
                              (void **) &hfp_marshal_inst->bd_addr,
                              &type))
                {
                    PanicFalse(type == MARSHAL_TYPE(bdaddr));

                    hfp_marshal_inst->state = HFP_MARSHAL_STATE_LINK;
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

                    stitchLink(link);

                    hfp_marshal_inst->state = HFP_MARSHAL_STATE_SERVER_CHANNEL;
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

    UNUSED(tp_bd_addr);

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


