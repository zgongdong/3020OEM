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

/*! Optional state proxy events to register, feature specific */
#ifdef INCLUDE_SHADOWING
#define appConfigStateProxyRegisteredEventsOptional()           \
                   (state_proxy_event_type_link_quality |       \
                    state_proxy_event_type_mic_quality)
#else
#define appConfigStateProxyRegisteredEventsOptional() 0
#endif

#endif /* EARBUD_CONFIG_H_ */