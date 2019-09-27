/*!
\copyright  Copyright (c) 2008 - 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       peer_link_keys.c
\brief      Implementation of functions to transfer link keys
*/

#include <panic.h>
#include <cryptovm.h>
#include <stdio.h>

#include "peer_link_keys.h"
#include "peer_signalling.h"
#include "logging.h"
#include "message.h"
#include "pairing.h"
#include "pairing_config.h"

static void peerLinkKeys_HandleMessage(Task task, MessageId id, Message message);

static const TaskData peer_link_keys_task = {peerLinkKeys_HandleMessage};
static uint16 outstanding_peer_sig_req;

/*! Number of bytes in the 128-bit salt. */
#define SALT_SIZE       16
/*! Offset into SALT for start of peer earbud address NAP */
#define OFFSET_SALT_NAP 10
/*! Offset into SALT for start of peer earbud address UAP */
#define OFFSET_SALT_UAP 12
/*! Offset into SALT for start of peer earbud address LAP */
#define OFFSET_SALT_LAP 13


/*! @brief Zero and write the peer earbud BT address into last 6 bytes of the SALT.

    The salt is 0x00000000000000000000xxxxxxxxxxxx
    Where the 6 bytes are LAP (3 bytes big-endian), UAP (1 byte), NAP (2 bytes big-endian)
*/
static void peerLinkKeys_SetupSaltWithPeerAddress(uint8 *salt, const bdaddr *peer_addr)
{
    memset(salt, 0, SALT_SIZE);
    salt[OFFSET_SALT_NAP]   = (peer_addr->nap >> 8) & 0xFF;
    salt[OFFSET_SALT_NAP+1] = peer_addr->nap & 0xFF;
    salt[OFFSET_SALT_UAP]   = peer_addr->uap & 0xFF;
    salt[OFFSET_SALT_LAP]   = (peer_addr->lap >> 16) & 0xFF;
    salt[OFFSET_SALT_LAP+1] = (peer_addr->lap >> 8) & 0xFF;
    salt[OFFSET_SALT_LAP+2] = peer_addr->lap & 0xFF;
}


bool PeerLinkKeys_Init(Task init_task)
{
    UNUSED(init_task);
    DEBUG_LOG("PeerLinkKeys_Init");

    outstanding_peer_sig_req = 0;

    /* register with peer signalling for notification of handset
     * link key arrival, should the other earbud pair with a new handset */
    appPeerSigLinkKeyTaskRegister((Task)&peer_link_keys_task);
    return TRUE;
}


void PeerLinkKeys_GenerateKey(const bdaddr *bd_addr, const uint16 *lk_packed, uint32 key_id_in, uint16 *lk_derived)
{
    uint8 salt[SALT_SIZE];
    uint8 lk_unpacked[AES_CMAC_BLOCK_SIZE];
    uint8 key_h7[AES_CMAC_BLOCK_SIZE];
    uint8 key_h6[AES_CMAC_BLOCK_SIZE];
    uint32_t key_id = ((key_id_in & 0xFF000000UL) >> 24) +
                      ((key_id_in & 0x00FF0000UL) >> 8) +
                      ((key_id_in & 0x0000FF00UL) << 8) +
                      ((key_id_in & 0x000000FFUL) << 24);

    DEBUG_PRINT("PeerLinkKeys_GenerateKey");

    DEBUG_LOGF("Key Id: %08lx, reversed %08lx", key_id_in, key_id);

    DEBUG_PRINT("LK In: ");
    for (int lk_i = 0; lk_i < 8; lk_i++)
        DEBUG_PRINTF("%02x ", lk_packed[lk_i]);
    DEBUG_PRINT("\n");

    peerLinkKeys_SetupSaltWithPeerAddress(salt, bd_addr);
    DEBUG_PRINT("Salt: ");
    for (int salt_i = 0; salt_i < SALT_SIZE; salt_i++)
        DEBUG_PRINTF("%02x ", salt[salt_i]);
    DEBUG_PRINT("\n");

    /* Unpack the link key: 0xCCDD 0xAABB -> 0xAA 0xBB 0xCC 0xDD */
    for (int lk_i = 0; lk_i < (AES_CMAC_BLOCK_SIZE / 2); lk_i++)
    {
        lk_unpacked[(lk_i * 2) + 0] = (lk_packed[7 - lk_i] >> 8) & 0xFF;
        lk_unpacked[(lk_i * 2) + 1] = (lk_packed[7 - lk_i] >> 0) & 0xFF;
    }
    DEBUG_PRINT("LK In: ");
    for (int lk_i = 0; lk_i < AES_CMAC_BLOCK_SIZE; lk_i++)
        DEBUG_PRINTF("%02x ", lk_unpacked[lk_i]);
    DEBUG_PRINT("\n");

    CryptoVmH7(lk_unpacked, salt, key_h7);
    DEBUG_PRINT("H7: ");
    for (int h7_i = 0; h7_i < AES_CMAC_BLOCK_SIZE; h7_i++)
        DEBUG_PRINTF("%02x ", key_h7[h7_i]);
    DEBUG_PRINT("\n");

    CryptoVmH6(key_h7, key_id, key_h6);
    DEBUG_PRINT("H6: ");
    for (int h6_i = 0; h6_i < AES_CMAC_BLOCK_SIZE; h6_i++)
        DEBUG_PRINTF("%02x ", key_h6[h6_i]);
    DEBUG_PRINT("\n");

    /* Pack the link key: 0xAA 0xBB 0xCC 0xDD -> 0xCCDD 0xAABB */
    for (int lk_i = 0; lk_i < (AES_CMAC_BLOCK_SIZE / 2); lk_i++)
    {
        lk_derived[7 - lk_i] = (key_h6[(lk_i * 2) + 0] << 8) |
                               (key_h6[(lk_i * 2) + 1] << 0);
    }

    DEBUG_PRINT("LK Out: ");
    for (int lk_i = 0; lk_i < 8; lk_i++)
        DEBUG_PRINTF("%02x ", lk_derived[lk_i]);
    DEBUG_PRINT("\n");
}


