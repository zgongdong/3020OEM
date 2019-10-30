/*!
\copyright  Copyright (c) 2008 - 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       earbud_config.h
\brief      Application configuration file
*/

#ifndef EARBUD_CONFIG_H_
#define EARBUD_CONFIG_H_

#include <earbud_init.h>

#ifdef USE_BDADDR_FOR_LEFT_RIGHT
/*! Left and right earbud roles are selected from Bluetooth address. */
/*! TRUE if this is the left earbud (Bluetooth address LAP is odd). */
#define appConfigIsLeft()           (InitGetTaskData()->appInitIsLeft)
/*! TRUE if this is the right earbud (Bluetooth address LAP is even). */
#define appConfigIsRight()          (appConfigIsLeft() ^ 1)
#else
/*! Left and right earbud roles are selected from the state of this PIO */
#define appConfigHandednessPio()    (2)
/*! TRUE if this is the left earbud (the #appConfigHandednessPio state is 1) */
#define appConfigIsLeft()           ((PioGet32Bank(appConfigHandednessPio() / 32) & (1UL << appConfigHandednessPio())) ? 1 : 0)
/*! TRUE if this is the right earbud (the #appConfigHandednessPio state is 0) */
#define appConfigIsRight()          (appConfigIsLeft() ^ 1)
#endif

/*! Default state proxy events to register */
#define appConfigStateProxyRegisteredEventsDefault()            \
                   (state_proxy_event_type_phystate |           \
                    state_proxy_event_type_a2dp_conn |          \
                    state_proxy_event_type_a2dp_discon |        \
                    state_proxy_event_type_avrcp_conn |         \
                    state_proxy_event_type_avrcp_discon |       \
                    state_proxy_event_type_hfp_conn |           \
                    state_proxy_event_type_hfp_discon |         \
                    state_proxy_event_type_a2dp_supported |     \
                    state_proxy_event_type_avrcp_supported |    \
                    state_proxy_event_type_hfp_supported |      \
                    state_proxy_event_type_peer_linkloss |      \
                    state_proxy_event_type_handset_linkloss)

#define SCRAMBLED_ASPK {0x55d4, 0xd417, 0x32da, 0x81bb, 0xbde7, 0xe2ee, 0x8a44, 0x6fd3, 0xa181, 0xda60, 0xb9b8, 0x7b16, 0x445b,\
0x7c3c, 0xb224, 0x0c35}

#endif /* EARBUD_CONFIG_H_ */
