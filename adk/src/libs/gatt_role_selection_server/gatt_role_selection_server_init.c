/* Copyright (c) 2019 Qualcomm Technologies International, Ltd. */
/*  */

#include <string.h>
#include <stdio.h>

#include "gatt_role_selection_server.h"
#include "gatt_role_selection_server_debug.h"
#include "gatt_role_selection_server_msg_handler.h"
#include "gatt_role_selection_server_db.h"

#include <panic.h>

/****************************************************************************/
bool GattRoleSelectionServerInit(GATT_ROLE_SELECTION_SERVER *instance, 
                                 Task app_task,
                                 uint16 start_handle,
                                 uint16 end_handle)
{
    gatt_manager_server_registration_params_t registration_params;

    if ((app_task == NULL) || (instance == NULL))
    {
        GATT_ROLE_SELECTION_SERVER_DEBUG("GattRoleSelectionServerInit: Invalid Initialisation parameters");
        Panic();
        return FALSE;
    }

    memset(instance, 0, sizeof(*instance));

    /* Set up library handler for external messages */
    instance->lib_task.handler = roleSelectionServerMsgHandler;

    /* Store the Task function parameter.
       All library messages need to be sent here */
    instance->app_task = app_task;

    instance->initialised = TRUE;

    /* Register the service with gatt manager */
    registration_params.task = &instance->lib_task;
    registration_params.start_handle = start_handle;
    registration_params.end_handle = end_handle;
        
    if (GattManagerRegisterServer(&registration_params) == gatt_manager_status_success)
    {
        return TRUE;
    }
    return FALSE;
}
