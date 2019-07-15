#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "test.h"

/* テスト対象のモジュール */
#include "../SLAPredictor.c"

/* 係数計算関数 */
typedef void (*CalculateCoefFunction)(const double* data, uint32_t num_samples, double* coef, uint32_t order);
/* 予測関数(double) */
typedef void (*PredictFloatFunction)(const double* data, uint32_t num_samples, const double* coef, uint32_t order, double* residual);
/* 合成関数(double) */
typedef void (*SynthesizeFloatFunction)(const double* residual, uint32_t num_samples, const double* coef, uint32_t order, double* output);
/* 予測関数(int32_t) */
typedef void (*PredictInt32Function)(const int32_t* data, uint32_t num_samples, const int32_t* coef, uint32_t order, int32_t* residual);
/* 合成関数(int32_t) */
typedef void (*SynthesizeInt32Function)(const int32_t* residual, uint32_t num_samples, const int32_t* coef, uint32_t order, int32_t* output);
/* 波形生成関数 */
typedef void (*GenerateWaveFunction)(double* data, uint32_t num_samples);

/* 予測合成(double)テストのテストケース */
typedef struct LPCPredictSynthDoubleTestCaseTag {
  uint32_t                  order;            /* 次数 */
  uint32_t                  num_samples;      /* サンプル数 */
  GenerateWaveFunction      gen_wave_func;    /* 波形生成関数 */
  CalculateCoefFunction     calc_coef_func;   /* 係数計算関数 */
  PredictFloatFunction      predict_func;     /* 予測関数 */
  SynthesizeFloatFunction   synth_func;       /* 合成関数 */
} LPCPredictSynthDoubleTestCase;

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
    double    test_data[NUM_SAMPLES];

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
    double    test_data[NUM_SAMPLES];

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
    double    test_data[NUM_SAMPLES];

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
static void testSLAPredictor_CalculateParcorCoef(
    const double* data, uint32_t num_samples, double* parcor_coef, uint32_t order)
{
  struct SLALPCCalculator* lpcc;

  assert(data != NULL && parcor_coef != NULL);

  lpcc = SLALPCCalculator_Create(order);
  assert(SLALPCCalculator_CalculatePARCORCoefDouble(lpcc,
        data, num_samples, parcor_coef, order) == SLAPREDICTOR_APIRESULT_OK);
  SLALPCCalculator_Destroy(lpcc);
}

/* 係数計算のダミー関数 */
static void testSLAPredictor_CalculateCoefDummyFunction(
    const double* data, uint32_t num_samples, double* coef, uint32_t order)
{
  TEST_UNUSED_PARAMETER(data);
  TEST_UNUSED_PARAMETER(num_samples);
  TEST_UNUSED_PARAMETER(coef);
  TEST_UNUSED_PARAMETER(order);
}

/* PARCOR係数による予測(int32_t) */
static void testSLAPredictor_PredictInt32ByParcor(
  const int32_t* data, uint32_t num_samples,
  const int32_t* parcor_coef, uint32_t order, int32_t* residual)
{
  struct SLALPCSynthesizer* lpcs = SLALPCSynthesizer_Create(order);

  assert(SLALPCSynthesizer_Reset(lpcs) == SLAPREDICTOR_APIRESULT_OK);
  assert(SLALPCSynthesizer_PredictByParcorCoefInt32(
        lpcs, data, num_samples, parcor_coef, order, residual) == SLAPREDICTOR_APIRESULT_OK);

  SLALPCSynthesizer_Destroy(lpcs);
}

/* PARCOR係数による合成(int32_t) */
static void testSLAPredictor_SynthesizeInt32ByParcor(
    const int32_t* residual, uint32_t num_samples,
    const int32_t* parcor_coef, uint32_t order, int32_t* output)
{
  struct SLALPCSynthesizer* lpcs = SLALPCSynthesizer_Create(order);

  assert(SLALPCSynthesizer_Reset(lpcs) == SLAPREDICTOR_APIRESULT_OK);
  assert(SLALPCSynthesizer_SynthesizeByParcorCoefInt32(
        lpcs, residual, num_samples, parcor_coef, order, output) == SLAPREDICTOR_APIRESULT_OK);

  SLALPCSynthesizer_Destroy(lpcs);
}

/* LMS係数による予測(int32_t) */
static void testSLAPredictor_PredictInt32ByLMS(
  const int32_t* data, uint32_t num_samples,
  const int32_t* coef, uint32_t order, int32_t* residual)
{
  struct SLALMSFilter* nlms;

  TEST_UNUSED_PARAMETER(coef);
  
  nlms = SLALMSFilter_Create(order);

  assert(SLALMSFilter_Reset(nlms) == SLAPREDICTOR_APIRESULT_OK);
  assert(SLALMSFilter_PredictInt32(
        nlms, order, data, num_samples, residual) == SLAPREDICTOR_APIRESULT_OK);

  SLALMSFilter_Destroy(nlms);
}

