/****************************************************************************
 * VelaGuard microphone/keyword front-end.
 ****************************************************************************/

#ifndef __VELAGUARD_AUDIO_H
#define __VELAGUARD_AUDIO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct vg_audio_level_s
{
  int32_t dc;
  uint32_t mean_abs;
  uint16_t peak;
  bool speech_active;
};

enum vg_audio_keyword_e
{
  VG_AUDIO_KEYWORD_NONE = 0,
  VG_AUDIO_KEYWORD_JIUMING,
  VG_AUDIO_KEYWORD_QIUZHU,
};

/* Analyze one mono signed-16 PCM frame. This hardware-independent stage is
 * shared by the upcoming AUDCODEC DMA backend and keyword detector.
 */

void vg_audio_analyze_pcm16(const int16_t *samples, size_t count,
                            struct vg_audio_level_s *level);

int vg_audio_capture_start(void);
void vg_audio_capture_stop(void);
bool vg_audio_capture_level(struct vg_audio_level_s *level,
                            uint32_t *sequence,
                            enum vg_audio_keyword_e *keyword);

/* Model integration point.  A Chinese offline KWS implementation may
 * override this weak function and return one of vg_audio_keyword_e.  The
 * default deliberately returns NONE: VAD alone is never an SOS keyword.
 */

enum vg_audio_keyword_e vg_audio_kws_infer(const int16_t *samples,
                                           size_t count,
                                           uint32_t sample_rate);

#endif
