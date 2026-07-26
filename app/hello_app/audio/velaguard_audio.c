/****************************************************************************
 * VelaGuard PCM front-end: DC removal and basic voice activity measurement.
 ****************************************************************************/

#include "velaguard_audio.h"

#include "register.h"
#include "sfconfig.h"
#include "bf0_hal_dma.h"
#include "bf0_hal_audcodec.h"
#include "bf0_hal_pmu.h"
#include "bf0_hal_rcc.h"
#include "config/sf32lb52x/dma_config.h"

#include <nuttx/cache.h>
#include <nuttx/arch.h>
#include <nuttx/irq.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

#define VG_AUDIO_SPEECH_MEAN_ABS 200
#define VG_AUDIO_SPEECH_PEAK     700
#define VG_AUDIO_DMA_BYTES       2048
#define VG_AUDIO_DMA_HALF_BYTES  (VG_AUDIO_DMA_BYTES / 2)
#define VG_AUDIO_SAMPLE_RATE     16000

static AUDCODEC_HandleTypeDef g_vg_codec;
static DMA_HandleTypeDef g_vg_codec_dma;
static uint8_t g_vg_audio_dma[VG_AUDIO_DMA_BYTES]
  __attribute__((aligned(32)));
static volatile uint8_t g_vg_audio_ready;
static volatile uint32_t g_vg_audio_sequence;
static bool g_vg_audio_started;

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

extern HAL_StatusTypeDef HAL_AUDCODEC_Config_ADCPath_Volume(
  AUDCODEC_HandleTypeDef *codec, int channel, int volume);

static const AUDCODE_ADC_CLK_CONFIG_TYPE g_vg_adc_clock =
{
  16000, 0, 10, 1, 0, 0, 5, 2
};

static int vg_audio_dma_isr(int irq, void *context, void *arg)
{
  HAL_DMA_IRQHandler(&g_vg_codec_dma);
  return 0;
}

void HAL_AUDCODEC_RxHalfCpltCallback(AUDCODEC_HandleTypeDef *codec, int cid)
{
  if (codec == &g_vg_codec && cid == HAL_AUDCODEC_ADC_CH0)
    {
      g_vg_audio_ready |= 1;
      g_vg_audio_sequence++;
    }
}

void HAL_AUDCODEC_RxCpltCallback(AUDCODEC_HandleTypeDef *codec, int cid)
{
  if (codec == &g_vg_codec && cid == HAL_AUDCODEC_ADC_CH0)
    {
      g_vg_audio_ready |= 2;
      g_vg_audio_sequence++;
    }
}