/*! \brief Handle receipt of link key from peer device and store in PDL.
 */
static void peerLinkKeys_HandlePeerSigLinkKeyRxInd(PEER_SIG_LINK_KEY_RX_IND_T *ind)
{
    DEBUG_LOGF("peerLinkKeys_HandlePeerSigLinkKeyRxInd %d", ind->status);

    if (ind->status == peerSigStatusSuccess)
    {
        Pairing_AddAuthDevice(&ind->handset_addr, 
                              ind->key_len,
                              ind->key);
    }
    else
    {
        /* We shouldn't get non-success message so panic we got a code bug */
        Panic();
    }
}


/*! \brief Handle result of attempt to send handset link key to peer headset. */
static void peerLinkKeys_HandlePeerSigLinkKeyTxConfirm(PEER_SIG_LINK_KEY_TX_CFM_T *cfm)
{
    DEBUG_LOGF("peerLinkKeys_HandlePeerSigLinkKeyTxConfirm %d", cfm->status);

    if (cfm->status == peerSigStatusSuccess)
    {
        /* update device manager that we have successfully sent
         * handset link key to peer headset, clear the flag */
        BtDevice_SetHandsetLinkKeyTxReqd(&cfm->handset_addr, FALSE);
    }
    else
    {
        DEBUG_LOG("Failed to send handset link key to peer earbud!");
    }
    outstanding_peer_sig_req--;
}

static void peerLinkKeys_HandlePeerSigPairHandsetConfirm(PEER_SIG_PAIR_HANDSET_CFM_T *cfm)
{
    DEBUG_LOGF("peerLinkKeys_HandlePeerSigPairHandsetConfirm %d", cfm->status);

    if (cfm->status == peerSigStatusSuccess)
    {
        appDeviceSetHandsetAddressForwardReq(&cfm->handset_addr, FALSE);
    }
    else
    {
        DEBUG_LOG("Failed to send standard handset address to peer earbud");
    }
    outstanding_peer_sig_req--;
}

static void peerLinkKeys_HandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch(id)
    {

        case PEER_SIG_LINK_KEY_RX_IND:
            peerLinkKeys_HandlePeerSigLinkKeyRxInd((PEER_SIG_LINK_KEY_RX_IND_T*)message);
            break;

        case PEER_SIG_LINK_KEY_TX_CFM:
            peerLinkKeys_HandlePeerSigLinkKeyTxConfirm((PEER_SIG_LINK_KEY_TX_CFM_T*)message);
            break;

        case PEER_SIG_PAIR_HANDSET_CFM:
            peerLinkKeys_HandlePeerSigPairHandsetConfirm((PEER_SIG_PAIR_HANDSET_CFM_T*)message);
            break;

        default:
            break;
    }

}

void PeerLinkKeys_SendKeyToPeer(const bdaddr* device_address, uint16 link_key_length, const uint16* link_key)
{
    bdaddr peer_addr;

    if (appDeviceGetPeerBdAddr(&peer_addr))
    {
        uint16 lk_out[8];

        PeerLinkKeys_GenerateKey(&peer_addr, link_key, appConfigTwsKeyId(), lk_out);

        /* ask peer signalling to send the new link key to the peer */
        DEBUG_LOG("PeerLinkKeys_SendKeyToPeer derived link key - sending to peer");
        outstanding_peer_sig_req++;
        appPeerSigLinkKeyToPeerRequest((Task)&peer_link_keys_task, device_address,
                                 lk_out, link_key_length);
    }
}
