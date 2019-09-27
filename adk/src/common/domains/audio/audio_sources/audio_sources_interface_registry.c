/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of the audio_interface composite.
*/

#include "audio_sources_interface_registry.h"

#include <logging.h>
#include <panic.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "audio_sources_audio_interface.h"
#include "audio_sources_media_control_interface.h"
#include "audio_sources_volume_interface.h"
#include "audio_sources_volume_control_interface.h"
#include "audio_sources_observer_interface.h"


/* ------------------------------ Definitions ------------------------------ */

/*! \brief Supported interfaces structure */
typedef struct
{
    /*! Audio source type */
    audio_source_t source;
    /*! Audio interface. */
    const audio_source_audio_interface_t * audio_interface;
    /*! Media control interface. */
    const media_control_interface_t * media_control_interface;
    /*! Volume interface. */
    const audio_source_volume_interface_t * volume_interface;
    /*! Volume control interface. */
    const audio_source_volume_control_interface_t * volume_control_interface;
    /*! Observer interface. */
    const audio_source_observer_interface_t * observer_interface;
} audio_interfaces_t;

typedef struct
{
    audio_interfaces_t * interfaces;
    unsigned number_of_registered_sources;
} audio_source_interface_registry_t;


/* ------------------------------ Global Variables ------------------------------ */

static audio_source_interface_registry_t source_registry;


/* ------------------------------ Static function prototypes ------------------------------ */

static bool audioInterface_SourceIsValid(audio_source_t source);
static bool audioInterface_InterfaceTypeIsValid(audio_interface_types_t interface_type);
static bool audioInterface_GetRegistryIndexBySource(audio_source_t source, uint8 *registry_index);
static unsigned audioInterface_CreateRegistryEntry(audio_source_t source);
static void audioInterface_RegisterInterfaceType(audio_interfaces_t * source, audio_interface_types_t interface_type, const void * interface);
static void * audioInterface_GetRegisteredInterface(audio_interfaces_t * source, audio_interface_types_t interface_type);


/* ------------------------------ API functions start here ------------------------------ */

void AudioInterface_Init(void)
{
    DEBUG_LOG("AudioInterface_Init called.");
    source_registry.interfaces = NULL;
    source_registry.number_of_registered_sources = 0;
}

void AudioInterface_Register(audio_source_t source, audio_interface_types_t interface_type, const void * interface)
{
    uint8 registry_index;

    DEBUG_LOGF("AudioInterface_Register, source: %d interface_type: %d.", source, interface_type);

    PanicFalse(audioInterface_SourceIsValid(source));
    PanicFalse(audioInterface_InterfaceTypeIsValid(interface_type));
    PanicNull((void *)interface);

    if (!audioInterface_GetRegistryIndexBySource(source, &registry_index))
    {
        registry_index = audioInterface_CreateRegistryEntry(source);
    }

    audioInterface_RegisterInterfaceType(&source_registry.interfaces[registry_index], interface_type, interface);
}

void * AudioInterface_Get(audio_source_t source, audio_interface_types_t interface_type)
{
    uint8 registry_index;
    void *interface = NULL;

    DEBUG_LOGF("AudioInterface_Get, source: %d interface_type: %d.", source, interface_type);

    PanicFalse(audioInterface_SourceIsValid(source));
    PanicFalse(audioInterface_InterfaceTypeIsValid(interface_type));

    if (audioInterface_GetRegistryIndexBySource(source, &registry_index))
    {
        interface = audioInterface_GetRegisteredInterface(&source_registry.interfaces[registry_index], interface_type);
    }

    return interface;
}


/* ------------------------------ Static functions start here ------------------------------ */

/*! \brief Ensures that the source passed is valid

    \param source Audio source type to be obtained

    \return True if the source is valid
*/
static bool audioInterface_SourceIsValid(audio_source_t source)
{
    bool is_valid = FALSE;

    if ((source > audio_source_none) && (source < max_audio_sources))
    {
        is_valid = TRUE;
    }
    else
    {
        DEBUG_LOG("audioInterface_SourceIsValid, invalid source.");
    }

    return is_valid;
}

