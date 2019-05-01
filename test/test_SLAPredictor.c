#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "test.h"

/* テスト対象のモジュール */
#include "../SLAPredictor.c"

/* 係数計算関数 */
typedef void (*CalculateCoefFunction)(const float* data, uint32_t num_samples, float* coef, uint32_t order);
/* 予測関数(float) */
typedef void (*PredictFloatFunction)(const float* data, uint32_t num_samples, const float* coef, uint32_t order, float* residual);
/* 合成関数(float) */
typedef void (*SynthesizeFloatFunction)(const float* residual, uint32_t num_samples, const float* coef, uint32_t order, float* output);
/* 予測関数(int32_t) */
typedef void (*PredictInt32Function)(const int32_t* data, uint32_t num_samples, const int32_t* coef, uint32_t order, int32_t* residual);
/* 合成関数(int32_t) */
typedef void (*SynthesizeInt32Function)(const int32_t* residual, uint32_t num_samples, const int32_t* coef, uint32_t order, int32_t* output);
/* 波形生成関数 */
typedef void (*GenerateWaveFunction)(float* data, uint32_t num_samples);

/* 予測合成(float)テストのテストケース */
typedef struct LPCPredictSynthFloatTestCaseTag {
  uint32_t                  order;            /* 次数 */
  uint32_t                  num_samples;      /* サンプル数 */
  GenerateWaveFunction      gen_wave_func;    /* 波形生成関数 */
  CalculateCoefFunction     calc_coef_func;   /* 係数計算関数 */
  PredictFloatFunction      predict_func;     /* 予測関数 */
  SynthesizeFloatFunction   synth_func;       /* 合成関数 */
} LPCPredictSynthFloatTestCase;

/* 予測合成(int32_t)テストのテストケース */
typedef struct LPCPredictSynthInt32TestCaseTag {
  uint32_t                  order;              /* 次数 */
  uint32_t                  num_samples;        /* サンプル数 */
  uint32_t                  data_bit_width;     /* データのビット幅 */
  GenerateWaveFunction      gen_wave_func;      /* 波形生成関数 */
  CalculateCoefFunction     calc_coef_func;     /* 係数計算関数 */
  PredictInt32Function      predict_func;       /* 予測関数 */
  SynthesizeInt32Function   synth_func;         /* 合成関数 */
} LPCPredictSynthInt32TestCase;

/* ロングタームテストのテストケース */
typedef struct SLALongTermTestCaseTag {
  uint32_t              num_taps;                 /* タップ数 */
  uint32_t              num_samples;              /* データサンプル数 */
  uint32_t              data_bit_width;           /* データのビット幅 */
  uint32_t              fft_size;                 /* FFTサイズ */
  uint32_t              max_pitch_num_samples;    /* 最大ピッチ周期 */
  uint32_t              max_num_pitch_candidates; /* 最大ピッチ候補数 */
  GenerateWaveFunction  gen_wave_func;            /* 波形生成関数 */
} SLALongTermTestCase;

int testLPC_Initialize(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);
  return 0;
}

int testLPC_Finalize(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);
  return 0;
}

