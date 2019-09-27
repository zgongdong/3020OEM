/* Copyright (c) 2019 Qualcomm Technologies International, Ltd. */
/*  */

#include <gatt_manager.h>

#include "gatt_role_selection_client.h"
#include "gatt_role_selection_client_write.h"
#include "gatt_role_selection_client_notification.h"
#include "gatt_role_selection_client_init.h"
#include "gatt_role_selection_client_state.h"
#include "gatt_role_selection_client_debug.h"


/****************************************************************************/
void roleSelectionWriteStateClientConfigValue(GATT_ROLE_SELECTION_CLIENT *instance)
{
    uint8 value[2] = {ROLE_SELECTION_SERVICE_NOTIFICATION_VALUE, 0};
    value[1] = 0;

    GattManagerWriteCharacteristicValue((Task)&instance->lib_task, 
                                        instance->handle_state_config, 
                                        sizeof(value)/sizeof(uint8), value);
}


/****************************************************************************/
void roleSelectionWriteFigureOfMeritClientConfigValue(GATT_ROLE_SELECTION_CLIENT *instance)
{
    uint8 value[2] = {ROLE_SELECTION_SERVICE_NOTIFICATION_VALUE, 0};
    value[1] = 0;

    GattManagerWriteCharacteristicValue((Task)&instance->lib_task, 
                                        instance->handle_figure_of_merit_config, 
                                        sizeof(value)/sizeof(uint8), value);
}


/****************************************************************************/
void handleRoleSelectionWriteValueResp(GATT_ROLE_SELECTION_CLIENT *instance, 
                                       const GATT_MANAGER_WRITE_CHARACTERISTIC_VALUE_CFM_T *write_cfm)
{
    if (   role_selection_client_waiting_write == gattRoleSelectionClientGetState(instance)
        && instance->handle_role_control == write_cfm->handle)
    {
        gattRoleSelectionClientSetState(instance, role_selection_client_initialised);
        if (gatt_status_success != write_cfm->status)
        {
            GATT_ROLE_SELECTION_CLIENT_DEBUG("handleRoleSelectionWriteValueResp: role_sel handle:%d bad status:%d",
                                             write_cfm->handle, write_cfm->status);
        }
        gattRoleSelectionSendClientCommandCfmMsg(instance, write_cfm->status);
    }
    else if (   role_selection_client_setting_notification == gattRoleSelectionClientGetState(instance)
             && instance->handle_state_config == write_cfm->handle)
    {
        gattRoleSelectionClientSetState(instance, role_selection_client_initialised);
        if (gatt_status_success != write_cfm->status)
        {
            GATT_ROLE_SELECTION_CLIENT_DEBUG("handleRoleSelectionWriteValueResp: state_cfg handle:%d bad status:%d",
                                             write_cfm->handle, write_cfm->status);
        }
    }
    else if (   role_selection_client_setting_notification_fom == gattRoleSelectionClientGetState(instance)
             && instance->handle_figure_of_merit_config == write_cfm->handle)
    {
        gattRoleSelectionClientSetState(instance, role_selection_client_initialised);
        if (gatt_status_success != write_cfm->status)
        {
            GATT_ROLE_SELECTION_CLIENT_DEBUG("handleRoleSelectionWriteValueResp: merit_cfg handle:%d bad status:%d",
                                             write_cfm->handle, write_cfm->status);
        }
    }
}


void gattRoleSelectionSendClientCommandCfmMsg(GATT_ROLE_SELECTION_CLIENT *instance, 
                                              gatt_status_t status)
{
    if (instance->control_response_needed)
    {
        MAKE_ROLE_SELECTION_MESSAGE(GATT_ROLE_SELECTION_CLIENT_COMMAND_CFM);

        message->instance = instance;
        message->result = status;

        MessageSend(instance->app_task, GATT_ROLE_SELECTION_CLIENT_COMMAND_CFM, message);
        
        instance->control_response_needed = FALSE;
    }
}


bool GattRoleSelectionClientChangePeerRole(GATT_ROLE_SELECTION_CLIENT *instance,
                                           GattRoleSelectionServiceControlOpCode state)
{
    uint8   state_to_write = (uint8)state;

    if (!instance)
    {
        return FALSE;
    }

    if (role_selection_client_initialised != gattRoleSelectionClientGetState(instance))
    {
        return FALSE;
    }

    gattRoleSelectionClientSetState(instance, role_selection_client_waiting_write);

    GattManagerWriteCharacteristicValue(&instance->lib_task, instance->handle_role_control, 
                                        1, &state_to_write);

    instance->control_response_needed = TRUE;

    return TRUE;
}

