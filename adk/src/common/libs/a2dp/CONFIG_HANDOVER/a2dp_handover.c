/****************************************************************************
Copyright (c) 2019 Qualcomm Technologies International, Ltd.


FILE NAME
    a2dp_handover.c

DESCRIPTION
    TWS Handover and Marshaling interface for A2DP
    
NOTES
    See handover_if.h for further interface description    
    
    Builds requiring this should include CONFIG_HANDOVER in the
    makefile. e.g.
        CONFIG_FEATURES:=CONFIG_HANDOVER    
*/


/****************************************************************************
    Header files
*/
#include "a2dp_marshal_desc.h"
#include "a2dp_private.h"
#include "a2dp_init.h"

#include "marshal.h"

#include <panic.h>
#include <sink.h>
#include <stream.h>
#include <stdlib.h>
#include <bdaddr.h>


typedef struct
{
    /* Same state variable used for both marshalling and unmarshalling
     * assuming that only one procedure would be ongoing at a time */
    enum
    {
        A2DP_MARSHAL_STATE_CONN_FLAG,       /* Marshal connection status */
        A2DP_MARSHAL_STATE_CONN,            /* Marshal connection information including signalling channel */
        A2DP_MARSHAL_STATE_MEDIA_FLAG,      /* Marshal media channel connection status */
        A2DP_MARSHAL_STATE_MEDIA,           /* Marshal connection information of media channel */
        A2DP_MARSHAL_STATE_DATA_BLOCK,      /* Marshal data blocks */
    } state;

    union
    {
        marshaller_t marshaller;
        unmarshaller_t unmarshaller;
    } marshal;

    uint8 device_id;
    uint8 conn_count;
    uint8 media_conn;
} a2dp_marshal_instance_t;

static a2dp_marshal_instance_t *a2dp_marshal_inst = NULL;

/* Checks if A2DP connection exists */
#define A2DP_CONNECTION_VALID(_conn)                \
    (a2dp->remote_conn[_conn].signal_conn.status.connection_state == avdtp_connection_connected)

/* Checks if media connection exists for the A2DP connection */
#define A2DP_MEDIA_CONNECTION_VALID(_conn, _media)  \
    (a2dp->remote_conn[_conn].media_conn[_media].status.conn_info.connection_state == avdtp_connection_connected)

static bool a2dpVeto(void);

static bool a2dpMarshal(const tp_bdaddr *tp_bd_addr,
                       uint8 *buf,
                       uint16 length,
                       uint16 *written);

static bool a2dpUnmarshal(const tp_bdaddr *tp_bd_addr,
                         const uint8 *buf,
                         uint16 length,
                         uint16 *consumed);

static void a2dpMarshalCommit( const bool newRole );

static void a2dpMarshalAbort( void );

extern const handover_interface a2dp_handover_if =  {
        &a2dpVeto,
        &a2dpMarshal,
        &a2dpUnmarshal,
        &a2dpMarshalCommit,
        &a2dpMarshalAbort};

