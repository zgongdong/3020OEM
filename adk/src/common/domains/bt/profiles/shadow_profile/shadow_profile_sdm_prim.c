/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Functions to send & process SDM prims to / from the firmware.
*/


#ifdef INCLUDE_SHADOWING

#include <bdaddr.h>
#include <sink.h>
#include <stream.h>

#include "bt_device.h"

#include "shadow_profile_private.h"
#include "shadow_profile_sdm_prim.h"


/*! Construct a SDM prim of the given type */
#define MAKE_SDM_PRIM_T(TYPE) MESSAGE_MAKE(prim,TYPE##_T); prim->type = TYPE;


/*
    Functions to send SDM prims to firmware.
*/

void ShadowProfile_ShadowRegisterReq(void)
{
    MAKE_SDM_PRIM_T(SDM_REGISTER_REQ);
    prim->phandle = 0;
    VmSendSdmPrim(prim);
}

void ShadowProfile_ShadowConnectReq(link_type_t type)
{
    bdaddr bd_addr;

    MAKE_SDM_PRIM_T(SDM_SHADOW_LINK_CREATE_REQ);
    prim->phandle = 0;

    SHADOW_LOGF("ShadowProfile_ShadowConnectReq type 0x%x", type);

    appDeviceGetHandsetBdAddr(&bd_addr);
    SHADOW_LOGF("ShadowProfile_ShadowConnectReq handset addr {%04x %02x %06lx}",
                bd_addr.nap, bd_addr.uap, bd_addr.lap);
    BdaddrConvertVmToBluestack(&prim->shadow_bd_addr.addrt.addr, &bd_addr);
    prim->shadow_bd_addr.addrt.type = TBDADDR_PUBLIC;
    prim->shadow_bd_addr.tp_type = BREDR_ACL;

    appDeviceGetSecondaryBdAddr(&bd_addr);
    SHADOW_LOGF("ShadowProfile_ShadowConnectReq peer addr {%04x %02x %06lx}",
                bd_addr.nap, bd_addr.uap, bd_addr.lap);
    BdaddrConvertVmToBluestack(&prim->secondary_bd_addr.addrt.addr, &bd_addr);
    prim->secondary_bd_addr.addrt.type = TBDADDR_PUBLIC;
    prim->secondary_bd_addr.tp_type = BREDR_ACL;

    prim->link_type = type;
    VmSendSdmPrim(prim);
}

void ShadowProfile_ShadowDisconnectReq(hci_connection_handle_t conn_handle,
                                              hci_reason_t reason)
{
    MAKE_SDM_PRIM_T(SDM_SHADOW_LINK_DISCONNECT_REQ);

    SHADOW_LOGF("shadowProfile_ShadowDisconnectReq handle 0x%x reason 0x%x", conn_handle, reason);

    prim->phandle = 0;
    prim->conn_handle = conn_handle;
    prim->reason = reason;
    VmSendSdmPrim(prim);
}

void ShadowProfile_ShadowL2capConnectReq(hci_connection_handle_t conn_handle,
                                         l2ca_cid_t cid)
{
    MAKE_SDM_PRIM_T(SDM_SHADOW_L2CAP_CREATE_REQ);

    SHADOW_LOGF("ShadowProfile_ShadowL2capConnectReq handle 0x%x cid 0x%x", conn_handle, cid);

    prim->phandle = 0;
    prim->flags = 0;
    prim->connection_handle = conn_handle;
    prim->cid = cid;
    VmSendSdmPrim(prim);
}

