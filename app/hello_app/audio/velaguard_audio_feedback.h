/****************************************************************************
 * VelaGuard audio feedback — plays WAV prompts for SOS call results.
 ****************************************************************************/

#ifndef __VELAGUARD_AUDIO_FEEDBACK_H
#define __VELAGUARD_AUDIO_FEEDBACK_H

enum vg_feedback_type_e
{
  VG_FEEDBACK_NONE = 0,
  VG_FEEDBACK_SUCCESS,   /* Play /etc/data/audio/success.wav */
  VG_FEEDBACK_FAILURE,   /* Play /etc/data/audio/failure.wav */
};

/* Trigger playback.  Non-blocking — only sets internal state. */

int vg_audio_feedback_trigger(enum vg_feedback_type_e type);

/* Drive the async playback state machine.  Call from the main loop. */

void vg_audio_feedback_process(void);

#endif
