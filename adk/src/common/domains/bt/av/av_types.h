/*!
\copyright  Copyright (c) 2018 - 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       av_types.h
\brief      Header file for AV types
*/

#ifndef AV_TYPES_H_
#define AV_TYPES_H_

#include <csrtypes.h>
#include <task_list.h>

#include "avrcp_profile_types.h"
#include "a2dp_profile_types.h"

/*! Maximum number of AV connections for TWS */
#define AV_MAX_NUM_TWS (1)
/*! Maximum number of AV connections for audio Sinks */
#define AV_MAX_NUM_SNK (1)

/*! \brief Maximum number of AV connections

    Defines the maximum number of simultaneous AV connections that are allowed.

    This is based on the number of links we want as TWS or as a standard Sink.

    Expectation is a value of 1 for a standard AV headset, or 2 for a True Wireless
    headset.
*/
#define AV_MAX_NUM_INSTANCES (AV_MAX_NUM_TWS + AV_MAX_NUM_SNK)

/*! \brief AV instance structure

    This structure contains all the information for a AV (A2DP & AVRCP)
    connection.
*/
typedef struct
{
    TaskData        av_task;                    /*!< Task/Message information for this instance */

    bdaddr          bd_addr;                    /*!< Bluetooth Address of remote device */
    bool            detach_pending;             /*!< Flag indicating if the instance is about to be detatched */

    avrcpTaskData   avrcp;
    a2dpTaskData    a2dp;

} avInstanceTaskData;

/*! \brief AV task data

    Structure used to hold data relevant only to the AV task. Other modules
    should not access this information directly.
*/
typedef struct
{
    TaskData        task;                   /*!< Task for messages */
    unsigned        state:2;                /*!< Current state of AV state machine */
    unsigned        volume_repeat:1;
    avSuspendReason suspend_state;          /*!< Bitmap of active suspend reasons */
    uint8           volume;                 /*!< The AV volume */
    avInstanceTaskData *av_inst[AV_MAX_NUM_INSTANCES];  /*!< AV Instances */

    task_list_t     avrcp_client_list;     /*!< List of tasks registered via \ref appAvAvrcpClientRegister */
    task_list_t     av_status_client_list; /*!< List of tasks registered via \ref appAvStatusClientRegister.
                                                These receive indications for AVRCP, A2DP and A2DP streaming */
    task_list_t     av_ui_client_list;     /*!< List of tasks registered for UI events */

    task_list_with_data_t a2dp_connect_request_clients;
    task_list_with_data_t a2dp_disconnect_request_clients;
    task_list_with_data_t avrcp_connect_request_clients;
    task_list_with_data_t avrcp_disconnect_request_clients;

} avTaskData;

#endif /* AV_TYPES_H_ */