void ShadowProfile_ShadowL2capConnectRsp(hci_connection_handle_t conn_handle,
                                         l2ca_cid_t cid)
{
    static const uint16 l2cap_conftab[] =
    {
        /* Configuration Table must start with a separator. */
        L2CAP_AUTOPT_SEPARATOR,
        /* Local MTU exact value (incoming). */
        L2CAP_AUTOPT_MTU_IN, 0x037F,
        /* Configuration Table must end with a terminator. */
        L2CAP_AUTOPT_TERMINATOR
    };

    MAKE_SDM_PRIM_T(SDM_SHADOW_L2CAP_CREATE_RSP);

    SHADOW_LOGF("ShadowProfile_ShadowL2capConnectRsp handle 0x%x cid 0x%x", conn_handle, cid);

    prim->phandle = 0;
    prim->connection_handle = conn_handle;
    prim->cid = cid;
    prim->conftab_length = CONFTAB_LEN(l2cap_conftab);
    prim->conftab = (uint16*)l2cap_conftab;
    prim->response = L2CA_CONNECT_SUCCESS;
    VmSendSdmPrim(prim);
}

void ShadowProfile_ShadowL2capDisconnectReq(l2ca_cid_t cid)
{
    MAKE_SDM_PRIM_T(SDM_SHADOW_L2CAP_DISCONNECT_REQ);

    SHADOW_LOGF("ShadowProfile_ShadowL2capDisconnectReq cid 0x%x", cid);

    prim->phandle = 0;
    prim->cid = cid;
    VmSendSdmPrim(prim);
}

void ShadowProfile_ShadowL2capDisconnectRsp(l2ca_cid_t cid)
{
    MAKE_SDM_PRIM_T(SDM_SHADOW_L2CAP_DISCONNECT_RSP);

    SHADOW_LOGF("ShadowProfile_ShadowL2capDisconnectRsp cid 0x%x", cid);

    prim->cid = cid;
    VmSendSdmPrim(prim);
}


/*
    Functions to handle SDM prims sent by firmware.
*/

/*! \brief Handle SDM_REGISTER_CFM

    This is common to both Primary & Secondary
*/
static void shadowProfile_HandleSdmRegisterCfm(const SDM_REGISTER_CFM_T *cfm)
{
    SHADOW_LOGF("shadowProfile_HandleSdmRegisterCfm result 0x%x", cfm->result);

    assert(SHADOW_PROFILE_STATE_INITALISING == ShadowProfile_GetState());

    if (cfm->result == SDM_RESULT_SUCCESS)
    {
        ShadowProfile_SetState(SHADOW_PROFILE_STATE_DISCONNECTED);
    }
    else
    {
        Panic();
    }
}

/*! \brief Handle SDM_SHADOW_ACL_LINK_CREATE_CFM

    Only Primary should receive this, because the Primary always
    initiates the shadow ACL connection.
*/
static void shadowProfile_HandleSdmAclLinkCreateCfm(const SDM_SHADOW_ACL_LINK_CREATE_CFM_T *cfm)
{
    shadow_profile_task_data_t *sp = ShadowProfile_Get();

    SHADOW_LOGF("shadowProfile_HandleSdmAclLinkCreateCfm status 0x%x handle 0x%x role 0x%x",
                    cfm->status, cfm->connection_handle, cfm->role);

    SHADOW_LOGF("shadowProfile_HandleSdmAclLinkCreateCfm buddy_addr {%04x %02x %06lx}",
                cfm->buddy_bd_addr.addrt.addr.nap,
                cfm->buddy_bd_addr.addrt.addr.uap,
                cfm->buddy_bd_addr.addrt.addr.lap);

    SHADOW_LOGF("shadowProfile_HandleSdmAclLinkCreateCfm shadow_addr {%04x %02x %06lx}",
                cfm->shadow_bd_addr.addrt.addr.nap,
                cfm->shadow_bd_addr.addrt.addr.uap,
                cfm->shadow_bd_addr.addrt.addr.lap);

    assert(ShadowProfile_IsPrimary());

    switch (ShadowProfile_GetState())
    {
    case SHADOW_PROFILE_STATE_ACL_CONNECTING:
        if (cfm->status == HCI_SUCCESS)
        {
            assert(ROLE_TYPE_PRIMARY == cfm->role);

            sp->acl.conn_handle = cfm->connection_handle;
            BdaddrConvertBluestackToVm(&sp->acl.bd_addr, &cfm->shadow_bd_addr.addrt.addr);

            ShadowProfile_SetState(SHADOW_PROFILE_STATE_ACL_CONNECTED);
        }
        else
        {
            /* Re-connect the shadow ACL after entering PEER_CONNECTED */
            ShadowProfile_SetDelayKick();
            ShadowProfile_SetState(SHADOW_PROFILE_STATE_PEER_CONNECTED);
        }
        break;

    default:
        ShadowProfile_StateError(SDM_SHADOW_ACL_LINK_CREATE_CFM, NULL);
        break;
    }
}

