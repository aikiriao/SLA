#include "test.h"
#include "../SLAEncoder.h"
#include "../SLADecoder.h"

#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <stdio.h>

/* このテストは様々な波形がエンコード -> デコードが元に戻るかを確認する */
/* ユニットテストは短く終わるのが大原則なので長尺の入力はNG */
/* TODO: ユニットテストから外す */

/* 波形生成関数 */
typedef void (*GenerateWaveFunction)(
    double** data, uint32_t num_channels, uint32_t num_samples);

/* テストケース */
struct EncodeDecodeTestCase {
  struct SLAWaveFormat      wave_format;        /* 入力波形データ */
  struct SLAEncodeParameter encode_parameter;   /* エンコードパラメータ */
  uint32_t                  num_samples;        /* サンプル数 */
  GenerateWaveFunction      gen_wave_func;      /* 波形生成関数 */
};

/* 無音の生成 */
static void testSLAEncodeDecode_GenerateSilence(
    double** data, uint32_t num_channels, uint32_t num_samples)
{
  uint32_t smpl, ch;

  assert(data != NULL);

  for (ch = 0; ch < num_channels; ch++) {
    for (smpl = 0; smpl < num_samples; smpl++) {
      data[ch][smpl] = 0.0f;
    }
  }
}

/* サイン波の生成 */
static void testSLAEncodeDecode_GenerateSinWave(
    double** data, uint32_t num_channels, uint32_t num_samples)
{
  uint32_t smpl, ch;

  assert(data != NULL);

  for (ch = 0; ch < num_channels; ch++) {
    for (smpl = 0; smpl < num_samples; smpl++) {
      data[ch][smpl] = sin(440.0f * 2 * M_PI * smpl / 44100.0f);
    }
  }
}

/* 白色雑音の生成 */
static void testSLAEncodeDecode_GenerateWhiteNoise(
    double** data, uint32_t num_channels, uint32_t num_samples)
{
  uint32_t smpl, ch;

  assert(data != NULL);

  /* デバッグのしやすさのため、乱数シードを固定 */
  srand(0);
  for (ch = 0; ch < num_channels; ch++) {
    for (smpl = 0; smpl < num_samples; smpl++) {
      data[ch][smpl] = 2.0f * ((double)rand() / RAND_MAX - 0.5f);
    }
  }
}

/* チャープ信号の生成 */
static void testSLAEncodeDecode_GenerateChirp(
    double** data, uint32_t num_channels, uint32_t num_samples)
{
  uint32_t smpl, ch;
  double period;

  assert(data != NULL);

  for (ch = 0; ch < num_channels; ch++) {
    for (smpl = 0; smpl < num_samples; smpl++) {
      period = num_samples - smpl;
      data[ch][smpl] = sin((2.0f * M_PI * smpl) / period);
    }
  }
}

/* 正定数信号の生成 */
static void testSLAEncodeDecode_GeneratePositiveConstant(
    double** data, uint32_t num_channels, uint32_t num_samples)
{
  uint32_t smpl, ch;

  assert(data != NULL);

  for (ch = 0; ch < num_channels; ch++) {
    for (smpl = 0; smpl < num_samples; smpl++) {
      data[ch][smpl] = 1.0f;
    }
  }
}

/* 負定数信号の生成 */
static void testSLAEncodeDecode_GenerateNegativeConstant(
    double** data, uint32_t num_channels, uint32_t num_samples)
{
  uint32_t smpl, ch;

  assert(data != NULL);

  for (ch = 0; ch < num_channels; ch++) {
    for (smpl = 0; smpl < num_samples; smpl++) {
      data[ch][smpl] = -1.0f;
    }
  }
}

/* ナイキスト周期の振動の生成 */
static void testSLAEncodeDecode_GenerateNyquistOsc(
    double** data, uint32_t num_channels, uint32_t num_samples)
{
  uint32_t smpl, ch;

  assert(data != NULL);

  for (ch = 0; ch < num_channels; ch++) {
    for (smpl = 0; smpl < num_samples; smpl++) {
      data[ch][smpl] = (smpl % 2 == 0) ? 1.0f : -1.0f;
    }
  }
}

