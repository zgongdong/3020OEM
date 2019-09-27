/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       handset_service_handover.c
\brief      Handset Service handover related interfaces

*/

#include <bdaddr.h>
#include <logging.h>
#include <panic.h>
#include <app_handover_if.h>
#include <device_properties.h>
#include <device_list.h>
#include "handset_service_sm.h"
#include "handset_service_protected.h"
/******************************************************************************
 * Local Function Prototypes
 ******************************************************************************/
static bool handsetService_Veto(void);

static bool handsetService_Marshal(const bdaddr *bd_addr, 
                                    marshal_type_t type,
                                    void **marshal_obj);

static void handsetService_Unmarshal(const bdaddr *bd_addr, 
                                      marshal_type_t type,
                                      void *unmarshal_obj);

static void handsetService_Commit(bool is_primary);

/******************************************************************************
 * Global Declarations
 ******************************************************************************/
REGISTER_HANDOVER_INTERFACE(HANDSET_SERVICE, NULL, handsetService_Veto, handsetService_Marshal, handsetService_Unmarshal, handsetService_Commit);

/******************************************************************************
 * Local Function Definitions
 ******************************************************************************/
/*! 
    \brief Handle Veto check during handover
           TRUE is returned if handset service is not in CONNECTED state.
    \return bool
*/
static bool handsetService_Veto(void)
{
    bool i_veto = FALSE;
    handset_service_state_machine_t *sm = HandsetService_GetSm();
    if(sm && (sm->state != HANDSET_SERVICE_STATE_CONNECTED))
    {
        i_veto = TRUE;
        DEBUG_LOG("handsetService_veto returns true");
    }

    return i_veto;
}

/*!
    \brief The function shall set marshal_obj to the address of the object to 
           be marshalled.

    \param[in] bd_addr      Bluetooth address of the link to be marshalled
                            \ref bdaddr
    \param[in] type         Type of the data to be marshalled \ref marshal_type_t
    \param[out] marshal_obj Holds address of data to be marshalled.
    \return TRUE: Required data has been copied to the marshal_obj.
            FALSE: No data is required to be marshalled. marshal_obj is set to NULL.

*/
static bool handsetService_Marshal(const bdaddr *bd_addr, 
                                    marshal_type_t type, 
                                    void **marshal_obj)
{
    UNUSED(bd_addr);
    UNUSED(type);
    *marshal_obj = NULL;
    /* Nothing to be marshalled for handset service */
    return FALSE;
}

/*! 
    \brief The function shall copy the unmarshal_obj associated to specific 
            marshal type \ref marshal_type_t

    \param[in] bd_addr      Bluetooth address of the link to be unmarshalled
                            \ref bdaddr
    \param[in] type         Type of the unmarshalled data \ref marshal_type_t
    \param[in] unmarshal_obj Address of the unmarshalled object.

*/
static void handsetService_Unmarshal(const bdaddr *bd_addr, 
                                      marshal_type_t type, 
                                      void *unmarshal_obj)
{
    UNUSED(bd_addr);
    UNUSED(type);
    UNUSED(unmarshal_obj);
    /* Nothing to be unmarshalled for handset service */
}

/*!
    \brief Component commits to the specified role

    The component should take any actions necessary to commit to the
    new role.

    \param[in] is_primary   TRUE if device role is primary, else secondary

*/
static void handsetService_Commit(bool is_primary)
{
    handset_service_state_machine_t *sm = HandsetService_GetSm();
    PanicNull(sm);

    if(is_primary)
    {
        bool mru = TRUE;
        HandsetServiceSm_Init(sm);
        device_t mru_device = DeviceList_GetFirstDeviceWithPropertyValue(device_property_mru, &mru, sizeof(uint8));
        /* Panic if there is no device. This could happen if BT Device list is not restored on secondary. */
        PanicNull(mru_device);
        sm->handset_device = mru_device;
        sm->state = HANDSET_SERVICE_STATE_CONNECTED;
    }
    else
    {
        HandsetServiceSm_DeInit(sm);
    }
}

