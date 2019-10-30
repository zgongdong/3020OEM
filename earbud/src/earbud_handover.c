/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       earbud_handover.c
\brief      This module implements handover interface (handover_interface_t) and
            acts as an aggregator for all application components which require
            handover.
*/
#ifdef INCLUDE_SHADOWING
#include <earbud_handover.h>
#include <app_handover_if.h>
#include <handover_if.h>
#include <handover_profile.h>
#include <domain_marshal_types.h>
#include <service_marshal_types.h>
#include <marshal_common.h>
#include <avrcp.h>
#include <a2dp.h>
#include <hfp.h>
#include <bdaddr_.h>
#include <panic.h>
#include <marshal.h>
#include <logging.h>

#define EB_HANDOVER_DEBUG_VERBOSE_LOG DEBUG_LOG

#define EXPAND_AS_TYPE_DEFINITION(type) (const marshal_type_descriptor_t *)&marshal_type_descriptor_##type,
const marshal_type_descriptor_t * const  mtd_handover_app[] = {
    MARSHAL_COMMON_TYPES_TABLE(EXPAND_AS_TYPE_DEFINITION)
    AV_MARSHAL_TYPES_TABLE(EXPAND_AS_TYPE_DEFINITION)
    A2DP_MARSHAL_TYPES_TABLE(EXPAND_AS_TYPE_DEFINITION)
    AVRCP_MARSHAL_TYPES_TABLE(EXPAND_AS_TYPE_DEFINITION)
    CONNECTION_MANAGER_LIST_MARSHAL_TYPES_TABLE(EXPAND_AS_TYPE_DEFINITION)
    HFP_PROFILE_MARSHAL_TYPES_TABLE(EXPAND_AS_TYPE_DEFINITION)
    BT_DEVICE_MARSHAL_TYPES_TABLE(EXPAND_AS_TYPE_DEFINITION)
    STATE_PROXY_MARSHAL_TYPES_TABLE(EXPAND_AS_TYPE_DEFINITION)
};

/******************************************************************************
 * Local Function Prototypes
 ******************************************************************************/

/* handover interface static function declarations */
static bool earbudHandover_Veto(void);
static bool earbudHandover_Marshal(const tp_bdaddr *addr, uint8 *buffer, uint16 buffer_size, uint16 *written);
static bool earbudHandover_Unmarshal(const tp_bdaddr *addr, const uint8 *buffer, uint16 buffer_size, uint16 *consumed);
static void earbudHandover_Commit(const bool role);
static void earbudHandover_Abort(void);
static const registered_handover_interface_t * getNextInterface(void);

/******************************************************************************
 * Local Structure Definitions
 ******************************************************************************/

/*! \brief Handover context maintains the handover state for the application */
typedef struct {
    /* Marshaling State */
    enum {
        MARSHAL_STATE_UNINITIALIZED,
        MARSHAL_STATE_INITIALIZED,
        MARSHAL_STATE_MARSHALING,
        MARSHAL_STATE_UNMARSHALING
    } marshal_state;

    /* Marshaller/Unmarshaller instance */
    union {
        marshaller_t marshaller;
        unmarshaller_t unmarshaller;
    } marshal;

    /* List of handover interfaces registered by application components */
    const registered_handover_interface_t *interfaces;
    /* Size of registered handover interface list */
    uint8 interfaces_len;
    /* Pointer to the interface currently in use for marshaling or unmarshaling */
    const registered_handover_interface_t *curr_interface;
    /* Index of current marshal type undergoing marshalling */
    uint8 *curr_type;
} handover_app_context_t;

/******************************************************************************
 * Local Declarations
 ******************************************************************************/

/* Handover interface */
const handover_interface application_handover_interface =
{
    &earbudHandover_Veto,
    &earbudHandover_Marshal,
    &earbudHandover_Unmarshal,
    &earbudHandover_Commit,
    &earbudHandover_Abort
};

/* Handover interfaces for all P1 components */
const handover_interface * p1_ho_interfaces[] = {
    &connection_handover_if,
    &a2dp_handover_if,
    &avrcp_handover,
    &hfp_handover_if,
    &application_handover_interface
};

/* Application context instance */
static handover_app_context_t handover_app;
#define earbudHandover_Get() (&handover_app)

/******************************************************************************
 * Local Function Definitions
 ******************************************************************************/

