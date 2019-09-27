/****************************************************************************
Copyright (c) 2019 Qualcomm Technologies International, Ltd.


FILE NAME
    avrcp_handover.c

DESCRIPTION
    Implements AVRCP handover logic (Veto, Marshals/Unmarshals, Handover, etc).

NOTES
    See handover_if.h for further interface description 

    Builds requiring this should include CONFIG_HANDOVER in the
    makefile. e.g.
        CONFIG_FEATURES:=CONFIG_HANDOVER    
*/


#include "avrcp_marshal_desc.h"
#include "avrcp_private.h"
#include "avrcp_init.h"
#include "avrcp_profile_handler.h"

#include "marshal.h"
#include <panic.h>
#include <stdlib.h>


#define AVRCP_MARSHAL_CONN_FLAG_NONE        0           /* AVRCP connection does not exists */
#define AVRCP_MARSHAL_CONN_MASK_BASIC       (1 << 0)    /* AVRCP connection exists */
#define AVRCP_MARSHAL_CONN_MASK_BROWSING    (1 << 1)    /* Browsing channel connection also exists */


static bool avrcpVeto( void );
static bool avrcpMarshal(const tp_bdaddr *tp_bd_addr,
                         uint8 *buf,
                         uint16 length,
                         uint16 *written);
static bool avrcpUnmarshal(const tp_bdaddr *tp_bd_addr,
                           const uint8 *buf,
                           uint16 length,
                           uint16 *consumed);
static void avrcpMarshalCommit( const bool newRole );
static void avrcpMarshalAbort(void);


const handover_interface avrcp_handover =
{
    avrcpVeto,
    avrcpMarshal,
    avrcpUnmarshal,
    avrcpMarshalCommit,
    avrcpMarshalAbort
};


typedef struct
{
    enum
    {
        AVRCP_MARSHAL_STATE_FLAG,           /* Marshal flag indicating type of connection */
        AVRCP_MARSHAL_STATE_CONN,           /* Marshal connection information */
    } state;

    union
    {
        marshaller_t marshaller;
        unmarshaller_t unmarshaller;
    } marshal;

    avrcpList *avrcp_node;
    uint8 connection_flag;
} avrcp_marshal_instance_t;

static avrcp_marshal_instance_t *avrcp_marshal_inst = NULL;


/****************************************************************************
NAME    
    browsingSupported

DESCRIPTION
    Finds out whether browsing is supported or not 

RETURNS
    bool TRUE if browsing supported
*/
static bool browsingSupported(void)
{
    AvrcpDeviceTask *avrcp_device_task = avrcpGetDeviceTask();

    return (isAvrcpBrowsingEnabled(avrcp_device_task) ||
            isAvrcpTargetCat1Supported(avrcp_device_task) ||
            isAvrcpTargetCat3Supported(avrcp_device_task));
}

/****************************************************************************
NAME    
    stitchAvrcp

DESCRIPTION
    Stitch an unmarshalled AVRCP connection instance 

RETURNS
    void
*/
static void stitchAvrcp(AVRCP *unmarshalled_avrcp)
{
    avrcpList *newList;

    unmarshalled_avrcp->task.handler = avrcpProfileHandler;
    unmarshalled_avrcp->dataFreeTask.cleanUpTask.handler = avrcpDataCleanUp;

    if (browsingSupported())
    {
        AVRCP_AVBP_INIT *avrcp_avbp = (AVRCP_AVBP_INIT *) unmarshalled_avrcp;

        avrcp_avbp->avbp.task.handler = avbpProfileHandler;
        avrcp_avbp->avrcp.avbp_task = &avrcp_avbp->avbp.task;
        avrcp_avbp->avbp.avrcp_task = &avrcp_avbp->avrcp.task;
    }

    /* Add to the connection list */
    newList = PanicUnlessNew(avrcpList);
    newList->avrcp = unmarshalled_avrcp;
    newList->next = avrcp_marshal_inst->avrcp_node;
    avrcp_marshal_inst->avrcp_node = newList;
}

/****************************************************************************
NAME    
    avrcpMarshalAbort

DESCRIPTION
    Abort the AVRCP library Handover process, free any memory
    associated with the marshalling process.

RETURNS
    void
*/
static void avrcpMarshalAbort(void)
{
    MarshalDestroy(avrcp_marshal_inst->marshal.marshaller,
                   FALSE);

    free(avrcp_marshal_inst);
    avrcp_marshal_inst = NULL;
}

