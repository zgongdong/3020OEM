/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for the Earbud Application user interface tone indications.
*/
#ifndef EARBUD_TONES_H
#define EARBUD_TONES_H

#include "domain_message.h"
#include "kymera.h"

/*! \brief Time between mute reminders (in seconds) */
#define EARBUD_TONES_MUTE_REMINDER_TIME               (15)

/*! \brief Time between SCO un-encrpyted warninged (in seconds) */
#define EARBUD_TONES_SCO_UNENCRYPTED_REMINDER_TIME	  (5)

/*! Internal messages */
enum
{
    INTERNAL_MUTE_REMINDER_IND = INTERNAL_MESSAGE_BASE,
};

/*! \brief Tones task structure */
typedef struct
{
    TaskData task;
} earbudTonesTaskData;

/*!< Tones indication data structure */
extern earbudTonesTaskData earbud_tones;

extern const ringtone_note app_tone_button[];
extern const ringtone_note app_tone_button_2[];
extern const ringtone_note app_tone_button_3[];
extern const ringtone_note app_tone_button_4[];
extern const ringtone_note app_tone_button_dfu[];
extern const ringtone_note app_tone_button_factory_reset[];

extern const ringtone_note app_tone_hfp_connect[];
extern const ringtone_note app_tone_hfp_connected[];
extern const ringtone_note app_tone_hfp_disconnected[];
extern const ringtone_note app_tone_hfp_link_loss[];
extern const ringtone_note app_tone_hfp_sco_connected[];
extern const ringtone_note app_tone_hfp_sco_disconnected[];
extern const ringtone_note app_tone_hfp_mute_reminder[];
extern const ringtone_note app_tone_hfp_sco_unencrypted_reminder[];
extern const ringtone_note app_tone_hfp_ring[];
extern const ringtone_note app_tone_hfp_ring_caller_id[];
extern const ringtone_note app_tone_hfp_voice_dial[];
extern const ringtone_note app_tone_hfp_voice_dial_disable[];
extern const ringtone_note app_tone_hfp_answer[];
extern const ringtone_note app_tone_hfp_hangup[];
extern const ringtone_note app_tone_hfp_mute_active[];
extern const ringtone_note app_tone_hfp_mute_inactive[];
extern const ringtone_note app_tone_hfp_talk_long_press[];
extern const ringtone_note app_tone_pairing[];
extern const ringtone_note app_tone_paired[];
extern const ringtone_note app_tone_pairing_deleted[];
extern const ringtone_note app_tone_volume[];
extern const ringtone_note app_tone_volume_limit[];
extern const ringtone_note app_tone_error[];
extern const ringtone_note app_tone_battery_empty[];
extern const ringtone_note app_tone_power_on[];
extern const ringtone_note app_tone_power_off[];
extern const ringtone_note app_tone_paging_reminder[];
extern const ringtone_note app_tone_peer_pairing[];
extern const ringtone_note app_tone_peer_pairing_error[];

#ifdef INCLUDE_DFU
extern const ringtone_note app_tone_dfu[];
#endif

#ifdef INCLUDE_AV
extern const ringtone_note app_tone_av_connect[];
extern const ringtone_note app_tone_av_disconnect[];
extern const ringtone_note app_tone_av_remote_control[];
extern const ringtone_note app_tone_av_connected[];
extern const ringtone_note app_tone_av_disconnected[];
extern const ringtone_note app_tone_av_link_loss[];
#endif
//!@}


/*! \brief Play button held/press connect tone */
#define appUiButton() \
    appUiPlayTone(app_tone_button)

/*! \brief Play button held/press connect tone, 2 beeps */
#define appUiButton2() \
    appUiPlayTone(app_tone_button_2)

/*! \brief Play button held/press connect tone, 3 beeps */
#define appUiButton3() \
    appUiPlayTone(app_tone_button_3)

/*! \brief Play button held/press connect tone, 4 beeps */
#define appUiButton4() \
    appUiPlayTone(app_tone_button_4)

/*! \brief Play DFU button held tone */
#define appUiButtonDfu() \
    appUiPlayTone(app_tone_button_dfu)

/*! \brief Play factory reset button held tone */
#define appUiButtonFactoryReset() \
    appUiPlayTone(app_tone_button_factory_reset)

/*! \brief Play HFP connect tone */
#define appUiHfpConnect() \
    appUiPlayTone(app_tone_hfp_connect)

/*! \brief Play HFP voice dial tone */
#define appUiHfpVoiceDial() \
    appUiPlayTone(app_tone_hfp_voice_dial)

/*! \brief Play HFP voice dial disable tone */
#define appUiHfpVoiceDialDisable() \
    appUiPlayTone(app_tone_hfp_voice_dial_disable)

/*! \brief Play HFP last number redial tone */
#define appUiHfpLastNumberRedial() \
    /* Tone already played by appUiHfpTalkLongPress() */

/*! \brief Play HFP answer call tone */
#define appUiHfpAnswer() \
    appUiPlayTone(app_tone_hfp_answer)

/*! \brief Play HFP reject call tone */
#define appUiHfpReject() \
    /* Tone already played by appUiHfpTalkLongPress() */

/*! \brief Play HFP hangup call tone */
#define appUiHfpHangup() \
    appUiPlayTone(app_tone_hfp_hangup)

/*! \brief Play HFP transfer call tone */
#define appUiHfpTransfer() \
    /* Tone already played by appUiHfpTalkLongPress() */

/*! \brief Play HFP volume down tone */
#define appUiHfpVolumeDown() \
    appUiPlayToneNotQueueable(app_tone_volume)

/*! \brief Play HFP volume up tone */
#define appUiHfpVolumeUp() \
    appUiPlayToneNotQueueable(app_tone_volume)

