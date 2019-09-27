/****************************************************************************
Copyright (c) 2019 Qualcomm Technologies International, Ltd.


FILE NAME
    a2dp_marshal_desc.c

DESCRIPTION
    Creates marshal type descriptors for A2DP data types

NOTES
    Builds requiring this should include CONFIG_HANDOVER in the
    makefile. e.g.
        CONFIG_FEATURES:=CONFIG_HANDOVER
*/

#include "a2dp_private.h"
#include "a2dp_marshal_desc.h"
#include <panic.h>


static uint32 unionMemberMediaChannel(const void *parent,
                                      const marshal_member_descriptor_t *member_descriptor,
                                      uint32 array_element);

static uint32 dynArrElemsSignallingConnectionActive(const void *parent,
                                                    const marshal_member_descriptor_t *member_descriptor,
                                                    uint32 array_element);

static uint32 unionMemberSignallingChannel(const void *parent,
                                           const marshal_member_descriptor_t *member_descriptor,
                                           uint32 array_element);

static uint32 dynArrElemsRemoteDevice(const void *parent,
                                      const marshal_member_descriptor_t *member_descriptor,
                                      uint32 array_element);


static uint32 dynArrElemsDataBlockHeader(const void *parent,
                                         const marshal_member_descriptor_t *member_descriptor,
                                         uint32 array_element);


static const marshal_type_descriptor_t mtd_sep_info =
    MAKE_MARSHAL_TYPE_DEFINITION_BASIC(sep_info);


static const marshal_type_descriptor_t mtd_connection_setup =
    MAKE_MARSHAL_TYPE_DEFINITION_BASIC(connection_setup);


static const marshal_type_descriptor_t mtd_media_connection_active_bitfields =
    MAKE_MARSHAL_TYPE_DEFINITION_BASIC(media_connection_active_bitfields);


static const marshal_member_descriptor_t mmd_media_connection_active[] =
{
    MAKE_MARSHAL_MEMBER(media_connection_active, L2capSink, sink), /* Gets converted to CID for marshalling */
    MAKE_MARSHAL_MEMBER(media_connection_active, media_connection_active_bitfields, bitfields),
};

static const marshal_type_descriptor_t mtd_media_connection_active =
    MAKE_MARSHAL_TYPE_DEFINITION(media_connection_active, mmd_media_connection_active);


static const marshal_member_descriptor_t mmd_media_connection[] =
{
    MAKE_MARSHAL_MEMBER(media_connection, connection_setup, setup), /* ???: Would we ever marshal when setup is going on? */
    MAKE_MARSHAL_MEMBER(media_connection, media_connection_active, active),
};

static const marshal_type_descriptor_t mtd_media_connection =
    MAKE_MARSHAL_TYPE_DEFINITION_UNION(media_connection,
                                       mmd_media_connection);


static const marshal_type_descriptor_t mtd_media_channel_status =
    MAKE_MARSHAL_TYPE_DEFINITION_BASIC(media_channel_status);


static const marshal_member_descriptor_t mmd_media_channel[] =
{
    MAKE_MARSHAL_MEMBER(media_channel, media_connection, connection),
    MAKE_MARSHAL_MEMBER(media_channel, media_channel_status, status),
};

static const marshal_type_descriptor_dynamic_t mtd_media_channel =
    MAKE_MARSHAL_TYPE_DEFINITION_HAS_DYNAMIC_UNION(media_channel,
                                                   mmd_media_channel,
                                                   unionMemberMediaChannel);


static const marshal_type_descriptor_t mtd_signalling_connection_bitfields =
    MAKE_MARSHAL_TYPE_DEFINITION_BASIC(signalling_connection_bitfields);


static const marshal_member_descriptor_t mmd_signalling_connection_active[] =
{
    MAKE_MARSHAL_MEMBER(signalling_connection_active, L2capSink, sink), /* Gets converted to CID for marshalling */
    MAKE_MARSHAL_MEMBER(signalling_connection_active, signalling_connection_bitfields, bitfields),
    MAKE_MARSHAL_MEMBER_POINTER(signalling_connection_active, uint8_dyn_arr_t, received_packet),
};

static const marshal_type_descriptor_dynamic_t mtd_signalling_connection_active =
    MAKE_MARSHAL_TYPE_DEFINITION_HAS_PTR_TO_DYNAMIC_ARRAY(signalling_connection_active,
                                                          mmd_signalling_connection_active,
                                                          dynArrElemsSignallingConnectionActive);


static const marshal_member_descriptor_t mmd_signalling_connection[] =
{
    MAKE_MARSHAL_MEMBER(signalling_connection, connection_setup, setup), /* ???: Would we ever marshal when setup is going on? */
    MAKE_MARSHAL_MEMBER(signalling_connection, signalling_connection_active, active),
};

static const marshal_type_descriptor_t mtd_signalling_connection =
    MAKE_MARSHAL_TYPE_DEFINITION_UNION(signalling_connection,
                                       mmd_signalling_connection);


static const marshal_type_descriptor_t mtd_signalling_channel_status =
    MAKE_MARSHAL_TYPE_DEFINITION_BASIC(signalling_channel_status);


static const marshal_member_descriptor_t mmd_signalling_channel[] =
{
    MAKE_MARSHAL_MEMBER(signalling_channel, signalling_connection, connection),
    MAKE_MARSHAL_MEMBER(signalling_channel, signalling_channel_status, status),
};