/* LPC係数計算テスト */
static void testLPC_CalculateCoefTest(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

  /* 生成破棄テスト */
  {
    struct SLALPCCalculator* lpc;

    lpc = SLALPCCalculator_Create(10);
    Test_AssertCondition(lpc != NULL);

    SLALPCCalculator_Destroy(lpc);
    Test_AssertCondition(lpc->a_vec == NULL);
    Test_AssertCondition(lpc->e_vec == NULL);
    Test_AssertCondition(lpc->u_vec == NULL);
    Test_AssertCondition(lpc->v_vec == NULL);
    Test_AssertCondition(lpc->auto_corr   == NULL);
    Test_AssertCondition(lpc->lpc_coef    == NULL);
    Test_AssertCondition(lpc->parcor_coef == NULL);
  }

  /* 無音入力時 */
  {
#define LPC_ORDER   10
#define NUM_SAMPLES 128
    struct SLALPCCalculator* lpc;
    uint32_t  ord, is_ok;
    float     test_data[NUM_SAMPLES];

    /* 無音作成 */
    for (ord = 0; ord < NUM_SAMPLES; ord++) {
      test_data[ord] = 0.0f;
    }

    lpc = SLALPCCalculator_Create(10);

    /* 係数計算を実行 */
    Test_AssertEqual(LPC_CalculateCoef(
          lpc, test_data, NUM_SAMPLES, LPC_ORDER), SLAPREDICTOR_ERROR_OK);

    /* LPC係数は全部0になっているはず */
    is_ok = 1;
    for (ord = 0; ord < LPC_ORDER + 1; ord++) {
      if (fabs(lpc->lpc_coef[ord]) > 0.01f) {
        is_ok = 0;
        break;
      }
    }
    Test_AssertEqual(is_ok, 1);

    SLALPCCalculator_Destroy(lpc);
#undef LPC_ORDER
#undef NUM_SAMPLES
  }

  /* 定数入力時 */
  {
#define LPC_ORDER   10
#define NUM_SAMPLES 128
    struct SLALPCCalculator* lpc;
    uint32_t  ord, is_ok;
    float     test_data[NUM_SAMPLES];

    /* 定数入力作成 */
    for (ord = 0; ord < NUM_SAMPLES; ord++) {
      test_data[ord] = 0.5f;
    }

    lpc = SLALPCCalculator_Create(10);

    /* 係数計算を実行 */
    Test_AssertEqual(LPC_CalculateCoef(
          lpc, test_data, NUM_SAMPLES, LPC_ORDER), SLAPREDICTOR_ERROR_OK);

    Test_SetFloat32Epsilon(0.01f);
    /* 先頭要素は確実に1.0f(自分自身の予測だから),
     * 直前の要素はほぼ-1.0fになるはず(係数には負号が付くから) */
    Test_AssertFloat32EpsilonEqual(lpc->lpc_coef[0], 1.0f);
    Test_AssertFloat32EpsilonEqual(lpc->lpc_coef[1], -1.0f);
    
    /* それ以降の係数は0.0fに近いことを期待 */
    is_ok = 1;
    for (ord = 2; ord < LPC_ORDER + 1; ord++) {
      if (fabs(lpc->lpc_coef[ord]) > 0.01f) {
        is_ok = 0;
        break;
      }
    }
    Test_AssertEqual(is_ok, 1);

    SLALPCCalculator_Destroy(lpc);
#undef LPC_ORDER
#undef NUM_SAMPLES
  }

  /* 1サンプル単位で揺れる入力 */
  {
#define LPC_ORDER   10
#define NUM_SAMPLES 128
    struct SLALPCCalculator* lpc;
    uint32_t  ord, is_ok;
    float     test_data[NUM_SAMPLES];

    /* 1サンプル単位で揺れる入力 */
    for (ord = 0; ord < NUM_SAMPLES; ord++) {
      test_data[ord] = (ord % 2 == 0) ? 1.0f : -1.0f;
    }

    lpc = SLALPCCalculator_Create(10);

    /* 係数計算を実行 */
    Test_AssertEqual(LPC_CalculateCoef(
          lpc, test_data, NUM_SAMPLES, LPC_ORDER), SLAPREDICTOR_ERROR_OK);

    Test_SetFloat32Epsilon(0.01f);
    /* 先頭要素は確実に1.0f(自分自身の予測だから),
     * 直前の要素はほぼ1.0fになるはず(係数には負号が付くから) */
    Test_AssertFloat32EpsilonEqual(lpc->lpc_coef[0], 1.0f);
    Test_AssertFloat32EpsilonEqual(lpc->lpc_coef[1], 1.0f);
    
    /* それ以降の係数は0.0fに近いことを期待 */
    is_ok = 1;
    for (ord = 2; ord < LPC_ORDER + 1; ord++) {
      if (fabs(lpc->lpc_coef[ord]) > 0.01f) {
        is_ok = 0;
        break;
      }
    }
    Test_AssertEqual(is_ok, 1);

    SLALPCCalculator_Destroy(lpc);
#undef LPC_ORDER
#undef NUM_SAMPLES
  }
}

/* データに対してPARCOR係数を計算するユーティリティ関数 */
static void testLPCUtility_CalculateParcorCoef(
    const float* data, uint32_t num_samples, float* parcor_coef, uint32_t order)
{
  struct SLALPCCalculator* lpcc;
  uint32_t ord;
  double* double_coef = (double *)malloc(sizeof(double) * (order + 1));

  assert(data != NULL && parcor_coef != NULL);

  lpcc = SLALPCCalculator_Create(order);
  assert(SLALPCCalculator_CalculatePARCORCoefDouble(lpcc,
        data, num_samples, double_coef, order) == SLAPREDICTOR_APIRESULT_OK);
  SLALPCCalculator_Destroy(lpcc);

  for (ord = 0; ord < order + 1; ord++) {
    parcor_coef[ord] = (float)double_coef[ord];
  }

  free(double_coef);
}

/* 係数計算のダミー関数 */
static void testLPCUtility_CalculateCoefDummyFunction(
    const float* data, uint32_t num_samples, float* coef, uint32_t order)
{
  TEST_UNUSED_PARAMETER(data);
  TEST_UNUSED_PARAMETER(num_samples);
  TEST_UNUSED_PARAMETER(coef);
  TEST_UNUSED_PARAMETER(order);
}

