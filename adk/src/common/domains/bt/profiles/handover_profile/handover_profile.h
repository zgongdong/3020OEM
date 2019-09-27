/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       handover_profile.h
\defgroup   handover_profile Handover Profile
\ingroup    profiles
\brief      Handover Profile
*/

#ifndef HANDOVER_PROFILE_H_
#define HANDOVER_PROFILE_H_


#define HandoverProfile_Init(init_task) (FALSE)

#define HandoverProfile_ClientRegister(client_task) /* Nothing to do */

#define HandoverProfile_ClientUnregister(client_task) /* Nothing to do */

#define HandoverProfile_RegisterHandoverClients(clients, clients_size) (FALSE)

#define HandoverProfile_Connect(task, peer_addr) /* Nothing to do */

#define HandoverProfile_Disconnect(task) /* Nothing to do */

#define HandoverProfile_Handover(remote_addr) (HANDOVER_PROFILE_STATUS_HANDOVER_FAILURE)

#define HandoverProfile_HandleConnectionLibraryMessages(id, message, already_handled) (already_handled)


#endif /*HANDOVER_PROFILE_H_*/