/****************************************************************************
NAME    
    a2dpVeto

DESCRIPTION
    Veto check for A2DP library

    Prior to handover commencing this function is called and
    the libraries internal state is checked to determine if the
    handover should proceed.

RETURNS
    bool TRUE if the A2DP Library wishes to veto the handover attempt.
*/
bool a2dpVeto( void )
{
    unsigned  device_id = A2DP_MAX_REMOTE_DEVICES;


    /* Check the a2dp library is initialised */
    if ( !a2dp )
    {
        return TRUE;
    }

    /* Check message queue status */
    if(MessagesPendingForTask(&a2dp->task, NULL) != 0)
    {
        return TRUE;
    }

    while (device_id-- > A2DP_MAX_REMOTE_DEVICES)
    {
        avdtp_connection_state  signal_state, media_state;
        avdtp_stream_state strm_state;
        uint8 stream_id = 0;

        if (BdaddrIsZero(&a2dp->remote_conn[device_id].bd_addr))
        {
            continue;
        }

        /* Check signalling channel status */
        signal_state = a2dp->remote_conn[device_id].signal_conn.status.connection_state;
        if (signal_state != avdtp_connection_idle && signal_state != avdtp_connection_connected)
        {
            return TRUE;
        }

        /* check media channel status */
        for (stream_id=0; stream_id<A2DP_MAX_MEDIA_CHANNELS; stream_id++)
        {
            media_state = a2dp->remote_conn[device_id].media_conn[stream_id].status.conn_info.connection_state;
            if (media_state != avdtp_connection_idle && media_state != avdtp_connection_connected)
            {
                return TRUE;
            }
        }

        /* check stream status */
        strm_state = a2dp->remote_conn[device_id].signal_conn.status.stream_state;
        if (strm_state != avdtp_stream_idle &&
            strm_state != avdtp_stream_configured &&
            strm_state != avdtp_stream_open &&
            strm_state != avdtp_stream_streaming )
        {
            return TRUE;
        }

        /* check for pending transactions */
        if( a2dp->remote_conn[device_id].signal_conn.status.pending_issued_transaction ||
            a2dp->remote_conn[device_id].signal_conn.status.pending_received_transaction )
        {
            return TRUE;
        }
    }

    /* No Veto, good to go */
    return FALSE;
}

/****************************************************************************
NAME    
    a2dpMarshalCommit

DESCRIPTION
    The A2DP library commits to the specified new role (primary or
    secondary)

RETURNS
    void
*/
static void a2dpMarshalCommit( const bool newRole )
{
    UNUSED(newRole);
}