/* PARCOR係数による予測(int32_t) */
static void testLPCUtility_PredictInt32ByParcor(
  const int32_t* data, uint32_t num_samples,
  const int32_t* parcor_coef, uint32_t order, int32_t* residual)
{
  struct SLALPCSynthesizer* lpcs = SLALPCSynthesizer_Create(order);

  assert(SLALPCSynthesizer_PredictByParcorCoefInt32(
        lpcs, data, num_samples, parcor_coef, order, residual) == SLAPREDICTOR_APIRESULT_OK);

  SLALPCSynthesizer_Destroy(lpcs);
}

/* PARCOR係数による合成(int32_t) */
static void testLPCUtility_SynthesizeInt32ByParcor(
    const int32_t* residual, uint32_t num_samples,
    const int32_t* parcor_coef, uint32_t order, int32_t* output)
{
  struct SLALPCSynthesizer* lpcs = SLALPCSynthesizer_Create(order);

  assert(SLALPCSynthesizer_SynthesizeByParcorCoefInt32(
        lpcs, residual, num_samples, parcor_coef, order, output) == SLAPREDICTOR_APIRESULT_OK);

  SLALPCSynthesizer_Destroy(lpcs);
}

/* LMS係数による予測(int32_t) */
static void testLPCUtility_PredictInt32ByLMS(
  const int32_t* data, uint32_t num_samples,
  const int32_t* coef, uint32_t order, int32_t* residual)
{
  struct SLALMSCalculator* nlms;

  TEST_UNUSED_PARAMETER(coef);
  
  nlms = SLALMSCalculator_Create(order);

  assert(SLALMSCalculator_PredictInt32(
        nlms, order, data, num_samples, residual) == SLAPREDICTOR_APIRESULT_OK);

  SLALMSCalculator_Destroy(nlms);
}

/* LMS係数による合成(int32_t) */
static void testLPCUtility_SynthesizeInt32ByLMS(
    const int32_t* residual, uint32_t num_samples,
    const int32_t* coef, uint32_t order, int32_t* output)
{
  struct SLALMSCalculator* nlms;

  TEST_UNUSED_PARAMETER(coef);

  nlms = SLALMSCalculator_Create(order);

  assert(SLALMSCalculator_SynthesizeInt32(
        nlms, order, residual, num_samples, output) == SLAPREDICTOR_APIRESULT_OK);

  SLALMSCalculator_Destroy(nlms);
}

/* 無音の生成 */
static void testLPCUtility_GenerateSilence(float* data, uint32_t num_samples)
{
  uint32_t smpl;

  for (smpl = 0; smpl < num_samples; smpl++) {
    data[smpl] = 0.0f;
  }
}

/* 定数音源の生成 */
static void testLPCUtility_GenerateConstant(float* data, uint32_t num_samples)
{
  uint32_t smpl;

  for (smpl = 0; smpl < num_samples; smpl++) {
    data[smpl] = 1.0f;
  }
}

/* 負値定数音源の生成 */
static void testLPCUtility_GenerateNegativeConstant(float* data, uint32_t num_samples)
{
  uint32_t smpl;

  for (smpl = 0; smpl < num_samples; smpl++) {
    data[smpl] = -1.0f;
  }
}

/* 正弦波の生成 */
static void testLPCUtility_GenerateSineWave(float* data, uint32_t num_samples)
{
  uint32_t smpl;

  for (smpl = 0; smpl < num_samples; smpl++) {
    data[smpl] = sin(smpl);
  }
}

/* 白色雑音の生成 */
static void testLPCUtility_GenerateWhiteNoize(float* data, uint32_t num_samples)
{
  uint32_t smpl;

  srand(0);
  for (smpl = 0; smpl < num_samples; smpl++) {
    data[smpl] = 2.0f * ((float)rand() / RAND_MAX - 0.5f);
  }
}

/* 1サンプル周期で振動する関数 */
static void testLPCUtility_GenerateNyquistOsc(float* data, uint32_t num_samples)
{
  uint32_t smpl;

  for (smpl = 0; smpl < num_samples; smpl++) {
    data[smpl] = (smpl % 2 == 0) ? 1.0f : -1.0f;
  }
}

