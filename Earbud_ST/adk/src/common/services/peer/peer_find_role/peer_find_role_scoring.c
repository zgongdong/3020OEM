/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Implementation of scoring for role selection.
            Most scoring inputs are collected automatically and stored in the task 
            data
*/

#include <logging.h>
#include <panic.h>

#include <phy_state.h>
#include <charger_monitor.h>
#include <acceleration.h>
#include <battery_monitor.h>
#include <bt_device.h>
#include <gatt_handler.h>

#include "peer_find_role_scoring.h"
#include "peer_find_role_private.h"

void PeerFindRole_OverrideScore(grss_figure_of_merit_t score)
{
    peerFindRoleTaskData *pfr = PeerFindRoleGetTaskData();

    DEBUG_LOG("PeerFindRole_OverrideScore override 0x%x", score); 

    /* set the override and call recalc, which will update the service. */
    pfr->score_override = score;
    peer_find_role_calculate_score();
}

void peer_find_role_scoring_setup(void)
{
    batteryRegistrationForm battery_reading = {PeerFindRoleGetTask(), battery_level_repres_percent, 3};

    (void)appPhyStateRegisterClient(PeerFindRoleGetTask());
    (void)appChargerClientRegister(PeerFindRoleGetTask());
    (void)appAccelerometerClientRegister(PeerFindRoleGetTask());
    PanicFalse(appBatteryRegister(&battery_reading));
}


/*! Internal function to re-request setting of score

    Send ourselves a message to try updating the score after a short delay.
*/
static void peer_find_role_request_score_update_later(void)
{
    MessageCancelAll(PeerFindRoleGetTask(), PEER_FIND_ROLE_INTERNAL_UPDATE_SCORE);
    MessageSendLater(PeerFindRoleGetTask(), PEER_FIND_ROLE_INTERNAL_UPDATE_SCORE, NULL, 10);
}


void peer_find_role_calculate_score(void)
{
    peer_find_role_scoring_t *scoring = PeerFindRoleGetScoring();
    grss_figure_of_merit_t score = PEER_FIND_ROLE_SCORE_LOWEST;
    bool server_score_set;
    peerFindRoleTaskData *pfr = PeerFindRoleGetTaskData();

    score += PEER_FIND_ROLE_SCORE(PHY_STATE, (uint16)scoring->phy_state);
    score += PEER_FIND_ROLE_SCORE(POWERED, scoring->charger_present);
    score += PEER_FIND_ROLE_SCORE(BATTERY, scoring->battery_level_percent);
    score += PEER_FIND_ROLE_SCORE(ROLE_CP, peer_find_role_is_central());
    score += PEER_FIND_ROLE_SCORE(HANDSET_CONNECTED, scoring->handset_connected);

    scoring->last_local_score = score;

    if (!pfr->score_override)
    {
        DEBUG_LOG("peer_find_role_calculate_score. Score 0x%x (%d)", score, score);
    }
    else
    {
        DEBUG_LOG("peer_find_role_calculate_score. Score overriden 0x%x (%d)",
                                            pfr->score_override, pfr->score_override);
    }

    server_score_set = GattRoleSelectionServerSetFigureOfMerit(&pfr->role_selection_server,
                                                               GattGetInstance(0)->conn_id,
                                                               pfr->score_override ? pfr->score_override : score);

    if (!server_score_set)
    {
        DEBUG_LOG("peer_find_role_calculate_score. Unable to set. Try again");
        peer_find_role_request_score_update_later();
    }
}