/*! \brief Find the registered interface which handles a given marshal type.
    Only used during unmarshaling procedure.
    \param[in] type Marshal type for which handover interface is to be searched.
    \returns Pointer to handover interface (if found), or NULL.
             Refer \ref registered_handover_interface_t.
*/
static const registered_handover_interface_t * earbudHandover_GetInterfaceForType(uint8 type)
{
    handover_app_context_t * app_data = earbudHandover_Get();
    const registered_handover_interface_t * curr_inf = NULL;
    /* For all registered interfaces */
    for(curr_inf = app_data->interfaces;
        curr_inf && (curr_inf < (app_data->interfaces + app_data->interfaces_len));
        curr_inf++)
    {
        if (curr_inf->type_list)
        {
            uint8 *curr_type;
            /* For all types in current interface's type_list */
            for(curr_type = curr_inf->type_list->types;
                curr_type < (curr_inf->type_list->types + curr_inf->type_list->list_size);
                curr_type++)
            {
                if(*curr_type == type)
                {
                    return curr_inf;
                }
            }
        }
    }

    return NULL;
}

/*! \brief Destroy marshaller/unmarshaller instances, if any. */
static void earbudHandover_DeinitializeMarshaller(void)
{
    handover_app_context_t * app_data = earbudHandover_Get();
    DEBUG_LOG("earbudHandover_DeinitializeMarshaller");
    if(app_data->marshal_state == MARSHAL_STATE_MARSHALING)
    {
        if(app_data->marshal.marshaller)
            MarshalDestroy(app_data->marshal.marshaller, FALSE);
        app_data->marshal.unmarshaller = NULL;
    }

    if(app_data->marshal_state == MARSHAL_STATE_UNMARSHALING)
    {
        if(app_data->marshal.unmarshaller)
            UnmarshalDestroy(app_data->marshal.unmarshaller, FALSE);
        app_data->marshal.unmarshaller = NULL;
    }

    app_data->marshal_state = MARSHAL_STATE_INITIALIZED;
}

/*! \brief Perform veto on all registered components.
    \returns TRUE if any registerd component vetos, FALSE otherwise.
*/
static bool earbudHandover_Veto(void)
{
    handover_app_context_t * app_data = earbudHandover_Get();
    const registered_handover_interface_t *curr_inf = NULL;
    bool veto = FALSE;
    PanicFalse(app_data->marshal_state == MARSHAL_STATE_INITIALIZED);
    DEBUG_LOG("earbudHandover_Veto");

    for(curr_inf = app_data->interfaces;
        (veto == FALSE) && (curr_inf < (app_data->interfaces + app_data->interfaces_len));
        curr_inf++)
    {
        veto = curr_inf->Veto();
    }

    return veto;
}

/*Get the next registered interface which has non Null Marshal TypeList*/
static const registered_handover_interface_t * getNextInterface(void)
{
    handover_app_context_t * app_data = earbudHandover_Get();
    const registered_handover_interface_t *curr_inf = NULL;
    const registered_handover_interface_t *start_inf = app_data->interfaces;

    if(app_data->curr_interface)
    {
        /*Start from next to current interface*/
        start_inf = app_data->curr_interface + 1;
    }


    for(curr_inf = start_inf;
        curr_inf < (app_data->interfaces + app_data->interfaces_len);
        curr_inf++)
    {
        /*We only need interfaces which have valid Marshal Types.*/
        if(curr_inf->type_list)
            return curr_inf;
    }

    return NULL;
}

/*! \brief Commit the roles on all registered components.
    \param[in] role TRUE if primary, FALSE otherwise
*/
static void earbudHandover_Commit(const bool role)
{
    handover_app_context_t * app_data = earbudHandover_Get();
    DEBUG_LOG("earbudHandover_Commit");

    const const registered_handover_interface_t *curr_inf = app_data->interfaces;
    for(curr_inf = app_data->interfaces;
        curr_inf && (curr_inf < (app_data->interfaces + app_data->interfaces_len));
        curr_inf++)
    {
        curr_inf->Commit(role);
    }

    /* Commit is the final interface invoked during handover. Cleanup now. */
    earbudHandover_DeinitializeMarshaller();
}

