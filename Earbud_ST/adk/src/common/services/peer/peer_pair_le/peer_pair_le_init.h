/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Header file for the init functions in the PEER PAIRING OVER LE service
*/

#ifndef PEER_PAIR_LE_INIT_H_
#define PEER_PAIR_LE_INIT_H_

/*! Initialise the peer service */
void peer_pair_le_init(void);

/*! Start using the service, eg. Allocates memory for the peer service. */
void peer_pair_le_start_service(void);


/*! Disconnect LE link (if any) */
void peer_pair_le_disconnect(void);


/*! Stop using the service, eg. Deallocates memory for the peer service */
void peer_pair_le_stop_service(void);


#endif /* PEER_PAIR_LE_INIT_H_ */