/*! \brief Handle SDM_SHADOW_ACL_LINK_CREATE_IND

    Only Secondary should receive this, because the Primary always
    initiates the shadow ACL connection.
*/
static void shadowProfile_HandleSdmAclLinkCreateInd(const SDM_SHADOW_ACL_LINK_CREATE_IND_T *ind)
{
    shadow_profile_task_data_t *sp = ShadowProfile_Get();

    SHADOW_LOGF("shadowProfile_HandleSdmAclLinkCreateInd status 0x%x handle 0x%x",
                    ind->status, ind->connection_handle);

    assert(!ShadowProfile_IsPrimary());

    switch (ShadowProfile_GetState())
    {
    case SHADOW_PROFILE_STATE_DISCONNECTED:
    case SHADOW_PROFILE_STATE_PEER_CONNECTED:
        if (ind->status == HCI_SUCCESS)
        {
            assert(ROLE_TYPE_SECONDARY == ind->role);

            sp->acl.conn_handle = ind->connection_handle;
            BdaddrConvertBluestackToVm(&sp->acl.bd_addr, &ind->shadow_bd_addr.addrt.addr);

            ShadowProfile_SetState(SHADOW_PROFILE_STATE_ACL_CONNECTED);
        }
        break;

    default:
        ShadowProfile_StateError(SDM_SHADOW_ACL_LINK_CREATE_IND, NULL);
        break;
    }
}

/*! \brief Handle SDM_SHADOW_ESCO_LINK_CREATE_CFM

    Only Primary should receive this, because the Primary always
    initiates the shadow eSCO connection.
*/
static void shadowProfile_HandleSdmEscoLinkCreateCfm(const SDM_SHADOW_ESCO_LINK_CREATE_CFM_T *cfm)
{
    shadow_profile_task_data_t *sp = ShadowProfile_Get();

    SHADOW_LOGF("shadowProfile_HandleSdmEscoLinkCreateCfm status 0x%x handle 0x%x",
                    cfm->status, cfm->connection_handle);

    assert(ShadowProfile_IsPrimary());

    switch (ShadowProfile_GetState())
    {
    case SHADOW_PROFILE_STATE_ESCO_CONNECTING:
        if (cfm->status == HCI_SUCCESS)
        {
            assert(ROLE_TYPE_PRIMARY == cfm->role);

            sp->esco.conn_handle = cfm->connection_handle;

            ShadowProfile_SetState(SHADOW_PROFILE_STATE_ESCO_CONNECTED);
        }
        else
        {
            ShadowProfile_SetState(SHADOW_PROFILE_STATE_ACL_CONNECTED);
        }
        break;

    default:
        ShadowProfile_StateError(SDM_SHADOW_ESCO_LINK_CREATE_CFM, NULL);
        break;
    }
}