/*! \brief Handover application's marshaling interface.

    \note Possible cases:
    1. written is not incremented and return value is FALSE: this means buffer is insufficient.
    2. written < buffer_size and return value is FALSE. This means buffer is insufficient.
    3. written <= buffer_size and return value is TRUE. This mean marshaling is complete.
    4. written is not incremented and return value is TRUE. This means marshaling not required.

    \param[in] addr address of handset.
    \param[in] buffer input buffer with data to be marshalled.
    \param[in] buffer_size size of input buffer.
    \param[out] written number of bytes consumed during marshalling.
    \returns TRUE, if marshal types from all registered interfaces have been marshalled
                  successfully. In this case we destory the marshaling instance and set
                  the state back to MARSHAL_STATE_INITIALIZED.
             FALSE, if the buffer is insufficient for marshalling.
 */
static bool earbudHandover_Marshal(const tp_bdaddr *addr, uint8 *buffer, uint16 buffer_size, uint16 *written)
{
    handover_app_context_t * app_data = earbudHandover_Get();
    bool marshalled = TRUE;
    void *data = NULL;

    PanicFalse(addr);
    PanicFalse(buffer);
    PanicFalse(written);
    PanicFalse(app_data->marshal_state == MARSHAL_STATE_INITIALIZED ||
               app_data->marshal_state == MARSHAL_STATE_MARSHALING);

    DEBUG_LOG("earbudHandover_Marshal");
    /* Create marshaller if not done yet */
    if(app_data->marshal_state == MARSHAL_STATE_INITIALIZED)
    {
        app_data->curr_interface = NULL;
        app_data->curr_interface = getNextInterface();
        if(app_data->curr_interface)
        {
            app_data->marshal_state = MARSHAL_STATE_MARSHALING;
            app_data->marshal.marshaller = MarshalInit(mtd_handover_app, NUMBER_OF_SERVICE_MARSHAL_OBJECT_TYPES);
            PanicFalse(app_data->marshal.marshaller);
            app_data->curr_type = app_data->curr_interface->type_list->types;
        }
        else
        {
            /* No marshal types registered, return success */
            return TRUE;
        }
    }

    MarshalSetBuffer(app_data->marshal.marshaller, buffer, buffer_size);
    /* For all remaining registered interfaces. */
    while(marshalled && (app_data->curr_interface))
    {
        /* For all remaining types in current interface. */
        while(app_data->curr_type < (app_data->curr_interface->type_list->types + app_data->curr_interface->type_list->list_size))
        {
            if(app_data->curr_interface->Marshal(&addr->taddr.addr, *app_data->curr_type, &data))
            {
                if(Marshal(app_data->marshal.marshaller, data, *app_data->curr_type))
                {
                    EB_HANDOVER_DEBUG_VERBOSE_LOG("earbudHandover_Marshal - Marshalling successfull for type: %d", *app_data->curr_type);
                }
                else
                {
                    /* Insufficient buffer for marshalling. */
                    DEBUG_LOG("earbudHandover_Marshal - Insufficient buffer for type: %d!", *app_data->curr_type);
                    marshalled = FALSE;
                    break;
                }
            }
            else
            {
                /* Nothing to marshal for this type. Continue ahead. */
            }

            /* Move to next Marshal type for current interface */
            app_data->curr_type++;
        }

        if(marshalled)
        {
            app_data->curr_interface = getNextInterface();
            if (app_data->curr_interface)
            {
                /* All types for current interface marshalled.
                 * Re-initialize app_data->curr_type, from next interface.
                 */
                app_data->curr_type = app_data->curr_interface->type_list->types;
            }
        }
    }

    *written = MarshalProduced(app_data->marshal.marshaller);
    return marshalled;
}

/*! \brief Handover application's unmarshaling interface.

    \note Possible cases,
    1. consumed is not incremented and return value is FALSE. This means buffer is
    insufficient to unmarshal.
    2. consumed < buffer_size and return value is FALSE. Need more data from caller.
    3. consumed == buffer_size and return value is TRUE. This means all data
      unmarshalled successfully. There could still be more data with caller
      in which case this function is invoked again.

    \param[in] addr address of handset.
    \param[in] buffer input buffer with data to be unmarshalled.
    \param[in] buffer_size size of input buffer.
    \param[out] consumed number of bytes consumed during unmarshalling.
    \returns TRUE, if all types have been successfully unmarshalled.
             FALSE, if buffer is insufficient for unmarshalling.
 */
