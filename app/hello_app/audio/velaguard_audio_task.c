/****************************************************************************
 * VelaGuard audio task.
 *
 * This task owns every call into the audio front-ends
 * (audio/velaguard_audio.c and audio/velaguard_audio_feedback.c), so those
 * modules keep their single-threaded, lock-free design.
 *
 * Command flow (POSIX message queues, no application locks):
 *   main task  --(vg_audio_cmd)-->  audio task  (blocking receive)
 *   audio task --(vg_audio_evt)-->  main task   (non-blocking poll)
 *
 * The audio task blocks on its command queue when idle.  While a MIC
 * capture session is active it services the capture DMA queue and checks
 * the command queue between frames, so a feedback request can preempt
 * capture (the SF32LB52 codec cannot capture and play at the same time).
 ****************************************************************************/

#include "velaguard_audio_task.h"
#include "velaguard_audio.h"
#include "velaguard_audio_feedback.h"

#include <nuttx/config.h>
#include <nuttx/sched.h>

#include <errno.h>
#include <fcntl.h>
#include <mqueue.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define VG_AUDIO_CMD_QNAME       "vg_audio_cmd"
#define VG_AUDIO_EVT_QNAME       "vg_audio_evt"
#define VG_AUDIO_MQ_MAXMSG       8

/* Poll granularity while a capture session or a feedback playback is
 * active.  The audio task must stay responsive to commands without
 * busy-spinning. */

#define VG_AUDIO_SESSION_POLL_US  2000
#define VG_AUDIO_PLAYBACK_POLL_US 2000

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* Main-task side (created by vg_audio_task_start(), used from the main
 * task only). */
static mqd_t g_cmd_send_mq = -1;   /* vg_audio_cmd : O_WRONLY|O_NONBLOCK */
static mqd_t g_evt_recv_mq = -1;   /* vg_audio_evt : O_RDONLY (blocking)  */

/* Audio-task side (opened in the task entry, used from the audio task
 * only). */
static mqd_t g_cmd_recv_mq = -1;   /* vg_audio_cmd : O_RDONLY (blocking)  */
static mqd_t g_evt_send_mq = -1;   /* vg_audio_evt : O_WRONLY|O_NONBLOCK  */

static bool g_task_started;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/* Post an event to the main task.  Non-blocking: if the queue is full the
 * event is dropped.  The main task drains the queue every loop iteration,
 * and a dropped event is safe (keyword triggers are decided by the main
 * task state machine). */

static void vg_audio_task_post_event(uint32_t type, int32_t arg)
{
  struct vg_audio_evt_s evt;

  if (g_evt_send_mq < 0)
    {
      return;
    }

  evt.type = type;
  evt.arg  = arg;
  mq_send(g_evt_send_mq, (FAR char *)&evt, sizeof(evt), 0);
}

/* Try to fetch one command without blocking.  Returns true on success. */

static bool vg_audio_task_try_recv_cmd(struct vg_audio_cmd_s *cmd)
{
  struct timespec ts;
  ssize_t r;

  if (g_cmd_recv_mq < 0)
    {
      return false;
    }

  memset(&ts, 0, sizeof(ts));
  r = mq_timedreceive(g_cmd_recv_mq, (FAR char *)cmd, sizeof(*cmd),
                      NULL, &ts);
  return r == (ssize_t)sizeof(*cmd);
}

/* Play a feedback WAV to completion inside this task.  When resume_capture
 * is set the MIC stream is paused first and restarted afterwards, keeping
 * the capture/playback arbitration of the shared codec in this task. */

static void vg_audio_task_play_feedback_now(int type, bool resume_capture)
{
  if (resume_capture)
    {
      printf("VelaGuard audio: pause capture for feedback\n");
      vg_audio_capture_stop();
    }

  if (vg_audio_feedback_trigger(type) == 0)
    {
      /* Block this task until the WAV has finished.  The feedback state
       * machine keeps the driver DMA fed; a short sleep keeps CPU usage
       * low between driver completion messages. */

      while (vg_audio_feedback_active())
        {
          vg_audio_feedback_process();
          usleep(VG_AUDIO_PLAYBACK_POLL_US);
        }
    }

  if (resume_capture)
    {
      if (vg_audio_capture_start() == 0)
        {
          printf("VelaGuard audio: capture resumed after feedback\n");
        }
      else
        {
          printf("VelaGuard audio: capture resume failed\n");
        }
    }
}

/* Continuous MIC capture/analysis session.  Started by VG_AUDIO_CMD_START_MIC
 * and kept alive until VG_AUDIO_CMD_STOP_MIC.  A feedback request arriving
 * during the session pauses capture, plays the WAV, then resumes capture. */

static void vg_audio_capture_session(void)
{
  struct vg_audio_cmd_s cmd;
  struct vg_audio_level_s level;
  enum vg_audio_keyword_e keyword;
  uint32_t sequence;
  uint32_t report_tick = 0;
  int start_ret;
  bool session_active = true;

  start_ret = vg_audio_capture_start();
  vg_audio_task_post_event(VG_AUDIO_EVT_CAPTURE_READY, start_ret);

  if (start_ret != 0)
    {
      printf("VelaGuard audio: microphone capture unavailable\n");
      return;
    }

  while (session_active)
    {
      if (vg_audio_capture_level(&level, &sequence, &keyword))
        {
          report_tick += 32;
          if (report_tick >= 1000)
            {
              printf("VelaGuard MIC: seq=%lu dc=%ld mean=%lu peak=%u "
                     "voice=%d\n",
                     (unsigned long)sequence,
                     (long)level.dc,
                     (unsigned long)level.mean_abs,
                     level.peak,
                     level.speech_active ? 1 : 0);
              report_tick = 0;
            }

          /* Report every keyword frame; the main task applies the
           * state/rearm gating before raising a voice SOS. */

          if (keyword != VG_AUDIO_KEYWORD_NONE)
            {
              vg_audio_task_post_event(VG_AUDIO_EVT_KEYWORD, (int)keyword);
            }
        }

      if (vg_audio_task_try_recv_cmd(&cmd))
        {
          switch (cmd.type)
            {
              case VG_AUDIO_CMD_STOP_MIC:
                session_active = false;
                break;

              case VG_AUDIO_CMD_PLAY_FEEDBACK:
                vg_audio_task_play_feedback_now(cmd.arg, true);
                break;

              default:
                break;
            }
        }
      else
        {
          usleep(VG_AUDIO_SESSION_POLL_US);
        }
    }

  vg_audio_capture_stop();
}

