/*!
\copyright  Copyright (c) 2008 - 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Link policy manager for TWS shadowing builds.

When shadowing, the topology is fixed and role switches are not desirable
for any device in the topology.

The primary earbud is master of the link to the secondary earbud, and the roles
only switch when the devices perform handover. These roles are enforced by a call
to #appLinkPolicyAlwaysMaster in tws topology.

The handset is master of the link to the primary earbud. Shadowing is currently
only supported when the primary earbud is ACL slave. Since being ACL master is
preferable (from link scheduling point of view) compared to being slave, there
is no reason for the primary earbud to request a role switch (once slave) and no
good reason for the handset to request a role switch to become slave. Therefore,
calls to allow/prohibit or change roles (that were valid in non-shadowing
topology) are ignored here.
*/

#include "link_policy.h"
#include "logging.h"
#include "bdaddr.h"
#include "bt_device.h"
#include "app/bluestack/dm_prim.h"
#include "panic.h"

#ifdef INCLUDE_SHADOWING

/*! Make and populate a bluestack DM primitive based on the type.

    \note that this is a multiline macro so should not be used after a
    control statement (if, while) without the use of braces
 */
#define MAKE_PRIM_C(TYPE) MESSAGE_MAKE(prim,TYPE##_T); prim->common.op_code = TYPE; prim->common.length = sizeof(TYPE##_T);

void appLinkPolicyAllowRoleSwitch(const bdaddr *bd_addr)
{
    UNUSED(bd_addr);
    DEBUG_LOG("appLinkPolicyAllowRoleSwitch ignored when shadowing");
}

void appLinkPolicyAllowRoleSwitchForSink(Sink sink)
{
    UNUSED(sink);
    DEBUG_LOG("appLinkPolicyAllowRoleSwitchForSink ignored when shadowing");
}

void appLinkPolicyPreventRoleSwitch(const bdaddr *bd_addr)
{
    UNUSED(bd_addr);
    DEBUG_LOG("appLinkPolicyPreventRoleSwitch ignored when shadowing");
}

void appLinkPolicyPreventRoleSwitchForSink(Sink sink)
{
    UNUSED(sink);
    DEBUG_LOG("appLinkPolicyPreventRoleSwitchForSink ignored when shadowing");
}

void appLinkPolicyUpdateRoleFromSink(Sink sink)
{
    UNUSED(sink);
    DEBUG_LOG("appLinkPolicyUpdateRoleFromSink ignored when shadowing");
}

/*! \brief Send a prim directly to bluestack to role switch to slave.
    \param The address of remote device link.
*/
static void role_switch_to_slave(const bdaddr *bd_addr)
{
    MAKE_PRIM_C(DM_HCI_SWITCH_ROLE_REQ);
    BdaddrConvertVmToBluestack(&prim->bd_addr, bd_addr);
    prim->role = HCI_SLAVE;
    VmSendDmPrim(prim);
}

/*! \brief Prevent role switching

    Update the policy for the connection (if any) to the specified
    Bluetooth address, so as to prevent any future role switching.

    \param  bd_addr The Bluetooth address of the device
*/
static void prevent_role_switch(const bdaddr *bd_addr)
{
    MAKE_PRIM_C(DM_HCI_WRITE_LINK_POLICY_SETTINGS_REQ);
    BdaddrConvertVmToBluestack(&prim->bd_addr, bd_addr);
    prim->link_policy_settings = ENABLE_SNIFF;
    VmSendDmPrim(prim);
}

void appLinkPolicyHandleClDmAclOpendedIndication(const CL_DM_ACL_OPENED_IND_T *ind)
{
    const bool is_local = !!(~ind->flags & DM_ACL_FLAG_INCOMING);
    const bool is_ble = !!(ind->flags & DM_ACL_FLAG_ULP);

    /* Set default link supervision timeout if locally initiated (i.e. we're master) */
    if (is_local)
    {
        const bdaddr *bd_addr = &ind->bd_addr.addr;

        /* For shadowing, the primary earbud must always role switch to be slave
           of the ACL link to the handset. */
        if (!is_ble && appDeviceIsHandset(bd_addr))
        {
            role_switch_to_slave(bd_addr);
        }

        appLinkPolicyUpdateLinkSupervisionTimeout(bd_addr);
   }
}

/*! \brief Indication of link role

    This function is called to handle a CL_DM_ROLE_IND message, this message is sent from the
    connection library whenever the role of a link changes.

    \param  ind The received indication
*/
static void appLinkPolicyHandleClDmRoleIndication(const CL_DM_ROLE_IND_T *ind)
{
    if (ind->status == hci_success)
    {
        const bdaddr *bd_addr = &(ind->bd_addr);
        const bool is_master = (ind->role == hci_role_master);
        const bool is_handset = appDeviceIsHandset(bd_addr);

        if (is_handset)
        {
            if (is_master)
            {
                DEBUG_LOG("appLinkPolicyHandleClDmRoleIndication, master of handset");
                /* This is not the desired topology, something has gone wrong */
                /* Try panicing, see if this ever fires */
                Panic();
            }
            else
            {
                DEBUG_LOG("appLinkPolicyHandleClDmRoleIndication, slave of handset");
                /* This is the desired topology, disallow role switches */
                prevent_role_switch(bd_addr);
            }
        }
        else
        {
            if (is_master)
            {
                DEBUG_LOG("appLinkPolicyHandleClDmRoleIndication, master of peer");
            }
            else
            {
                DEBUG_LOG("appLinkPolicyHandleClDmRoleIndication, slave of peer");
            }
        }
    }
    else
    {
        /* Handset has refused to become slave, shadowing cannot operate in this
           mode, this needs to be investigated. */
        Panic();
    }
}

bool appLinkPolicyHandleConnectionLibraryMessages(MessageId id, Message message, bool already_handled)
{
    switch (id)
    {
        case CL_DM_ROLE_CFM:
            /* Role switches should not be performed using CL when shadowing */
            Panic();
        return TRUE;

        case CL_DM_ROLE_IND:
            appLinkPolicyHandleClDmRoleIndication(message);
            return TRUE;
    }
    return already_handled;
}

#endif
