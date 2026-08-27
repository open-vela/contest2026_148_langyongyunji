/****************************************************************************
 * contest2026_148_langyongyunji/app/hello_app/audio/vg_denoise.h
 *
 * VelaGuard noise suppression wrapper around libspeexdsp preprocess.
 *
 * The preprocessor implements speech enhancement (Ephraim-Malah spectral
 * subtraction): it suppresses stationary environmental noise while keeping
 * the speech details, designed exactly for 16 kHz mono speech capture.
 *
 * When CONFIG_CONTEST2026_148_DENOISE is disabled the API degenerates to
 * no-ops, so callers can use it unconditionally.
 ****************************************************************************/

#ifndef __VG_DENOISE_H
#define __VG_DENOISE_H

#include <nuttx/config.h>

#include <stdint.h>
#include <stdbool.h>

#ifdef CONFIG_CONTEST2026_148_DENOISE

typedef struct vg_denoise_s vg_denoise_s;

/* Create a denoiser for the given sample rate.  frame_size is the
 * preprocessor frame length in samples (power of two, e.g. 256).
 * Returns NULL on failure. */
struct vg_denoise_s *vg_denoise_create(uint32_t sample_rate,
                                       uint32_t frame_size);

void vg_denoise_destroy(struct vg_denoise_s *denoise);

/* Denoise samples in place.  count should be a multiple of frame_size.
 * Returns the number of frames processed, or <0 on error. */
int vg_denoise_run(struct vg_denoise_s *denoise, int16_t *samples,
                   uint32_t count);

#else

/* Denoise disabled: pass-through no-op implementation. */

typedef void vg_denoise_s;

static inline void *vg_denoise_create(uint32_t sample_rate,
                                      uint32_t frame_size)
{
  (void)sample_rate;
  (void)frame_size;
  return NULL;
}

static inline void vg_denoise_destroy(void *denoise)
{
  (void)denoise;
}

static inline int vg_denoise_run(void *denoise, int16_t *samples,
                                 uint32_t count)
{
  (void)denoise;
  (void)samples;
  (void)count;
  return 0;
}

#endif /* CONFIG_CONTEST2026_148_DENOISE */

#endif /* __VG_DENOISE_H */