/*! \brief Ensures that the interface type passed is valid

    \param source Audio source type

    \return True if the source is valid
*/
static bool audioInterface_InterfaceTypeIsValid(audio_interface_types_t interface_type)
{
    bool is_valid = FALSE;

    if ((interface_type >= audio_interface_type_audio_source_registry) && (interface_type <= audio_interface_type_observer_registry))
    {
        is_valid = TRUE;
    }
    else
    {
        DEBUG_LOG("audioInterface_IterfaceTypeIsValid, invalid interface type.");
    }

    return is_valid;
}

/*! \brief Looks for a connection index based on source

    \param source Audio source type

    \param registry_index Conenction index to be updated

    \return True if a connection of that source exists
*/
static bool audioInterface_GetRegistryIndexBySource(audio_source_t source, uint8 *registry_index)
{
    bool connection_exists = FALSE;
    uint8 index = 0;

    while (index < source_registry.number_of_registered_sources)
    {
        if (source_registry.interfaces[index].source == source)
        {
            connection_exists = TRUE;
            *registry_index = index;
            break;
        }

        index++;
    }

    return connection_exists;
}

/*! \brief Allocates memory for a new audio connection

    \param source Audio source type

    \return Index to the slot for the new connection
*/
static unsigned audioInterface_CreateRegistryEntry(audio_source_t source)
{
    if(source_registry.number_of_registered_sources == 0)
    {
        source_registry.interfaces = PanicUnlessMalloc(sizeof(audio_interfaces_t));
    }
    else
    {
        audio_interfaces_t * new_registry = realloc(source_registry.interfaces, ((source_registry.number_of_registered_sources+1) * sizeof(audio_interfaces_t)));
        PanicNull(new_registry);
        source_registry.interfaces = new_registry;
    }

    memset(&source_registry.interfaces[source_registry.number_of_registered_sources], 0, sizeof(audio_interfaces_t));
    source_registry.interfaces[source_registry.number_of_registered_sources].source = source;
    source_registry.number_of_registered_sources++;

    return (source_registry.number_of_registered_sources-1);

}

/*! \brief Interface registration function

    \param source Audio source type to be registered

    \param interface_type Audio interface type to be registered

    \param interface The interface to be registered
*/
static void audioInterface_RegisterInterfaceType(audio_interfaces_t * source, audio_interface_types_t interface_type, const void * interface)
{
    DEBUG_LOGF("audioInterface_RegisterInterfaceType, interface_type: %d.", interface_type);

    switch (interface_type)
    {
        case audio_interface_type_audio_source_registry:
            source->audio_interface = interface;
            break;

        case audio_interface_type_media_control:
            source->media_control_interface = interface;
            break;

        case audio_interface_type_volume_registry:
            source->volume_interface = interface;
            break;

        case audio_interface_type_volume_control:
            source->volume_control_interface = interface;
            break;

        case audio_interface_type_observer_registry:
            source->observer_interface = interface;
            break;

        default:
            DEBUG_LOG("audioInterface_RegisterInterfaceType, unsupported interface type.");
            break;
    }
}

/*! \brief Interface obtaining function

    \param source Audio source type to be obtained

    \param interface_type Audio interface type to be obtained

    \return Pointer to the registered interface
*/
static void * audioInterface_GetRegisteredInterface(audio_interfaces_t * source, audio_interface_types_t interface_type)
{
    void *interface = NULL;

    DEBUG_LOGF("audioInterface_GetRegisteredInterface, interface_type: %d.", interface_type);

    switch (interface_type)
    {
        case audio_interface_type_audio_source_registry:
            interface = (void *)source->audio_interface;
            break;

        case audio_interface_type_media_control:
            interface = (void *)source->media_control_interface;
            break;

        case audio_interface_type_volume_registry:
            interface = (void *)source->volume_interface;
            break;

        case audio_interface_type_volume_control:
            interface = (void *)source->volume_control_interface;
            break;

        case audio_interface_type_observer_registry:
            interface = (void *)source->observer_interface;
            break;

        default:
            DEBUG_LOG("audioInterface_GetRegisteredInterface, unsupported interface type.");
            break;
    }

    return interface;
}