/* 1つのint32_tテストケースを実行 成功時は1, 失敗時は0を返す */
static uint32_t testSLALPCSynthesizer_DoPredictSynthInt32TestCase(const LPCPredictSynthInt32TestCase* test_case)
{
  int32_t     ret;
  float       *data, *coef;
  int32_t     *int32_data, *int32_coef, *residual, *output;
  uint32_t    num_samples, order, smpl, ord;

  assert(test_case != NULL);

  num_samples       = test_case->num_samples;
  order             = test_case->order;

  /* データの領域割当 */
  data        = (float *)malloc(sizeof(float) * num_samples);
  coef        = (float *)malloc(sizeof(float) * (order + 1));
  int32_data  = (int32_t *)malloc(sizeof(int32_t) * num_samples);
  int32_coef  = (int32_t *)malloc(sizeof(int32_t) * (order + 1));
  residual    = (int32_t *)malloc(sizeof(int32_t) * num_samples);
  output      = (int32_t *)malloc(sizeof(int32_t) * num_samples);

  /* 入力波形の作成 */
  test_case->gen_wave_func(data, num_samples);

  /* 固定小数化 */
  for (smpl = 0; smpl < num_samples; smpl++) {
    int32_data[smpl] = (int32_t)(round((double)data[smpl] * (1UL << test_case->data_bit_width)));
    /*
    printf("%4d %+f %08x %08x \n", 
        smpl, data[smpl], int32_data[smpl], int32_data[smpl] & 0xFFFF0000);
        */
  }

  /* 係数の計算 */
  test_case->calc_coef_func(data, num_samples, coef, order);

  /* 係数固定小数化 */
  for (ord = 0; ord < order + 1; ord++) {
    int32_coef[ord] = (int32_t)(round((double)coef[ord] * (1UL << 31)));
  }

  /* 予測 */
  test_case->predict_func(int32_data, num_samples, int32_coef, order, residual);

  /* 合成 */
  test_case->synth_func(residual, num_samples, int32_coef, order, output);

  /* 一致チェック */
  ret = 1;
  for (smpl = 0; smpl < num_samples; smpl++) {
    if (int32_data[smpl] != output[smpl]) {
      printf("%d vs %d \n", int32_data[smpl], output[smpl]);
      ret = 0;
      goto EXIT_WITH_DATA_RELEASE;
    }
  }

EXIT_WITH_DATA_RELEASE:
  free(data);
  free(coef);
  free(int32_data);
  free(int32_coef);
  free(residual);
  free(output);
  return ret;
}

