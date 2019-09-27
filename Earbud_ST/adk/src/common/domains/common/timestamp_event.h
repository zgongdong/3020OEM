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

#else /* DISABLE_TIMESTAMP_EVENT */

/*! \brief Not implemented */
#define TimestampEvent(id)

/*! \brief Not implemented */
#define TimestampEvent_Delta(first_id, second_id) (0)

#endif /* DISABLE_TIMESTAMP_EVENT */

#endif /* TIMESTAMP_EVENT_H_ */