/****************************************************************************
NAME    
    stitchRemoteDevice

DESCRIPTION
    Stitch connection information to A2DP instance 

RETURNS
    void
*/
static void stitchRemoteDevice(remote_device *unmarshalled_remote_conn)
{
    uint16 cid;
    uint8 mediaChannelIndex;
    remote_device *remote_conn = NULL;

#ifdef A2DP_MARSHAL_PEER_EARBUD_CONN
    /* Peer earbud connections, if maintained, are also be marshalled.
     * CID of unmarshalled peer earbud connection would match with that of
     * existing peer earbud connection.
     * Values like device ID and client task (application) needs be inherited
     * from existing peer earbud connection.  */

    uint8 i;
    bool found = FALSE;

    cid = SinkGetL2capCid(unmarshalled_remote_conn->signal_conn.connection.active.sink);
    PanicZero(cid);   /* Invalid Connection ID */

    for (i = 0; i < ARRAY_DIM(a2dp->remote_conn); i++)
    {
        remote_conn = &a2dp->remote_conn[i];

        if (BdaddrIsZero(&remote_conn->bd_addr))
        {
            /* Not connected, skip it */
        }
        else if (SinkGetL2capCid(remote_conn->signal_conn.connection.active.sink) == cid)
        { /* Matching connection already exists */
            found = TRUE;

            /* Device ID and application from existing connections instance are inherited */
            unmarshalled_remote_conn->bitfields.device_id = remote_conn->bitfields.device_id;
            unmarshalled_remote_conn->clientTask = remote_conn->clientTask;

            /* Release dynamic allocated fields in existing connection instance */
            if (remote_conn->reconfig_caps)
            {
                free(remote_conn->reconfig_caps);
            }

            if (remote_conn->signal_conn.connection.active.received_packet)
            {
                free(remote_conn->signal_conn.connection.active.received_packet);
            }

            /* Initialize the connection context for the relevant connection id on signalling channel */
            VmOverrideL2capConnContext(cid, &a2dp->task);
            /* Stitch the sink and the task */
            MessageStreamTaskFromSink(unmarshalled_remote_conn->signal_conn.connection.active.sink, &a2dp->task);
            /* Configure sink messages */
            SinkConfigure(unmarshalled_remote_conn->signal_conn.connection.active.sink, VM_SINK_MESSAGES, VM_MESSAGES_ALL);              
            /* Initialize the connection context for the relevant connection id on the media channels */
            for (mediaChannelIndex=0; mediaChannelIndex < A2DP_MAX_MEDIA_CHANNELS; mediaChannelIndex++)
            {
                media_channel *media = &unmarshalled_remote_conn->media_conn[mediaChannelIndex];
                
                if ( ((media->status.conn_info.connection_state == avdtp_connection_connected) || 
                      (media->status.conn_info.connection_state == avdtp_connection_disconnecting) ||
                      (media->status.conn_info.connection_state == avdtp_connection_disconnect_pending)) && 
                      (media->connection.active.sink) )
                {
                    cid = SinkGetL2capCid(media->connection.active.sink);
                    PanicZero(cid); /* Invalid Connection ID */
                    VmOverrideL2capConnContext(cid, (conn_context_t)&a2dp->task);
                    /* Stitch the media channel sink and the task */
                    MessageStreamTaskFromSink(media->connection.active.sink, &a2dp->task);
                    /* Configure sink messages */
                    SinkConfigure(media->connection.active.sink, VM_SINK_MESSAGES, VM_MESSAGES_ALL);            
                }
            }
            return;
        }
    }

    /* If matching connection instance was not found, add a new connection instance */
    if (!found)
#endif /* A2DP_MARSHAL_PEER_EARBUD_CONN */
    {
        remote_conn = a2dpAddDevice(&unmarshalled_remote_conn->bd_addr);
    }
    PanicNull(remote_conn);

    a2dp_marshal_inst->device_id = remote_conn->bitfields.device_id;
    unmarshalled_remote_conn->bitfields.device_id = a2dp_marshal_inst->device_id;
    *remote_conn = *unmarshalled_remote_conn;
    
    /* Initialize the connection context for the relevant connection id on signalling channel */
    cid = SinkGetL2capCid(unmarshalled_remote_conn->signal_conn.connection.active.sink);
    PanicZero(cid); /* Invalid Connection ID */    
    VmOverrideL2capConnContext(cid, (conn_context_t)&a2dp->task);
    /* Stitch the signalling channel sink and the task */
    MessageStreamTaskFromSink(unmarshalled_remote_conn->signal_conn.connection.active.sink, &a2dp->task);
    /* Configure sink messages */
    SinkConfigure(unmarshalled_remote_conn->signal_conn.connection.active.sink, VM_SINK_MESSAGES, VM_MESSAGES_ALL);    
    
    /* Initialize the connection context for the relevant connection id on the media channels */
    for (mediaChannelIndex=0; mediaChannelIndex < A2DP_MAX_MEDIA_CHANNELS; mediaChannelIndex++)
    {
        media_channel *media = &unmarshalled_remote_conn->media_conn[mediaChannelIndex];
        
        if ( ((media->status.conn_info.connection_state == avdtp_connection_connected) || 
              (media->status.conn_info.connection_state == avdtp_connection_disconnecting) ||
              (media->status.conn_info.connection_state == avdtp_connection_disconnect_pending)) && 
              (media->connection.active.sink) )
        {
            cid = SinkGetL2capCid(media->connection.active.sink);
            PanicZero(cid); /* Invalid Connection ID */            
            VmOverrideL2capConnContext(cid, (conn_context_t)&a2dp->task);
            /* Stitch the media channel sink and the task */
            MessageStreamTaskFromSink(media->connection.active.sink, &a2dp->task);
            /* Configure sink messages */
            SinkConfigure(media->connection.active.sink, VM_SINK_MESSAGES, VM_MESSAGES_ALL);            
        }
    }
    free(unmarshalled_remote_conn);
}

/****************************************************************************
NAME    
    stitchMediaChannel

DESCRIPTION
    Stitch media connection information to the connection instance

RETURNS
    void
*/
static void stitchMediaChannel(media_channel *media_conn)
{
    a2dp->remote_conn[a2dp_marshal_inst->device_id].media_conn[a2dp_marshal_inst->media_conn] = *media_conn;
    free(media_conn);
}