/* LPCによる予測/合成テスト */
static void testSLALPCSynthesizer_PredictSynthTest(void* obj)
{
  uint32_t i, is_ok;
  /* int32_tテストケース */
  static const LPCPredictSynthInt32TestCase test_case_int32[] = {
    { 16, 8192, 16, testLPCUtility_GenerateSilence, testLPCUtility_CalculateParcorCoef,
      testLPCUtility_PredictInt32ByParcor, testLPCUtility_SynthesizeInt32ByParcor },
    { 16, 8192, 16, testLPCUtility_GenerateConstant, testLPCUtility_CalculateParcorCoef,
      testLPCUtility_PredictInt32ByParcor, testLPCUtility_SynthesizeInt32ByParcor },
    { 16, 8192, 16, testLPCUtility_GenerateNegativeConstant, testLPCUtility_CalculateParcorCoef,
      testLPCUtility_PredictInt32ByParcor, testLPCUtility_SynthesizeInt32ByParcor },
    { 16, 8192, 16, testLPCUtility_GenerateSineWave, testLPCUtility_CalculateParcorCoef,
      testLPCUtility_PredictInt32ByParcor, testLPCUtility_SynthesizeInt32ByParcor },
    { 16, 8192, 16, testLPCUtility_GenerateWhiteNoize, testLPCUtility_CalculateParcorCoef,
      testLPCUtility_PredictInt32ByParcor, testLPCUtility_SynthesizeInt32ByParcor },
    { 16, 8192, 16, testLPCUtility_GenerateNyquistOsc, testLPCUtility_CalculateParcorCoef,
      testLPCUtility_PredictInt32ByParcor, testLPCUtility_SynthesizeInt32ByParcor },
    { 256, 8192, 16, testLPCUtility_GenerateSilence, testLPCUtility_CalculateParcorCoef,
      testLPCUtility_PredictInt32ByParcor, testLPCUtility_SynthesizeInt32ByParcor },
    { 256, 8192, 16, testLPCUtility_GenerateConstant, testLPCUtility_CalculateParcorCoef,
      testLPCUtility_PredictInt32ByParcor, testLPCUtility_SynthesizeInt32ByParcor },
    { 256, 8192, 16, testLPCUtility_GenerateNegativeConstant, testLPCUtility_CalculateParcorCoef,
      testLPCUtility_PredictInt32ByParcor, testLPCUtility_SynthesizeInt32ByParcor },
    { 256, 8192, 16, testLPCUtility_GenerateSineWave, testLPCUtility_CalculateParcorCoef,
      testLPCUtility_PredictInt32ByParcor, testLPCUtility_SynthesizeInt32ByParcor },
    { 256, 8192, 16, testLPCUtility_GenerateWhiteNoize, testLPCUtility_CalculateParcorCoef,
      testLPCUtility_PredictInt32ByParcor, testLPCUtility_SynthesizeInt32ByParcor },
    { 256, 8192, 16, testLPCUtility_GenerateNyquistOsc, testLPCUtility_CalculateParcorCoef,
      testLPCUtility_PredictInt32ByParcor, testLPCUtility_SynthesizeInt32ByParcor },
    { 16, 8192, 24, testLPCUtility_GenerateSilence, testLPCUtility_CalculateParcorCoef,
      testLPCUtility_PredictInt32ByParcor, testLPCUtility_SynthesizeInt32ByParcor },
    { 16, 8192, 24, testLPCUtility_GenerateConstant, testLPCUtility_CalculateParcorCoef,
      testLPCUtility_PredictInt32ByParcor, testLPCUtility_SynthesizeInt32ByParcor },
    { 16, 8192, 24, testLPCUtility_GenerateNegativeConstant, testLPCUtility_CalculateParcorCoef,
      testLPCUtility_PredictInt32ByParcor, testLPCUtility_SynthesizeInt32ByParcor },
    { 16, 8192, 24, testLPCUtility_GenerateSineWave, testLPCUtility_CalculateParcorCoef,
      testLPCUtility_PredictInt32ByParcor, testLPCUtility_SynthesizeInt32ByParcor },
    { 16, 8192, 24, testLPCUtility_GenerateWhiteNoize, testLPCUtility_CalculateParcorCoef,
      testLPCUtility_PredictInt32ByParcor, testLPCUtility_SynthesizeInt32ByParcor },
    { 16, 8192, 24, testLPCUtility_GenerateNyquistOsc, testLPCUtility_CalculateParcorCoef,
      testLPCUtility_PredictInt32ByParcor, testLPCUtility_SynthesizeInt32ByParcor },
    { 256, 8192, 24, testLPCUtility_GenerateSilence, testLPCUtility_CalculateParcorCoef,
      testLPCUtility_PredictInt32ByParcor, testLPCUtility_SynthesizeInt32ByParcor },
    { 256, 8192, 24, testLPCUtility_GenerateConstant, testLPCUtility_CalculateParcorCoef,
      testLPCUtility_PredictInt32ByParcor, testLPCUtility_SynthesizeInt32ByParcor },
    { 256, 8192, 24, testLPCUtility_GenerateNegativeConstant, testLPCUtility_CalculateParcorCoef,
      testLPCUtility_PredictInt32ByParcor, testLPCUtility_SynthesizeInt32ByParcor },
    { 256, 8192, 24, testLPCUtility_GenerateSineWave, testLPCUtility_CalculateParcorCoef,
      testLPCUtility_PredictInt32ByParcor, testLPCUtility_SynthesizeInt32ByParcor },
    { 256, 8192, 24, testLPCUtility_GenerateWhiteNoize, testLPCUtility_CalculateParcorCoef,
      testLPCUtility_PredictInt32ByParcor, testLPCUtility_SynthesizeInt32ByParcor },
    { 256, 8192, 24, testLPCUtility_GenerateNyquistOsc, testLPCUtility_CalculateParcorCoef,
      testLPCUtility_PredictInt32ByParcor, testLPCUtility_SynthesizeInt32ByParcor },
    { 5, 8192, 16, testLPCUtility_GenerateSilence, testLPCUtility_CalculateCoefDummyFunction,
      testLPCUtility_PredictInt32ByLMS, testLPCUtility_SynthesizeInt32ByLMS },
    { 5, 8192, 16, testLPCUtility_GenerateConstant, testLPCUtility_CalculateCoefDummyFunction,
      testLPCUtility_PredictInt32ByLMS, testLPCUtility_SynthesizeInt32ByLMS },
    { 5, 8192, 16, testLPCUtility_GenerateNegativeConstant, testLPCUtility_CalculateCoefDummyFunction,
      testLPCUtility_PredictInt32ByLMS, testLPCUtility_SynthesizeInt32ByLMS },
    { 5, 8192, 16, testLPCUtility_GenerateSineWave, testLPCUtility_CalculateCoefDummyFunction,
      testLPCUtility_PredictInt32ByLMS, testLPCUtility_SynthesizeInt32ByLMS },
    { 5, 8192, 16, testLPCUtility_GenerateWhiteNoize, testLPCUtility_CalculateCoefDummyFunction,
      testLPCUtility_PredictInt32ByLMS, testLPCUtility_SynthesizeInt32ByLMS },
    { 5, 8192, 16, testLPCUtility_GenerateNyquistOsc, testLPCUtility_CalculateCoefDummyFunction,
      testLPCUtility_PredictInt32ByLMS, testLPCUtility_SynthesizeInt32ByLMS },
    {15, 8192, 16, testLPCUtility_GenerateSilence, testLPCUtility_CalculateCoefDummyFunction,
      testLPCUtility_PredictInt32ByLMS, testLPCUtility_SynthesizeInt32ByLMS },
    {15, 8192, 16, testLPCUtility_GenerateConstant, testLPCUtility_CalculateCoefDummyFunction,
      testLPCUtility_PredictInt32ByLMS, testLPCUtility_SynthesizeInt32ByLMS },
    {15, 8192, 16, testLPCUtility_GenerateNegativeConstant, testLPCUtility_CalculateCoefDummyFunction,
      testLPCUtility_PredictInt32ByLMS, testLPCUtility_SynthesizeInt32ByLMS },
    {15, 8192, 16, testLPCUtility_GenerateSineWave, testLPCUtility_CalculateCoefDummyFunction,
      testLPCUtility_PredictInt32ByLMS, testLPCUtility_SynthesizeInt32ByLMS },
    {15, 8192, 16, testLPCUtility_GenerateWhiteNoize, testLPCUtility_CalculateCoefDummyFunction,
      testLPCUtility_PredictInt32ByLMS, testLPCUtility_SynthesizeInt32ByLMS },
    {15, 8192, 16, testLPCUtility_GenerateNyquistOsc, testLPCUtility_CalculateCoefDummyFunction,
      testLPCUtility_PredictInt32ByLMS, testLPCUtility_SynthesizeInt32ByLMS },
    { 5, 8192, 24, testLPCUtility_GenerateSilence, testLPCUtility_CalculateCoefDummyFunction,
      testLPCUtility_PredictInt32ByLMS, testLPCUtility_SynthesizeInt32ByLMS },
    { 5, 8192, 24, testLPCUtility_GenerateConstant, testLPCUtility_CalculateCoefDummyFunction,
      testLPCUtility_PredictInt32ByLMS, testLPCUtility_SynthesizeInt32ByLMS },
    { 5, 8192, 24, testLPCUtility_GenerateNegativeConstant, testLPCUtility_CalculateCoefDummyFunction,
      testLPCUtility_PredictInt32ByLMS, testLPCUtility_SynthesizeInt32ByLMS },
    { 5, 8192, 24, testLPCUtility_GenerateSineWave, testLPCUtility_CalculateCoefDummyFunction,
      testLPCUtility_PredictInt32ByLMS, testLPCUtility_SynthesizeInt32ByLMS },
    { 5, 8192, 24, testLPCUtility_GenerateWhiteNoize, testLPCUtility_CalculateCoefDummyFunction,
      testLPCUtility_PredictInt32ByLMS, testLPCUtility_SynthesizeInt32ByLMS },
    { 5, 8192, 24, testLPCUtility_GenerateNyquistOsc, testLPCUtility_CalculateCoefDummyFunction,
      testLPCUtility_PredictInt32ByLMS, testLPCUtility_SynthesizeInt32ByLMS },
    {15, 8192, 24, testLPCUtility_GenerateSilence, testLPCUtility_CalculateCoefDummyFunction,
      testLPCUtility_PredictInt32ByLMS, testLPCUtility_SynthesizeInt32ByLMS },
    {15, 8192, 24, testLPCUtility_GenerateConstant, testLPCUtility_CalculateCoefDummyFunction,
      testLPCUtility_PredictInt32ByLMS, testLPCUtility_SynthesizeInt32ByLMS },
    {15, 8192, 24, testLPCUtility_GenerateNegativeConstant, testLPCUtility_CalculateCoefDummyFunction,
      testLPCUtility_PredictInt32ByLMS, testLPCUtility_SynthesizeInt32ByLMS },
    {15, 8192, 24, testLPCUtility_GenerateSineWave, testLPCUtility_CalculateCoefDummyFunction,
      testLPCUtility_PredictInt32ByLMS, testLPCUtility_SynthesizeInt32ByLMS },
    {15, 8192, 24, testLPCUtility_GenerateWhiteNoize, testLPCUtility_CalculateCoefDummyFunction,
      testLPCUtility_PredictInt32ByLMS, testLPCUtility_SynthesizeInt32ByLMS },
    {15, 8192, 24, testLPCUtility_GenerateNyquistOsc, testLPCUtility_CalculateCoefDummyFunction,
      testLPCUtility_PredictInt32ByLMS, testLPCUtility_SynthesizeInt32ByLMS },
  };
  /* int32_tテストケース数 */
  const uint32_t num_test_case_int32 = sizeof(test_case_int32) / sizeof(test_case_int32[0]);

  TEST_UNUSED_PARAMETER(obj);

  /* int32_t版のテストを実行 */
  for (i = 0; i < num_test_case_int32; i++) {
    is_ok = testSLALPCSynthesizer_DoPredictSynthInt32TestCase(&test_case_int32[i]);
    Test_AssertEqual(is_ok, 1);
    if (!is_ok) {
      fprintf(stderr, "Int32 test failed at %d \n", i);
    }
  }
}

