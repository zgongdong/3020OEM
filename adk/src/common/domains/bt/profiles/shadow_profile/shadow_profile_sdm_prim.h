/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Functions to send & process SDM prims to / from the firmware.
*/

#ifndef SHADOW_PROFILE_SDM_PRIM_H_
#define SHADOW_PROFILE_SDM_PRIM_H_

#ifdef INCLUDE_SHADOWING

#include <app/bluestack/sdm_prim.h>
#include <hfp.h>


/*! \brief Register with the shadowing (SDM) sub-system.

    The firmware will reply with a SDM_REGISTER_CFM sent to the task
    registered with MessageSdmTask().
*/
void ShadowProfile_ShadowRegisterReq(void);

/*! \brief Create a shadow ACL or eSCO link from the Primary

    Only the Primary can send this message. The Secondary must leave the
    creation of shadow ACL/eSCO links to the Primary.

    The handset bdaddr is the addr of the link to shadow.
    The peer earbud is the addr to use as the Secondary.

    \param type The type of shadow link to request; either ACL or eSCO.
*/
void ShadowProfile_ShadowConnectReq(link_type_t type);

/*! \brief Disconnect a shadow ACL or eSCO link from the Primary.

    Only the Primary can send this message. The Secondary must leave the
    disconnect of shadow ACL/eSCO links to the Primary.

    \param conn_handle The shadow connection handle to disconnect.
    \param reason The reason for the disconnect.
*/
void ShadowProfile_ShadowDisconnectReq(hci_connection_handle_t conn_handle, hci_reason_t reason);


/*! \brief Connect a shadow L2CAP.

    Only the Primary can send this message. The Secondary must leave the
    creation of shadow ACL/eSCO links to the Primary.
    This function sends a SDM_SHADOW_L2CAP_CREATE_REQ.

    \param conn_handle The connection handle of the shadow ACL.
    \param cid The L2CAP CID (from primary to handset) to shadow.
*/
void ShadowProfile_ShadowL2capConnectReq(hci_connection_handle_t conn_handle,
                                         l2ca_cid_t cid);

/*! \brief Respond to a SDM_SHADOW_L2CAP_CREATE_IND.

    Only the secondary should call this.
    This function sends a SDM_SHADOW_L2CAP_CREATE_RSP.

    \param conn_handle The connection handle of the shadow ACL.
    \param cid The shadow L2CAP CID.
*/
void ShadowProfile_ShadowL2capConnectRsp(hci_connection_handle_t conn_handle,
                                         l2ca_cid_t cid);

/*! \brief Disconnect a shadow L2CAP.

    Only the Primary should call this.
    This function sends a SDM_SHADOW_L2CAP_DISCONNECT_REQ.

    \param cid The shadow L2CAP CID.
*/
void ShadowProfile_ShadowL2capDisconnectReq(l2ca_cid_t cid);

/*! \brief Respond to a SDM_SHADOW_L2CAP_DISCONNECT_IND.

    Only the secondary should call this.
    This function sends a SDM_SHADOW_L2CAP_DISCONNECT_RSP.

    \param cid The shadow L2CAP CID.
*/
void ShadowProfile_ShadowL2capDisconnectRsp(l2ca_cid_t cid);

/*! \brief Handle MESSAGE_BLUESTACK_SDM_PRIM payloads sent from firmware.

    #uprim is a union of all the SDM prim types and this function determines
    which SDM prim it is and passes it onto the relevant handler.

    \param uprim The SDM prim payload to be processed.
*/
void ShadowProfile_HandleMessageBluestackSdmPrim(const SDM_UPRIM_T *uprim);

#endif /* INCLUDE_SHADOWING */

#endif /* SHADOW_PROFILE_SDM_PRIM_H_ */