/* ガウス雑音の生成 */
static void testSLAEncodeDecode_GenerateGaussNoise(
    double** data, uint32_t num_channels, uint32_t num_samples)
{
  uint32_t smpl, ch;
  double   x, y;

  assert(data != NULL);

  /* デバッグのしやすさのため、乱数シードを固定 */
  srand(0);
  for (ch = 0; ch < num_channels; ch++) {
    for (smpl = 0; smpl < num_samples; smpl++) {
      /* ボックス-ミューラー法 */
      x = (double)rand() / RAND_MAX;
      y = (double)rand() / RAND_MAX;
      /* 分散は0.1f */
      data[ch][smpl] = 0.25f * sqrt(-2.0f * log(x)) * cos(2.0f * M_PI * y);
      data[ch][smpl] = (data[ch][smpl] >= 1.0f) ?   1.0f : data[ch][smpl];
      data[ch][smpl] = (data[ch][smpl] <= -1.0f) ? -1.0f : data[ch][smpl];
    }
  }
}

int testSLAEncodeDecode_Initialize(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);
  return 0;
}

int testSLAEncodeDecode_Finalize(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);
  return 0;
}

/* 単一のテストケースを実行 */
static int32_t testSLAEncodeDecode_DoTestCase(const struct EncodeDecodeTestCase* test_case)
{
  int32_t       ret;
  uint32_t      smpl, ch;
  uint32_t      num_samples, num_channels, data_size;
  uint32_t      output_size, output_samples;
  double        **input_double;
  int32_t       **input;
  uint8_t       *data;
  int32_t       **output;
  SLAApiResult  api_ret;

  struct SLAEncoderConfig encoder_config;
  struct SLADecoderConfig decoder_config;
  struct SLAEncoder* encoder;
  struct SLADecoder* decoder;

  assert(test_case != NULL);
  assert(test_case->num_samples <= (1UL << 14));  /* 長過ぎる入力はNG */

  num_samples   = test_case->num_samples;
  num_channels  = test_case->wave_format.num_channels;
  data_size     = SLA_HEADER_SIZE + SLA_CalculateSufficientBlockSize(num_channels, num_samples, test_case->wave_format.bit_per_sample);

  /* エンコード・デコードコンフィグ作成 */
  encoder_config.max_num_channels         = num_channels;
  encoder_config.max_num_block_samples    = test_case->encode_parameter.max_num_block_samples;
  encoder_config.max_parcor_order         = test_case->encode_parameter.parcor_order;
  encoder_config.max_longterm_order       = test_case->encode_parameter.longterm_order;
  encoder_config.max_lms_order_par_filter = test_case->encode_parameter.lms_order_par_filter;
  encoder_config.verpose_flag             = 0;
  decoder_config.max_num_channels         = num_channels;
  decoder_config.max_num_block_samples    = test_case->encode_parameter.max_num_block_samples;
  decoder_config.max_parcor_order         = test_case->encode_parameter.parcor_order;
  decoder_config.max_longterm_order       = test_case->encode_parameter.longterm_order;
  decoder_config.max_lms_order_par_filter = test_case->encode_parameter.lms_order_par_filter;
  decoder_config.enable_crc_check         = 1;
  decoder_config.verpose_flag             = 0;

  /* 一時領域の割り当て */
  input_double  = (double **)malloc(sizeof(double*) * num_channels);
  input         = (int32_t **)malloc(sizeof(int32_t*) * num_channels);
  output        = (int32_t **)malloc(sizeof(int32_t*) * num_channels);
  data          = (uint8_t *)malloc(data_size);
  for (ch = 0; ch < num_channels; ch++) {
    input_double[ch]  = (double *)malloc(sizeof(double) * num_samples);
    input[ch]         = (int32_t *)malloc(sizeof(int32_t) * num_samples);
    output[ch]        = (int32_t *)malloc(sizeof(int32_t) * num_samples);
  }

  /* エンコード・デコードハンドル作成 */
  encoder = SLAEncoder_Create(&encoder_config);
  decoder = SLADecoder_Create(&decoder_config);
  if (encoder == NULL || decoder == NULL) {
    ret = 1;
    goto EXIT;
  }

  /* 波形生成 */
  test_case->gen_wave_func(input_double, num_channels, num_samples);

  /* 固定小数化 */
  for (ch = 0; ch < num_channels; ch++) {
    for (smpl = 0; smpl < num_samples; smpl++) {
      assert(fabs(input_double[ch][smpl]) <= 1.0f);
      /* まずはビット幅のデータを作る */
      input[ch][smpl]
        = (int32_t)round(input_double[ch][smpl] * pow(2, test_case->wave_format.bit_per_sample - 1));
      /* クリップ */
      if (input[ch][smpl] >= (1L << (test_case->wave_format.bit_per_sample - 1))) {
        input[ch][smpl] = (1L << (test_case->wave_format.bit_per_sample - 1)) - 1;
      }
      /* 左シフト量だけ下位ビットのデータを消す */
      input[ch][smpl] &= ~((1UL << test_case->wave_format.offset_lshift) - 1);
      /* 32bit化 */
      assert(((int64_t)input[ch][smpl] << (32 - test_case->wave_format.bit_per_sample)) <= INT32_MAX);
      assert(((int64_t)input[ch][smpl] << (32 - test_case->wave_format.bit_per_sample)) >= INT32_MIN);
      input[ch][smpl] <<= (32 - test_case->wave_format.bit_per_sample);
    }
  }

  /* 波形フォーマットと波形パラメータをセット */
  if ((api_ret = SLAEncoder_SetWaveFormat(
          encoder, &test_case->wave_format)) != SLA_APIRESULT_OK) {
    fprintf(stderr, "Failed to set wave format. ret:%d \n", api_ret);
    ret = 2;
    goto EXIT;
  }
  if ((api_ret = SLAEncoder_SetEncodeParameter(
          encoder, &test_case->encode_parameter)) != SLA_APIRESULT_OK) {
    fprintf(stderr, "Failed to set encode parameter. ret:%d \n", api_ret);
    ret = 3;
    goto EXIT;
  }

  /* エンコード */
  if ((api_ret = SLAEncoder_EncodeWhole(encoder,
        (const int32_t **)input, num_samples, data, data_size, &output_size)) != SLA_APIRESULT_OK) {
    fprintf(stderr, "Encode failed! ret:%d \n", api_ret);
    ret = 4;
    goto EXIT;
  }

  /* デコード */
  if ((api_ret = SLADecoder_DecodeWhole(decoder,
        data, output_size, output, num_samples, &output_samples)) != SLA_APIRESULT_OK) {
    fprintf(stderr, "Decode failed! ret:%d \n", api_ret);
    ret = 5;
    goto EXIT;
  }

  /* 出力サンプル数が異常 */
  if (num_samples != output_samples) {
    ret = 6;
    goto EXIT;
  }

  /* 一致確認 */
  for (ch = 0; ch < num_channels; ch++) {
    for (smpl = 0; smpl < num_samples; smpl++) {
      if (input[ch][smpl] != output[ch][smpl]) {
        printf("%5d %12d vs %12d \n", smpl, input[ch][smpl] >> 16, output[ch][smpl] >> 16);
        ret = 7;
        goto EXIT;
      }
    }
  }

  /* ここまで来れば成功 */
  ret = 0;

EXIT:
  /* ハンドル開放 */
  SLAEncoder_Destroy(encoder);
  SLADecoder_Destroy(decoder);

  /* 一時領域の開放 */
  for (ch = 0; ch < num_channels; ch++) {
    free(input_double[ch]);
    free(input[ch]);
    free(output[ch]);
  }
  free(input_double);
  free(input);
  free(output);
  free(data);

  return ret;
}