/****************************************************************************
NAME    
    stitchDataBlockHeader

DESCRIPTION
    Stitch data block headers
    
RETURNS
    void
*/
static void stitchDataBlockHeader(data_block_header *data_blocks)
{
    a2dp->data_blocks[a2dp_marshal_inst->device_id] = data_blocks;
}

/****************************************************************************
NAME    
    a2dpMarshalAbort

DESCRIPTION
    Abort the A2DP Handover process, free any memory
    associated with the marshalling process.

RETURNS
    void
*/
static void a2dpMarshalAbort(void)
{
    MarshalDestroy(a2dp_marshal_inst->marshal.marshaller,
                   FALSE);

    free(a2dp_marshal_inst);
    a2dp_marshal_inst = NULL;
}

/****************************************************************************
NAME    
    a2dpMarshal

DESCRIPTION
    Marshal the data associated with A2DP connections

RETURNS
    bool TRUE if A2DP module marshalling complete, otherwise FALSE
*/
static bool a2dpMarshal(const tp_bdaddr *tp_bd_addr,
                        uint8 *buf,
                        uint16 length,
                        uint16 *written)
{
    bool marshalled = TRUE;

    if (!a2dp_marshal_inst)
    { /* Initiating marshalling, initialize the instance */
        a2dp_marshal_inst = PanicUnlessNew(a2dp_marshal_instance_t);
        a2dp_marshal_inst->marshal.marshaller = MarshalInit(mtd_a2dp,
                                                            A2DP_MARSHAL_OBJ_TYPE_COUNT);
        a2dp_marshal_inst->conn_count = 0;
        a2dp_marshal_inst->state = A2DP_MARSHAL_STATE_CONN_FLAG;
    }
    else
    {
        /* Resuming the marshalling */
    }

    MarshalSetBuffer(a2dp_marshal_inst->marshal.marshaller,
                     (void *) buf,
                     length);

    do
    {
        switch (a2dp_marshal_inst->state)
        {
            case A2DP_MARSHAL_STATE_CONN_FLAG:
                if (a2dp_marshal_inst->conn_count < A2DP_MAX_REMOTE_DEVICES_DEFAULT)
                {
                    bool flag = A2DP_CONNECTION_VALID(a2dp_marshal_inst->conn_count);

                    if (Marshal(a2dp_marshal_inst->marshal.marshaller,
                                &flag,
                                MARSHAL_TYPE(uint8)))
                    {
                        if (flag)
                        { /* Connection exists, marshal connection data in next state */
                            a2dp_marshal_inst->state = A2DP_MARSHAL_STATE_CONN;
                        }
                        else
                        { /* This connection does not exist, move to next connection */
                            a2dp_marshal_inst->conn_count++;
                        }
                    }
                    else
                    { /* Insufficient buffer */
                        marshalled = FALSE;
                    }
                }
                else
                { /* All connections marshalled */
                    a2dpMarshalAbort();

                    return TRUE;
                }

                break;

            case A2DP_MARSHAL_STATE_CONN:
                if (Marshal(a2dp_marshal_inst->marshal.marshaller,
                            &a2dp->remote_conn[a2dp_marshal_inst->conn_count],
                            MARSHAL_TYPE(remote_device)))
                {
                    /* Marshal media channel data in next state */
                    a2dp_marshal_inst->media_conn = 0;
                    a2dp_marshal_inst->state = A2DP_MARSHAL_STATE_MEDIA_FLAG;
                }
                else
                { /* Insufficient buffer */
                    marshalled = FALSE;
                }
                break;

            case A2DP_MARSHAL_STATE_MEDIA_FLAG:
                if (a2dp_marshal_inst->media_conn < A2DP_MAX_MEDIA_CHANNELS)
                {
                    bool flag = A2DP_MEDIA_CONNECTION_VALID(a2dp_marshal_inst->conn_count,
                                                            a2dp_marshal_inst->media_conn);

                    if (Marshal(a2dp_marshal_inst->marshal.marshaller,
                                &flag,
                                MARSHAL_TYPE(uint8)))
                    {
                        if (flag)
                        { /* This media channel does not exists, move to next media channel */
                            a2dp_marshal_inst->media_conn++;
                        }
                        else
                        { /* Media channel exists, marshal media channel connection data in next state */
                            a2dp_marshal_inst->state = A2DP_MARSHAL_STATE_MEDIA;
                        }
                    }
                    else
                    { /* Insufficient buffer */
                        marshalled = FALSE;
                    }
                }
                else
                { /* All media channels of this connection have been marshalled */
                    a2dp_marshal_inst->media_conn = 0; /* Reset */
                    a2dp_marshal_inst->state = A2DP_MARSHAL_STATE_DATA_BLOCK;
                }
                break;

            case A2DP_MARSHAL_STATE_MEDIA:
                if (Marshal(a2dp_marshal_inst->marshal.marshaller,
                            &a2dp->remote_conn[a2dp_marshal_inst->conn_count].media_conn[a2dp_marshal_inst->media_conn],
                            MARSHAL_TYPE(media_channel)))
                {
                    /* Move to next media channel connection */
                    a2dp_marshal_inst->media_conn++;
                    a2dp_marshal_inst->state = A2DP_MARSHAL_STATE_MEDIA_FLAG;
                }
                else
                { /* Insufficient buffer */
                    marshalled = FALSE;
                }
                break;

            case A2DP_MARSHAL_STATE_DATA_BLOCK:
            {
                data_block_header *data_blocks = a2dp->data_blocks[a2dp_marshal_inst->conn_count];

                if (Marshal(a2dp_marshal_inst->marshal.marshaller,
                            data_blocks,
                            MARSHAL_TYPE(data_block_header)))
                { /* 'data_block' marshalled, marshal connection data next */
                    a2dp_marshal_inst->conn_count++;
                    a2dp_marshal_inst->state = A2DP_MARSHAL_STATE_CONN_FLAG;
                }
                else
                { /* Insufficient buffer */
                    marshalled = FALSE;
                }
                break;
            }

            default: /* Unexpected state */
                Panic();
                break;
        }

        *written = MarshalProduced(a2dp_marshal_inst->marshal.marshaller);

    } while (marshalled && *written <= length);

    UNUSED(tp_bd_addr);

    return FALSE;
}

