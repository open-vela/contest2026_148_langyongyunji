/****************************************************************************
 * VelaGuard lightweight WAV file parser.
 *
 * Reuses NuttX audio/pcm.h WAV header definitions and magic constants.
 ****************************************************************************/

#ifndef __VELAGUARD_WAV_PARSER_H
#define __VELAGUARD_WAV_PARSER_H

#include <stddef.h>
#include <stdint.h>

struct vg_wav_info_s
{
  uint32_t sample_rate;
  uint32_t data_size;       /* PCM data size in bytes */
  const uint8_t *data;      /* Pointer to PCM samples */
};

/* Parse a WAV file at `path`.  PCM data is read into the caller-supplied
 * buffer `buf` (size `buf_size`).  On success the function returns 0 and
 * fills `info` with sample rate, data size and a pointer into `buf`.
 * Only 16 kHz / 16-bit / mono PCM WAV files are accepted.
 */

int vg_wav_parse(const char *path, struct vg_wav_info_s *info,
                 uint8_t *buf, size_t buf_size);

#endif
