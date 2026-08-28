/****************************************************************************
 * VelaGuard PCM front-end: DC removal and basic voice activity measurement.
 *
 * Uses the NuttX standard audio interface to capture from the SiFli
 * lower-half driver (sf32lb_audio.c) at 16 kHz mono 16-bit PCM via
 * /dev/audio/pcm0c.  Buffers are delivered asynchronously through a POSIX
 * message queue and polled non-blocking in the main loop.
 ****************************************************************************/

#include "velaguard_audio.h"
#include "vg_denoise.h"

#include <nuttx/config.h>
#include <nuttx/audio/audio.h>

#include <errno.h>
#include <fcntl.h>
#include <mqueue.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#define DEVICE_CAPT              "/dev/audio/pcm0c"
#define MQ_NAME                  "velaguard_audio_mq"
#define SAMPLE_RATE              16000
#define VG_AUDIO_SPEECH_MEAN_ABS 200
#define VG_AUDIO_SPEECH_PEAK     700
#define VG_AUDIO_MAX_BUFS        4

/* Application-level capture gain: the driver defaults to +30 dB (volume
 * 1000); back off 3 dB to +27 dB to avoid clipping on loud speech.
 * Volume uses the OpenVela 0..1000 convention.
 */

#define REC_CAPTURE_GAIN         967     /* +27 dB */

static int g_vg_audio_fd = -1;
static mqd_t g_vg_audio_mq = -1;
static struct ap_buffer_s *g_vg_audio_bufs[VG_AUDIO_MAX_BUFS];
static unsigned int g_vg_audio_nbufs;
static uint32_t g_vg_audio_sequence;
static bool g_vg_audio_started;
static vg_denoise_s *g_vg_denoise;

/****************************************************************************
 * Keyword model integration point (weak, override with offline KWS model)
 ****************************************************************************/

__attribute__((weak))
enum vg_audio_keyword_e vg_audio_kws_infer(const int16_t *samples,
                                           size_t count,
                                           uint32_t sample_rate)
{
  (void)samples;
  (void)count;
  (void)sample_rate;
  return VG_AUDIO_KEYWORD_NONE;
}

/****************************************************************************
 * audio_config
 ****************************************************************************/

static int audio_config(int fd)
{
  struct audio_caps_s caps;
  memset(&caps, 0, sizeof(caps));
  caps.ac_len        = sizeof(caps);
  caps.ac_type       = AUDIO_TYPE_INPUT;
  caps.ac_channels   = 1;
  caps.ac_format.hw  = AUDIO_FMT_PCM;
  caps.ac_controls.hw[0] = SAMPLE_RATE;
  return ioctl(fd, AUDIOIOC_CONFIGURE, &caps);
}

/****************************************************************************
 * set_capture_gain — set capture (MIC) gain via the standard NuttX/OpenVela
 * audio interface: AUDIOIOC_CONFIGURE + AUDIO_TYPE_FEATURE + AUDIO_FU_VOLUME.
 * value is 0..1000 (same convention as tinycompress).
 ****************************************************************************/

