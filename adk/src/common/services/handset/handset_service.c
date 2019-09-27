/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Handset service
*/

#include <bdaddr.h>
#include <logging.h>
#include <task_list.h>

#include <bt_device.h>
#include <bredr_scan_manager.h>
#include <connection_manager.h>
#include <device_properties.h>
#include <profile_manager.h>

#include "handset_service.h"
#include "handset_service_sm.h"
#include "handset_service_protected.h"



/*! Handset Service module data. */
handset_service_data_t handset_service;


/*
    Helper functions
*/

/*! Try to find an active handset state machine for a device_t.

    Note: handset_service currently supports only one handset sm.

    \param device Device to search for.
    \return Pointer to the matching state machine, or NULL if no match.
*/
static handset_service_state_machine_t *handsetService_GetSmForDevice(device_t device)
{
    handset_service_state_machine_t *sm_match = NULL;
    handset_service_state_machine_t *sm = HandsetService_GetSm();

    if (sm && (sm->state != HANDSET_SERVICE_STATE_NULL)
        && (sm->handset_device == device))
    {
        sm_match = sm;
    }

    return sm_match;
}

/*! Try to find an active handset state machine for a bdaddr.

    Note: handset_service currently supports only one handset sm.

    \param addr Address to search for.
    \return Pointer to the matching state machine, or NULL if no match.
*/
static handset_service_state_machine_t *handsetService_GetSmForBdAddr(const bdaddr *addr)
{
    device_t dev = BtDevice_GetDeviceForBdAddr(addr);
    return handsetService_GetSmForDevice(dev);
}

/*! \brief Create a new instance of a handset state machine.

    This will return NULL if a new state machine cannot be created,
    for example if the maximum number of handsets already exists.

    \param device Device to create state machine for.

    \return Pointer to new state machine, or NULL if it couldn't be created.
*/
static handset_service_state_machine_t *handsetService_CreateSm(device_t device)
{
    handset_service_state_machine_t *new_sm = NULL;
    handset_service_state_machine_t *sm = HandsetService_GetSm();

    /* Only one handset sm is supported at the moment. */
    if (sm->state == HANDSET_SERVICE_STATE_NULL)
    {
        new_sm = sm;
        HandsetServiceSm_Init(new_sm);
        new_sm->handset_device = device;
        HandsetServiceSm_SetState(new_sm, HANDSET_SERVICE_STATE_DISCONNECTED);

    }

    return new_sm;
}

/*! \brief Send a HANDSET_SERVICE_INTERNAL_CONNECT_REQ to a state machine. */
static void handsetService_InternalConnectReq(handset_service_state_machine_t *sm, uint8 profiles)
{
    MESSAGE_MAKE(req, HANDSET_SERVICE_INTERNAL_CONNECT_REQ_T);
    req->device = sm->handset_device;
    req->profiles = profiles;
    MessageSend(&sm->task_data, HANDSET_SERVICE_INTERNAL_CONNECT_REQ, req);
}

/*! \brief Send a HANDSET_SERVICE_INTERNAL_DISCONNECT_REQ to a state machine. */
static void handsetService_InternalDisconnectReq(handset_service_state_machine_t *sm)
{
    MESSAGE_MAKE(req, HANDSET_SERVICE_INTERNAL_DISCONNECT_REQ_T);
    req->device = sm->handset_device;
    MessageSend(&sm->task_data, HANDSET_SERVICE_INTERNAL_DISCONNECT_REQ, req);
}

/*! \brief Send a HANDSET_SERVICE_INTERNAL_CONNECT_STOP_REQ to a state machine. */
static void handsetService_InternalConnectStopReq(handset_service_state_machine_t *sm)
{
    MESSAGE_MAKE(req, HANDSET_SERVICE_INTERNAL_CONNECT_STOP_REQ_T);
    req->device = sm->handset_device;
    MessageSend(&sm->task_data, HANDSET_SERVICE_INTERNAL_CONNECT_STOP_REQ, req);
}

/*! \brief Helper function for starting a connect req. */
static void handsetService_ConnectReq(Task task, const bdaddr *addr, uint8 profiles)
{
    handset_service_state_machine_t *sm = handsetService_GetSmForBdAddr(addr);

    /* If state machine doesn't exist yet, need to create a new one */
    if (!sm)
    {
        device_t dev = BtDevice_GetDeviceForBdAddr(addr);

        HS_LOG("handsetService_ConnectReq creating new handset sm");
        sm = handsetService_CreateSm(dev);
    }

    if (sm)
    {
        HandsetServiceSm_CompleteDisconnectRequests(sm, handset_service_status_cancelled);
        handsetService_InternalConnectReq(sm, profiles);
        TaskList_AddTask(&sm->connect_list, task);
    }
    else
    {
        HS_LOG("handsetService_ConnectReq Couldn't create a new handset sm");

        MESSAGE_MAKE(cfm, HANDSET_SERVICE_CONNECT_CFM_T);
        cfm->addr = *addr;
        cfm->status = handset_service_status_failed;
        MessageSend(task, HANDSET_SERVICE_CONNECT_CFM, cfm);
    }
}

