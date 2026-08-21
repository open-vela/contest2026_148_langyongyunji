/****************************************************************************
 * VelaGuard lightweight WAV file parser.
 *
 * Reuses NuttX audio/pcm.h definitions.  Only 16 kHz / 16-bit / mono PCM
 * WAV files are accepted.
 *
 * PCM data is placed at buf[0] regardless of the WAV header size, so the
 * caller does not need to track internal offsets.
 ****************************************************************************/

#include "velaguard_wav_parser.h"

#include <nuttx/audio/pcm.h>

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/****************************************************************************
 * Private Helpers
 ****************************************************************************/

static uint16_t le16(const uint8_t *p)
{
  return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t le32(const uint8_t *p)
{
  return (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int vg_wav_parse(const char *path, struct vg_wav_info_s *info,
                 uint8_t *buf, size_t buf_size)
{
  struct stat st;
  uint8_t riff[12];        /* "RIFF" + size + "WAVE" */
  uint8_t fmt_hdr[8];      /* "fmt " + chunk size */
  uint8_t fmt[16];         /* Core PCM fields of the fmt chunk */
  uint32_t riff_size;
  uint32_t fmt_size;
  uint32_t chunk_offset;   /* Offset of the next chunk header */
  uint32_t data_chunk_size;
  uint32_t fmt_samprate;
  uint32_t fmt_byterate;
  uint16_t fmt_bpsamp;
  uint16_t fmt_nchannels;
  uint16_t fmt_format;
  uint16_t fmt_align;
  int fd;
  ssize_t nread;

  memset(info, 0, sizeof(*info));

  fd = open(path, O_RDONLY);
  if (fd < 0)
    {
      printf("VelaGuard WAV: open %s failed: %d\n", path, errno);
      return -ENOENT;
    }

  if (fstat(fd, &st) < 0)
    {
      printf("VelaGuard WAV: stat %s failed: %d\n", path, errno);
      close(fd);
      return -EIO;
    }

  /* Validate the RIFF/WAVE container header. */

  nread = pread(fd, riff, sizeof(riff), 0);
  if (nread != (ssize_t)sizeof(riff))
    {
      printf("VelaGuard WAV: short read for RIFF header (%zd)\n", nread);
      close(fd);
      return -EIO;
    }

  if (le32(riff + 0) != WAV_HDR_CHUNKID ||  /* "RIFF" */
      le32(riff + 8) != WAV_HDR_FORMAT)     /* "WAVE" */
    {
      printf("VelaGuard WAV: not a RIFF/WAVE file\n");
      close(fd);
      return -EINVAL;
    }

  riff_size = le32(riff + 4);
  if ((uint64_t)riff_size + 8 > (uint64_t)st.st_size)
    {
      printf("VelaGuard WAV: RIFF size overruns file "
             "(header=%" PRIu32 " file=%" PRIu64 ")\n",
             riff_size, (uint64_t)st.st_size);
      close(fd);
      return -EINVAL;
    }

  /* Parse the "fmt " chunk.  Its length is variable, so derive the offset
   * of the following chunk from it instead of assuming a 44-byte header.
   */

  nread = pread(fd, fmt_hdr, sizeof(fmt_hdr), 12);
  if (nread != (ssize_t)sizeof(fmt_hdr))
    {
      printf("VelaGuard WAV: short read for fmt header (%zd)\n", nread);
      close(fd);
      return -EIO;
    }

  if (le32(fmt_hdr + 0) != WAV_FMT_CHUNKID)  /* "fmt " */
    {
      printf("VelaGuard WAV: fmt chunk not found\n");
      close(fd);
      return -EINVAL;
    }

  fmt_size = le32(fmt_hdr + 4);
  if (fmt_size < 16 || fmt_size > 4096)
    {
      printf("VelaGuard WAV: bad fmt chunk size (%" PRIu32 ")\n",
             fmt_size);
      close(fd);
      return -EINVAL;
    }

  nread = pread(fd, fmt, sizeof(fmt), 20);
  if (nread != (ssize_t)sizeof(fmt))
    {
      printf("VelaGuard WAV: short read for fmt data (%zd)\n", nread);
      close(fd);
      return -EIO;
    }

  fmt_format    = le16(fmt + 0);
  fmt_nchannels = le16(fmt + 2);
  fmt_samprate  = le32(fmt + 4);
  fmt_byterate  = le32(fmt + 8);
  fmt_align     = le16(fmt + 12);
  fmt_bpsamp    = le16(fmt + 14);

  if (fmt_format    != WAV_FMT_FORMAT ||
      fmt_samprate  != 16000          ||
      fmt_bpsamp    != 16             ||
      fmt_nchannels != 1)
    {
      printf("VelaGuard WAV: unsupported format "
             "fmt=%d rate=%" PRIu32 " bits=%d ch=%d (need 1/16000/16/1)\n",
             (int)fmt_format, (uint32_t)fmt_samprate,
             (int)fmt_bpsamp, (int)fmt_nchannels);
      close(fd);
      return -ENOTSUP;
    }

  if (fmt_align != (uint16_t)(fmt_nchannels * fmt_bpsamp / 8) ||
      fmt_byterate != fmt_samprate * fmt_align)
    {
      printf("VelaGuard WAV: inconsistent fmt "
             "(align=%d byterate=%" PRIu32 ")\n",
             (int)fmt_align, (uint32_t)fmt_byterate);
      close(fd);
      return -EINVAL;
    }

  /* Skip any optional chunks until we reach "data". */

  chunk_offset = 20 + fmt_size;
  data_chunk_size = 0;

  for (;;)
    {
      uint8_t ck[8];

      if ((uint64_t)chunk_offset + 8 > (uint64_t)st.st_size)
        {
          printf("VelaGuard WAV: data chunk not found\n");
          close(fd);
          return -EINVAL;
        }

      nread = pread(fd, ck, 8, (off_t)chunk_offset);
      if (nread != 8)
        {
          printf("VelaGuard WAV: read chunk header at offset %" PRIu32
                 " failed\n", chunk_offset);
          close(fd);
          return -EIO;
        }

      if (le32(ck) == WAV_DATA_CHUNKID)  /* "data" */
        {
          data_chunk_size = le32(ck + 4);
          chunk_offset += 8;
          break;
        }

      chunk_offset += 8 + le32(ck + 4);
      if (chunk_offset > 1024 * 1024)  /* safety limit */
        {
          printf("VelaGuard WAV: data chunk not found\n");
          close(fd);
          return -EINVAL;
        }
    }

  /* Bound the PCM data by the real file size and the caller's buffer. */

  if (data_chunk_size > buf_size)
    {
      printf("VelaGuard WAV: PCM too large (%" PRIu32 " > %zu)\n",
             data_chunk_size, buf_size);
      close(fd);
      return -ENOMEM;
    }

  if ((uint64_t)chunk_offset + data_chunk_size > (uint64_t)st.st_size)
    {
      printf("VelaGuard WAV: data chunk overruns file "
             "(off=%" PRIu32 " size=%" PRIu32 " file=%" PRIu64 ")\n",
             chunk_offset, data_chunk_size, (uint64_t)st.st_size);
      close(fd);
      return -EINVAL;
    }

  nread = pread(fd, buf, data_chunk_size, (off_t)chunk_offset);
  if (nread != (ssize_t)data_chunk_size)
    {
      printf("VelaGuard WAV: short read for PCM (%zd/%" PRIu32 ")\n",
             nread, data_chunk_size);
      close(fd);
      return -EIO;
    }

  close(fd);

  info->sample_rate = 16000;
  info->data_size   = data_chunk_size;
  info->data        = buf;            /* PCM starts at buf[0] */

  printf("VelaGuard WAV: loaded %" PRIu32 " B PCM from %s\n",
         data_chunk_size, path);

  return 0;
}