/****************************************************************************
NAME    
    avrcpMarshal

DESCRIPTION
    Marshal the data associated with AVRCP connections

RETURNS
    bool TRUE if AVRCP module marshalling complete, otherwise FALSE
*/
static bool avrcpMarshal(const tp_bdaddr *tp_bd_addr,
                         uint8 *buf,
                         uint16 length,
                         uint16 *written)
{
    marshal_type_t type;
    bool marshalled = TRUE;

    if (!avrcp_marshal_inst)
    { /* Initiating marshalling, initialize the instance */
        avrcp_marshal_inst = PanicUnlessNew(avrcp_marshal_instance_t);
        avrcp_marshal_inst->marshal.marshaller = MarshalInit(mtd_avrcp,
                                                             AVRCP_MARSHAL_OBJ_TYPE_COUNT);

        avrcp_marshal_inst->avrcp_node = avrcpListHead;
        avrcp_marshal_inst->state = AVRCP_MARSHAL_STATE_FLAG;
    }
    else
    {
        /* Resuming the marshalling */
    }

    MarshalSetBuffer(avrcp_marshal_inst->marshal.marshaller,
                     (void *) buf,
                     length);


    if (browsingSupported())
    {
        /* When browsing is supported, AVRCP and AVBP structures are part of
         * same allocation. But browsing connection information may or may not
         * need to be marshalled depending upon whether browsing channel connection
         * exists or not. Thus union of AVRCP and AVRCP_AVBP_INIT is marshalled. */
        type = MARSHAL_TYPE(avrcp_union_t);
    }
    else
    {
        type = MARSHAL_TYPE(AVRCP);
    }

    do
    {
        switch (avrcp_marshal_inst->state)
        {
            case AVRCP_MARSHAL_STATE_FLAG:
                if (avrcp_marshal_inst->avrcp_node)
                {
                    avrcp_marshal_inst->connection_flag = AVRCP_MARSHAL_CONN_MASK_BASIC;

                    if (browsingSupported())
                    {
                        AVBP *avbp = &((AVRCP_AVBP_INIT *) avrcp_marshal_inst->avrcp_node->avrcp)->avbp;

                        if (isAvbpConnected(avbp))
                        {
                            avrcp_marshal_inst->connection_flag |= AVRCP_MARSHAL_CONN_MASK_BROWSING;
                        }
                    }
                }
                else
                {
                    avrcp_marshal_inst->connection_flag = AVRCP_MARSHAL_CONN_FLAG_NONE;
                }

                if (Marshal(avrcp_marshal_inst->marshal.marshaller,
                            &avrcp_marshal_inst->connection_flag,
                            MARSHAL_TYPE(uint8)))
                {
                    avrcp_marshal_inst->state = AVRCP_MARSHAL_STATE_CONN;
                }
                else
                { /* Insufficient buffer */
                    marshalled = FALSE;
                }

                break;

            case AVRCP_MARSHAL_STATE_CONN:
                if (avrcp_marshal_inst->avrcp_node)
                {
                    if (Marshal(avrcp_marshal_inst->marshal.marshaller,
                                &avrcp_marshal_inst->avrcp_node->avrcp,
                                type))
                    {
                        avrcp_marshal_inst->avrcp_node = avrcp_marshal_inst->avrcp_node->next;
                        avrcp_marshal_inst->state = AVRCP_MARSHAL_STATE_FLAG;
                    }
                    else
                    { /* Insufficient buffer */
                        marshalled = FALSE;
                    }
                }
                else
                { /* Marshaling completed */
                    avrcpMarshalAbort();

                    return TRUE;
                }

                break;

            default: /* Unexpected state */
                Panic();
                break;
        }

        *written = MarshalProduced(avrcp_marshal_inst->marshal.marshaller);
    } while (marshalled && *written <= length);

    UNUSED(tp_bd_addr);

    return marshalled;
}

