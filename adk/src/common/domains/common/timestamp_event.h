/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      This component records the time at which events occur and calculates
            the time difference between two events.

            A one milli-second time resolution is used. The timestamps are stored
            internally in 16-bit variables resulting in a maximum time difference
            between two events of 65535ms.

            The module is used for measurning KPIs on-chip during tests and can
            be removed from production software by defining DISABLE_TIMESTAMP_EVENT.
*/
#ifndef TIMESTAMP_EVENT_H_
#define TIMESTAMP_EVENT_H_

#include "csrtypes.h"

/*! \brief The ids for events that may be timestampted by this component. */
typedef enum
{
    /*! The chip and os have booted */
    TIMESTAMP_EVENT_BOOTED,

    /*! The application software has initialised */
    TIMESTAMP_EVENT_INITIALISED,
    
    /*! Peer Find Role has been called */
    TIMESTAMP_EVENT_PEER_FIND_ROLE_STARTED,
    
    /*! Peer Find Role is scanning/advertising */
    TIMESTAMP_EVENT_PEER_FIND_ROLE_DISCOVERING_CONNECTABLE,
    
    /*! Peer Find Role has discovered a device */
    TIMESTAMP_EVENT_PEER_FIND_ROLE_DISCOVERED_DEVICE,
    
    /*! Peer Find Role is connected as server */
    TIMESTAMP_EVENT_PEER_FIND_ROLE_CONNECTED_SERVER,
    
    /*! Peer Find Role is connected as client */
    TIMESTAMP_EVENT_PEER_FIND_ROLE_CONNECTED_CLIENT,
    
    /*! Peer Find Role client has discovered GATT primary services of server */
    TIMESTAMP_EVENT_PEER_FIND_ROLE_DISCOVERED_PRIMARY_SERVICE,
    
    /*! Peer Find Role client is deciding roles */
    TIMESTAMP_EVENT_PEER_FIND_ROLE_DECIDING_ROLES,
    
    /*! Peer Find Role client has received figure of merit */
    TIMESTAMP_EVENT_PEER_FIND_ROLE_MERIT_RECEIVED,
    
    /*! Peer Find Role has notified registered tasks of role  */
    TIMESTAMP_EVENT_PEER_FIND_ROLE_NOTIFIED_ROLE,

    /*! Always the final event id */
    NUMBER_OF_TIMESTAMP_EVENTS,

} timestamp_event_id_t;

#ifndef DISABLE_TIMESTAMP_EVENT

/*! \brief Timestamp an event

    \param id The id of the event to timestamp
*/
void TimestampEvent(timestamp_event_id_t id);

/*! \brief Calculate the time difference between two timestamped events

    \param first_id The id of the first timestamped event
    \param second_id The id of the second timestamped event

    \return The elapsed time between the first and second events in milli-seconds.
*/
uint32 TimestampEvent_Delta(timestamp_event_id_t first_id, timestamp_event_id_t second_id);

/*! \brief Gets the timestamped event time

    \param id The id of the timestamped event

    \return The timestamped event time.
*/
uint16 TimestampEvent_GetTime(timestamp_event_id_t id);


#else /* DISABLE_TIMESTAMP_EVENT */

/*! \brief Not implemented */
#define TimestampEvent(id)

/*! \brief Not implemented */
#define TimestampEvent_Delta(first_id, second_id) (0)

/*! \brief Not implemented */
#define TimestampEvent_GetTime(timestamp_event_id_t id) (0)

#endif /* DISABLE_TIMESTAMP_EVENT */

#endif /* TIMESTAMP_EVENT_H_ */