void HAL_AUDCODEC_ErrorCallback(AUDCODEC_HandleTypeDef *codec, int cid)
{
  if (codec == &g_vg_codec && cid == HAL_AUDCODEC_ADC_CH0)
    {
      g_vg_audio_ready |= 4;
    }
}

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
  AUDCODEC_ADCCfgTypeDef adc_cfg;
  int ret;

  if (g_vg_audio_started)
    {
      return 0;
    }

  memset(&g_vg_codec, 0, sizeof(g_vg_codec));
  memset(&g_vg_codec_dma, 0, sizeof(g_vg_codec_dma));
  memset(g_vg_audio_dma, 0, sizeof(g_vg_audio_dma));

  g_vg_codec.Instance = hwp_audcodec;
  g_vg_codec.hdma[HAL_AUDCODEC_ADC_CH0] = &g_vg_codec_dma;
  g_vg_codec.Init.en_dly_sel = 0;
  g_vg_codec.Init.adc_cfg.opmode = 1;
  g_vg_codec.Init.adc_cfg.adc_clk =
    (AUDCODE_ADC_CLK_CONFIG_TYPE *)&g_vg_adc_clock;
  g_vg_codec.bufSize = sizeof(g_vg_audio_dma);

  g_vg_codec_dma.Instance = AUDCODEC_ADC0_DMA_INSTANCE;
  g_vg_codec_dma.Init.Request = AUDCODEC_ADC0_DMA_REQUEST;

  HAL_PMU_EnableAudio(1);
  HAL_RCC_EnableModule(RCC_MOD_AUDCODEC);

  /* Power and calibrate the audio PLL/bandgap before touching the ADC.
   * Enabling only the RCC clock lets DMA run, but the analog converter then
   * produces an all-zero stream.  16 kHz belongs to the 48 kHz clock family.
   */

  ret = bf0_enable_pll(g_vg_adc_clock.samplerate, 1);
  if (ret < 0)
    {
      printf("VelaGuard audio: PLL enable failed (%d)\n", ret);
      return ret;
    }

  if (HAL_AUDCODEC_Init(&g_vg_codec) != HAL_OK)
    {
      return -EIO;
    }

  memset(&adc_cfg, 0, sizeof(adc_cfg));
  adc_cfg.opmode = 1;
  adc_cfg.adc_clk = (AUDCODE_ADC_CLK_CONFIG_TYPE *)&g_vg_adc_clock;
  if (HAL_AUDCODEC_Config_RChanel(&g_vg_codec, 0, &adc_cfg) != HAL_OK)
    {
      return -EIO;
    }

  if (HAL_AUDCODEC_Config_ADCPath_Volume(&g_vg_codec, 0, 12) != HAL_OK)
    {
      return -EIO;
    }

  ret = irq_attach(NX_IRQ(AUDCODEC_ADC0_DMA_IRQ), vg_audio_dma_isr, NULL);
  if (ret < 0)
    {
      return ret;
    }

  /* Prevent dirty cache lines from overwriting samples written by DMA. */

  up_clean_dcache((uintptr_t)g_vg_audio_dma,
                  (uintptr_t)g_vg_audio_dma + sizeof(g_vg_audio_dma));
  if (HAL_AUDCODEC_Receive_DMA(&g_vg_codec, g_vg_audio_dma,
                               sizeof(g_vg_audio_dma),
                               HAL_AUDCODEC_ADC_CH0) != HAL_OK)
    {
      irq_detach(NX_IRQ(AUDCODEC_ADC0_DMA_IRQ));
      return -EIO;
    }

  HAL_AUCODEC_Refgen_Init();
  HAL_AUDCODEC_Config_Analog_ADCPath(
    (AUDCODE_ADC_CLK_CONFIG_TYPE *)&g_vg_adc_clock);
  __HAL_AUDCODEC_ADC_ENABLE(&g_vg_codec);
  up_enable_irq(NX_IRQ(AUDCODEC_ADC0_DMA_IRQ));

  g_vg_audio_ready = 0;
  g_vg_audio_sequence = 0;
  g_vg_audio_started = true;
  printf("VelaGuard audio: MIC ADC0 16kHz/16-bit DMA started\n");
  return 0;
}

void vg_audio_capture_stop(void)
{
  if (!g_vg_audio_started)
    {
      return;
    }

  up_disable_irq(NX_IRQ(AUDCODEC_ADC0_DMA_IRQ));
  HAL_AUDCODEC_DMAStop(&g_vg_codec, HAL_AUDCODEC_ADC_CH0);
  HAL_AUDCODEC_Close_Analog_ADCPath();
  HAL_AUDCODEC_DeInit(&g_vg_codec);
  irq_detach(NX_IRQ(AUDCODEC_ADC0_DMA_IRQ));
  g_vg_audio_started = false;
}

bool vg_audio_capture_level(struct vg_audio_level_s *level,
                            uint32_t *sequence,
                            enum vg_audio_keyword_e *keyword)
{
  irqstate_t flags;
  const int16_t *samples = NULL;
  uint8_t ready;
  uint32_t seq;

  flags = up_irq_save();
  ready = g_vg_audio_ready;
  if (ready & 4)
    {
      g_vg_audio_ready &= ~4;
      up_irq_restore(flags);
      printf("VelaGuard audio: DMA error\n");
      return false;
    }

  if (ready & 1)
    {
      g_vg_audio_ready &= ~1;
      samples = (const int16_t *)g_vg_audio_dma;
    }
  else if (ready & 2)
    {
      g_vg_audio_ready &= ~2;
      samples = (const int16_t *)(g_vg_audio_dma + VG_AUDIO_DMA_HALF_BYTES);
    }

  seq = g_vg_audio_sequence;
  up_irq_restore(flags);

  if (samples == NULL)
    {
      return false;
    }

  up_invalidate_dcache((uintptr_t)samples,
                       (uintptr_t)samples + VG_AUDIO_DMA_HALF_BYTES);
  vg_audio_analyze_pcm16(samples, VG_AUDIO_DMA_HALF_BYTES / sizeof(int16_t),
                         level);
  if (keyword != NULL)
    {
      /* KWS is gated by VAD to avoid spending model time on silence.  A
       * positive VAD result is not itself treated as a recognized keyword.
       */

      *keyword = level->speech_active ?
        vg_audio_kws_infer(samples,
                           VG_AUDIO_DMA_HALF_BYTES / sizeof(int16_t),
                           VG_AUDIO_SAMPLE_RATE) : VG_AUDIO_KEYWORD_NONE;
    }
  if (sequence != NULL)
    {
      *sequence = seq;
    }

  return true;
}