static int set_capture_gain(int fd, unsigned int gain)
{
  struct audio_caps_desc_s caps_desc;

  memset(&caps_desc, 0, sizeof(caps_desc));
  caps_desc.caps.ac_len            = sizeof(struct audio_caps_s);
  caps_desc.caps.ac_type           = AUDIO_TYPE_FEATURE;
  caps_desc.caps.ac_format.hw      = AUDIO_FU_VOLUME;
  caps_desc.caps.ac_controls.hw[0] = gain;

  return ioctl(fd, AUDIOIOC_CONFIGURE, &caps_desc);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void vg_audio_analyze_pcm16(const int16_t *samples, size_t count,
                            struct vg_audio_level_s *level)
{
  int64_t sum = 0;
  uint64_t abs_sum = 0;
  uint32_t peak = 0;
  int32_t dc;
  size_t i;

  memset(level, 0, sizeof(*level));
  if (samples == NULL || count == 0)
    {
      return;
    }

  for (i = 0; i < count; i++)
    {
      sum += samples[i];
    }

  dc = (int32_t)(sum / (int64_t)count);
  for (i = 0; i < count; i++)
    {
      int32_t value = (int32_t)samples[i] - dc;
      uint32_t magnitude = value < 0 ? (uint32_t)-value : (uint32_t)value;

      abs_sum += magnitude;
      if (magnitude > peak)
        {
          peak = magnitude;
        }
    }

  level->dc = dc;
  level->mean_abs = (uint32_t)(abs_sum / count);
  level->peak = peak > UINT16_MAX ? UINT16_MAX : (uint16_t)peak;
  level->speech_active = level->mean_abs >= VG_AUDIO_SPEECH_MEAN_ABS &&
                         level->peak >= VG_AUDIO_SPEECH_PEAK;
}

int vg_audio_capture_start(void)
{
  struct ap_buffer_info_s bufinfo;
  struct audio_buf_desc_s desc;
  struct mq_attr attr;
  unsigned int i;
  int ret;

  if (g_vg_audio_started)
    {
      return 0;
    }

  /* Open capture device registered by sf32lb_audio_initialize(). */

  g_vg_audio_fd = open(DEVICE_CAPT, O_RDONLY);
  if (g_vg_audio_fd < 0)
    {
      printf("VelaGuard audio: open %s failed: %d\n", DEVICE_CAPT, errno);
      return -ENODEV;
    }

  /* Configure 16 kHz mono PCM. */

  if (audio_config(g_vg_audio_fd) < 0)
    {
      printf("VelaGuard audio: configure failed\n");
      goto err_close;
    }

  /* Apply application-level capture gain: driver default +30 dB minus
   * 3 dB headroom to avoid clipping on loud speech.
   */

  if (set_capture_gain(g_vg_audio_fd, REC_CAPTURE_GAIN) < 0)
    {
      printf("VelaGuard audio: WARNING set capture gain failed\n");
    }

  /* Allocate audio buffers. */

  memset(&bufinfo, 0, sizeof(bufinfo));
  ret = ioctl(g_vg_audio_fd, AUDIOIOC_GETBUFFERINFO, &bufinfo);
  if (ret < 0)
    {
      printf("VelaGuard audio: get buffer info failed\n");
      goto err_close;
    }

  g_vg_audio_nbufs = (unsigned int)bufinfo.nbuffers;
  if (g_vg_audio_nbufs < 1) g_vg_audio_nbufs = 1;
  if (g_vg_audio_nbufs > VG_AUDIO_MAX_BUFS) g_vg_audio_nbufs = VG_AUDIO_MAX_BUFS;

  for (i = 0; i < g_vg_audio_nbufs; i++)
    {
      memset(&desc, 0, sizeof(desc));
      desc.numbytes = (apb_samp_t)bufinfo.buffer_size;
      desc.u.pbuffer = &g_vg_audio_bufs[i];
      ret = ioctl(g_vg_audio_fd, AUDIOIOC_ALLOCBUFFER, &desc);
      if (ret < 0)
        {
          printf("VelaGuard audio: alloc buffer %u failed\n", i);
          goto err_free_bufs;
        }
    }

  /* Create message queue for asynchronous dequeue notifications. */

  attr.mq_maxmsg  = 8;
  attr.mq_msgsize = sizeof(struct audio_msg_s);
  attr.mq_flags   = 0;
  g_vg_audio_mq = mq_open(MQ_NAME, O_RDWR | O_CREAT | O_NONBLOCK, 0666,
                          &attr);
  if (g_vg_audio_mq < 0)
    {
      printf("VelaGuard audio: mq_open failed\n");
      goto err_free_bufs;
    }
  mq_unlink(MQ_NAME);

  ioctl(g_vg_audio_fd, AUDIOIOC_REGISTERMQ,
        (unsigned long)g_vg_audio_mq);

  /* Enqueue empty buffers so the driver can fill them with DMA. */

  for (i = 0; i < g_vg_audio_nbufs; i++)
    {
      memset(&desc, 0, sizeof(desc));
      desc.u.buffer = g_vg_audio_bufs[i];
      ret = ioctl(g_vg_audio_fd, AUDIOIOC_ENQUEUEBUFFER, &desc);
      if (ret < 0)
        {
          printf("VelaGuard audio: enqueue buffer %u failed\n", i);
          goto err_unreg_mq;
        }
    }

  /* Start capture. */

  if (ioctl(g_vg_audio_fd, AUDIOIOC_START, 0) < 0)
    {
      printf("VelaGuard audio: start failed\n");
      goto err_unreg_mq;
    }

  g_vg_audio_sequence = 0;
  g_vg_audio_started = true;

  /* Noise suppression state (no-op when CONFIG_CONTEST2026_148_DENOISE
   * is disabled). */

  g_vg_denoise = vg_denoise_create(SAMPLE_RATE, 256);

  printf("VelaGuard audio: MIC 16kHz/16-bit capture started\n");
  return 0;

err_unreg_mq:
  ioctl(g_vg_audio_fd, AUDIOIOC_UNREGISTERMQ,
        (unsigned long)g_vg_audio_mq);
  mq_close(g_vg_audio_mq);
  g_vg_audio_mq = -1;
err_free_bufs:
  for (i = 0; i < g_vg_audio_nbufs; i++)
    {
      if (g_vg_audio_bufs[i] != NULL)
        {
          memset(&desc, 0, sizeof(desc));
          desc.u.buffer = g_vg_audio_bufs[i];
          ioctl(g_vg_audio_fd, AUDIOIOC_FREEBUFFER, &desc);
          g_vg_audio_bufs[i] = NULL;
        }
    }
err_close:
  close(g_vg_audio_fd);
  g_vg_audio_fd = -1;
  return -EIO;
}

void vg_audio_capture_stop(void)
{
  struct audio_buf_desc_s desc;
  unsigned int i;

  if (!g_vg_audio_started)
    {
      return;
    }

  ioctl(g_vg_audio_fd, AUDIOIOC_STOP, 0);
  ioctl(g_vg_audio_fd, AUDIOIOC_UNREGISTERMQ,
        (unsigned long)g_vg_audio_mq);
  mq_close(g_vg_audio_mq);
  g_vg_audio_mq = -1;

  for (i = 0; i < g_vg_audio_nbufs; i++)
    {
      if (g_vg_audio_bufs[i] != NULL)
        {
          memset(&desc, 0, sizeof(desc));
          desc.u.buffer = g_vg_audio_bufs[i];
          ioctl(g_vg_audio_fd, AUDIOIOC_FREEBUFFER, &desc);
          g_vg_audio_bufs[i] = NULL;
        }
    }

  close(g_vg_audio_fd);
  g_vg_audio_fd = -1;
  g_vg_audio_started = false;

  vg_denoise_destroy(g_vg_denoise);
  g_vg_denoise = NULL;
}

bool vg_audio_capture_level(struct vg_audio_level_s *level,
                            uint32_t *sequence,
                            enum vg_audio_keyword_e *keyword)
{
  struct audio_msg_s msg;
  struct audio_buf_desc_s desc;
  struct timespec ts;
  struct ap_buffer_s *apb;
  ssize_t r;

  if (!g_vg_audio_started)
    {
      return false;
    }

  /* Non-blocking poll: return immediately if no message is ready. */

  memset(&ts, 0, sizeof(ts));
  r = mq_timedreceive(g_vg_audio_mq, (FAR char *)&msg, sizeof(msg),
                      NULL, &ts);
  if (r < 0)
    {
      return false;
    }

  if (msg.msg_id == AUDIO_MSG_DEQUEUE)
    {
      apb = (struct ap_buffer_s *)msg.u.ptr;
      unsigned int nsamp = (unsigned int)apb->nbytes / sizeof(int16_t);

      g_vg_audio_sequence++;

      /* Denoise before analysis / keyword inference (in-place; no-op when
       * denoise is disabled). */

      if (g_vg_denoise != NULL)
        {
          vg_denoise_run(g_vg_denoise, (int16_t *)apb->samp, nsamp);
        }

      vg_audio_analyze_pcm16((const int16_t *)apb->samp, nsamp, level);

      if (keyword != NULL)
        {
          *keyword = level->speech_active ?
            vg_audio_kws_infer((const int16_t *)apb->samp, nsamp,
                               SAMPLE_RATE) : VG_AUDIO_KEYWORD_NONE;
        }

      if (sequence != NULL)
        {
          *sequence = g_vg_audio_sequence;
        }

      /* Re-enqueue buffer for the next DMA cycle. */

      memset(&desc, 0, sizeof(desc));
      desc.u.buffer = apb;
      ioctl(g_vg_audio_fd, AUDIOIOC_ENQUEUEBUFFER, &desc);

      return true;
    }

  /* Re-enqueue on error / complete so the DMA pipeline stays alive. */

  if (msg.u.ptr != NULL)
    {
      memset(&desc, 0, sizeof(desc));
      desc.u.buffer = (struct ap_buffer_s *)msg.u.ptr;
      ioctl(g_vg_audio_fd, AUDIOIOC_ENQUEUEBUFFER, &desc);
    }

  return false;
}
