/****************************************************************************
 * packages/demos/contest2026_148_hello_app/audio/audio_test.c
 *
 * Audio test NSH command — uses NuttX standard audio API with message queue
 * callbacks for continuous recording and playback.
 *
 * Usage:
 *   audio_test               Record then playback (default 3s)
 *   audio_test rec [ms]      Record for <ms> then playback
 *   audio_test tone [ms]     Play a 1 kHz tone
 *
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/ioctl.h>
#include <errno.h>
#include <fcntl.h>
#include <mqueue.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <nuttx/audio/audio.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define SAMPLE_RATE          16000
#define DEVICE_PLAY          "/dev/audio/pcm0p"
#define DEVICE_CAPT          "/dev/audio/pcm0c"
#define DEFAULT_DURATION_MS  3000
#define TONE_FREQ            1000
#define MQ_NAME              "audio_test_mq"

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int audio_config(int fd, uint8_t type)
{
  struct audio_caps_s caps;
  memset(&caps, 0, sizeof(caps));
  caps.ac_len        = sizeof(caps);
  caps.ac_type       = type;
  caps.ac_channels   = 1;
  caps.ac_format.hw  = AUDIO_FMT_PCM;
  caps.ac_controls.hw[0] = SAMPLE_RATE;
  return ioctl(fd, AUDIOIOC_CONFIGURE, &caps);
}

/****************************************************************************
 * do_tone
 ****************************************************************************/

static int do_tone(unsigned int duration_ms)
{
  struct ap_buffer_info_s bufinfo;
  struct audio_buf_desc_s desc;
  struct ap_buffer_s *bufs[4];
  mqd_t mq;
  struct mq_attr attr;
  struct sigevent event;
  FAR int16_t *samp;
  unsigned int nbufs, bufsize, allocated;
  unsigned int i, n, total_samp, done = 0;
  int fd, ret = -1;

  for (n = 0; n < 4; n++) bufs[n] = NULL;
  printf("[tone] === 1 kHz Tone Test ===\n");

  fd = open(DEVICE_PLAY, O_WRONLY);
  if (fd < 0) { printf("[tone] open err\n"); return -1; }

  memset(&bufinfo, 0, sizeof(bufinfo));
  ret = ioctl(fd, AUDIOIOC_GETBUFFERINFO, &bufinfo);
  if (ret < 0) goto err_close;
  nbufs = (unsigned int)bufinfo.nbuffers; if (nbufs < 1) nbufs = 1;
  if (nbufs > 4) nbufs = 4;
  bufsize = (unsigned int)bufinfo.buffer_size;

  if (audio_config(fd, AUDIO_TYPE_OUTPUT) < 0) goto err_close;

  /* Allocate buffers */
  for (n = 0; n < nbufs; n++)
    {
      memset(&desc, 0, sizeof(desc));
      desc.numbytes = (apb_samp_t)bufsize;
      desc.u.pbuffer = &bufs[n];
      ret = ioctl(fd, AUDIOIOC_ALLOCBUFFER, &desc);
      if (ret < 0) break;
    }
  allocated = n;
  if (allocated < 1)
    { printf("[tone] need >=1 buf\n"); goto err_close; }

  /* Create message queue */
  attr.mq_maxmsg  = 8;
  attr.mq_msgsize = sizeof(struct audio_msg_s);
  attr.mq_flags   = 0;
  mq = mq_open(MQ_NAME, O_RDWR | O_CREAT, 0666, &attr);
  if (mq < 0) { printf("[tone] mq_open err\n"); goto err_free_bufs; }
  mq_unlink(MQ_NAME);

  ioctl(fd, AUDIOIOC_REGISTERMQ, (unsigned long)mq);

  /* Fill all buffers with tone data and enqueue */
  total_samp = SAMPLE_RATE * duration_ms / 1000;
  for (n = 0; n < allocated; n++)
    {
      samp = (FAR int16_t *)bufs[n]->samp;
      unsigned int per = (unsigned int)bufs[n]->nmaxbytes / sizeof(int16_t);
      for (i = 0; i < per; i++)
        samp[i] = (int16_t)(sin(2.0 * M_PI * TONE_FREQ * i / SAMPLE_RATE)
                            * 32767.0);
      bufs[n]->nbytes = per;
      memset(&desc, 0, sizeof(desc));
      desc.u.buffer = bufs[n];
      ret = ioctl(fd, AUDIOIOC_ENQUEUEBUFFER, &desc);
      if (ret < 0) goto err_free_mq;
      done += per;
    }

  if (ioctl(fd, AUDIOIOC_START, 0) < 0) goto err_free_mq;
  printf("[tone] playing %u ms...\n", duration_ms);
  fflush(stdout);

  /* Feed more data as buffers are consumed, until all samples played */
  while (done < total_samp)
    {
      struct audio_msg_s msg;
      ssize_t r = mq_receive(mq, (FAR char *)&msg, sizeof(msg), NULL);
      if (r < 0) break;

      if (msg.msg_id == AUDIO_MSG_DEQUEUE)
        {
          struct ap_buffer_s *apb = (struct ap_buffer_s *)msg.u.ptr;
          samp = (FAR int16_t *)apb->samp;
          unsigned int per = (unsigned int)apb->nmaxbytes / sizeof(int16_t);
          for (i = 0; i < per; i++)
            samp[i] = (int16_t)(sin(2.0 * M_PI * TONE_FREQ *
                                     (done + i) / SAMPLE_RATE) * 32767.0);
          apb->nbytes = per;
          done += per;
          memset(&desc, 0, sizeof(desc));
          desc.u.buffer = apb;
          ioctl(fd, AUDIOIOC_ENQUEUEBUFFER, &desc);
        }
      else if (msg.msg_id == AUDIO_MSG_COMPLETE)
        {
          break;
        }
    }

  ioctl(fd, AUDIOIOC_STOP, 0);
  printf("[tone] done %u samp\n", done);

err_free_mq:
  ioctl(fd, AUDIOIOC_UNREGISTERMQ, (unsigned long)mq);
  mq_close(mq);
err_free_bufs:
  for (n = 0; n < allocated; n++)
    {
      memset(&desc, 0, sizeof(desc));
      desc.u.buffer = bufs[n];
      ioctl(fd, AUDIOIOC_FREEBUFFER, &desc);
    }
err_close:
  close(fd);
  return ret;
}