/*! \brief Handle SDM_SHADOW_ESCO_LINK_CREATE_IND

    Only Secondary should receive this, because the Primary always
    initiates the shadow eSCO connection.
*/
static void shadowProfile_HandleSdmEscoLinkCreateInd(const SDM_SHADOW_ESCO_LINK_CREATE_IND_T *ind)
{
    shadow_profile_task_data_t *sp = ShadowProfile_Get();

    SHADOW_LOGF("shadowProfile_HandleSdmEscoLinkCreateInd status 0x%x handle 0x%x role %d type %d",
                    ind->status, ind->connection_handle, ind->role, ind->link_type);
    SHADOW_LOGF("shadowProfile_HandleSdmEscoLinkCreateInd buddy addr {%04x %02x %06lx}",
                    ind->buddy_bd_addr.addrt.addr.nap,
                    ind->buddy_bd_addr.addrt.addr.uap,
                    ind->buddy_bd_addr.addrt.addr.lap);
    SHADOW_LOGF("shadowProfile_HandleSdmEscoLinkCreateInd shadow addr {%04x %02x %06lx}",
                    ind->shadow_bd_addr.addrt.addr.nap,
                    ind->shadow_bd_addr.addrt.addr.uap,
                    ind->shadow_bd_addr.addrt.addr.lap);
    SHADOW_LOGF("shadowProfile_HandleSdmEscoLinkCreateInd tx_interval %d wesco %d rx_len 0x%x tx_len 0x%x air_mode 0x%x",
                    ind->tx_interval, ind->wesco, ind->rx_packet_length, ind->tx_packet_length, ind->air_mode);

    assert(!ShadowProfile_IsPrimary());

    switch (ShadowProfile_GetState())
    {
    case SHADOW_PROFILE_STATE_ACL_CONNECTED:
        if (ind->status == HCI_SUCCESS)
        {
            assert(ROLE_TYPE_SECONDARY == ind->role);

            sp->esco.conn_handle = ind->connection_handle;
            sp->esco.wesco = ind->wesco;
            sp->esco.sink = StreamScoSink(ind->connection_handle);

            assert(SinkIsValid(sp->esco.sink));

            SHADOW_LOGF("shadowProfile_HandleSdmEscoLinkCreateInd sco_sink 0x%x", sp->esco.sink);

            ShadowProfile_SetState(SHADOW_PROFILE_STATE_ESCO_CONNECTED);
        }
        break;

    default:
        ShadowProfile_StateError(SDM_SHADOW_ACL_LINK_CREATE_IND, NULL);
        break;
    }
}

/*! \brief Handle SDM_SHADOW_ESCO_RENEGOTIATED_IND

    The eSCO parameters have changed but the app is not required to do
    anything in reply as it has been handled by the lower layers.

    Handle it here to keep things tidy.
*/
static void shadowProfile_HandleSdmEscoRenegotiatedInd(const SDM_SHADOW_ESCO_RENEGOTIATED_IND_T *ind)
{
    SHADOW_LOGF("shadowProfile_HandleSdmEscoRenegotiatedInd tx_interval %d wesco %d rx_len 0x%x tx_len 0x%x",
                    ind->tx_interval, ind->wesco, ind->rx_packet_length, ind->tx_packet_length);
}

