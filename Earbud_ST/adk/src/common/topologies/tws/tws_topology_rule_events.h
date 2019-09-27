/*!
\copyright  Copyright (c) 2005 - 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Definition of events which can initiate TWS topology rule processing.
*/

#ifndef _TWS_TOPOLOGY_RULE_EVENTS_H_
#define _TWS_TOPOLOGY_RULE_EVENTS_H_

#define TWSTOP_RULE_EVENT_STARTUP                       (1ULL << 0)
#define TWSTOP_RULE_EVENT_NO_PEER                       (1ULL << 1)
#define TWSTOP_RULE_EVENT_PEER_PAIRED                   (1ULL << 2)

#define TWSTOP_RULE_EVENT_IN_CASE                       (1ULL << 3)
#define TWSTOP_RULE_EVENT_OUT_CASE                      (1ULL << 4)
#define TWSTOP_RULE_EVENT_PEER_IN_CASE                  (1ULL << 5)
#define TWSTOP_RULE_EVENT_PEER_OUT_CASE                 (1ULL << 6)

#define TWSTOP_RULE_EVENT_ROLE_SWITCH                   (1ULL << 7)
#define TWSTOP_RULE_EVENT_ROLE_SELECTED_PRIMARY         (1ULL << 8)
#define TWSTOP_RULE_EVENT_ROLE_SELECTED_SECONDARY       (1ULL << 9)
#define TWSTOP_RULE_EVENT_ROLE_SELECTED_ACTING_PRIMARY  (1ULL << 10)

#define TWSTOP_RULE_EVENT_PEER_LINKLOSS                 (1ULL << 11)
#define TWSTOP_RULE_EVENT_HANDSET_LINKLOSS              (1ULL << 12)

#define TWSTOP_RULE_EVENT_PEER_CONNECTED_BREDR          (1ULL << 13)
#define TWSTOP_RULE_EVENT_PEER_DISCONNECTED_BREDR       (1ULL << 14)

#define TWSTOP_RULE_EVENT_HANDSET_CONNECTED_BREDR       (1ULL << 15)
#define TWSTOP_RULE_EVENT_HANDSET_CONNECTED_A2DP        (1ULL << 16)
#define TWSTOP_RULE_EVENT_HANDSET_CONNECTED_AVRCP       (1ULL << 17)
#define TWSTOP_RULE_EVENT_HANDSET_CONNECTED_HFP         (1ULL << 18)

#define TWSTOP_RULE_EVENT_HANDSET_DISCONNECTED_BREDR    (1ULL << 19)
#define TWSTOP_RULE_EVENT_HANDSET_DISCONNECTED_A2DP     (1ULL << 20)
#define TWSTOP_RULE_EVENT_HANDSET_DISCONNECTED_AVRCP    (1ULL << 21)
#define TWSTOP_RULE_EVENT_HANDSET_DISCONNECTED_HFP      (1ULL << 22)

    /*! Event used as a command to enter the DFU role */
#define TWSTOP_RULE_EVENT_DFU_ROLE                      (1ULL << 23)
#define TWSTOP_RULE_EVENT_DFU_ROLE_COMPLETE             (1ULL << 24)

#define TWSTOP_RULE_EVENT_FAILED_PEER_CONNECT           (1ULL << 25)

#endif /* _TWS_TOPOLOGY_RULE_EVENTS_H_ */