/****************************************************************************
 * do_record — continuous recording then playback via mq callbacks
 ****************************************************************************/

static int do_record(unsigned int duration_ms)
{
  struct ap_buffer_info_s play_info, cap_info;
  struct audio_buf_desc_s desc;
  struct ap_buffer_s *bufs[4];
  FAR int16_t *rec_data = NULL;
  unsigned int nbufs, bufsize, allocated;
  unsigned int total_samp, rec_samp = 0, play_samp;
  unsigned int n;
  int fd, ret = -1;
  mqd_t mq;

  for (n = 0; n < 4; n++) bufs[n] = NULL;
  printf("[record] === Record then Playback ===\n");

  total_samp = SAMPLE_RATE * duration_ms / 1000;
  rec_data = malloc(total_samp * sizeof(int16_t));
  if (!rec_data) { printf("[record] malloc err\n"); return -1; }

  /* ── Phase 1: Continuous Capture via mq ── */

  fd = open(DEVICE_CAPT, O_RDONLY);
  if (fd < 0) { printf("[record] ERROR open cap\n"); free(rec_data); return -1; }

  memset(&cap_info, 0, sizeof(cap_info));
  if (ioctl(fd, AUDIOIOC_GETBUFFERINFO, &cap_info) < 0)
    { printf("[record] ERROR bufinfo cap\n"); free(rec_data); goto close_cap; }
  nbufs = (unsigned int)cap_info.nbuffers; if (nbufs < 1) nbufs = 1;
  if (nbufs > 4) nbufs = 4;
  bufsize = (unsigned int)cap_info.buffer_size;

  if (audio_config(fd, AUDIO_TYPE_INPUT) < 0)
    { printf("[record] ERROR config cap\n"); free(rec_data); goto close_cap; }

  /* Allocate buffers */
  for (n = 0; n < nbufs; n++)
    {
      memset(&desc, 0, sizeof(desc));
      desc.numbytes = (apb_samp_t)bufsize;
      desc.u.pbuffer = &bufs[n];
      ret = ioctl(fd, AUDIOIOC_ALLOCBUFFER, &desc);
      if (ret < 0) break;
    }
  allocated = n;
  if (allocated < 1)
    { printf("[record] need >=1 buf\n"); free(rec_data); goto close_cap; }

  printf("[record] cap configured, %u bufs x %u B\n", allocated, bufsize);
  fflush(stdout);

  /* Create message queue */
  {
    struct mq_attr attr;
    attr.mq_maxmsg  = 8;
    attr.mq_msgsize = sizeof(struct audio_msg_s);
    attr.mq_flags   = 0;
    mq = mq_open(MQ_NAME, O_RDWR | O_CREAT, 0666, &attr);
    if (mq < 0)
      { printf("[record] mq_open err\n"); free(rec_data); goto free_cap_bufs; }
    mq_unlink(MQ_NAME);
  }

  ioctl(fd, AUDIOIOC_REGISTERMQ, (unsigned long)mq);

  /* Enqueue empty buffers for driver to fill */
  for (n = 0; n < allocated; n++)
    {
      memset(&desc, 0, sizeof(desc));
      desc.u.buffer = bufs[n];
      ret = ioctl(fd, AUDIOIOC_ENQUEUEBUFFER, &desc);
      if (ret < 0)
        {
          printf("[record] ERROR enq cap buf %u: %d\n", n, get_errno());
          free(rec_data);
          goto unreg_cap_mq;
        }
    }

  if (ioctl(fd, AUDIOIOC_START, 0) < 0)
    { printf("[record] ERROR START cap\n"); free(rec_data); goto unreg_cap_mq; }

  printf("[record] recording...\n");
  fflush(stdout);

  /* Receive filled buffers via mq, copy data, re-enqueue */
  while (rec_samp < total_samp)
    {
      struct audio_msg_s msg;
      ssize_t r = mq_receive(mq, (FAR char *)&msg, sizeof(msg), NULL);
      if (r < 0) break;

      if (msg.msg_id == AUDIO_MSG_DEQUEUE)
        {
          struct ap_buffer_s *apb = (struct ap_buffer_s *)msg.u.ptr;
          unsigned int nb = (unsigned int)apb->nbytes / sizeof(int16_t);
          unsigned int remaining = total_samp - rec_samp;
          unsigned int to_copy = nb < remaining ? nb : remaining;

          memcpy(rec_data + rec_samp, apb->samp, to_copy * sizeof(int16_t));
          rec_samp += to_copy;

          /* Re-enqueue buffer unless we have enough */
          if (rec_samp < total_samp)
            {
              memset(&desc, 0, sizeof(desc));
              desc.u.buffer = apb;
              ioctl(fd, AUDIOIOC_ENQUEUEBUFFER, &desc);
            }
        }
      else if (msg.msg_id == AUDIO_MSG_COMPLETE)
        {
          break;
        }
    }

  ioctl(fd, AUDIOIOC_STOP, 0);
  printf("[record] captured %u samples (%u ms)\n",
         rec_samp, rec_samp * 1000 / SAMPLE_RATE);

  /* Diagnostic */
  {
    FAR int16_t *s = rec_data;
    unsigned int k;
    printf("[record] DIAG first 16:\n  ");
    for (k = 0; k < 16 && k < rec_samp; k++)
      printf("%6d ", s[k]);
    printf("\n");
    if (rec_samp > 16384)
      {
        unsigned int mid = rec_samp / 2;
        printf("[record] DIAG mid (sample %u):\n  ", mid);
        for (k = 0; k < 16; k++)
          printf("%6d ", s[mid + k]);
        printf("\n");
      }
  }
  fflush(stdout);

unreg_cap_mq:
  ioctl(fd, AUDIOIOC_UNREGISTERMQ, (unsigned long)mq);
  mq_close(mq);
free_cap_bufs:
  for (n = 0; n < allocated; n++)
    {
      memset(&desc, 0, sizeof(desc));
      desc.u.buffer = bufs[n];
      ioctl(fd, AUDIOIOC_FREEBUFFER, &desc);
      bufs[n] = NULL;
    }
close_cap:
  close(fd);

  if (rec_samp == 0)
    { printf("[record] nothing to play\n"); free(rec_data); return -1; }

  /* ── Phase 2: Continuous Playback via mq ── */

  for (n = 0; n < 4; n++) bufs[n] = NULL;
  printf("[record] --- playback ---\n");

  fd = open(DEVICE_PLAY, O_WRONLY);
  if (fd < 0) { printf("[record] ERROR open play\n"); free(rec_data); return -1; }

  memset(&play_info, 0, sizeof(play_info));
  if (ioctl(fd, AUDIOIOC_GETBUFFERINFO, &play_info) < 0)
    { printf("[record] ERROR bufinfo play\n"); free(rec_data); goto close_play; }
  nbufs = (unsigned int)play_info.nbuffers; if (nbufs < 1) nbufs = 1;
  if (nbufs > 4) nbufs = 4;
  bufsize = (unsigned int)play_info.buffer_size;

  printf("[record] play bufinfo: nbufs=%u bufsize=%u\n", nbufs, bufsize);
  fflush(stdout);

  if (audio_config(fd, AUDIO_TYPE_OUTPUT) < 0)
    { printf("[record] ERROR config play\n"); free(rec_data); goto close_play; }

  for (n = 0; n < nbufs; n++)
    {
      memset(&desc, 0, sizeof(desc));
      desc.numbytes = (apb_samp_t)bufsize;
      desc.u.pbuffer = &bufs[n];
      ret = ioctl(fd, AUDIOIOC_ALLOCBUFFER, &desc);
      if (ret < 0) break;
    }
  allocated = n;
  if (allocated < 1)
    { printf("[record] need >=1 play buf\n"); free(rec_data); goto close_play; }

  {
    struct mq_attr attr;
    attr.mq_maxmsg  = 8;
    attr.mq_msgsize = sizeof(struct audio_msg_s);
    attr.mq_flags   = 0;
    mq = mq_open(MQ_NAME, O_RDWR | O_CREAT, 0666, &attr);
    if (mq < 0)
      { printf("[record] mq_open play err\n"); free(rec_data); goto free_play_bufs; }
    mq_unlink(MQ_NAME);
  }

  ioctl(fd, AUDIOIOC_REGISTERMQ, (unsigned long)mq);

  /* Fill initial buffers and enqueue */
  play_samp = 0;
  for (n = 0; n < allocated; n++)
    {
      unsigned int per = (unsigned int)bufs[n]->nmaxbytes / sizeof(int16_t);
      unsigned int remaining = rec_samp - play_samp;
      unsigned int to_copy = per < remaining ? per : remaining;
      memcpy(bufs[n]->samp, rec_data + play_samp,
             to_copy * sizeof(int16_t));
      bufs[n]->nbytes = to_copy;
      play_samp += to_copy;

      printf("[record] PLAY buf %u: samples=%u\n", n, to_copy);
      fflush(stdout);

      memset(&desc, 0, sizeof(desc));
      desc.u.buffer = bufs[n];
      ret = ioctl(fd, AUDIOIOC_ENQUEUEBUFFER, &desc);
      if (ret < 0)
        {
          printf("[record] ERROR enq play buf %u: %d\n", n, get_errno());
          free(rec_data);
          goto unreg_play_mq;
        }
    }

  /* DIAG: first 16 samples of first buffer */
  {
    FAR int16_t *s = (FAR int16_t *)bufs[0]->samp;
    unsigned int k;
    printf("[record] PLAY DIAG first 16:\n  ");
    for (k = 0; k < 16; k++)
      printf("%d ", s[k]);
    printf("\n");
    fflush(stdout);
  }

  if (ioctl(fd, AUDIOIOC_START, 0) < 0)
    { printf("[record] ERROR START play\n"); free(rec_data); goto unreg_play_mq; }

  printf("[record] playing...\n");
  fflush(stdout);

  /* Feed remaining data via mq callbacks */
  while (play_samp < rec_samp)
    {
      struct audio_msg_s msg;
      ssize_t r = mq_receive(mq, (FAR char *)&msg, sizeof(msg), NULL);
      if (r < 0) break;

      if (msg.msg_id == AUDIO_MSG_DEQUEUE)
        {
          struct ap_buffer_s *apb = (struct ap_buffer_s *)msg.u.ptr;
          unsigned int per = (unsigned int)apb->nmaxbytes / sizeof(int16_t);
          unsigned int remaining = rec_samp - play_samp;
          unsigned int to_copy = per < remaining ? per : remaining;

          if (to_copy == 0) break;

          memcpy(apb->samp, rec_data + play_samp,
                 to_copy * sizeof(int16_t));
          apb->nbytes = to_copy;
          play_samp += to_copy;

          memset(&desc, 0, sizeof(desc));
          desc.u.buffer = apb;
          ioctl(fd, AUDIOIOC_ENQUEUEBUFFER, &desc);
        }
      else if (msg.msg_id == AUDIO_MSG_COMPLETE)
        {
          printf("[record] play COMPLETE msg\n");
          break;
        }
    }

  ioctl(fd, AUDIOIOC_STOP, 0);
  printf("[record] played %u samples\n", play_samp);

unreg_play_mq:
  ioctl(fd, AUDIOIOC_UNREGISTERMQ, (unsigned long)mq);
  mq_close(mq);
free_play_bufs:
  for (n = 0; n < allocated; n++)
    {
      memset(&desc, 0, sizeof(desc));
      desc.u.buffer = bufs[n];
      ioctl(fd, AUDIOIOC_FREEBUFFER, &desc);
    }
close_play:
  close(fd);

  free(rec_data);
  printf("[record] === Done ===\n");
  return ret;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  const char *cmd;
  unsigned int dur;

  cmd = (argc > 1) ? argv[1] : "rec";
  dur = (argc > 2) ? (unsigned int)strtoul(argv[2], NULL, 0) : DEFAULT_DURATION_MS;
  if (dur == 0 || dur > 30000)
    { printf("[audio_test] ERROR: duration\n"); return -1; }

  if (strcmp(cmd, "tone") == 0) return do_tone(dur);
  if (strcmp(cmd, "rec") == 0)  return do_record(dur);
  printf("[audio_test] usage: audio_test [rec|tone] [ms]\n");
  return -1;
}
