/*!
\copyright  Copyright (c) 2005 - 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       earbud_sm_marshal_defs.h
\brief      Definition of messages that can be sent between Earbud application entities.
*/

#ifndef EARBUD_SM_MARSHAL_DEFS_H
#define EARBUD_SM_MARSHAL_DEFS_H

#include "earbud_sm_private.h"

/* framework includes */
#include <marshal_common.h>

/* system includes */
#include <marshal.h>

typedef struct earbud_sm_msg_empty_payload
{
    earbud_sm_msg_type type;
} earbud_sm_msg_empty_payload_t;

/* Create base list of marshal types the Earbud SM will use. */
#define MARSHAL_TYPES_TABLE(ENTRY) \
    ENTRY(earbud_sm_msg_empty_payload_t)

/* X-Macro generate enumeration of all marshal types */
#define EXPAND_AS_ENUMERATION(type) MARSHAL_TYPE(type),
enum MARSHAL_TYPES
{
    /* common types must be placed at the start of the enum */
    DUMMY = NUMBER_OF_COMMON_MARSHAL_OBJECT_TYPES-1,
    /* now expand the marshal types specific to this component. */
    MARSHAL_TYPES_TABLE(EXPAND_AS_ENUMERATION)
    NUMBER_OF_MARSHAL_OBJECT_TYPES
};
#undef EXPAND_AS_ENUMERATION

/* Make the array of all message marshal descriptors available. */
extern const marshal_type_descriptor_t * const earbud_sm_marshal_type_descriptors[];

#endif /* EARBUD_SM_MARSHAL_DEFS_H */