/*! \brief Helper function for starting a connect req. */
static void handsetService_DisconnectReq(Task task, const bdaddr *addr)
{
    handset_service_state_machine_t *sm = handsetService_GetSmForBdAddr(addr);

    if (sm)
    {
        HandsetServiceSm_CompleteConnectRequests(sm, handset_service_status_cancelled);
        handsetService_InternalDisconnectReq(sm);
        TaskList_AddTask(&sm->disconnect_list, task);
    }
    else
    {
        HS_LOG("handsetService_DisconnectReq sm not found");

        MESSAGE_MAKE(cfm, HANDSET_SERVICE_DISCONNECT_CFM_T);
        cfm->addr = *addr;
        cfm->status = handset_service_status_success;
        MessageSend(task, HANDSET_SERVICE_DISCONNECT_CFM, cfm);
    }
}

/*
    Message handler functions
*/

/* \brief Handle a CON_MANAGER_CONNECTION_IND */
static void handsetService_HandleConManagerConnectionInd(const CON_MANAGER_CONNECTION_IND_T *ind)
{
    handset_service_state_machine_t *sm = handsetService_GetSmForBdAddr(&ind->bd_addr);

    HS_LOG("handsetService_HandleConManagerConnectionInd addr [%04x,%02x,%06lx] connected %d ble %d",
           ind->bd_addr.nap, ind->bd_addr.uap, ind->bd_addr.lap, ind->connected, ind->ble);

    if (sm)
    {
        HS_LOG("handsetService_HandleConManagerConnectionInd received for managed handset");
        HandsetServiceSm_HandleConManagerConnectionInd(sm, ind);
    }
    else
    {
        /* TBD: Create a new state machine for a newly connected device? */
    }
    
}

static void handsetService_HandleProfileManagerConnectedInd(const CONNECTED_PROFILE_IND_T *ind)
{
    bdaddr addr = DeviceProperties_GetBdAddr(ind->device);
    bool is_handset = appDeviceIsHandset(&addr);

    HS_LOG("handsetService_HandleProfileManagerConnectedInd device 0x%x profile 0x%x handset %d",
           ind->device, ind->profile, is_handset);

    if (is_handset)
    {
        handset_service_state_machine_t *sm = handsetService_GetSmForDevice(ind->device);

        /* If state machine doesn't exist yet, need to create a new one */
        if (!sm)
        {
            HS_LOG("handsetService_HandleProfileManagerConnectedInd creating new handset sm");
            sm = handsetService_CreateSm(ind->device);
        }

        /* TBD: handset_service currently only supports one active handset, 
                so what should happen if a new device can't be created? */
        assert(sm);

        /* Forward the connect ind to the state machine */
        HandsetServiceSm_HandleProfileManagerConnectedInd(sm, ind);
    }
}

static void handsetService_HandleProfileManagerDisconnectedInd(const DISCONNECTED_PROFILE_IND_T *ind)
{
    bdaddr addr = DeviceProperties_GetBdAddr(ind->device);
    bool is_handset = appDeviceIsHandset(&addr);

    HS_LOG("handsetService_HandleProfileManagerDisconnectedInd device 0x%x profile 0x%x handset %d",
           ind->device, ind->profile, is_handset);

    if (is_handset)
    {
        handset_service_state_machine_t *sm = handsetService_GetSmForDevice(ind->device);

        if (sm)
        {
            /* Forward the disconnect ind to the state machine */
            HandsetServiceSm_HandleProfileManagerDisconnectedInd(sm, ind);
        }
    }
}

static void handsetService_MessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch (id)
    {
    /* Connection Manager messages */
    case CON_MANAGER_CONNECTION_IND:
        handsetService_HandleConManagerConnectionInd((const CON_MANAGER_CONNECTION_IND_T *)message);
        break;

    /* Profile Manager messages */
    case CONNECTED_PROFILE_IND:
        handsetService_HandleProfileManagerConnectedInd((const CONNECTED_PROFILE_IND_T *)message);
        break;

    case DISCONNECTED_PROFILE_IND:
        handsetService_HandleProfileManagerDisconnectedInd((const DISCONNECTED_PROFILE_IND_T *)message);
        break;

    /* BREDR Scan Manager messages */
    case BREDR_SCAN_MANAGER_PAGE_SCAN_PAUSED_IND:
    case BREDR_SCAN_MANAGER_PAGE_SCAN_RESUMED_IND:
        /* These are informational so no need to act on them. */
        break;

    default:
        HS_LOG("handsetService_MessageHandler unhandled id 0x%x", id);
        break;
    }
}