/*! \brief Handle SDM_SHADOW_LINK_DISCONNECT_CFM

    Only Primary should receive this, in response to Primary sending
    a SDM_SHADOW_LINK_DISCONNECT_REQ.
*/
static void shadowProfile_HandleSdmLinkDisconnectCfm(const SDM_SHADOW_LINK_DISCONNECT_CFM_T *cfm)
{
    shadow_profile_task_data_t *sp = ShadowProfile_Get();

    SHADOW_LOGF("shadowProfile_HandleSdmLinkDisconnectCfm handle 0x%x reason 0x%x type 0x%x role 0x%x",
                cfm->conn_handle, cfm->reason, cfm->link_type, cfm->role);

    assert(ShadowProfile_IsPrimary());

    switch (ShadowProfile_GetState())
    {
    case SHADOW_PROFILE_STATE_ACL_DISCONNECTING:
        if (cfm->link_type == LINK_TYPE_ACL && cfm->conn_handle == sp->acl.conn_handle)
        {
            if (cfm->reason == HCI_ERROR_CONN_TERM_LOCAL_HOST)
            {
                ShadowProfile_SetState(SHADOW_PROFILE_STATE_PEER_CONNECTED);
                /* Reset state after setting state, so state change handling
                can use this state. */
                sp->acl.conn_handle = SHADOW_PROFILE_CONNECTION_HANDLE_INVALID;
                BdaddrSetZero(&sp->acl.bd_addr);
            }
            else
            {
                /* Failure to disconnect typically means the ACL is already
                disconnected and a disconnection indication is in-flight.
                Do nothing and wait for the indication. */
            }
        }
        else
        {
            SHADOW_LOG("shadowProfile_HandleSdmLinkDisconnectCfm Unrecognised shadow link; ignoring");
        }
        break;

    case SHADOW_PROFILE_STATE_ESCO_DISCONNECTING:
        if (cfm->link_type == LINK_TYPE_ESCO && cfm->conn_handle == sp->esco.conn_handle)
        {
            if (cfm->reason == HCI_ERROR_CONN_TERM_LOCAL_HOST)
            {
                ShadowProfile_SetState(SHADOW_PROFILE_STATE_ACL_CONNECTED);
                ShadowProfile_ResetEscoConnectionState();
            }
            else
            {
                /* Failure to disconnect normally means the eSCO is already
                disconnected and a disconnection indication is in-flight.
                Do nothing and wait for the indication. */
            }
        }
        else
        {
            SHADOW_LOG("shadowProfile_HandleSdmLinkDisconnectCfm Unrecognised shadow link; ignoring");
        }
        break;

    default:
        ShadowProfile_StateError(SDM_SHADOW_LINK_DISCONNECT_CFM, NULL);
        break;
    }
}

/*! \brief Handle SDM_SHADOW_LINK_DISCONNECT_IND

    Both Primary & Secondary can get this at any time.
    For example, if there is a link-loss between Primary & Secondary.
*/
static void shadowProfile_HandleSdmLinkDisconnectInd(const SDM_SHADOW_LINK_DISCONNECT_IND_T *ind)
{
    shadow_profile_task_data_t *sp = ShadowProfile_Get();

    SHADOW_LOGF("shadowProfile_HandleSdmLinkDisconnectInd handle 0x%x reason 0x%x type 0x%x role 0x%x",
                ind->conn_handle, ind->reason, ind->link_type, ind->role);

    switch (ShadowProfile_GetState())
    {
    case SHADOW_PROFILE_STATE_PEER_CONNECTED:
        /* All shadow links should be disconnected anyway, but we may still get
           a DISCONNECT_IND after DISCONNECT_CFM for the same handle.
           It happens when the handset disconnects; then Primary sends a
           DISCONNECT_REQ; then there is a race between that and a disconnect
           caused by a SDM timeout to the Secondary and/or the Handset. */
        break;

    case SHADOW_PROFILE_STATE_ACL_CONNECTED:
    case SHADOW_PROFILE_STATE_ESCO_CONNECTING:
        if (ind->link_type == LINK_TYPE_ACL
            && ind->conn_handle == sp->acl.conn_handle)
        {
            /* \todo If this is a link-loss, should we keep acl.conn_handle? */
            sp->acl.conn_handle = SHADOW_PROFILE_CONNECTION_HANDLE_INVALID;

            /* If we are the Primary, we should retry creating the shadow ACL connection */
            if (ShadowProfile_IsPrimary() && ind->reason == HCI_ERROR_CONN_TIMEOUT)
            {
                /* Re-connect the shadow ACL after entering PEER_CONNECTED */
                ShadowProfile_SetDelayKick();
            }

            ShadowProfile_SetState(SHADOW_PROFILE_STATE_PEER_CONNECTED);
        }
        else
        {
            SHADOW_LOG("shadowProfile_HandleSdmLinkDisconnectInd Unrecognised shadow link; ignoring");
        }
        break;

    case SHADOW_PROFILE_STATE_ESCO_CONNECTED:
    case SHADOW_PROFILE_STATE_ESCO_DISCONNECTING:
        if (ind->link_type == LINK_TYPE_ESCO
            && ind->conn_handle == sp->esco.conn_handle)
        {
            ShadowProfile_ResetEscoConnectionState();
            ShadowProfile_SetState(SHADOW_PROFILE_STATE_ACL_CONNECTED);
        }
        else
        {
            SHADOW_LOG("shadowProfile_HandleSdmLinkDisconnectInd Unrecognised shadow link; ignoring");
        }
        break;

    default:
        ShadowProfile_StateError(SDM_SHADOW_LINK_DISCONNECT_IND, NULL);
        break;
    }
}

