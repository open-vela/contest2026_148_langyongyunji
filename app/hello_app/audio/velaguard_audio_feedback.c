/****************************************************************************
 * VelaGuard audio feedback — async WAV playback state machine.
 *
 * Non-blocking: vg_audio_feedback_trigger() sets a pending type, and
 * vg_audio_feedback_process() drives playback step by step in the main
 * loop without blocking.
 ****************************************************************************/

#include "velaguard_audio_feedback.h"
#include "velaguard_wav_parser.h"

#include <nuttx/config.h>
#include <nuttx/audio/audio.h>

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <mqueue.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define DEVICE_PLAY     "/dev/audio/pcm0p"
#define MQ_NAME         "vgfb_mq"
#define SAMPLE_RATE     16000
#define MAX_BUFS        4
#define PCM_BUF_SIZE    (64 * 1024)   /* ~2 s of 16-bit mono */

#define PATH_SUCCESS    "/etc/data/audio/success.wav"
#define PATH_FAILURE    "/etc/data/audio/failure.wav"

/****************************************************************************
 * Private Types
 ****************************************************************************/

enum vg_feedback_state_e
{
  VG_FBSTATE_IDLE = 0,
  VG_FBSTATE_LOADING,
  VG_FBSTATE_PLAYING,
  VG_FBSTATE_CLEANUP,
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static enum vg_feedback_state_e g_state = VG_FBSTATE_IDLE;
static enum vg_feedback_type_e  g_pending;

static int          g_dev_fd = -1;
static mqd_t        g_mq = -1;
static struct ap_buffer_s *g_bufs[MAX_BUFS];
static unsigned int g_allocated;

static uint8_t      g_pcm_buf[PCM_BUF_SIZE];
static uint32_t     g_pcm_total;       /* Total PCM bytes */
static uint32_t     g_pcm_done;        /* Bytes already enqueued */

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int audio_config(int fd)
{
  struct audio_caps_s caps;

  memset(&caps, 0, sizeof(caps));
  caps.ac_len        = sizeof(caps);
  caps.ac_type       = AUDIO_TYPE_OUTPUT;
  caps.ac_channels   = 1;
  caps.ac_format.hw  = AUDIO_FMT_PCM;
  caps.ac_controls.hw[0] = SAMPLE_RATE;

  return ioctl(fd, AUDIOIOC_CONFIGURE, &caps);
}

static void do_cleanup(void)
{
  struct audio_buf_desc_s desc;
  unsigned int i;

  if (g_dev_fd >= 0)
    {
      ioctl(g_dev_fd, AUDIOIOC_STOP, 0);
    }

  if (g_mq >= 0)
    {
      if (g_dev_fd >= 0)
        {
          ioctl(g_dev_fd, AUDIOIOC_UNREGISTERMQ,
                (unsigned long)g_mq);
        }

      mq_close(g_mq);
      g_mq = -1;
    }

  for (i = 0; i < g_allocated; i++)
    {
      if (g_bufs[i] != NULL)
        {
          memset(&desc, 0, sizeof(desc));
          desc.u.buffer = g_bufs[i];
          if (g_dev_fd >= 0)
            {
              ioctl(g_dev_fd, AUDIOIOC_FREEBUFFER, &desc);
            }

          g_bufs[i] = NULL;
        }
    }

  g_allocated = 0;

  if (g_dev_fd >= 0)
    {
      close(g_dev_fd);
      g_dev_fd = -1;
    }

  g_state     = VG_FBSTATE_IDLE;
  g_pending   = VG_FEEDBACK_NONE;
  g_pcm_total = 0;
  g_pcm_done  = 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int vg_audio_feedback_trigger(enum vg_feedback_type_e type)
{
  if (type == VG_FEEDBACK_NONE)
    {
      return -EINVAL;
    }

  if (g_state != VG_FBSTATE_IDLE)
    {
      return 0;
    }

  g_pending = type;
  g_state   = VG_FBSTATE_LOADING;

  return 0;
}

void vg_audio_feedback_process(void)
{
  struct audio_msg_s msg;
  struct audio_buf_desc_s desc;
  struct ap_buffer_info_s bufinfo;
  struct mq_attr attr;
  struct timespec ts;
  ssize_t r;
  int ret;

  switch (g_state)
    {
    case VG_FBSTATE_IDLE:
      return;

    /* ── LOADING ─────────────────────────────────────────── */

    case VG_FBSTATE_LOADING:
      {
        struct vg_wav_info_s info;
        const char *wav_path;

        wav_path = (g_pending == VG_FEEDBACK_SUCCESS) ?
                   PATH_SUCCESS : PATH_FAILURE;

        ret = vg_wav_parse(wav_path, &info,
                           g_pcm_buf, sizeof(g_pcm_buf));
        if (ret < 0)
          {
            printf("VelaGuard feedback: WAV parse failed, skip\n");
            do_cleanup();
            return;
          }

        g_pcm_total = info.data_size;
        g_pcm_done  = 0;

        /* Open playback device. */

        g_dev_fd = open(DEVICE_PLAY, O_WRONLY);
        if (g_dev_fd < 0)
          {
            printf("VelaGuard feedback: open %s failed: %d\n",
                   DEVICE_PLAY, errno);
            do_cleanup();
            return;
          }

        if (audio_config(g_dev_fd) < 0)
          {
            printf("VelaGuard feedback: configure failed\n");
            do_cleanup();
            return;
          }

        /* Allocate DMA buffers. */

        memset(&bufinfo, 0, sizeof(bufinfo));
        ret = ioctl(g_dev_fd, AUDIOIOC_GETBUFFERINFO, &bufinfo);
        if (ret < 0)
          {
            printf("VelaGuard feedback: bufinfo failed\n");
            do_cleanup();
            return;
          }

        {
          unsigned int nbufs;

          nbufs = (unsigned int)bufinfo.nbuffers;
          if (nbufs > MAX_BUFS) nbufs = MAX_BUFS;
          if (nbufs < 1) nbufs = 1;

          for (g_allocated = 0; g_allocated < nbufs; g_allocated++)
            {
              memset(&desc, 0, sizeof(desc));
              desc.numbytes  = (apb_samp_t)bufinfo.buffer_size;
              desc.u.pbuffer = &g_bufs[g_allocated];
              if (ioctl(g_dev_fd, AUDIOIOC_ALLOCBUFFER, &desc) < 0)
                {
                  break;
                }
            }
        }

        if (g_allocated < 1)
          {
            printf("VelaGuard feedback: no buffers\n");
            do_cleanup();
            return;
          }

        /* Message queue. */

        attr.mq_maxmsg  = 8;
        attr.mq_msgsize = sizeof(struct audio_msg_s);
        attr.mq_flags   = 0;
        g_mq = mq_open(MQ_NAME,
                       O_RDWR | O_CREAT | O_NONBLOCK, 0666, &attr);
        if (g_mq < 0)
          {
            printf("VelaGuard feedback: mq_open failed\n");
            do_cleanup();
            return;
          }

        mq_unlink(MQ_NAME);
        ioctl(g_dev_fd, AUDIOIOC_REGISTERMQ, (unsigned long)g_mq);

        /* Fill initial buffers. */

        {
          unsigned int i;
          uint32_t offset = 0;

          for (i = 0; i < g_allocated; i++)
            {
              uint32_t per_bytes =
                (uint32_t)g_bufs[i]->nmaxbytes / sizeof(int16_t)
                * sizeof(int16_t);
              uint32_t remaining = g_pcm_total - offset;
              uint32_t to_copy = per_bytes < remaining ?
                                 per_bytes : remaining;

              memcpy(g_bufs[i]->samp, g_pcm_buf + offset, to_copy);
              g_bufs[i]->nbytes = to_copy;
              offset += to_copy;

              memset(&desc, 0, sizeof(desc));
              desc.u.buffer = g_bufs[i];
              ret = ioctl(g_dev_fd, AUDIOIOC_ENQUEUEBUFFER, &desc);
              if (ret < 0)
                {
                  printf("VelaGuard feedback: enqueue failed\n");
                  do_cleanup();
                  return;
                }

              g_pcm_done += to_copy;
            }
        }

        if (ioctl(g_dev_fd, AUDIOIOC_START, 0) < 0)
          {
            printf("VelaGuard feedback: start failed\n");
            do_cleanup();
            return;
          }

        printf("VelaGuard feedback: playing %s (%" PRIu32 " B)\n",
               wav_path, g_pcm_total);

        g_state = VG_FBSTATE_PLAYING;
        return;
      }

    /* ── PLAYING ─────────────────────────────────────────── */

    case VG_FBSTATE_PLAYING:
      memset(&ts, 0, sizeof(ts));
      r = mq_timedreceive(g_mq, (FAR char *)&msg, sizeof(msg),
                          NULL, &ts);
      if (r < 0)
        {
          return;   /* No message ready — try next iteration */
        }

      if (msg.msg_id == AUDIO_MSG_DEQUEUE)
        {
          struct ap_buffer_s *apb;

          apb = (struct ap_buffer_s *)msg.u.ptr;

          if (g_pcm_done < g_pcm_total)
            {
              uint32_t per_bytes =
                (uint32_t)apb->nmaxbytes / sizeof(int16_t)
                * sizeof(int16_t);
              uint32_t remaining = g_pcm_total - g_pcm_done;
              uint32_t to_copy = per_bytes < remaining ?
                                 per_bytes : remaining;

              memcpy(apb->samp, g_pcm_buf + g_pcm_done, to_copy);
              apb->nbytes = to_copy;
              g_pcm_done += to_copy;

              memset(&desc, 0, sizeof(desc));
              desc.u.buffer = apb;
              ioctl(g_dev_fd, AUDIOIOC_ENQUEUEBUFFER, &desc);
            }

          /* Check completion. */

          if (g_pcm_done >= g_pcm_total)
            {
              g_state = VG_FBSTATE_CLEANUP;
            }
        }
      else if (msg.msg_id == AUDIO_MSG_COMPLETE)
        {
          g_state = VG_FBSTATE_CLEANUP;
        }

      return;

    /* ── CLEANUP ─────────────────────────────────────────── */

    case VG_FBSTATE_CLEANUP:
      printf("VelaGuard feedback: done\n");
      do_cleanup();
      return;
    }
}