/* LMS係数による合成(int32_t) */
static void testSLAPredictor_SynthesizeInt32ByLMS(
    const int32_t* residual, uint32_t num_samples,
    const int32_t* coef, uint32_t order, int32_t* output)
{
  struct SLALMSFilter* nlms;

  TEST_UNUSED_PARAMETER(coef);

  nlms = SLALMSFilter_Create(order);

  assert(SLALMSFilter_Reset(nlms) == SLAPREDICTOR_APIRESULT_OK);
  assert(SLALMSFilter_SynthesizeInt32(
        nlms, order, residual, num_samples, output) == SLAPREDICTOR_APIRESULT_OK);

  SLALMSFilter_Destroy(nlms);
}

/* 無音の生成 */
static void testSLAPredictor_GenerateSilence(double* data, uint32_t num_samples)
{
  uint32_t smpl;

  for (smpl = 0; smpl < num_samples; smpl++) {
    data[smpl] = 0.0f;
  }
}

/* 定数音源の生成 */
static void testSLAPredictor_GenerateConstant(double* data, uint32_t num_samples)
{
  uint32_t smpl;

  for (smpl = 0; smpl < num_samples; smpl++) {
    data[smpl] = 1.0f;
  }
}

/* 負値定数音源の生成 */
static void testSLAPredictor_GenerateNegativeConstant(double* data, uint32_t num_samples)
{
  uint32_t smpl;

  for (smpl = 0; smpl < num_samples; smpl++) {
    data[smpl] = -1.0f;
  }
}

/* 正弦波の生成 */
static void testSLAPredictor_GenerateSineWave(double* data, uint32_t num_samples)
{
  uint32_t smpl;

  for (smpl = 0; smpl < num_samples; smpl++) {
    data[smpl] = sin(smpl);
  }
}

/* 白色雑音の生成 */
static void testSLAPredictor_GenerateWhiteNoize(double* data, uint32_t num_samples)
{
  uint32_t smpl;

  srand(0);
  for (smpl = 0; smpl < num_samples; smpl++) {
    data[smpl] = 2.0f * ((double)rand() / RAND_MAX - 0.5f);
  }
}