static bool earbudHandover_Unmarshal(const tp_bdaddr *addr, const uint8 *buffer, uint16 buffer_size, uint16 *consumed)
{
    handover_app_context_t * app_data = earbudHandover_Get();
    bool unmarshalled = TRUE;
    marshal_type_t type;
    void * data = NULL;

    PanicFalse(buffer);
    PanicFalse(buffer_size);
    PanicFalse(consumed);
    PanicFalse(app_data->marshal_state == MARSHAL_STATE_INITIALIZED ||
               app_data->marshal_state == MARSHAL_STATE_UNMARSHALING);

    DEBUG_LOG("earbudHandover_Unmarshal");
    /* Create unmarshaller on first call */
    if(app_data->marshal_state == MARSHAL_STATE_INITIALIZED)
    {
        /* Unexpected unmarshal call, as no type handlers exist */
        PanicFalse(getNextInterface());

        app_data->marshal_state = MARSHAL_STATE_UNMARSHALING;
        app_data->marshal.unmarshaller = UnmarshalInit(mtd_handover_app, NUMBER_OF_SERVICE_MARSHAL_OBJECT_TYPES);
        PanicNull(app_data->marshal.unmarshaller);
    }

    UnmarshalSetBuffer(app_data->marshal.unmarshaller, buffer, buffer_size);

    /* Loop until all types are extracted from buffer */
    while(unmarshalled && (*consumed < buffer_size))
    {
        if(Unmarshal(app_data->marshal.unmarshaller, &data, &type))
        {
            const registered_handover_interface_t * curr_inf = earbudHandover_GetInterfaceForType(type);
            /* Could not find interface for marshal_type */
            if(curr_inf == NULL)
            {
                DEBUG_LOG("earbudHandover_Unmarshal - Could not find interface for type: %d", type);
                Panic();
            }
            else
            {
                EB_HANDOVER_DEBUG_VERBOSE_LOG("earbudHandover_Unmarshal - Unmarshaling complete for type: %d", type);
                curr_inf->Unmarshal(&addr->taddr.addr, type, data);
                *consumed = UnmarshalConsumed(app_data->marshal.unmarshaller);
            }
        }
        else
        {
            /* No types found. Buffer has incomplete data, ask for remaining. */
            DEBUG_LOG("earbudHandover_Unmarshal - Incomplete data for unmarshaling!");
            unmarshalled = FALSE;
        }
    }

    return unmarshalled;
}

/*! \brief Handover application's Abort interface
 *
 *  Cleans up the marshaller instance.
 */
static void earbudHandover_Abort(void)
{
    handover_app_context_t * app_data = earbudHandover_Get();
    PanicFalse(app_data->marshal_state == MARSHAL_STATE_UNMARSHALING ||
               app_data->marshal_state == MARSHAL_STATE_MARSHALING);
    earbudHandover_DeinitializeMarshaller();
}

bool EarbudHandover_Init(Task init_task)
{
    UNUSED(init_task);
    handover_app_context_t * app_data = earbudHandover_Get();
    unsigned handover_registrations_array_dim;
    DEBUG_LOG("EarbudHandover_Init");

    memset(app_data, 0, sizeof(*app_data));

    /* Register handover interfaces with earbud handover application. */
    handover_registrations_array_dim = (unsigned)handover_interface_registrations_end -
                                       (unsigned)handover_interface_registrations_begin;
    PanicFalse((handover_registrations_array_dim % sizeof(registered_handover_interface_t)) == 0);
    handover_registrations_array_dim /= sizeof(registered_handover_interface_t);

    if(handover_registrations_array_dim)
    {
        app_data->interfaces = handover_interface_registrations_begin;
        app_data->interfaces_len = handover_registrations_array_dim;
    }

    /* Register handover interfaces with profile. We Panic if registration fails as handover will
     * not be possible without registration. Registration could only fail if handover profile is not
     * initialized prior to registration.
     */
    PanicFalse(HandoverProfile_RegisterHandoverClients(p1_ho_interfaces, sizeof(p1_ho_interfaces)/sizeof(p1_ho_interfaces[0])));
    app_data->marshal_state = MARSHAL_STATE_INITIALIZED;
    return TRUE;
}
#endif