/* ロングタームの係数計算テスト */
static void testLPCLongTermCalculator_CalculateCoefTest(void* obj)
{
  uint32_t                      i, j, smpl, is_ok;
  const SLALongTermTestCase*    test_case_p;
  struct SLALongTermCalculator* lpcltmc;
  double*                       ltm_coef;
  int32_t*                      int32_ltm_coef;
  float*                        data;
  int32_t*                      int32_data;
  int32_t*                      residual;
  int32_t*                      output;
  uint32_t                      pitch_period;
  SLAPredictorApiResult         ret;
  static const SLALongTermTestCase test_case[] = {
    { 1, 8192, 16, 2 * 8192, 1024, 20, testLPCUtility_GenerateSilence },
    { 1, 8192, 16, 2 * 8192, 1024, 20, testLPCUtility_GenerateConstant },
    { 1, 8192, 16, 2 * 8192, 1024, 20, testLPCUtility_GenerateNegativeConstant },
    { 1, 8192, 16, 2 * 8192, 1024, 20, testLPCUtility_GenerateSineWave },
    { 1, 8192, 16, 2 * 8192, 1024, 20, testLPCUtility_GenerateWhiteNoize },
    { 1, 8192, 16, 2 * 8192, 1024, 20, testLPCUtility_GenerateNyquistOsc },
#if 0
    /* 今はタップ数1のみ */
    { 3, 8192, 16, 2 * 8192, 1024, 20, testLPCUtility_GenerateSilence },
    { 3, 8192, 16, 2 * 8192, 1024, 20, testLPCUtility_GenerateConstant },
    { 3, 8192, 16, 2 * 8192, 1024, 20, testLPCUtility_GenerateNegativeConstant },
    { 3, 8192, 16, 2 * 8192, 1024, 20, testLPCUtility_GenerateSineWave },
    { 3, 8192, 16, 2 * 8192, 1024, 20, testLPCUtility_GenerateWhiteNoize },
    { 3, 8192, 16, 2 * 8192, 1024, 20, testLPCUtility_GenerateNyquistOsc },
#endif
  };
  const uint32_t num_test_case = sizeof(test_case) / sizeof(test_case[0]);

  TEST_UNUSED_PARAMETER(obj);

  /* 全テストケースを実行 */
  for (i = 0; i < num_test_case; i++) {
    double ltm_coef_sum;
    test_case_p = &test_case[i];
    lpcltmc     = SLALongTermCalculator_Create(
                    test_case->fft_size, test_case_p->max_pitch_num_samples, test_case_p->num_taps);
    ltm_coef        = (double *)malloc(sizeof(double) * test_case_p->num_taps);
    int32_ltm_coef  = (int32_t *)malloc(sizeof(int32_t) * test_case_p->num_taps);
    data            = (float *)malloc(sizeof(float) * test_case_p->num_samples);
    int32_data      = (int32_t *)malloc(sizeof(int32_t) * test_case_p->num_samples);
    residual        = (int32_t *)malloc(sizeof(int32_t) * test_case_p->num_samples);
    output          = (int32_t *)malloc(sizeof(int32_t) * test_case_p->num_samples);

    Test_AssertCondition(lpcltmc != NULL);

    /* 波形生成 */
    test_case_p->gen_wave_func(data, test_case_p->num_samples);

    /* 波形を固定小数化 */
    for (smpl = 0; smpl < test_case_p->num_samples; smpl++) {
      int32_data[smpl] = (int32_t)(round((double)data[smpl] * (1UL << test_case->data_bit_width)));
    }

    /* 解析 */
    ret = SLALongTermCalculator_CalculateCoef(lpcltmc,
        int32_data, test_case_p->num_samples, &pitch_period,
        ltm_coef, test_case_p->num_taps);

    /* 返り値のチェック */
    Test_AssertCondition(ret == SLAPREDICTOR_APIRESULT_OK || ret == SLAPREDICTOR_APIRESULT_FAILED_TO_CALCULATION);

    if (ret == SLAPREDICTOR_APIRESULT_OK) {

      if (lpcltmc->auto_corr[0] > 0) {
        /* ピッチ周期は0より大きいか */
        Test_AssertCondition(pitch_period > 0);
        Test_AssertCondition(lpcltmc->auto_corr[0] > 0.0f);
      } else {
        /* 無音フレーム判定になっているか */
        Test_AssertCondition(pitch_period == 0);
        Test_AssertFloat32EpsilonEqual(lpcltmc->auto_corr[0], 0.0f);
      }

      /* 自己相関値は1.0fを越えていないか */
      Test_AssertCondition(lpcltmc->auto_corr[0] <= 1.0f);

      /* ピッチ周期は最大を越えていないか */
      Test_AssertCondition(pitch_period <= test_case_p->max_pitch_num_samples);

      /* 係数の絶対値和が1以下（収束条件）を満たしているか */
      ltm_coef_sum = 0.0f;
      for (j = 0; j < test_case_p->num_taps; j++) {
        ltm_coef_sum += fabs(ltm_coef[j]);
        /* printf("%e, ", ltm_coef[j]); */
      }
      /* printf("Pitch:%d \n", pitch_period); */
      Test_AssertCondition(ltm_coef_sum < 1.0f);

      /* 係数固定小数化 */
      for (j = 0; j < test_case_p->num_taps; j++) {
        int32_ltm_coef[j] = (int32_t)(round(ltm_coef[j] * (1UL << 31)));
      }

      /* 残差計算 */
      Test_AssertEqual(
          SLALongTerm_PredictInt32(
            int32_data, test_case_p->num_samples,
            pitch_period, int32_ltm_coef, test_case_p->num_taps,
            residual),
          SLAPREDICTOR_APIRESULT_OK);

      /* 合成 */
      Test_AssertEqual(
          SLALongTerm_SynthesizeInt32(
            residual, test_case_p->num_samples,
            pitch_period, int32_ltm_coef, test_case_p->num_taps,
            output),
          SLAPREDICTOR_APIRESULT_OK);

      /* 一致確認 */
      is_ok = 1;
      for (smpl = 0; smpl < test_case_p->num_samples; smpl++) {
        if (output[smpl] != int32_data[smpl]) {
          printf("%d vs %d \n", output[smpl], int32_data[smpl]);
          is_ok = 0;
          break;
        }
      }
      Test_AssertEqual(is_ok, 1);
    }

    /* データ開放 */
    free(data);
    free(residual);
    free(int32_data);
    free(output);
    free(int32_ltm_coef);
    free(ltm_coef);
    SLALongTermCalculator_Destroy(lpcltmc);
  }
}

