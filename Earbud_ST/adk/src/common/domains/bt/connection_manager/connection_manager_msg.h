/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       
\brief      Header file for Connection Manager internal messages
*/

#ifndef __CON_MANAGER_MSG_H
#define __CON_MANAGER_MSG_H

#include <connection_manager.h>
#include "connection_manager_list.h"

/*! Connection Manager message IDs. */
typedef enum
{
    CON_MANAGER_INTERNAL_MSG_UPDATE_QOS
} con_manager_internal_msg_id_t;

/*! Internal  */
typedef struct
{
    tp_bdaddr tpaddr;
} CON_MANAGER_INTERNAL_MSG_UPDATE_QOS_T;

/*! \brief Send Write page timeout to BlueStack
    \param page_timeout The page timeout
 */
void conManagerSendWritePageTimeout(uint16 page_timeout);

/*! \brief Send Open ACL request to BlueStack
    \param tpaddr The address of the remote device
 */
void conManagerSendOpenTpAclRequest(const tp_bdaddr* tpaddr);

/*! \brief Configure role switch policy.

    Currently we want to never automatically ask for a role switch nor
    refuse a request for a role switch.
 */
void ConManagerSetupRoleSwitchPolicy(void);

/*! \brief Send Close ACL request to BlueStack
    \param tpaddr The address of the remote device
    \param force Set TRUE to force close the ACL immediately regardless of
           whether there are active L2CAP connections.
 */
void conManagerSendCloseTpAclRequest(const tp_bdaddr* tpaddr, bool force);

/*! \brief Send internal message to update QoS
    \param connection The connection to send the message to
 */
void conManagerSendInternalMsgUpdateQos(cm_connection_t* connection);

/*! \brief Handle a message sent to a connection task
    \param task The task the message was sent to
    \param id The message ID
    \param id The message payload
 */
void ConManagerConnectionHandleMessage(Task task, MessageId id, Message message);

#endif
