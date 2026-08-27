#ifndef __SPEEX_TYPES_H__
#define __SPEEX_TYPES_H__

/* Replaces configure-generated speexdsp_config_types.h for the NuttX/OpenVela
 * build: the toolchain provides standard <stdint.h> types. */

#include <stdint.h>

typedef int16_t spx_int16_t;
typedef uint16_t spx_uint16_t;
typedef int32_t spx_int32_t;
typedef uint32_t spx_uint32_t;

#endif