/* エンコードデコードテスト実行 */
static void testSLAEncodeDecode_EncodeDecodeTest(void *obj)
{
  int32_t   test_ret;
  uint32_t  test_no;

  /* テストケース配列 */
  static const struct EncodeDecodeTestCase test_case[] = {
    /* 無音の部 */
    { { 1,  8, 0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateSilence },
    { { 1, 16,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateSilence },
    { { 1, 16,  0,  8 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateSilence },
    { { 1, 24,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateSilence },
    { { 1, 24,  0,  8 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateSilence },
    { { 1, 24,  0, 16 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateSilence },
    { { 2,  8,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_STEREO_MS, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateSilence },
    { { 2, 16,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_STEREO_MS, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateSilence },
    { { 2, 16,  0,  8 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_STEREO_MS, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateSilence },
    { { 2, 24,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_STEREO_MS, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateSilence },
    { { 2, 24,  0,  8 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_STEREO_MS, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateSilence },
    { { 2, 24,  0, 16 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_STEREO_MS, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateSilence },
    { { 8,  8, 0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      4096,
      testSLAEncodeDecode_GenerateSilence },
    { { 8, 16,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      4096,
      testSLAEncodeDecode_GenerateSilence },
    { { 8, 16,  0,  8 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      4096,
      testSLAEncodeDecode_GenerateSilence },
    { { 8, 24,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      4096,
      testSLAEncodeDecode_GenerateSilence },
    { { 8, 24,  0,  8 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      4096,
      testSLAEncodeDecode_GenerateSilence },
    { { 8, 24,  0, 16 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      4096,
      testSLAEncodeDecode_GenerateSilence },

    /* サイン波の部 */
    { { 1,  8,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateSinWave },
    { { 1, 16,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateSinWave },
    { { 1, 16,  0,  8 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateSinWave },
    { { 1, 24,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateSinWave },
    { { 1, 24,  0,  8 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateSinWave },
    { { 1, 24,  0, 16 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateSinWave },
    { { 2,  8,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_STEREO_MS, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateSinWave },
    { { 2, 16,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_STEREO_MS, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateSinWave },
    { { 2, 16,  0,  8 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_STEREO_MS, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateSinWave },
    { { 2, 24,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_STEREO_MS, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateSinWave },
    { { 2, 24,  0,  8 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_STEREO_MS, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateSinWave },
    { { 2, 24,  0, 16 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_STEREO_MS, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateSinWave },
    { { 8,  8,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      4096,
      testSLAEncodeDecode_GenerateSinWave },
    { { 8, 16,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      4096,
      testSLAEncodeDecode_GenerateSinWave },
    { { 8, 16,  0,  8 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      4096,
      testSLAEncodeDecode_GenerateSinWave },
    { { 8, 24,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      4096,
      testSLAEncodeDecode_GenerateSinWave },
    { { 8, 24,  0,  8 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      4096,
      testSLAEncodeDecode_GenerateSinWave },
    { { 8, 24,  0, 16 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      4096,
      testSLAEncodeDecode_GenerateSinWave },

    /* 白色雑音の部 */
    { { 1,  8,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateWhiteNoise },
    { { 1, 16,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateWhiteNoise },
    { { 1, 16,  0,  8 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateWhiteNoise },
    { { 1, 24,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateWhiteNoise },
    { { 1, 24,  0,  8 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateWhiteNoise },
    { { 1, 24,  0, 16 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateWhiteNoise },
    { { 2,  8,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_STEREO_MS, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateWhiteNoise },
    { { 2, 16,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_STEREO_MS, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateWhiteNoise },
    { { 2, 16,  0,  8 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_STEREO_MS, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateWhiteNoise },
    { { 2, 24,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_STEREO_MS, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateWhiteNoise },
    { { 2, 24,  0,  8 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_STEREO_MS, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateWhiteNoise },
    { { 2, 24,  0, 16 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_STEREO_MS, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateWhiteNoise },
    { { 8,  8,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      4096,
      testSLAEncodeDecode_GenerateWhiteNoise },
    { { 8, 16,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      4096,
      testSLAEncodeDecode_GenerateWhiteNoise },
    { { 8, 16,  0,  8 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      4096,
      testSLAEncodeDecode_GenerateWhiteNoise },
    { { 8, 24,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      4096,
      testSLAEncodeDecode_GenerateWhiteNoise },
    { { 8, 24,  0,  8 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      4096,
      testSLAEncodeDecode_GenerateWhiteNoise },
    { { 8, 24,  0, 16 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      4096,
      testSLAEncodeDecode_GenerateWhiteNoise },

    /* チャープ信号の部 */
    { { 1,  8,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateChirp },
    { { 1, 16,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateChirp },
    { { 1, 16,  0,  8 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateChirp },
    { { 1, 24,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateChirp },
    { { 1, 24,  0,  8 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateChirp },
    { { 1, 24,  0, 16 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateChirp },
    { { 2,  8,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_STEREO_MS, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateChirp },
    { { 2, 16,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_STEREO_MS, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateChirp },
    { { 2, 16,  0,  8 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_STEREO_MS, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateChirp },
    { { 2, 24,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_STEREO_MS, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateChirp },
    { { 2, 24,  0,  8 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_STEREO_MS, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateChirp },
    { { 2, 24,  0, 16 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_STEREO_MS, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateChirp },
    { { 8,  8,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      4096,
      testSLAEncodeDecode_GenerateChirp },
    { { 8, 16,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      4096,
      testSLAEncodeDecode_GenerateChirp },
    { { 8, 16,  0,  8 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      4096,
      testSLAEncodeDecode_GenerateChirp },
    { { 8, 24,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      4096,
      testSLAEncodeDecode_GenerateChirp },
    { { 8, 24,  0,  8 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      4096,
      testSLAEncodeDecode_GenerateChirp },
    { { 8, 24,  0, 16 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      4096,
      testSLAEncodeDecode_GenerateChirp },

    /* 正定数信号の部 */
    { { 1,  8,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GeneratePositiveConstant },
    { { 1, 16,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GeneratePositiveConstant },
    { { 1, 16,  0,  8 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GeneratePositiveConstant },
    { { 1, 24,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GeneratePositiveConstant },
    { { 1, 24,  0,  8 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GeneratePositiveConstant },
    { { 1, 24,  0, 16 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GeneratePositiveConstant },
    { { 2,  8,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_STEREO_MS, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GeneratePositiveConstant },
    { { 2, 16,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_STEREO_MS, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GeneratePositiveConstant },
    { { 2, 16,  0,  8 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_STEREO_MS, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GeneratePositiveConstant },
    { { 2, 24,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_STEREO_MS, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GeneratePositiveConstant },
    { { 2, 24,  0,  8 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_STEREO_MS, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GeneratePositiveConstant },
    { { 2, 24,  0, 16 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_STEREO_MS, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GeneratePositiveConstant },
    { { 8,  8,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      4096,
      testSLAEncodeDecode_GeneratePositiveConstant },
    { { 8, 16,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      4096,
      testSLAEncodeDecode_GeneratePositiveConstant },
    { { 8, 16,  0,  8 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      4096,
      testSLAEncodeDecode_GeneratePositiveConstant },
    { { 8, 24,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      4096,
      testSLAEncodeDecode_GeneratePositiveConstant },
    { { 8, 24,  0,  8 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      4096,
      testSLAEncodeDecode_GeneratePositiveConstant },
    { { 8, 24,  0, 16 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      4096,
      testSLAEncodeDecode_GeneratePositiveConstant },

    /* 負定数信号の部 */
    { { 1,  8,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateNegativeConstant },
    { { 1, 16,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateNegativeConstant },
    { { 1, 16,  0,  8 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateNegativeConstant },
    { { 1, 24,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateNegativeConstant },
    { { 1, 24,  0,  8 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateNegativeConstant },
    { { 1, 24,  0, 16 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateNegativeConstant },
    { { 2,  8,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_STEREO_MS, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateNegativeConstant },
    { { 2, 16,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_STEREO_MS, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateNegativeConstant },
    { { 2, 16,  0,  8 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_STEREO_MS, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateNegativeConstant },
    { { 2, 24,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_STEREO_MS, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateNegativeConstant },
    { { 2, 24,  0,  8 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_STEREO_MS, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateNegativeConstant },
    { { 2, 24,  0, 16 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_STEREO_MS, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateNegativeConstant },
    { { 8,  8,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      4096,
      testSLAEncodeDecode_GenerateNegativeConstant },
    { { 8, 16,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      4096,
      testSLAEncodeDecode_GenerateNegativeConstant },
    { { 8, 16,  0,  8 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      4096,
      testSLAEncodeDecode_GenerateNegativeConstant },
    { { 8, 24,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      4096,
      testSLAEncodeDecode_GenerateNegativeConstant },
    { { 8, 24,  0,  8 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      4096,
      testSLAEncodeDecode_GenerateNegativeConstant },
    { { 8, 24,  0, 16 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      4096,
      testSLAEncodeDecode_GenerateNegativeConstant },

    /* ナイキスト周期振動信号の部 */
    { { 1,  8,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateNyquistOsc },
    { { 1, 16,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateNyquistOsc },
    { { 1, 16,  0,  8 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateNyquistOsc },
    { { 1, 24,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateNyquistOsc },
    { { 1, 24,  0,  8 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateNyquistOsc },
    { { 1, 24,  0, 16 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateNyquistOsc },
    { { 2,  8,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_STEREO_MS, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateNyquistOsc },
    { { 2, 16,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_STEREO_MS, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateNyquistOsc },
    { { 2, 16,  0,  8 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_STEREO_MS, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateNyquistOsc },
    { { 2, 24,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_STEREO_MS, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateNyquistOsc },
    { { 2, 24,  0,  8 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_STEREO_MS, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateNyquistOsc },
    { { 2, 24,  0, 16 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_STEREO_MS, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateNyquistOsc },
    { { 8,  8,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      4096,
      testSLAEncodeDecode_GenerateNyquistOsc },
    { { 8, 16,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      4096,
      testSLAEncodeDecode_GenerateNyquistOsc },
    { { 8, 16,  0,  8 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      4096,
      testSLAEncodeDecode_GenerateNyquistOsc },
    { { 8, 24,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      4096,
      testSLAEncodeDecode_GenerateNyquistOsc },
    { { 8, 24,  0,  8 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      4096,
      testSLAEncodeDecode_GenerateNyquistOsc },
    { { 8, 24,  0, 16 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      4096,
      testSLAEncodeDecode_GenerateNyquistOsc },

    /* ガウス雑音信号の部 */
    { { 1,  8,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateGaussNoise },
    { { 1, 16,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateGaussNoise },
    { { 1, 16,  0,  8 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateGaussNoise },
    { { 1, 24,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateGaussNoise },
    { { 1, 24,  0,  8 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateGaussNoise },
    { { 1, 24,  0, 16 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateGaussNoise },
    { { 2,  8,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_STEREO_MS, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateGaussNoise },
    { { 2, 16,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_STEREO_MS, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateGaussNoise },
    { { 2, 16,  0,  8 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_STEREO_MS, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateGaussNoise },
    { { 2, 24,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_STEREO_MS, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateGaussNoise },
    { { 2, 24,  0,  8 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_STEREO_MS, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateGaussNoise },
    { { 2, 24,  0, 16 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_STEREO_MS, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      8192,
      testSLAEncodeDecode_GenerateGaussNoise },
    { { 8,  8,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      4096,
      testSLAEncodeDecode_GenerateGaussNoise },
    { { 8, 16,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      4096,
      testSLAEncodeDecode_GenerateGaussNoise },
    { { 8, 16,  0,  8 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      4096,
      testSLAEncodeDecode_GenerateGaussNoise },
    { { 8, 24,  0,  0 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      4096,
      testSLAEncodeDecode_GenerateGaussNoise },
    { { 8, 24,  0,  8 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      4096,
      testSLAEncodeDecode_GenerateGaussNoise },
    { { 8, 24,  0, 16 },
      { 5, 1, 5, 1, SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_SIN, 16384 },
      4096,
      testSLAEncodeDecode_GenerateGaussNoise },
  };

  /* テストケース数 */
  const uint32_t num_test_case = sizeof(test_case) / sizeof(test_case[0]);

  TEST_UNUSED_PARAMETER(obj);

  for (test_no = 0; test_no < num_test_case; test_no++) {
    test_ret = testSLAEncodeDecode_DoTestCase(&test_case[test_no]);
    Test_AssertEqual(test_ret, 0);
    if (test_ret != 0) {
      fprintf(stderr, "Encode / Decode Test Failed at case %d. ret:%d \n", test_no, test_ret);
    }
  }

}

void testSLAEncodeDecode_Setup(void)
{
  struct TestSuite *suite
    = Test_AddTestSuite("SLA EncodeDecode Test Suite",
        NULL, testSLAEncodeDecode_Initialize, testSLAEncodeDecode_Finalize);

  Test_AddTest(suite, testSLAEncodeDecode_EncodeDecodeTest);
}
