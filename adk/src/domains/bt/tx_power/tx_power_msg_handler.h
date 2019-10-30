/* Copyright (c) 2019 Qualcomm Technologies International, Ltd. */
/*  */

#ifndef TX_POWER_MSG_HANDLER_H_
#define TX_POWER_MSG_HANDLER_H_

/***************************************************************************
NAME
    txPowerTaskHandler

DESCRIPTION
    Handler for external messages sent to the tx power module.
*/
void txPowerTaskHandler(Task task, MessageId id, Message message);

Task txpower_GetTaskData(void);

#endif