static const marshal_type_descriptor_dynamic_t mtd_signalling_channel =
    MAKE_MARSHAL_TYPE_DEFINITION_HAS_DYNAMIC_UNION(signalling_channel,
                                                   mmd_signalling_channel,
                                                   unionMemberSignallingChannel);


static const marshal_member_descriptor_t mmd_remote_device[] =
{
    MAKE_MARSHAL_MEMBER(remote_device, bdaddr, bd_addr),
    MAKE_MARSHAL_MEMBER(remote_device, sep_info, remote_sep),
    MAKE_MARSHAL_MEMBER(remote_device, sep_info, local_sep),
    MAKE_MARSHAL_MEMBER(remote_device, uint16, reconfig_caps_size), /* Size needs to be marshalled before pointer */
    MAKE_MARSHAL_MEMBER_POINTER(remote_device, uint8_dyn_arr_t, reconfig_caps),
    MAKE_MARSHAL_MEMBER(remote_device, signalling_channel, signal_conn),
 /* MAKE_MARSHAL_MEMBER_ARRAY(remote_device, media_channel, media_conn, A2DP_MAX_MEDIA_CHANNELS), marshalled separately */
 /* MAKE_MARSHAL_MEMBER(remote_device, Task, clientTask),  Not to be marshalled, to be updated separately through an API */
};

static const marshal_type_descriptor_dynamic_t mtd_remote_device =
    MAKE_MARSHAL_TYPE_DEFINITION_HAS_PTR_TO_DYNAMIC_ARRAY(remote_device,
                                                          mmd_remote_device,
                                                          dynArrElemsRemoteDevice);


static const marshal_member_descriptor_t mmd_data_block_header[] =
{
    MAKE_MARSHAL_MEMBER(data_block_header, uint16, size_blocks_padded),
    MAKE_MARSHAL_MEMBER_POINTER(data_block_header, uint8_dyn_arr_t, block),
};

static const marshal_type_descriptor_dynamic_t mtd_data_block_header =
    MAKE_MARSHAL_TYPE_DEFINITION_HAS_PTR_TO_DYNAMIC_ARRAY(data_block_header,
                                                          mmd_data_block_header,
                                                          dynArrElemsDataBlockHeader);


/* Use xmacro to expand type table as array of type descriptors */
#define EXPAND_AS_TYPE_DEFINITION(type) (const marshal_type_descriptor_t *) &mtd_##type,
const marshal_type_descriptor_t * const  mtd_a2dp[] =
{
    COMMON_MARSHAL_TYPES_TABLE(EXPAND_AS_TYPE_DEFINITION)
    COMMON_DYN_MARSHAL_TYPES_TABLE(EXPAND_AS_TYPE_DEFINITION)
    A2DP_MARSHAL_TYPES_TABLE(EXPAND_AS_TYPE_DEFINITION)
};
#undef EXPAND_AS_TYPE_DEFINITION


static uint32 unionMemberMediaChannel(const void *parent,
                                      const marshal_member_descriptor_t *member_descriptor,
                                      uint32 array_element)
{
    uint8 member = 0;
    const media_channel *obj = parent;

    PanicFalse(obj && member_descriptor);
    PanicFalse(array_element == 0);
    PanicFalse(member_descriptor->offset == offsetof(media_channel, connection));

    switch (obj->status.conn_info.connection_state)
    {
        case avdtp_connection_idle: /* Connection getting setup */
            member = 0;
            break;

        case avdtp_connection_connected:
            member = 1;
            break;

        default:
            Panic();
            break;
    }

    return member;
}


static uint32 dynArrElemsSignallingConnectionActive(const void *parent,
                                                    const marshal_member_descriptor_t *member_descriptor,
                                                    uint32 array_element)
{
    const signalling_connection_active *obj = parent;

    PanicFalse(obj && member_descriptor);
    PanicFalse(array_element == 0);
    PanicFalse(member_descriptor->offset == offsetof(signalling_connection_active, received_packet));

    return obj->bitfields.received_packet_length;
}


static uint32 unionMemberSignallingChannel(const void *parent,
                                           const marshal_member_descriptor_t *member_descriptor,
                                           uint32 array_element)
{
    uint8 member = 0;
    const signalling_channel *obj = parent;

    PanicFalse(obj && member_descriptor);
    PanicFalse(array_element == 0);
    PanicFalse(member_descriptor->offset == offsetof(signalling_channel, connection));

    switch (obj->status.connection_state)
    {
        case avdtp_connection_idle: /* Connection getting setup */
            member = 0;
            break;

        case avdtp_connection_connected:
            member = 1;
            break;

        default:
            Panic();
            break;
    }

    return member;
}


static uint32 dynArrElemsRemoteDevice(const void *parent,
                                      const marshal_member_descriptor_t *member_descriptor,
                                      uint32 array_element)
{
    const remote_device *obj = parent;

    PanicFalse(obj && member_descriptor);
    PanicFalse(array_element == 0);
    PanicFalse(member_descriptor->offset == offsetof(remote_device, reconfig_caps));

    return obj->reconfig_caps_size;
}


static uint32 dynArrElemsDataBlockHeader(const void *parent,
                                         const marshal_member_descriptor_t *member_descriptor,
                                         uint32 array_element)
{
    uint16 block_length;
    const data_block_header *obj = parent;

    PanicFalse(obj && member_descriptor);
    PanicFalse(array_element == 0);
    PanicFalse(member_descriptor->offset == offsetof(data_block_header, block));

    block_length = obj->size_blocks_padded + (sizeof(data_block_info) * max_data_blocks);

    return block_length;
}

