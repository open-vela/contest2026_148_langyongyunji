/****************************************************************************
 * contest2026_148_langyongyunji/app/hello_app/audio/vg_denoise.c
 *
 * VelaGuard noise suppression wrapper around libspeexdsp preprocess.
 * See vg_denoise.h for details.
 ****************************************************************************/

#include "vg_denoise.h"

#ifdef CONFIG_CONTEST2026_148_DENOISE

#include <stdlib.h>
#include <string.h>

#include <speex/speex_preprocess.h>

/* Frame size used by the preprocessor (16 ms at 16 kHz).  Power of two. */

#define VG_DENOISE_FRAME     256

/* Noise suppression amount in dB: strong enough to remove environmental
 * noise while keeping speech natural.  -30 dB suppresses noise harder but
 * eats weak speech (fricatives/tail of syllables); -20 dB keeps more speech
 * detail at the cost of slightly less noise reduction.
 */

#define VG_DENOISE_SUPPRESS  -20

struct vg_denoise_s
{
  SpeexPreprocessState *state;
  uint32_t sample_rate;
  uint32_t frame_size;
};

struct vg_denoise_s *vg_denoise_create(uint32_t sample_rate,
                                       uint32_t frame_size)
{
  struct vg_denoise_s *denoise;
  int on = 1;
  int off = 0;
  int suppress = VG_DENOISE_SUPPRESS;

  denoise = calloc(1, sizeof(*denoise));
  if (denoise == NULL)
    {
      return NULL;
    }

  denoise->state = speex_preprocess_state_init((int)frame_size,
                                               (int)sample_rate);
  if (denoise->state == NULL)
    {
      free(denoise);
      return NULL;
    }

  denoise->sample_rate = sample_rate;
  denoise->frame_size  = frame_size;

  /* Enable denoising with the configured suppression level. */

  speex_preprocess_ctl(denoise->state, SPEEX_PREPROCESS_SET_DENOISE,
                       &on);
  speex_preprocess_ctl(denoise->state,
                       SPEEX_PREPROCESS_SET_NOISE_SUPPRESS, &suppress);

  /* Leave dynamics untouched: no AGC and no VAD gating, so the recording
   * keeps the full speech detail at its original level.
   */

  speex_preprocess_ctl(denoise->state, SPEEX_PREPROCESS_SET_AGC, &off);
  speex_preprocess_ctl(denoise->state, SPEEX_PREPROCESS_SET_VAD, &off);

  return denoise;
}

void vg_denoise_destroy(struct vg_denoise_s *denoise)
{
  if (denoise != NULL)
    {
      if (denoise->state != NULL)
        {
          speex_preprocess_state_destroy(denoise->state);
        }

      free(denoise);
    }
}

int vg_denoise_run(struct vg_denoise_s *denoise, int16_t *samples,
                   uint32_t count)
{
  uint32_t offset;

  if (denoise == NULL || denoise->state == NULL ||
      denoise->frame_size == 0)
    {
      return 0;
    }

  for (offset = 0; offset + denoise->frame_size <= count;
       offset += denoise->frame_size)
    {
      /* speex_preprocess_run() processes in place: x == output. */

      speex_preprocess_run(denoise->state, samples + offset);
    }

  return (int)(offset / denoise->frame_size);
}

#endif /* CONFIG_CONTEST2026_148_DENOISE */