/*! \brief Handle SDM_SHADOW_L2CAP_CREATE_IND.
*/
static void shadowProfile_HandleSdmShadowL2capCreateInd(const SDM_SHADOW_L2CAP_CREATE_IND_T *ind)
{
    SHADOW_LOGF("shadowProfile_HandleSdmShadowL2capCreateInd cid 0x%x handle 0x%x",
                ind->cid, ind->connection_handle);

    switch (ShadowProfile_GetState())
    {
        case SHADOW_PROFILE_STATE_ACL_CONNECTED:
        {
            ShadowProfile_GetA2dpState()->cid = ind->cid;
            PanicFalse(ind->connection_handle == ShadowProfile_GetAclState()->conn_handle);
            ShadowProfile_SetState(SHADOW_PROFILE_STATE_A2DP_CONNECTING);
        }
        break;
        default:
            ShadowProfile_StateError(SDM_SHADOW_L2CAP_CREATE_IND, NULL);
        break;
    }
}

/*! \brief Handle SDM_SHADOW_L2CAP_CREATE_CFM.
*/
static void shadowProfile_HandleSdmShadowL2capCreateCfm(const SDM_SHADOW_L2CAP_CREATE_CFM_T *cfm)
{
    SHADOW_LOGF("shadowProfile_HandleSdmShadowL2capCreateCfm cid 0x%x handle 0x%x result 0x%x",
                cfm->cid, cfm->connection_handle, cfm->result);

    switch (ShadowProfile_GetState())
    {
        case SHADOW_PROFILE_STATE_A2DP_CONNECTING:
        {
            if (cfm->result == L2CA_CONNECT_SUCCESS)
            {
                PanicFalse(cfm->cid == ShadowProfile_GetA2dpState()->cid);
                PanicFalse(cfm->connection_handle == ShadowProfile_GetAclState()->conn_handle);
                ShadowProfile_SetState(SHADOW_PROFILE_STATE_A2DP_CONNECTED);
            }
            else
            {
                ShadowProfile_SetDelayKick();
                ShadowProfile_SetState(SHADOW_PROFILE_STATE_ACL_CONNECTED);
            }
        }
        break;
        default:
            ShadowProfile_StateError(SDM_SHADOW_L2CAP_CREATE_CFM, NULL);
        break;
    }
}

/*! \brief Handle SDM_SHADOW_L2CAP_DISCONNECT_IND.
*/
static void shadowProfile_HandleSdmShadowL2capDisconnectInd(const SDM_SHADOW_L2CAP_DISCONNECT_IND_T *ind)
{
    SHADOW_LOGF("shadowProfile_HandleSdmShadowL2capDisconnectInd cid 0x%x", ind->cid);

    switch (ShadowProfile_GetState())
    {
        case SHADOW_PROFILE_STATE_A2DP_CONNECTED:
        {
            PanicFalse(ind->cid == ShadowProfile_GetA2dpState()->cid);
            ShadowProfile_SetState(SHADOW_PROFILE_STATE_A2DP_DISCONNECTING);
        }
        break;
        default:
            ShadowProfile_StateError(SDM_SHADOW_L2CAP_DISCONNECT_IND, NULL);
        break;
    }
}