/*
    Public functions
*/

bool HandsetService_Init(Task task)
{
    handset_service_data_t *hs = HandsetService_Get();
    memset(hs, 0, sizeof(*hs));
    hs->task_data.handler = handsetService_MessageHandler;

    ConManagerRegisterConnectionsClient(&hs->task_data);

    ProfileManager_ClientRegister(&hs->task_data);

    HandsetServiceSm_Init(&hs->state_machine);

    TaskList_Initialise(&hs->client_list);

    UNUSED(task);
    return TRUE;
}

void HandsetService_ClientRegister(Task client_task)
{
    handset_service_data_t *hs = HandsetService_Get();
    TaskList_AddTask(&hs->client_list, client_task);
}

void HandsetService_ClientUnregister(Task client_task)
{
    handset_service_data_t *hs = HandsetService_Get();
    TaskList_RemoveTask(&hs->client_list, client_task);
}

void HandsetService_ConnectAddressRequest(Task task, const bdaddr *addr, uint8 profiles)
{
    HS_LOG("HandsetService_ConnectAddressRequest addr [%04x,%02x,%06lx] profiles 0x%x",
            addr->nap, addr->uap, addr->lap, profiles);

    handsetService_ConnectReq(task, addr, profiles);
}

void HandsetService_ConnectMruRequest(Task task, uint8 profiles)
{
    bdaddr hs_addr;

    HS_LOG("HandsetService_ConnectMruRequest profiles 0x%x", profiles);

    if (appDeviceGetHandsetBdAddr(&hs_addr))
    {
        HS_LOG("HandsetService_ConnectMruRequest addr [%04x,%02x,%06lx]",
                hs_addr.nap, hs_addr.uap, hs_addr.lap);

        handsetService_ConnectReq(task, &hs_addr, profiles);
    }
    else
    {
        /* No MRU handset - send CFM immediately */
        MESSAGE_MAKE(cfm, HANDSET_SERVICE_CONNECT_CFM_T);
        BdaddrSetZero(&cfm->addr);
        cfm->status = handset_service_status_no_mru;
        MessageSend(task, HANDSET_SERVICE_CONNECT_CFM, cfm);
    }
}

void HandsetService_DisconnectRequest(Task task, const bdaddr *addr)
{
    HS_LOG("HandsetService_DisconnectRequest addr [%04x,%02x,%06lx]",
            addr->nap, addr->uap, addr->lap);

    handsetService_DisconnectReq(task, addr);
}

void HandsetService_StopConnect(Task task, const bdaddr *addr)
{
    handset_service_state_machine_t *sm = handsetService_GetSmForBdAddr(addr);

    if (sm)
    {
        PanicFalse(sm->connect_stop_task == (Task)0);
        handsetService_InternalConnectStopReq(sm);
        sm->connect_stop_task = task;
    }
    else
    {
        HS_LOG("HandsetService_StopConnect no handset connection to stop");

        MESSAGE_MAKE(cfm, HANDSET_SERVICE_CONNECT_STOP_CFM_T);
        cfm->addr = *addr;
        cfm->status = handset_service_status_disconnected;
        MessageSend(task, HANDSET_SERVICE_CONNECT_STOP_CFM, cfm);
    }
}

void HandsetService_ConnectableRequest(Task task)
{
    UNUSED(task);
    BredrScanManager_PageScanRequest(HandsetService_GetTask(), SCAN_MAN_PARAMS_TYPE_SLOW);
}

void HandsetService_CancelConnectableRequest(Task task)
{
    UNUSED(task);
    BredrScanManager_PageScanRelease(HandsetService_GetTask());
}

void HandsetService_SendConnectedIndNotification(device_t device,
    uint8 profiles_connected)
{
    handset_service_data_t *hs = HandsetService_Get();

    MESSAGE_MAKE(ind, HANDSET_SERVICE_CONNECTED_IND_T);
    ind->addr = DeviceProperties_GetBdAddr(device);
    ind->profiles_connected = profiles_connected;
    TaskList_MessageSend(&hs->client_list, HANDSET_SERVICE_CONNECTED_IND, ind);
}

void HandsetService_SendDisconnectedIndNotification(device_t device,
    handset_service_status_t status)
{
    handset_service_data_t *hs = HandsetService_Get();

    MESSAGE_MAKE(ind, HANDSET_SERVICE_DISCONNECTED_IND_T);
    ind->addr = DeviceProperties_GetBdAddr(device);
    ind->status = status;
    TaskList_MessageSend(&hs->client_list, HANDSET_SERVICE_DISCONNECTED_IND, ind);
}