/*! \brief Play HFP volume limit reached tone */
#define appUiHfpVolumeLimit() \
    appUiPlayToneNotQueueable(app_tone_volume_limit)

/*! \brief Play HFP SLC link loss tone */
#define appUiHfpLinkLoss() \
    appUiPlayTone(app_tone_hfp_link_loss)

/*! \brief Play HFP ring indication tone */
#define appUiHfpRing(caller_id) \
    appUiPlayTone(app_tone_hfp_ring)

/*! \brief Handle caller ID */
#define appUiHfpCallerId(caller_number, size_caller_number, caller_name, size_caller_name)
    /* Add text to speech call here */

/*! \brief Play HFP SCO connected tone */
#define appUiHfpScoConnected() \
    appUiPlayTone(app_tone_hfp_sco_connected)

/*! \brief Play HFP SCO disconnected tone */
#define appUiHfpScoDisconnected() \
    appUiPlayTone(app_tone_hfp_sco_disconnected)

/*! \brief Play HFP mute active tone */
#define appUiHfpMuteActive() \
    appUiPlayTone(app_tone_hfp_mute_active)

/*! \brief Play HFP mute inactive tone */
#define appUiHfpMuteInactive() \
    appUiPlayTone(app_tone_hfp_mute_inactive)

/*! \brief Play HFP mute reminder tone */
#define appUiHfpMuteReminder() \
    appUiPlayTone(app_tone_hfp_mute_reminder)

/*! \brief Play HFP SCO un-encrypted tone */
#define appUiHfpScoUnencryptedReminder() \
    appUiPlayTone(app_tone_hfp_sco_unencrypted_reminder)

/*! \brief Handle UI changes for HFP state change */
#define appUiHfpState(state) \
    /* Add any HFP state indication here */

/*! \brief Play HFP talk button long press tone */
#define appUiHfpTalkLongPress() \
    appUiPlayTone(app_tone_hfp_talk_long_press)

#ifdef INCLUDE_AV
/*! \brief Play AV connect tone */
#define appUiAvConnect() \
    appUiPlayTone(app_tone_av_connect)

/*! \brief Play AV disconnect tone */
#define appUiAvDisconnect() \
    appUiPlayTone(app_tone_av_disconnect)

/*! \brief Play AV volume down tone */
#define appUiAvVolumeDown() \
    appUiPlayToneNotQueueable(app_tone_volume)

/*! \brief Play AV volume up tone */
#define appUiAvVolumeUp() \
   appUiPlayToneNotQueueable(app_tone_volume)

/*! \brief Play AV volume limit reached tone */
#define appUiAvVolumeLimit() \
    appUiPlayToneNotQueueable(app_tone_volume_limit)

/*! \brief Play AVRCP remote control tone */
#define appUiAvRemoteControl() \
    appUiPlayToneNotQueueable(app_tone_av_remote_control)

/*! \brief Play AV peer connected indication */
#define appUiAvPeerConnected() \
    appUiPlayTone(app_tone_av_connected)

/*! \brief Play AV link-loss tone */
#define appUiAvLinkLoss() \
    appUiPlayTone(app_tone_av_link_loss)
#endif

/*! \brief Play pairing start tone */
#define appUiPairingStart() \
    appUiPlayTone(app_tone_pairing)

/*! \brief Play a tone to completion */
#define appUiPlayTone(tone) EarbudTones_PlayToneCore(tone, FALSE, TRUE, NULL, 0)
/*! \brief Play a tone allowing another tone/prompt/event to interrupt (stop) this tone
     before completion. */
#define appUiPlayToneInterruptible(tone) EarbudTones_PlayToneCore(tone, TRUE, TRUE, NULL, 0)
/*! \brief Play a tone only if it's not going to be queued. */
#define appUiPlayToneNotQueueable(tone) EarbudTones_PlayToneCore(tone, FALSE, FALSE, NULL, 0)
/*! \brief Play a tone to completion. mask bits will be cleared in lock
    when the tone completes, or if it is not played at all. */
#define appUiPlayToneClearLock(tone, lock, mask) EarbudTones_PlayToneCore(tone, FALSE, TRUE, lock, mask)
/*! \brief Play a tone allowing another tone/prompt/event to interrupt (stop) this tone
     before completion. mask bits will be cleared in lock when the tone completes or
     is interrupted, or if it is not played at all. */
#define appUiPlayToneInterruptibleClearLock(tone, lock, mask) EarbudTones_PlayToneCore(tone, TRUE, TRUE, lock, mask)

/*! \brief Play the tone passed to this function.
    \param tone The tone to play.
    \param interruptible If TRUE, always play to completion, if FALSE, the tone may be
    interrupted before completion.
    \param queueable If TRUE, tone can be queued behind already playing tone, if FALSE, the tone will
    only play if no other tone playing or queued.
    \param client_lock If not NULL, bits set in client_lock_mask will be cleared
    in client_lock when the tone finishes - either on completion, when interrupted,
    or if the tone is not played at all, because the UI is not currently playing tones.
    \param client_lock_mask A mask of bits to clear in the client_lock.
*/
void EarbudTones_PlayToneCore(const ringtone_note *tone, bool interruptible, bool queueable,
                       uint16 *client_lock, uint16 client_lock_mask);

/*! \brief Play a microphone muted reminder tone and schedule another to be played in
           EARBUD_TONES_MUTE_REMINDER_TIME seconds.
 */
void EarbudTones_MuteReminderIndication(void);

/*! \brief Cancel any microphone muted reminder tone.
*/
void EarbudTones_CancelMuteReminder(void);

/*! \brief Initialise the Tones module.
*/
void EarbudTones_Init(void);

#endif // EARBUD_TONE_H