/* 指定周期で正弦波を生成 */
static void testLPCLongTermCalculator_GeneratePeriodSineWave(
    uint32_t period, double* data, uint32_t num_data)
{
  uint32_t i;
  assert(data != NULL);

  for (i = 0; i < num_data; i++) {
    data[i] = sin(fmod((2.0f * M_PI * i) / period, 2.0f * M_PI));
  }
}

/* ロングタームのピッチ予測テスト */
static void testLPCLongTermCalculator_PitchDetectTest(void* obj)
{
#define NUM_SAMPLES (4096)
  uint32_t i, smpl;
  /* テストケースたる周期リスト */
  static const uint32_t test_case[] = {
    /* 2 -> テストケースにならない 
     * sin(pi), sin(2pi), ..., = 0, 0, ..., となるため */
    3,                /* 意味のある最小周期 */
    4, 5, 6, 7, 8, 9, 10, 
    16, 32, 64, 128, 256, 512, 1024, 2048,
    NUM_SAMPLES - 1,  /* 意味のある最大周期 */
  };
  const uint32_t num_test_case = sizeof(test_case) / sizeof(test_case[0]);
  double* data;
  int32_t* int32_data;
  struct SLALongTermCalculator* lpcltmc;
  uint32_t pitch_period;
  double dummy_ltm_coef;

  TEST_UNUSED_PARAMETER(obj);

  data        = (double *)malloc(sizeof(double) * NUM_SAMPLES);
  int32_data  = (int32_t *)malloc(sizeof(int32_t) * NUM_SAMPLES);
  lpcltmc     = SLALongTermCalculator_Create(NUM_SAMPLES * 2, NUM_SAMPLES, 20);

  for (i = 0; i < num_test_case; i++) {
    /* 波形生成 */
    testLPCLongTermCalculator_GeneratePeriodSineWave(
        test_case[i], data, NUM_SAMPLES);
    /* 波形を固定小数化 */
    for (smpl = 0; smpl < NUM_SAMPLES; smpl++) {
      if (data[smpl] > 0.0f) {
        int32_data[smpl] = (int32_t)(round(data[smpl] * ((1UL << 31) - 1)));
      } else {
        int32_data[smpl] = (int32_t)(round(data[smpl] * (1UL << 31)));
      }
    }
    /* ピッチ解析 */
    Test_AssertEqual(
        SLALongTermCalculator_CalculateCoef(lpcltmc,
          int32_data, NUM_SAMPLES, &pitch_period,
          &dummy_ltm_coef, 1),
        SLAPREDICTOR_APIRESULT_OK);
    Test_AssertEqual(pitch_period, test_case[i]);
  }

  SLALongTermCalculator_Destroy(lpcltmc);
  free(data);
  free(int32_data);
#undef NUM_SAMPLES 
}

void testSLAPredictor_Setup(void)
{
  struct TestSuite *suite
    = Test_AddTestSuite("SLA Predictor Test Suite",
        NULL, testLPC_Initialize, testLPC_Finalize);

  Test_AddTest(suite, testLPC_CalculateCoefTest);
  Test_AddTest(suite, testSLALPCSynthesizer_PredictSynthTest);
  Test_AddTest(suite, testLPCLongTermCalculator_CalculateCoefTest);
  Test_AddTest(suite, testLPCLongTermCalculator_PitchDetectTest);
}
