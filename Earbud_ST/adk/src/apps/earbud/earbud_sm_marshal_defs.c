/*!
\copyright  Copyright (c) 2015 - 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       earbud_sm_marshal_defs.c
\brief      Marshal type definitions for Earbud application.
*/

/* component includes */
#include "earbud_sm_marshal_defs.h"

/* framework includes */
#include <marshal_common.h>

/* system includes */
#include <marshal.h>

/*! earbud_sm_empty payload message marshalling type descriptor. */
const marshal_type_descriptor_t marshal_type_descriptor_earbud_sm_msg_empty_payload_t =
    MAKE_MARSHAL_TYPE_DEFINITION_BASIC(sizeof(earbud_sm_msg_empty_payload_t));

/*! X-Macro generate earbud SM marshal type descriptor set that can be passed to a (un)marshaller
 *  to initialise it.
 *  */
#define EXPAND_AS_TYPE_DEFINITION(type) (const marshal_type_descriptor_t *)&marshal_type_descriptor_##type,
const marshal_type_descriptor_t * const earbud_sm_marshal_type_descriptors[NUMBER_OF_MARSHAL_OBJECT_TYPES] = {
    MARSHAL_COMMON_TYPES_TABLE(EXPAND_AS_TYPE_DEFINITION)
    MARSHAL_TYPES_TABLE(EXPAND_AS_TYPE_DEFINITION)
};