/*! \brief Handle SDM_SHADOW_L2CAP_DISCONNECT_CFM.
*/
static void shadowProfile_HandleSdmShadowL2capDisconnectCfm(const SDM_SHADOW_L2CAP_DISCONNECT_CFM_T *cfm)
{
    SHADOW_LOGF("shadowProfile_HandleSdmShadowL2capDisconnectCfm cid 0x%x, result 0x%x",
                cfm->cid, cfm->result);

    assert(ShadowProfile_IsPrimary());

    switch (ShadowProfile_GetState())
    {
        case SHADOW_PROFILE_STATE_A2DP_DISCONNECTING:
        {
            PanicFalse(cfm->cid == ShadowProfile_GetA2dpState()->cid);
            ShadowProfile_SetState(SHADOW_PROFILE_STATE_ACL_CONNECTED);
        }
        break;
        default:
            ShadowProfile_StateError(SDM_SHADOW_L2CAP_DISCONNECT_CFM, NULL);
        break;
    }
}


/*! \brief Handle MESSAGE_BLUESTACK_SDM_PRIM payloads. */
void ShadowProfile_HandleMessageBluestackSdmPrim(const SDM_UPRIM_T *uprim)
{
    switch (uprim->type)
    {
    case SDM_REGISTER_CFM:
        shadowProfile_HandleSdmRegisterCfm((const SDM_REGISTER_CFM_T *)uprim);
        break;

    case SDM_SHADOW_ACL_LINK_CREATE_CFM:
        shadowProfile_HandleSdmAclLinkCreateCfm((const SDM_SHADOW_ACL_LINK_CREATE_CFM_T *)uprim);
        break;

    case SDM_SHADOW_ACL_LINK_CREATE_IND:
        shadowProfile_HandleSdmAclLinkCreateInd((const SDM_SHADOW_ACL_LINK_CREATE_IND_T *)uprim);
        break;

    case SDM_SHADOW_ESCO_LINK_CREATE_CFM:
        shadowProfile_HandleSdmEscoLinkCreateCfm((const SDM_SHADOW_ESCO_LINK_CREATE_CFM_T *)uprim);
        break;

    case SDM_SHADOW_ESCO_LINK_CREATE_IND:
        shadowProfile_HandleSdmEscoLinkCreateInd((const SDM_SHADOW_ESCO_LINK_CREATE_IND_T *)uprim);
        break;

    case SDM_SHADOW_LINK_DISCONNECT_CFM:
        shadowProfile_HandleSdmLinkDisconnectCfm((const SDM_SHADOW_LINK_DISCONNECT_CFM_T *)uprim);
        break;

    case SDM_SHADOW_LINK_DISCONNECT_IND:
        shadowProfile_HandleSdmLinkDisconnectInd((const SDM_SHADOW_LINK_DISCONNECT_IND_T *)uprim);
        break;

    case SDM_SHADOW_ESCO_RENEGOTIATED_IND:
        shadowProfile_HandleSdmEscoRenegotiatedInd((const SDM_SHADOW_ESCO_RENEGOTIATED_IND_T *)uprim);
        break;

    case SDM_SHADOW_L2CAP_CREATE_IND:
        shadowProfile_HandleSdmShadowL2capCreateInd((const SDM_SHADOW_L2CAP_CREATE_IND_T *)uprim);
        break;

    case SDM_SHADOW_L2CAP_CREATE_CFM:
        shadowProfile_HandleSdmShadowL2capCreateCfm((const SDM_SHADOW_L2CAP_CREATE_CFM_T *)uprim);
        break;

    case SDM_SHADOW_L2CAP_DISCONNECT_IND:
        shadowProfile_HandleSdmShadowL2capDisconnectInd((const SDM_SHADOW_L2CAP_DISCONNECT_IND_T *)uprim);
        break;

    case SDM_SHADOW_L2CAP_DISCONNECT_CFM:
        shadowProfile_HandleSdmShadowL2capDisconnectCfm((const SDM_SHADOW_L2CAP_DISCONNECT_CFM_T *)uprim);
        break;

    default:
        SHADOW_LOGF("ShadowProfile_HandleMessageBluestackSdmPrim: Unhandled SDM prim 0x%x", uprim->type);
        break;
    }
}

#endif /* INCLUDE_SHADOWING */