/* Audio task body.  Idle: blocked on the command queue. */

static int vg_audio_task_entry(int argc, FAR char *argv[])
{
  struct vg_audio_cmd_s cmd;
  ssize_t r;

  (void)argc;
  (void)argv;

  g_cmd_recv_mq = mq_open(VG_AUDIO_CMD_QNAME, O_RDONLY);
  g_evt_send_mq = mq_open(VG_AUDIO_EVT_QNAME, O_WRONLY | O_NONBLOCK);
  if (g_cmd_recv_mq < 0 || g_evt_send_mq < 0)
    {
      printf("VelaGuard audio: task queue open failed (%d)\n", errno);
      return -errno;
    }

  printf("VelaGuard audio: task started\n");

  for (; ; )
    {
      r = mq_receive(g_cmd_recv_mq, (FAR char *)&cmd, sizeof(cmd), NULL);
      if (r < 0)
        {
          /* No permanent failure expected; retry after a short delay. */

          usleep(100000);
          continue;
        }

      switch (cmd.type)
        {
          case VG_AUDIO_CMD_START_MIC:
            vg_audio_capture_session();
            break;

          case VG_AUDIO_CMD_STOP_MIC:
            vg_audio_capture_stop();   /* no-op when capture is not running */
            break;

          case VG_AUDIO_CMD_PLAY_FEEDBACK:
            vg_audio_task_play_feedback_now(cmd.arg, false);
            break;

          default:
            break;
        }
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int vg_audio_task_start(void)
{
  struct mq_attr attr;
  pid_t pid;

  if (g_task_started)
    {
      return 0;
    }

  memset(&attr, 0, sizeof(attr));
  attr.mq_maxmsg  = VG_AUDIO_MQ_MAXMSG;
  attr.mq_msgsize = sizeof(struct vg_audio_cmd_s);

  g_cmd_send_mq = mq_open(VG_AUDIO_CMD_QNAME,
                          O_WRONLY | O_CREAT | O_NONBLOCK, 0666, &attr);
  if (g_cmd_send_mq < 0)
    {
      printf("VelaGuard audio: create command queue failed (%d)\n", errno);
      return -errno;
    }

  attr.mq_maxmsg  = VG_AUDIO_MQ_MAXMSG;
  attr.mq_msgsize = sizeof(struct vg_audio_evt_s);

  g_evt_recv_mq = mq_open(VG_AUDIO_EVT_QNAME,
                          O_RDONLY | O_CREAT, 0666, &attr);
  if (g_evt_recv_mq < 0)
    {
      printf("VelaGuard audio: create event queue failed (%d)\n", errno);
      mq_close(g_cmd_send_mq);
      g_cmd_send_mq = -1;
      return -errno;
    }

  pid = task_create("vg_audio",
                    CONFIG_CONTEST2026_148_AUDIO_TASK_PRIORITY,
                    CONFIG_CONTEST2026_148_AUDIO_TASK_STACKSIZE,
                    vg_audio_task_entry, NULL);
  if (pid < 0)
    {
      printf("VelaGuard audio: task create failed (%d)\n", errno);
      mq_close(g_evt_recv_mq);
      mq_close(g_cmd_send_mq);
      g_evt_recv_mq = -1;
      g_cmd_send_mq = -1;
      return -errno;
    }

  g_task_started = true;
  printf("VelaGuard audio: task created pid=%d\n", pid);
  return 0;
}

int vg_audio_task_send_cmd(uint32_t type, int32_t arg)
{
  struct vg_audio_cmd_s cmd;

  if (g_cmd_send_mq < 0)
    {
      return -ENODEV;
    }

  cmd.type = type;
  cmd.arg  = arg;
  if (mq_send(g_cmd_send_mq, (FAR char *)&cmd, sizeof(cmd), 0) < 0)
    {
      return -errno;
    }

  return 0;
}

int vg_audio_task_play_feedback(int type)
{
  return vg_audio_task_send_cmd(VG_AUDIO_CMD_PLAY_FEEDBACK, type);
}

bool vg_audio_task_get_event(struct vg_audio_evt_s *evt)
{
  struct timespec ts;
  ssize_t r;

  if (g_evt_recv_mq < 0)
    {
      return false;
    }

  memset(&ts, 0, sizeof(ts));
  r = mq_timedreceive(g_evt_recv_mq, (FAR char *)evt, sizeof(*evt),
                      NULL, &ts);
  return r == (ssize_t)sizeof(*evt);
}

int vg_audio_task_wait_capture_ready(int timeout_ms)
{
  struct vg_audio_evt_s evt;

  while (timeout_ms > 0)
    {
      if (vg_audio_task_get_event(&evt))
        {
          if (evt.type == VG_AUDIO_EVT_CAPTURE_READY)
            {
              return evt.arg;
            }

          /* Ignore unrelated events (none expected during bring-up). */

          continue;
        }

      usleep(10000);
      timeout_ms -= 10;
    }

  return -ETIMEDOUT;
}
