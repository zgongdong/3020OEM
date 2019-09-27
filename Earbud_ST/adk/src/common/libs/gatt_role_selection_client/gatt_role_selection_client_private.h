/* Copyright (c) 2019 Qualcomm Technologies International, Ltd. */
/*  */
/*! \file
    Private header 
*/

#ifndef GATT_ROLE_SELECTION_CLIENT_PRIVATE_H_
#define GATT_ROLE_SELECTION_CLIENT_PRIVATE_H_

#include <csrtypes.h>
#include <panic.h>
#include <stdlib.h>

#include <gatt_role_selection_service.h>
#include "gatt_role_selection_client.h"


/* Macros for creating messages */
#define MAKE_ROLE_SELECTION_MESSAGE(TYPE) TYPE##_T *message = (TYPE##_T*)PanicNull(calloc(1,sizeof(TYPE##_T)))
#define MAKE_ROLE_SELECTION_MESSAGE_WITH_LEN(TYPE, LEN) TYPE##_T *message = (TYPE##_T *) PanicNull(calloc(1,sizeof(TYPE##_T) + ((LEN) - 1) * sizeof(uint8)))


/* Enum for role selection client library internal message. */
typedef enum
{
    ROLE_SELECTION_CLIENT_INTERNAL_UNUSED,
} role_selection_internal_msg_t;

#endif /* GATT_ROLE_SELECTION_CLIENT_PRIVATE_H_ */