/****************************************************************************
NAME    
    avrcpUnmarshal

DESCRIPTION
    Unmarshal the data associated with AVRCP connections

RETURNS
    bool TRUE if AVRCP unmarshalling complete, otherwise FALSE
*/
static bool avrcpUnmarshal(const tp_bdaddr *tp_bd_addr,
                           const uint8 *buf,
                           uint16 length,
                           uint16 *consumed)
{
    marshal_type_t unmarshalled_type, expected_type;
    bool unmarshalled = TRUE;

    if (!avrcp_marshal_inst)
    { /* Initiating unmarshalling, initialize the instance */
        avrcp_marshal_inst = PanicUnlessNew(avrcp_marshal_instance_t);
        avrcp_marshal_inst->marshal.unmarshaller = UnmarshalInit(mtd_avrcp,
                                                                 AVRCP_MARSHAL_OBJ_TYPE_COUNT);

        avrcp_marshal_inst->avrcp_node = NULL;
        avrcp_marshal_inst->state = AVRCP_MARSHAL_STATE_FLAG;
    }
    else
    {
        /* Resuming the unmarshalling */
    }

    UnmarshalSetBuffer(avrcp_marshal_inst->marshal.unmarshaller,
                       (void *) buf,
                       length);

    if (browsingSupported())
    {
        /* When browsing is supported, AVRCP and AVBP structures are part of
         * same allocation. But browsing connection information may or may not
         * need to be marshalled depending upon whether browsing channel connection
         * exists or not. Thus union of AVRCP and AVRCP_AVBP_INIT is marshalled. */
        expected_type = MARSHAL_TYPE(avrcp_union_t);
    }
    else
    {
        expected_type = MARSHAL_TYPE(AVRCP);
    }

    do
    {
        switch (avrcp_marshal_inst->state)
        {
            case AVRCP_MARSHAL_STATE_FLAG:
            {
                uint8 *flag = NULL;

                if (Unmarshal(avrcp_marshal_inst->marshal.unmarshaller,
                              (void **) &flag,
                              &unmarshalled_type))
                {
                    PanicFalse(unmarshalled_type == MARSHAL_TYPE(uint8));

                    avrcp_marshal_inst->connection_flag = *flag;
                    free(flag);

                    avrcp_marshal_inst->state = AVRCP_MARSHAL_STATE_CONN;
                }
                else
                { /* Insufficient buffer */
                    unmarshalled = FALSE;
                }

                break;
            }

            case AVRCP_MARSHAL_STATE_CONN:
                if (avrcp_marshal_inst->connection_flag != AVRCP_MARSHAL_CONN_FLAG_NONE)
                {
                    AVRCP *unmarshalled_avrcp = NULL;

                    if (Unmarshal(avrcp_marshal_inst->marshal.unmarshaller,
                                  (void **) &unmarshalled_avrcp,
                                  &unmarshalled_type))
                    {
                        PanicFalse(unmarshalled_type == expected_type);

                        /* Stitch unmarshalled AVRCP connection instance */
                        stitchAvrcp(unmarshalled_avrcp);

                        avrcp_marshal_inst->state = AVRCP_MARSHAL_STATE_FLAG;
                    }
                    else
                    { /* Insufficient buffer */
                        unmarshalled = FALSE;
                    }
                }
                else
                {
                    UnmarshalDestroy(avrcp_marshal_inst->marshal.unmarshaller,
                                     FALSE);

                    free(avrcp_marshal_inst);
                    avrcp_marshal_inst = NULL;

                    return TRUE;
                }

                break;

            default: /* Unexpected state */
                Panic();
                break;
        }

        *consumed = UnmarshalConsumed(avrcp_marshal_inst->marshal.unmarshaller);
    } while (unmarshalled && *consumed <= length);

    UNUSED(tp_bd_addr);

    return unmarshalled;
}

/****************************************************************************
NAME    
    avrcpMarshalCommit

DESCRIPTION
    The AVRCP  library commits to the specified new role (primary or
    secondary)

RETURNS
    void
*/
static void avrcpMarshalCommit( const bool newRole )
{
    UNUSED(newRole);
}

bool avrcpMarshalBrowsingChannelExists(void)
{
    PanicNull(avrcp_marshal_inst);

    return (avrcp_marshal_inst->connection_flag & AVRCP_MARSHAL_CONN_MASK_BROWSING);
}


/****************************************************************************
NAME    
    avrcpVeto

DESCRIPTION
    Veto check for AVRCP library

    Prior to handover commencing this function is called and
    the libraries internal state is checked to determine if the
    handover should proceed.

RETURNS
    bool TRUE if the AVRCP Library wishes to veto the handover attempt.
*/
static bool avrcpVeto( void )
{
    avrcpList *list = avrcpListHead;
    AvrcpDeviceTask *avrcp_device_task = avrcpGetDeviceTask();
    avrcp_device_role device_role = avrcp_device_task->bitfields.device_type;

    /* If AVRCP library initialization is not complete or AvrcpInit has not been
     * called the set device role will not be set */
    if (device_role != avrcp_target ||
        device_role != avrcp_controller ||
        device_role != avrcp_target_and_controller  )
    {
        return TRUE;
    }

    /* Check message queue status */
    if(MessagesPendingForTask(avrcp_device_task->app_task, NULL) != 0)
    {
        return TRUE;
    }

    /* Check the AVRCP tasks */
    while (list != NULL)
    {
        AVRCP *avrcp = list->avrcp;

        /* Check whether there is a connection in progress. */
        if(avrcp->bitfields.state == avrcpConnecting)
        {
            return TRUE;
        }

        list = list->next;
    }

    return FALSE;
}



