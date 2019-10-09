/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Handover Decision Making Algorithm Event Handler.
*/

#include "hdma.h"
#include <message.h>
#include <logging.h>

/*! Stub Functionality: Initialise the HDMA component.*/
bool Hdma_Init(Task client_task)
{
    (void)client_task;
    DEBUG_LOG("HDMA Module Not included: Hdma_Init");
    return FALSE;
}

/*! Stub Functionality: De-Initialise the HDMA module */
bool Hdma_Destroy(void)
{
    DEBUG_LOG("HDMA Module Not included: Hdma_Destroy");
    return FALSE;
}