/****************************************************************************
NAME    
    a2dpUnmarshal

DESCRIPTION
    Unmarshal the data associated with the A2DP connections

RETURNS
    bool TRUE if A2DP unmarshalling complete, otherwise FALSE
*/
bool a2dpUnmarshal(const tp_bdaddr *tp_bd_addr,
                          const uint8 *buf,
                          uint16 length,
                          uint16 *consumed)
{
    marshal_type_t type;
    bool unmarshalled = TRUE;

    if (!a2dp_marshal_inst)
    { /* Initiating unmarshalling, initialize the instance */
        a2dp_marshal_inst = PanicUnlessNew(a2dp_marshal_instance_t);
        a2dp_marshal_inst->marshal.unmarshaller = UnmarshalInit(mtd_a2dp,
                                                                A2DP_MARSHAL_OBJ_TYPE_COUNT);
        a2dp_marshal_inst->conn_count = 0;
        a2dp_marshal_inst->state = A2DP_MARSHAL_STATE_CONN_FLAG;
    }
    else
    {
        /* Resuming the unmarshalling */
    }

    UnmarshalSetBuffer(a2dp_marshal_inst->marshal.unmarshaller,
                       (void *) buf,
                       length);

    do
    {
        switch (a2dp_marshal_inst->state)
        {
            case A2DP_MARSHAL_STATE_CONN_FLAG:
                if (a2dp_marshal_inst->conn_count < A2DP_MAX_REMOTE_DEVICES_DEFAULT)
                {
                    bool *flag = NULL;

                    if (Unmarshal(a2dp_marshal_inst->marshal.unmarshaller,
                                  (void **) &flag,
                                  &type))
                    {
                        PanicFalse(type == MARSHAL_TYPE(uint8));

                        a2dp_marshal_inst->conn_count++;

                        if (*flag)
                        { /* Connection exists, marshal connection data in next state */
                            a2dp_marshal_inst->state = A2DP_MARSHAL_STATE_CONN;
                        }
                        else
                        { /* This connection does not exist, move to next connection */
                        }

                        free(flag);
                    }
                    else
                    { /* Insufficient buffer */
                        unmarshalled = FALSE;
                    }
                }
                else
                { /* All connections marshalled */
                    UnmarshalDestroy(a2dp_marshal_inst->marshal.unmarshaller,
                                     FALSE);

                    free(a2dp_marshal_inst);
                    a2dp_marshal_inst = NULL;

                    return TRUE;
                }

                break;

            case A2DP_MARSHAL_STATE_CONN:
            {
                remote_device *remote_conn = NULL;

                if (Unmarshal(a2dp_marshal_inst->marshal.unmarshaller,
                              (void **) &remote_conn,
                              &type))
                {
                    PanicFalse(type == MARSHAL_TYPE(remote_device));

                    /* Stitch connection information to A2DP instance */
                    stitchRemoteDevice(remote_conn);

                    /* Marshal media channel data in next state */
                    a2dp_marshal_inst->media_conn = 0;
                    a2dp_marshal_inst->state = A2DP_MARSHAL_STATE_MEDIA_FLAG;
                }
                else
                { /* Insufficient buffer */
                    unmarshalled = FALSE;
                }
                break;
            }

            case A2DP_MARSHAL_STATE_MEDIA_FLAG:
                if (a2dp_marshal_inst->media_conn < A2DP_MAX_MEDIA_CHANNELS)
                {
                    bool *flag = NULL;

                    if (Unmarshal(a2dp_marshal_inst->marshal.unmarshaller,
                                  (void **) &flag,
                                  &type))
                    {
                        PanicFalse(type == MARSHAL_TYPE(uint8));

                        if (*flag)
                        { /* This media channel does not exists, move to next media channel */
                            a2dp_marshal_inst->media_conn++;
                        }
                        else
                        { /* Media channel exists, marshal media channel connection data in next state */
                            a2dp_marshal_inst->state = A2DP_MARSHAL_STATE_MEDIA;
                        }

                        free(flag);
                    }
                    else
                    { /* Insufficient buffer */
                        unmarshalled = FALSE;
                    }
                }
                else
                { /* All media channels of this connection have been unmarshalled */
                    a2dp_marshal_inst->media_conn = 0; /* Reset */
                    a2dp_marshal_inst->state = A2DP_MARSHAL_STATE_DATA_BLOCK;
                }
                break;

            case A2DP_MARSHAL_STATE_MEDIA:
            {
                media_channel *media_conn = NULL;

                if (Unmarshal(a2dp_marshal_inst->marshal.unmarshaller,
                              (void **) &media_conn,
                              &type))
                {
                    PanicFalse(type == MARSHAL_TYPE(media_channel));

                    /* Stitch media connection information to the connection instance */
                    stitchMediaChannel(media_conn);

                    /* Move to next media channel connection */
                    a2dp_marshal_inst->media_conn++;
                    a2dp_marshal_inst->state = A2DP_MARSHAL_STATE_MEDIA_FLAG;
                }
                else
                { /* Insufficient buffer */
                    unmarshalled = FALSE;
                }
                break;
            }

            case A2DP_MARSHAL_STATE_DATA_BLOCK:
            {
                data_block_header *data_blocks = NULL;

                if (Unmarshal(a2dp_marshal_inst->marshal.unmarshaller,
                              (void **) &data_blocks,
                              &type))
                {
                    PanicFalse(type == MARSHAL_TYPE(data_block_header));

                    /* Stitch data_blocks */
                    stitchDataBlockHeader(data_blocks);

                    /* Marshal next connection. */
                    a2dp_marshal_inst->state = A2DP_MARSHAL_STATE_CONN_FLAG;
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

        *consumed = UnmarshalConsumed(a2dp_marshal_inst->marshal.unmarshaller);

    } while (unmarshalled && *consumed <= length);

    UNUSED(tp_bd_addr);

    return FALSE;
}