/* 1サンプル周期で振動する関数 */
static void testSLAPredictor_GenerateNyquistOsc(double* data, uint32_t num_samples)
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
  double      *data, *coef;
  int32_t     *int32_data, *int32_coef, *residual, *output;
  uint32_t    num_samples, order, smpl, ord, bitwidth, rshift;

  assert(test_case != NULL);

  num_samples       = test_case->num_samples;
  order             = test_case->order;

  /* データの領域割当 */
  data        = (double *)malloc(sizeof(double) * num_samples);
  coef        = (double *)malloc(sizeof(double) * (order + 1));
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

  /* 入力データのビット幅計測 */
  bitwidth = SLAUtility_GetDataBitWidth(int32_data, num_samples);
  /* 16bitより大きければ乗算時の右シフト量を決める */
  rshift   = SLAUTILITY_CALC_RSHIFT_FOR_SINT32(bitwidth);
  /* 係数を16bitベースに右シフト */
  for (ord = 0; ord < order + 1; ord++) {
    int32_coef[ord] = SLAUTILITY_SHIFT_RIGHT_ARITHMETIC(int32_coef[ord], 16 + rshift);
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
    { 16, 8192, 16, testSLAPredictor_GenerateSilence, testSLAPredictor_CalculateParcorCoef,
      testSLAPredictor_PredictInt32ByParcor, testSLAPredictor_SynthesizeInt32ByParcor },
    { 16, 8192, 16, testSLAPredictor_GenerateConstant, testSLAPredictor_CalculateParcorCoef,
      testSLAPredictor_PredictInt32ByParcor, testSLAPredictor_SynthesizeInt32ByParcor },
    { 16, 8192, 16, testSLAPredictor_GenerateNegativeConstant, testSLAPredictor_CalculateParcorCoef,
      testSLAPredictor_PredictInt32ByParcor, testSLAPredictor_SynthesizeInt32ByParcor },
    { 16, 8192, 16, testSLAPredictor_GenerateSineWave, testSLAPredictor_CalculateParcorCoef,
      testSLAPredictor_PredictInt32ByParcor, testSLAPredictor_SynthesizeInt32ByParcor },
    { 16, 8192, 16, testSLAPredictor_GenerateWhiteNoize, testSLAPredictor_CalculateParcorCoef,
      testSLAPredictor_PredictInt32ByParcor, testSLAPredictor_SynthesizeInt32ByParcor },
    { 16, 8192, 16, testSLAPredictor_GenerateNyquistOsc, testSLAPredictor_CalculateParcorCoef,
      testSLAPredictor_PredictInt32ByParcor, testSLAPredictor_SynthesizeInt32ByParcor },
    { 256, 8192, 16, testSLAPredictor_GenerateSilence, testSLAPredictor_CalculateParcorCoef,
      testSLAPredictor_PredictInt32ByParcor, testSLAPredictor_SynthesizeInt32ByParcor },
    { 256, 8192, 16, testSLAPredictor_GenerateConstant, testSLAPredictor_CalculateParcorCoef,
      testSLAPredictor_PredictInt32ByParcor, testSLAPredictor_SynthesizeInt32ByParcor },
    { 256, 8192, 16, testSLAPredictor_GenerateNegativeConstant, testSLAPredictor_CalculateParcorCoef,
      testSLAPredictor_PredictInt32ByParcor, testSLAPredictor_SynthesizeInt32ByParcor },
    { 256, 8192, 16, testSLAPredictor_GenerateSineWave, testSLAPredictor_CalculateParcorCoef,
      testSLAPredictor_PredictInt32ByParcor, testSLAPredictor_SynthesizeInt32ByParcor },
    { 256, 8192, 16, testSLAPredictor_GenerateWhiteNoize, testSLAPredictor_CalculateParcorCoef,
      testSLAPredictor_PredictInt32ByParcor, testSLAPredictor_SynthesizeInt32ByParcor },
    { 256, 8192, 16, testSLAPredictor_GenerateNyquistOsc, testSLAPredictor_CalculateParcorCoef,
      testSLAPredictor_PredictInt32ByParcor, testSLAPredictor_SynthesizeInt32ByParcor },
    { 16, 8192, 24, testSLAPredictor_GenerateSilence, testSLAPredictor_CalculateParcorCoef,
      testSLAPredictor_PredictInt32ByParcor, testSLAPredictor_SynthesizeInt32ByParcor },
    { 16, 8192, 24, testSLAPredictor_GenerateConstant, testSLAPredictor_CalculateParcorCoef,
      testSLAPredictor_PredictInt32ByParcor, testSLAPredictor_SynthesizeInt32ByParcor },
    { 16, 8192, 24, testSLAPredictor_GenerateNegativeConstant, testSLAPredictor_CalculateParcorCoef,
      testSLAPredictor_PredictInt32ByParcor, testSLAPredictor_SynthesizeInt32ByParcor },
    { 16, 8192, 24, testSLAPredictor_GenerateSineWave, testSLAPredictor_CalculateParcorCoef,
      testSLAPredictor_PredictInt32ByParcor, testSLAPredictor_SynthesizeInt32ByParcor },
    { 16, 8192, 24, testSLAPredictor_GenerateWhiteNoize, testSLAPredictor_CalculateParcorCoef,
      testSLAPredictor_PredictInt32ByParcor, testSLAPredictor_SynthesizeInt32ByParcor },
    { 16, 8192, 24, testSLAPredictor_GenerateNyquistOsc, testSLAPredictor_CalculateParcorCoef,
      testSLAPredictor_PredictInt32ByParcor, testSLAPredictor_SynthesizeInt32ByParcor },
    { 256, 8192, 24, testSLAPredictor_GenerateSilence, testSLAPredictor_CalculateParcorCoef,
      testSLAPredictor_PredictInt32ByParcor, testSLAPredictor_SynthesizeInt32ByParcor },
    { 256, 8192, 24, testSLAPredictor_GenerateConstant, testSLAPredictor_CalculateParcorCoef,
      testSLAPredictor_PredictInt32ByParcor, testSLAPredictor_SynthesizeInt32ByParcor },
    { 256, 8192, 24, testSLAPredictor_GenerateNegativeConstant, testSLAPredictor_CalculateParcorCoef,
      testSLAPredictor_PredictInt32ByParcor, testSLAPredictor_SynthesizeInt32ByParcor },
    { 256, 8192, 24, testSLAPredictor_GenerateSineWave, testSLAPredictor_CalculateParcorCoef,
      testSLAPredictor_PredictInt32ByParcor, testSLAPredictor_SynthesizeInt32ByParcor },
    { 256, 8192, 24, testSLAPredictor_GenerateWhiteNoize, testSLAPredictor_CalculateParcorCoef,
      testSLAPredictor_PredictInt32ByParcor, testSLAPredictor_SynthesizeInt32ByParcor },
    { 256, 8192, 24, testSLAPredictor_GenerateNyquistOsc, testSLAPredictor_CalculateParcorCoef,
      testSLAPredictor_PredictInt32ByParcor, testSLAPredictor_SynthesizeInt32ByParcor },
    { 5, 8192, 16, testSLAPredictor_GenerateSilence, testSLAPredictor_CalculateCoefDummyFunction,
      testSLAPredictor_PredictInt32ByLMS, testSLAPredictor_SynthesizeInt32ByLMS },
    { 5, 8192, 16, testSLAPredictor_GenerateConstant, testSLAPredictor_CalculateCoefDummyFunction,
      testSLAPredictor_PredictInt32ByLMS, testSLAPredictor_SynthesizeInt32ByLMS },
    { 5, 8192, 16, testSLAPredictor_GenerateNegativeConstant, testSLAPredictor_CalculateCoefDummyFunction,
      testSLAPredictor_PredictInt32ByLMS, testSLAPredictor_SynthesizeInt32ByLMS },
    { 5, 8192, 16, testSLAPredictor_GenerateSineWave, testSLAPredictor_CalculateCoefDummyFunction,
      testSLAPredictor_PredictInt32ByLMS, testSLAPredictor_SynthesizeInt32ByLMS },
    { 5, 8192, 16, testSLAPredictor_GenerateWhiteNoize, testSLAPredictor_CalculateCoefDummyFunction,
      testSLAPredictor_PredictInt32ByLMS, testSLAPredictor_SynthesizeInt32ByLMS },
    { 5, 8192, 16, testSLAPredictor_GenerateNyquistOsc, testSLAPredictor_CalculateCoefDummyFunction,
      testSLAPredictor_PredictInt32ByLMS, testSLAPredictor_SynthesizeInt32ByLMS },
    {15, 8192, 16, testSLAPredictor_GenerateSilence, testSLAPredictor_CalculateCoefDummyFunction,
      testSLAPredictor_PredictInt32ByLMS, testSLAPredictor_SynthesizeInt32ByLMS },
    {15, 8192, 16, testSLAPredictor_GenerateConstant, testSLAPredictor_CalculateCoefDummyFunction,
      testSLAPredictor_PredictInt32ByLMS, testSLAPredictor_SynthesizeInt32ByLMS },
    {15, 8192, 16, testSLAPredictor_GenerateNegativeConstant, testSLAPredictor_CalculateCoefDummyFunction,
      testSLAPredictor_PredictInt32ByLMS, testSLAPredictor_SynthesizeInt32ByLMS },
    {15, 8192, 16, testSLAPredictor_GenerateSineWave, testSLAPredictor_CalculateCoefDummyFunction,
      testSLAPredictor_PredictInt32ByLMS, testSLAPredictor_SynthesizeInt32ByLMS },
    {15, 8192, 16, testSLAPredictor_GenerateWhiteNoize, testSLAPredictor_CalculateCoefDummyFunction,
      testSLAPredictor_PredictInt32ByLMS, testSLAPredictor_SynthesizeInt32ByLMS },
    {15, 8192, 16, testSLAPredictor_GenerateNyquistOsc, testSLAPredictor_CalculateCoefDummyFunction,
      testSLAPredictor_PredictInt32ByLMS, testSLAPredictor_SynthesizeInt32ByLMS },
    { 5, 8192, 24, testSLAPredictor_GenerateSilence, testSLAPredictor_CalculateCoefDummyFunction,
      testSLAPredictor_PredictInt32ByLMS, testSLAPredictor_SynthesizeInt32ByLMS },
    { 5, 8192, 24, testSLAPredictor_GenerateConstant, testSLAPredictor_CalculateCoefDummyFunction,
      testSLAPredictor_PredictInt32ByLMS, testSLAPredictor_SynthesizeInt32ByLMS },
    { 5, 8192, 24, testSLAPredictor_GenerateNegativeConstant, testSLAPredictor_CalculateCoefDummyFunction,
      testSLAPredictor_PredictInt32ByLMS, testSLAPredictor_SynthesizeInt32ByLMS },
    { 5, 8192, 24, testSLAPredictor_GenerateSineWave, testSLAPredictor_CalculateCoefDummyFunction,
      testSLAPredictor_PredictInt32ByLMS, testSLAPredictor_SynthesizeInt32ByLMS },
    { 5, 8192, 24, testSLAPredictor_GenerateWhiteNoize, testSLAPredictor_CalculateCoefDummyFunction,
      testSLAPredictor_PredictInt32ByLMS, testSLAPredictor_SynthesizeInt32ByLMS },
    { 5, 8192, 24, testSLAPredictor_GenerateNyquistOsc, testSLAPredictor_CalculateCoefDummyFunction,
      testSLAPredictor_PredictInt32ByLMS, testSLAPredictor_SynthesizeInt32ByLMS },
    {15, 8192, 24, testSLAPredictor_GenerateSilence, testSLAPredictor_CalculateCoefDummyFunction,
      testSLAPredictor_PredictInt32ByLMS, testSLAPredictor_SynthesizeInt32ByLMS },
    {15, 8192, 24, testSLAPredictor_GenerateConstant, testSLAPredictor_CalculateCoefDummyFunction,
      testSLAPredictor_PredictInt32ByLMS, testSLAPredictor_SynthesizeInt32ByLMS },
    {15, 8192, 24, testSLAPredictor_GenerateNegativeConstant, testSLAPredictor_CalculateCoefDummyFunction,
      testSLAPredictor_PredictInt32ByLMS, testSLAPredictor_SynthesizeInt32ByLMS },
    {15, 8192, 24, testSLAPredictor_GenerateSineWave, testSLAPredictor_CalculateCoefDummyFunction,
      testSLAPredictor_PredictInt32ByLMS, testSLAPredictor_SynthesizeInt32ByLMS },
    {15, 8192, 24, testSLAPredictor_GenerateWhiteNoize, testSLAPredictor_CalculateCoefDummyFunction,
      testSLAPredictor_PredictInt32ByLMS, testSLAPredictor_SynthesizeInt32ByLMS },
    {15, 8192, 24, testSLAPredictor_GenerateNyquistOsc, testSLAPredictor_CalculateCoefDummyFunction,
      testSLAPredictor_PredictInt32ByLMS, testSLAPredictor_SynthesizeInt32ByLMS },
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
  struct SLALongTermSynthesizer* ltms;
  double*                       ltm_coef;
  int32_t*                      int32_ltm_coef;
  double*                       data;
  int32_t*                      int32_data;
  int32_t*                      residual;
  int32_t*                      output;
  uint32_t                      pitch_period;
  SLAPredictorApiResult         ret;
  static const SLALongTermTestCase test_case[] = {
    { 1, 8192, 16, 2 * 8192, 1024, 20, testSLAPredictor_GenerateSilence },
    { 1, 8192, 16, 2 * 8192, 1024, 20, testSLAPredictor_GenerateConstant },
    { 1, 8192, 16, 2 * 8192, 1024, 20, testSLAPredictor_GenerateNegativeConstant },
    { 1, 8192, 16, 2 * 8192, 1024, 20, testSLAPredictor_GenerateSineWave },
    { 1, 8192, 16, 2 * 8192, 1024, 20, testSLAPredictor_GenerateWhiteNoize },
    { 1, 8192, 16, 2 * 8192, 1024, 20, testSLAPredictor_GenerateNyquistOsc },
    { 3, 8192, 16, 2 * 8192, 1024, 20, testSLAPredictor_GenerateSilence },
    { 3, 8192, 16, 2 * 8192, 1024, 20, testSLAPredictor_GenerateConstant },
    { 3, 8192, 16, 2 * 8192, 1024, 20, testSLAPredictor_GenerateNegativeConstant },
    { 3, 8192, 16, 2 * 8192, 1024, 20, testSLAPredictor_GenerateSineWave },
    { 3, 8192, 16, 2 * 8192, 1024, 20, testSLAPredictor_GenerateWhiteNoize },
    { 3, 8192, 16, 2 * 8192, 1024, 20, testSLAPredictor_GenerateNyquistOsc },
    { 5, 8192, 16, 2 * 8192, 1024, 20, testSLAPredictor_GenerateSilence },
    { 5, 8192, 16, 2 * 8192, 1024, 20, testSLAPredictor_GenerateConstant },
    { 5, 8192, 16, 2 * 8192, 1024, 20, testSLAPredictor_GenerateNegativeConstant },
    { 5, 8192, 16, 2 * 8192, 1024, 20, testSLAPredictor_GenerateSineWave },
    { 5, 8192, 16, 2 * 8192, 1024, 20, testSLAPredictor_GenerateWhiteNoize },
    { 5, 8192, 16, 2 * 8192, 1024, 20, testSLAPredictor_GenerateNyquistOsc },
  };
  const uint32_t num_test_case = sizeof(test_case) / sizeof(test_case[0]);

  TEST_UNUSED_PARAMETER(obj);

  /* 全テストケースを実行 */
  for (i = 0; i < num_test_case; i++) {
    double ltm_coef_sum;
    test_case_p = &test_case[i];
    lpcltmc     = SLALongTermCalculator_Create(
                    test_case->fft_size, test_case_p->max_pitch_num_samples,
                    test_case_p->num_taps, test_case_p->num_taps);
    ltms            = SLALongTermSynthesizer_Create(test_case_p->num_taps, test_case_p->max_pitch_num_samples);
    ltm_coef        = (double *)malloc(sizeof(double) * test_case_p->num_taps);
    int32_ltm_coef  = (int32_t *)malloc(sizeof(int32_t) * test_case_p->num_taps);
    data            = (double *)malloc(sizeof(double) * test_case_p->num_samples);
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

    /* 係数をクリア */
    for (j = 0; j < test_case_p->num_taps; j++) {
      ltm_coef[j] = 0.0f;
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
        /* printf("%d %e, ", j, ltm_coef[j]); */
      }
      /* printf("Pitch:%d \n", pitch_period); */
      Test_AssertCondition(ltm_coef_sum < 1.0f);

      /* 係数固定小数化 */
      for (j = 0; j < test_case_p->num_taps; j++) {
        int32_ltm_coef[j] = (int32_t)(round(ltm_coef[j] * (1UL << 31)));
      }

      /* 残差計算 */
      Test_AssertEqual(
          SLALongTermSynthesizer_PredictInt32(
            ltms, int32_data, test_case_p->num_samples,
            pitch_period, int32_ltm_coef, test_case_p->num_taps,
            residual),
          SLAPREDICTOR_APIRESULT_OK);

      /* 合成 */
      SLALongTermSynthesizer_Reset(ltms);
      Test_AssertEqual(
          SLALongTermSynthesizer_SynthesizeInt32(
            ltms, residual, test_case_p->num_samples,
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
    SLALongTermSynthesizer_Destroy(ltms);
  }
}

/* 指定周期で正弦波を生成 */
static void testLPCLongTermCalculator_GeneratePeriodSineWave(
    uint32_t period, double* data, uint32_t num_data)
{
  uint32_t i;
  assert(data != NULL);

  for (i = 0; i < num_data; i++) {
    data[i] = sin(fmod((2.0f * SLA_PI * i) / period, 2.0f * SLA_PI));
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
  lpcltmc     = SLALongTermCalculator_Create(NUM_SAMPLES * 2, NUM_SAMPLES, 20, 1);

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

/* （デバッグ用）隣接行列と結果の表示 */
void SLAOptimalEncodeEstimator_PrettyPrint(
    const struct SLAOptimalBlockPartitionEstimator* oee,
    uint32_t num_nodes, uint32_t start_node, uint32_t goal_node)
{
  uint32_t i, j, tmp_node;

  printf("----- ");
  for (i = 0; i < num_nodes; i++) {
    printf("%5d ", i);
  }
  printf("\n");
  for (i = 0; i < num_nodes; i++) {
    printf("%5d ", i);
    for (j = 0; j < num_nodes; j++) {
      if (oee->adjacency_matrix[i][j] != SLAOPTIMALENCODEESTIMATOR_DIJKSTRA_BIGWEIGHT) {
        printf("%5d ", (int32_t)(oee->adjacency_matrix[i][j]));
      } else {
        printf("----- ");
      }
    }
    printf("\n");
  }

  /* 最適パスの表示 */
  tmp_node = goal_node;
  printf("%d ", tmp_node);
  while (tmp_node != start_node) {
    tmp_node = oee->path[tmp_node];
    printf("<- %d ", tmp_node);
  }
  printf("\n");

  /* 最小コストの表示 */
  printf("Min Cost: %e \n", oee->cost[goal_node]);
}

static void testSLAOptimalEncodeEstimator_DijkstraTest(void* obj)
{
  /* 隣接行列の重み */
  typedef struct DijkstraTestCaseAdjacencyMatrixWeightTag {
    uint32_t  i;
    uint32_t  j;
    double    weight;
  } DijkstraTestCaseAdjacencyMatrixWeight;

  /* ダイクストラ法のテストケース */
  typedef struct DijkstraTestCaseTag {
    uint32_t  num_nodes;
    uint32_t  start_node;
    uint32_t  goal_node;
    double    min_cost;
    uint32_t  num_weights;
    const DijkstraTestCaseAdjacencyMatrixWeight* weight;
    uint32_t  len_answer_route_path;
    const uint32_t*  answer_route_path;
  } DijkstraTestCase; 

  /* テストケース0の重み */
  static const DijkstraTestCaseAdjacencyMatrixWeight test_case0_weight[] = {
    { 0, 1, 114514 }
  };
  /* テストケース0の回答経路 */
  static const uint32_t test_case0_answer_route[] = { 0, 1 };

  /* テストケース1の重み */
  static const DijkstraTestCaseAdjacencyMatrixWeight test_case1_weight[] = {
    { 0, 1, 30 }, { 0, 3, 10 }, { 0, 2, 15 },
    { 1, 3, 25 }, { 1, 4, 60 },
    { 2, 3, 40 }, { 2, 5, 20 },
    { 3, 6, 35 },
    { 4, 6, 20 },
    { 5, 6, 30 }
  };
  /* テストケース1の回答経路 */
  static const uint32_t test_case1_answer_route[] = { 0, 3, 6 };

  /* テストケース2の重み */
  static const DijkstraTestCaseAdjacencyMatrixWeight test_case2_weight[] = {
    {  0,  1,  15 }, {  0,  2,  58 }, {  0,  3,  79 }, {  0,  4,   1 }, {  0,  5,  44 },
    {  0,  6,  78 }, {  0,  7,  61 }, {  0,  8,  90 }, {  0,  9,  95 },
    {  1,  2,  53 }, {  1,  3,  78 }, {  1,  4,  49 }, {  1,  5,  72 }, {  1,  6,  50 },
    {  1,  7,  43 }, {  1,  8,  25 }, {  1,  9, 100 },
    {  2,  3,  51 }, {  2,  4,  70 }, {  2,  5,  59 }, {  2,  6,  31 }, {  2,  7,  71 },
    {  2,  8,  21 }, {  2,  9,  55 },
    {  3,  4,  46 }, {  3,  5,   7 }, {  3,  6,  81 }, {  3,  7,  92 }, {  3,  8,  71 },
    {  3,  9,  48 },
    {  4,  5,   7 }, {  4,  6,  18 }, {  4,  7,  11 }, {  4,  8,  36 },
    {  4,  9,  38 },
    {  5,  6,  54 }, {  5,  7,  85 }, {  5,  8,  84 }, {  5,  9,  36 }, {  5, 10,   1 },
    {  6,  7,  57 }, {  6,  8,  85 }, {  6,  9,  45 },
    {  7,  8,  28 }, {  7,  9,  93 },
    {  8,  9,  11 },
    {  9, 10,  92 },
    { 10, 11,  29 }, { 10, 12,  45 }, { 10, 13,  53 }, { 10, 14,   8 },
    { 10, 15,  16 }, { 10, 16,  41 }, { 10, 17,  51 }, { 10, 18,  95 },
    { 10, 19,  94 },
    { 11, 12,  64 }, { 11, 13,  31 }, { 11, 14,   6 }, { 11, 15,  91 },
    { 11, 16,  72 }, { 11, 17,  90 }, { 11, 18,  56 }, { 11, 19,  41 },
    { 12, 13, 100 }, { 12, 14,  68 }, { 12, 15,  48 }, { 12, 16,  73 },
    { 12, 17,  25 }, { 12, 18,  31 }, { 12, 19,  79 },
    { 13, 14,   1 }, { 13, 15,  38 }, { 13, 16,  17 }, { 13, 17,  81 },
    { 13, 18,  21 }, { 13, 19,  58 },
    { 14, 15,  47 }, { 14, 16,  35 }, { 14, 17,  36 }, { 14, 18,   3 },
    { 14, 19,  64 },
    { 15, 16,  19 }, { 15, 17,  22 }, { 15, 18,  51 }, { 15, 19,  58 },
    { 15, 20,  99 },
    { 16, 17,  11 }, { 16, 18,  68 }, { 16, 19,  86 },
    { 17, 18,  63 }, { 17, 19,  97 },
    { 18, 19,  64 },
    { 19, 20,  86 },
    { 20, 21,  40 }, { 20, 22,  28 }, { 20, 23,  59 }, { 20, 24, 14 },
    { 20, 25,  77 }, { 20, 26,  90 }, { 20, 27,  91 }, { 20, 28, 74 },
    { 21, 22,  21 }, { 21, 23,  78 }, { 21, 24,  26 }, { 21, 25, 76 },
    { 21, 26,  38 }, { 21, 27,  32 }, { 21, 28,  36 },
    { 22, 23,  12 }, { 22, 24,  18 }, { 22, 25,  68 }, { 22, 26, 40 },
    { 22, 27,  86 }, { 22, 28,  19 },
    { 23, 24,  32 }, { 23, 25,  77 }, { 23, 26,  63 }, { 23, 27, 57 },
    { 23, 28,  78 },
    { 24, 25,  33 }, { 24, 26,  81 }, { 24, 27,  58 }, { 24, 28, 3 },
    { 25, 26,  89 }, { 25, 27,  28 }, { 25, 28,  83 }, { 25, 29, 42 },
    { 26, 27,  83 }, { 26, 28,  87 },
    { 27, 28,  75 },
    { 28, 29,  85 }
  };
  /* テストケース2の回答経路 */
  static const uint32_t test_case2_answer_route[] = { 0, 4, 5, 10, 15, 20, 24, 25, 29 };

  /* ダイクストラ法のテストケース */
  static const DijkstraTestCase test_cases[] = {
    /* テストケース0（コーナーケース）: 
     * ノード数2, 最小コスト: 114514, 経路 0 -> 1 */
    {
      2, 
      0,
      1,
      114514,
      sizeof(test_case0_weight) / sizeof(test_case0_weight[0]),
      test_case0_weight,
      sizeof(test_case0_answer_route) / sizeof(test_case0_answer_route[0]),
      test_case0_answer_route
    },
    /* テストケース1: 
     * ノード数7, 最小コスト: 45, 経路 0 -> 3 -> 6 */
    {
      7, 
      0,
      6,
      45,
      sizeof(test_case1_weight) / sizeof(test_case1_weight[0]),
      test_case1_weight,
      sizeof(test_case1_answer_route) / sizeof(test_case1_answer_route[0]),
      test_case1_answer_route
    },
    /* テストケース2: 
     * ノード数30, 最小コスト: 213, 経路 0 -> 4 -> 5 -> 10 -> 15 -> 20 -> 24 -> 25 -> 29 */
    {
      30, 
      0,
      29,
      213,
      sizeof(test_case2_weight) / sizeof(test_case2_weight[0]),
      test_case2_weight,
      sizeof(test_case2_answer_route) / sizeof(test_case2_answer_route[0]),
      test_case2_answer_route
    }
  };
  /* ダイクストラ法のテストケース数 */
  const uint32_t num_test_case = sizeof(test_cases) / sizeof(test_cases[0]);

  TEST_UNUSED_PARAMETER(obj);

  /* ダイクストラ法の実行テスト */
  {
    struct SLAOptimalBlockPartitionEstimator* oee;
    uint32_t test_no, i, j, node, is_ok;
    double   cost;

    /* 全テストケースに対してテスト */
    for (test_no = 0; test_no < num_test_case; test_no++) {
      const DijkstraTestCase* p_test = &test_cases[test_no];

      /* ノード数num_nodesでハンドルを作成 */
      oee = SLAOptimalEncodeEstimator_Create(p_test->num_nodes, 1);
      Test_AssertCondition(oee != NULL);

      /* 隣接行列をセット */
      for (i = 0; i < oee->max_num_nodes; i++) {
        for (j = 0; j < oee->max_num_nodes; j++) {
          oee->adjacency_matrix[i][j] = SLAOPTIMALENCODEESTIMATOR_DIJKSTRA_BIGWEIGHT;
        }
      }
      for (i = 0; i < p_test->num_weights; i++) {
        const DijkstraTestCaseAdjacencyMatrixWeight *p = &p_test->weight[i];
        oee->adjacency_matrix[p->i][p->j] = p->weight;
      }

      /* ダイクストラ法実行 */
      Test_AssertEqual(
          SLAOptimalEncodeEstimator_ApplyDijkstraMethod(oee,
            p_test->num_nodes, p_test->start_node, p_test->goal_node, &cost),
          SLAPREDICTOR_APIRESULT_OK);

      /* コストのチェック */
      Test_AssertCondition(cost == p_test->min_cost);

      /* 経路のチェック */
      is_ok = 1;
      node = p_test->goal_node;
      for (i = 0; i < p_test->len_answer_route_path; i++) {
        if (node != p_test->answer_route_path[p_test->len_answer_route_path - i - 1]) {
          is_ok = 0;
          break;
        }
        node = oee->path[node];
      }
      Test_AssertEqual(is_ok, 1);
      SLAOptimalEncodeEstimator_Destroy(oee);
    }
  }
}

/* リファレンスとなる低速な自己相関計算関数 */
static void LPC_CalculateAutoCorrelationReference(
    const double* data, uint32_t num_sample,
    double* auto_corr, uint32_t order)
{
  uint32_t smpl, lag;

  /* （標本）自己相関の計算 */
  for (lag = 0; lag < order; lag++) {
    auto_corr[lag] = 0.0f;
    /* 係数が0以上の時のみ和を取る */
    for (smpl = lag; smpl < num_sample; smpl++) {
      auto_corr[lag] += data[smpl] * data[smpl - lag];
    }
  }
}

/* 自己相関係数計算テスト */
void testLPC_CalculateAutoCorrelationTest(void* obj)
{
  /* 判定精度 */
#define FLOAT_ERROR_EPISILON 1.0e-8

  TEST_UNUSED_PARAMETER(obj);

  {
    const uint32_t NUM_SAMPLES  = 256;
    const uint32_t MAX_ORDER    = 256;
    /* テスト対象の波形を生成する関数配列 */
    static const GenerateWaveFunction test_waves[] = {
      testSLAPredictor_GenerateSilence,
      testSLAPredictor_GenerateConstant,
      testSLAPredictor_GenerateNegativeConstant,
      testSLAPredictor_GenerateSineWave,
      testSLAPredictor_GenerateWhiteNoize,
      testSLAPredictor_GenerateNyquistOsc,
    };
    const uint32_t num_test_waves = sizeof(test_waves) / sizeof(test_waves[0]);
    uint32_t test_no, lag, is_ok;
    double* data;
    double* corr_ref;
    double* corr;

    /* 領域割り当て */
    data = (double *)malloc(sizeof(double) * NUM_SAMPLES);
    corr_ref = (double *)malloc(sizeof(double) * MAX_ORDER);
    corr = (double *)malloc(sizeof(double) * MAX_ORDER);

    /* 全波形に対してテスト */
    for (test_no = 0; test_no < num_test_waves; test_no++) {
      /* データ生成 */
      test_waves[test_no](data, NUM_SAMPLES);

      /* リファレンス関数で自己相関計算 */
      LPC_CalculateAutoCorrelationReference(
          data, NUM_SAMPLES, corr_ref, MAX_ORDER);

      /* 実際に使用する関数で自己相関計算 */
      Test_AssertEqual(
          LPC_CalculateAutoCorrelation(
            data, NUM_SAMPLES, corr, MAX_ORDER),
          SLAPREDICTOR_ERROR_OK);

      /* 一致確認 */
      is_ok = 1;
      for (lag = 0; lag < MAX_ORDER; lag++) {
        if (fabs(corr[lag] - corr_ref[lag]) > FLOAT_ERROR_EPISILON) {
          printf("[%d] ref:%e vs get:%e \n", lag, corr_ref[lag], corr[lag]);
          is_ok = 0;
          break;
        }
      }
      Test_AssertEqual(is_ok, 1);

    }

    /* 領域開放 */
    free(data);
    free(corr_ref);
    free(corr);
  }

#undef FLOAT_ERROR_EPISILON
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
  Test_AddTest(suite, testSLAOptimalEncodeEstimator_DijkstraTest);
  Test_AddTest(suite, testLPC_CalculateAutoCorrelationTest);
}
