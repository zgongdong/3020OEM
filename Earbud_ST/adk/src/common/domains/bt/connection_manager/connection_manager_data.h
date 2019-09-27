/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       connection_manager_data.h
\brief      Header file for Connection Manager internal data types
*/

#ifndef __CON_MANAGER_DATA_H
#define __CON_MANAGER_DATA_H

#include <connection_manager.h>
#include <le_scan_manager.h>

/*! Connection Manager module task data. */
typedef struct
{
    /*! The task (message) information for the connection manager module */
    TaskData         task;

    /*! Flag indicating if incoming handset connections are allowed */
    bool handset_connect_allowed:1;

    cm_transport_t connectable_transports;
    
    le_scan_handle_t scan_pause_handle;

    /*! Structure grouping data for forced disconnect */
    struct {
        /*! Address being disconnected. Note that the disconnect API does 
            not have address type information */
        bdaddr acl;
        /*! Task requesting the forced disconnect */
        Task   requesting_task;
    } forced_disconnect;
} conManagerTaskData;


#endif
