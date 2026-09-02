/****************************************************************************
 * VelaGuard audio task.
 *
 * All audio execution (MIC capture polling, voice-keyword detection,
 * feedback WAV playback, and the capture/playback arbitration of the
 * single SF32LB52 codec) runs in a dedicated task.  The VelaGuard main
 * task owns the UI/BLE/event state machine and only:
 *   1. starts the audio task,
 *   2. posts commands to it (start/stop MIC, play feedback),
 *   3. consumes events it reports (keyword -> voice SOS, capture ready).
 *
 * The audio task blocks on its command queue whenever it has no work.
 ****************************************************************************/

#ifndef __VELAGUARD_AUDIO_TASK_H
#define __VELAGUARD_AUDIO_TASK_H

#include <stdbool.h>
#include <stdint.h>

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* Commands sent from the main task to the audio task. */

enum vg_audio_cmd_type_e
{
  VG_AUDIO_CMD_START_MIC = 1,   /* Start continuous MIC capture/analysis */
  VG_AUDIO_CMD_STOP_MIC,        /* Stop MIC capture                     */
  VG_AUDIO_CMD_PLAY_FEEDBACK,   /* Play a feedback WAV (arg: vg_feedback_type_e) */
};

/* Events reported from the audio task back to the main task. */

enum vg_audio_evt_type_e
{
  VG_AUDIO_EVT_CAPTURE_READY = 1,  /* arg: 0 = started, -errno on failure */
  VG_AUDIO_EVT_KEYWORD,            /* arg: enum vg_audio_keyword_e        */
};

struct vg_audio_cmd_s
{
  uint32_t type;
  int32_t arg;
};

struct vg_audio_evt_s
{
  uint32_t type;
  int32_t arg;
};

/****************************************************************************
 * Public Function Prototypes (called from the VelaGuard main task)
 ****************************************************************************/

/* Create the command/event queues and spawn the audio task.  Returns 0 on
 * success.  Idempotent. */

int vg_audio_task_start(void);

/* Queue a command without blocking.  Returns 0 on success, or a negative
 * errno when the queue is full / not available. */

int vg_audio_task_send_cmd(uint32_t type, int32_t arg);

/* Convenience wrapper for vg_audio_task_send_cmd(): request feedback
 * playback.  arg is an enum vg_feedback_type_e value. */

int vg_audio_task_play_feedback(int type);

/* Non-blocking fetch of the next event.  Returns true and fills *evt when
 * an event is available. */

bool vg_audio_task_get_event(struct vg_audio_evt_s *evt);

/* Block (polling) until the audio task reports VG_AUDIO_EVT_CAPTURE_READY
 * or timeout_ms elapses.  Returns the event arg (0 on success) or a
 * negative errno.  Used to keep the audio DMA setup ahead of BLE
 * bring-up. */

int vg_audio_task_wait_capture_ready(int timeout_ms);

#endif /* __VELAGUARD_AUDIO_TASK_H */
