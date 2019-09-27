/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Handset service types to be used within handset_service only
*/

#ifndef HANDSET_SERVICE_PROTECTED_H_
#define HANDSET_SERVICE_PROTECTED_H_

#include <bdaddr.h>
#include <logging.h>
#include <panic.h>
#include <task_list.h>

#include "handset_service.h"


/*! \{
    Macros for diagnostic output that can be suppressed.
*/
#define HS_LOG         DEBUG_LOG
/*! \} */

/*! Code assertion that can be checked at run time. This will cause a panic. */
#define assert(x) PanicFalse(x)


/*! \brief Internal messages for the handset_service */
typedef enum
{
    /*! Request to connect to a handset */
    HANDSET_SERVICE_INTERNAL_CONNECT_REQ = 0,

    /*! Request to disconnect a handset */
    HANDSET_SERVICE_INTERNAL_DISCONNECT_REQ,

    /*! Delivered when an ACL connect request has completed. */
    HANDSET_SERVICE_INTERNAL_CONNECT_ACL_COMPLETE,
} handset_service_internal_msg_t;

typedef struct
{
    /* Handset device to connect. */
    device_t device;

    /* Mask of profile(s) to connect. */
    uint8 profiles;
} HANDSET_SERVICE_INTERNAL_CONNECT_REQ_T;

typedef struct
{
    /* Handset device to disconnect. */
    device_t device;
} HANDSET_SERVICE_INTERNAL_DISCONNECT_REQ_T;


/*! \brief Send a HANDSET_SERVICE_CONNECTED_IND to registered clients.

    \param device Device that represents the handset
    \param status Status of the connection.
    \param profiles_connected Profiles currently connected to this handset.
*/
void HandsetService_SendConnectedIndNotification(device_t device,
    uint8 profiles_connected);

/*! \brief Send a HANDSET_SERVICE_DISCONNECTED_IND to registered clients.

    \param device Device that represents the handset
    \param status Status of the connection.
*/
void HandsetService_SendDisconnectedIndNotification(device_t device,
    handset_service_status_t status);

#endif /* HANDSET_SERVICE_PROTECTED_H_ */