#include "SLAPredictor.h"
#include "SLAUtility.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <assert.h>

/* ロングタームの最大タップ数 */
#define LPC_LONGTERM_MAX_NUM_TAP (1)

/* 最大自己相関値からどの比率のピークをピッチとして採用するか */
/* TODO:ピッチが取りたいのではなく純粋に相関除去したいから1.0fでいいかも */
#define LPC_LONGTERM_PITCH_RATIO_VS_MAX_THRESHOULD  (1.0f)

/* NULLチェックと領域解放 */
#define NULLCHECK_AND_FREE(ptr) { \
  if ((ptr) != NULL) {            \
    free(ptr);                    \
    (ptr) = NULL;                 \
  }                               \
}

/* 内部エラー型 */
typedef enum SLAPredictorErrorTag {
  SLAPREDICTOR_ERROR_OK,
  SLAPREDICTOR_ERROR_NG,
  SLAPREDICTOR_ERROR_INVALID_ARGUMENT
} SLAPredictorError;

/* 内部データ型 */
typedef union SLALPCDataUnitTag {
  float     f32;
  int32_t   s32;
  int64_t   s64;
} SLALPCDataUnit;

/* LPC計算ハンドル */
struct SLALPCCalculator {
  uint32_t  max_order;     /* 最大次数           */
  /* 内部的な計算結果は精度を担保するため全てdoubleで持つ */
  /* floatだとサンプル数を増やすと標本自己相関値の誤差に起因して出力の計算結果がnanになる */
  double*   a_vec;         /* 計算用ベクトル1    */
  double*   e_vec;         /* 計算用ベクトル2    */
  double*   u_vec;         /* 計算用ベクトル3    */
  double*   v_vec;         /* 計算用ベクトル4    */
  double*   auto_corr;     /* 標本自己相関       */
  double*   lpc_coef;      /* LPC係数ベクトル    */
  double*   parcor_coef;   /* PARCOR係数ベクトル */
};

/* 音声合成ハンドル（格子型フィルタ） */
struct SLALPCSynthesizer {
  uint32_t        max_order;                 /* 最大次数               */
  SLALPCDataUnit* forward_residual_work;     /* 前向き誤差の計算ワーク領域   */
  SLALPCDataUnit* backward_residual_work;    /* 後ろ向き誤差の計算ワーク領域 */
};

/* ロングターム計算ハンドル */
struct SLALongTermCalculator {
  uint32_t  fft_size;                 /* FFTサイズ          */
  double*   auto_corr;                /* 自己相関           */
  uint32_t  max_num_pitch_candidates; /* 最大のピッチ候補数 */
  uint32_t  max_pitch_period;         /* 最大ピッチ         */
  uint32_t* pitch_candidate;          /* ピッチ候補配列     */
};

/* NLMS計算ハンドル */
struct SLANLMSCalculator {
	int64_t   alpha;			    /* ステップサイズ（現在未使用） */
	int64_t* 	coef;			      /* 係数 */
	uint32_t	max_num_coef;	  /* 最大の係数個数 */
};

/*（標本）自己相関の計算 */
static SLAPredictorError LPC_CalculateAutoCorrelation(
    const float* data, uint32_t num_sample,
    double* auto_corr, uint32_t order);

/* Levinson-Durbin再帰計算 */
static SLAPredictorError LPC_LevinsonDurbinRecursion(
    struct SLALPCCalculator* lpc, const double* auto_corr,
    double* lpc_coef, double* parcor_coef, uint32_t order);

/* 係数計算の共通関数 */
static SLAPredictorError LPC_CalculateCoef(
    struct SLALPCCalculator* lpc, 
    const float* data, uint32_t num_samples, uint32_t order);

/* LPC係数計算ハンドルの作成 */
struct SLALPCCalculator* SLALPCCalculator_Create(uint32_t max_order)
{
  struct SLALPCCalculator* lpc;

  lpc = (struct SLALPCCalculator *)malloc(sizeof(struct SLALPCCalculator));

  lpc->max_order = max_order;

  /* 計算用ベクトルの領域割当 */
  lpc->a_vec = (double *)malloc(sizeof(double) * (max_order + 2)); /* a_0, a_k+1を含めるとmax_order+2 */
  lpc->e_vec = (double *)malloc(sizeof(double) * (max_order + 2)); /* e_0, e_k+1を含めるとmax_order+2 */
  lpc->u_vec = (double *)malloc(sizeof(double) * (max_order + 2));
  lpc->v_vec = (double *)malloc(sizeof(double) * (max_order + 2));

  /* 標本自己相関の領域割当 */
  lpc->auto_corr = (double *)malloc(sizeof(double) * (max_order + 1));

  /* 係数ベクトルの領域割当 */
  lpc->lpc_coef     = (double *)malloc(sizeof(double) * (max_order + 1));
  lpc->parcor_coef  = (double *)malloc(sizeof(double) * (max_order + 1));

  return lpc;
}

/* LPC係数計算ハンドルの破棄 */
void SLALPCCalculator_Destroy(struct SLALPCCalculator* lpcc)
{
  if (lpcc != NULL) {
    NULLCHECK_AND_FREE(lpcc->a_vec);
    NULLCHECK_AND_FREE(lpcc->e_vec);
    NULLCHECK_AND_FREE(lpcc->u_vec);
    NULLCHECK_AND_FREE(lpcc->v_vec);
    NULLCHECK_AND_FREE(lpcc->auto_corr);
    NULLCHECK_AND_FREE(lpcc->lpc_coef);
    NULLCHECK_AND_FREE(lpcc->parcor_coef);
    free(lpcc);
  }
}

/* Levinson-Durbin再帰計算によりPARCOR係数を求める（倍精度） */
/* 係数parcor_coefはorder+1個の配列 */
SLAPredictorApiResult SLALPCCalculator_CalculatePARCORCoefDouble(
    struct SLALPCCalculator* lpc,
    const float* data, uint32_t num_samples,
    double* parcor_coef, uint32_t order)
{
  /* 引数チェック */
  if (lpc == NULL || data == NULL || parcor_coef == NULL) {
    return SLAPREDICTOR_APIRESULT_INVALID_ARGUMENT;
  }

  /* 次数チェック */
  if (order > lpc->max_order) {
    return SLAPREDICTOR_APIRESULT_EXCEED_MAX_ORDER;
  }

  /* 係数計算 */
  if (LPC_CalculateCoef(lpc, data, num_samples, order) != SLAPREDICTOR_ERROR_OK) {
    return SLAPREDICTOR_APIRESULT_FAILED_TO_CALCULATION;
  }

  /* 計算成功時は結果をコピー */
  memcpy(parcor_coef, lpc->parcor_coef, sizeof(double) * (order + 1));

  return SLAPREDICTOR_APIRESULT_OK;
}

/* 係数計算の共通関数 */
static SLAPredictorError LPC_CalculateCoef(
    struct SLALPCCalculator* lpc, 
    const float* data, uint32_t num_samples, uint32_t order)
{
  /* 引数チェック */
  if (lpc == NULL) {
    return SLAPREDICTOR_ERROR_INVALID_ARGUMENT;
  }

  /* 自己相関を計算 */
  if (LPC_CalculateAutoCorrelation(
        data, num_samples, lpc->auto_corr, order + 1) != SLAPREDICTOR_ERROR_OK) {
    return SLAPREDICTOR_ERROR_NG;
  }

  /* 再帰計算を実行 */
  if (LPC_LevinsonDurbinRecursion(
        lpc, lpc->auto_corr,
        lpc->lpc_coef, lpc->parcor_coef, order) != SLAPREDICTOR_ERROR_OK) {
    return SLAPREDICTOR_ERROR_NG;
  }

  return SLAPREDICTOR_ERROR_OK;
}

/* Levinson-Durbin再帰計算 */
static SLAPredictorError LPC_LevinsonDurbinRecursion(
    struct SLALPCCalculator* lpc, const double* auto_corr,
    double* lpc_coef, double* parcor_coef, uint32_t order)
{
  uint32_t delay, i;
  double gamma;      /* 反射係数 */
  /* オート変数にポインタをコピー */
  double* a_vec = lpc->a_vec;
  double* e_vec = lpc->e_vec;
  double* u_vec = lpc->u_vec;
  double* v_vec = lpc->v_vec;

  /* 引数チェック */
  if (lpc == NULL || lpc_coef == NULL || auto_corr == NULL) {
    return SLAPREDICTOR_ERROR_INVALID_ARGUMENT;
  }

  /* 
   * 0次自己相関（信号の二乗和）が小さい場合
   * => 係数は全て0として無音出力システムを予測.
   */
  if (fabs(auto_corr[0]) < FLT_EPSILON) {
    for (i = 0; i < order + 1; i++) {
      lpc_coef[i] = parcor_coef[i] = 0.0f;
    }
    return SLAPREDICTOR_ERROR_OK;
  }

  /* 初期化 */
  for (i = 0; i < order + 2; i++) {
    a_vec[i] = u_vec[i] = v_vec[i] = 0.0f;
  }

  /* 最初のステップの係数をセット */
  a_vec[0]        = 1.0f;
  e_vec[0]        = auto_corr[0];
  a_vec[1]        = - auto_corr[1] / auto_corr[0];
  parcor_coef[0]  = 0.0f;
  parcor_coef[1]  = auto_corr[1] / e_vec[0];
  e_vec[1]        = auto_corr[0] + auto_corr[1] * a_vec[1];
  u_vec[0]        = 1.0f; u_vec[1] = 0.0f; 
  v_vec[0]        = 0.0f; v_vec[1] = 1.0f; 

  /* 再帰処理 */
  for (delay = 1; delay < order; delay++) {
    gamma = 0.0f;
    for (i = 0; i < delay + 1; i++) {
      gamma += a_vec[i] * auto_corr[delay + 1 - i];
    }
    gamma /= (-e_vec[delay]);
    e_vec[delay + 1] = (1.0f - gamma * gamma) * e_vec[delay];
    /* 誤差分散（パワー）は非負 */
    assert(e_vec[delay] >= 0.0f);

    /* u_vec, v_vecの更新 */
    for (i = 0; i < delay; i++) {
      u_vec[i + 1] = v_vec[delay - i] = a_vec[i + 1];
    }
    u_vec[0] = 1.0f; u_vec[delay+1] = 0.0f;
    v_vec[0] = 0.0f; v_vec[delay+1] = 1.0f;

    /* 係数の更新 */
    for (i = 0; i < delay + 2; i++) {
       a_vec[i] = u_vec[i] + gamma * v_vec[i];
    }
    /* PARCOR係数は反射係数の符号反転 */
    parcor_coef[delay + 1] = -gamma;
    /* PARCOR係数の絶対値は1未満（収束条件） */
    assert(fabs(gamma) < 1.0f);
  }

  /* 結果を取得 */
  memcpy(lpc_coef, a_vec, sizeof(double) * (order + 1));

  return SLAPREDICTOR_ERROR_OK;
}

/*（標本）自己相関の計算 */
static SLAPredictorError LPC_CalculateAutoCorrelation(
    const float* data, uint32_t num_sample,
    double* auto_corr, uint32_t order)
{
  uint32_t i_sample, delay_time;

  /* 引数チェック */
  if (data == NULL || auto_corr == NULL) {
    return SLAPREDICTOR_ERROR_INVALID_ARGUMENT;
  }

  /* （標本）自己相関の計算 */
  for (delay_time = 0; delay_time < order; delay_time++) {
    auto_corr[delay_time] = 0.0f;
    /* 係数が0以上の時のみ和を取る */
    for (i_sample = delay_time; i_sample < num_sample; i_sample++) {
      auto_corr[delay_time] += (double)data[i_sample] * data[i_sample - delay_time];
    }
    /* 平均を取ってはいけない */
  }

  return SLAPREDICTOR_ERROR_OK;
}

/* PARCOR係数から分散比を求める */
static SLAPredictorApiResult SLALPCCalculator_CalculateVarianceRatio(
    const double* parcor_coef, uint32_t order,
    double* variance_ratio)
{
  uint32_t ord;
  double tmp_var_ratio;

  /* 引数チェック */
  if (parcor_coef == NULL || variance_ratio == NULL) {
    return SLAPREDICTOR_APIRESULT_INVALID_ARGUMENT;
  }

  /* PARCOR係数を用いて分散比を計算 */
  /* 補足）0次のPARCOR係数は0で確定だから抜かす */
  tmp_var_ratio = 1.0f;
  for (ord = 1; ord <= order; ord++) {
    tmp_var_ratio *= (1.0f - parcor_coef[ord] * parcor_coef[ord]);
  }

  /* 成功終了 */
  *variance_ratio = tmp_var_ratio;
  return SLAPREDICTOR_APIRESULT_OK;
}

/* 入力データとPARCOR係数から残差パワーを求める */
SLAPredictorApiResult SLALPCCalculator_CalculateResidualPower(
    const float* data, uint32_t num_samples,
    const double* parcor_coef, uint32_t order,
    double* residual_power)
{
  uint32_t smpl;
  double tmp_res_power, var_ratio;
  SLAPredictorApiResult ret;

  /* 引数チェック */
  if (data == NULL || parcor_coef == NULL || residual_power == NULL) {
    return SLAPREDICTOR_APIRESULT_INVALID_ARGUMENT;
  }

  /* 入力データの平均パワー（二乗平均）を求める */
  tmp_res_power = 0.0f;
  for (smpl = 0; smpl < num_samples; smpl++) {
    tmp_res_power += (double)data[smpl] * data[smpl];
  }
  tmp_res_power /= num_samples;

  /* 分散比を求める */
  if ((ret = SLALPCCalculator_CalculateVarianceRatio(parcor_coef, order, &var_ratio))
      != SLAPREDICTOR_APIRESULT_OK) {
    return ret;
  }

  /* 分散比を掛けることで予測後の残差パワーが得られる */
  tmp_res_power *= var_ratio;
  *residual_power = tmp_res_power;
  return SLAPREDICTOR_APIRESULT_OK;
}

/* LPC音声合成ハンドルの作成 */
struct SLALPCSynthesizer* SLALPCSynthesizer_Create(uint32_t max_order)
{
  struct SLALPCSynthesizer* lpcs;

  lpcs = (struct SLALPCSynthesizer *)malloc(sizeof(struct SLALPCSynthesizer));

  lpcs->max_order = max_order;

  /* 前向き/後ろ向き誤差の領域確保 */
  lpcs->forward_residual_work   = malloc(sizeof(SLALPCDataUnit) * (max_order + 1));
  lpcs->backward_residual_work  = malloc(sizeof(SLALPCDataUnit) * (max_order + 1));

  return lpcs;
}

/* LPC音声合成ハンドルの破棄 */
void SLALPCSynthesizer_Destroy(struct SLALPCSynthesizer* lpc)
{
  if (lpc != NULL) {
    NULLCHECK_AND_FREE(lpc->forward_residual_work);
    NULLCHECK_AND_FREE(lpc->backward_residual_work);
    free(lpc);
  }
}

/* PARCOR係数により予測/誤差出力（32bit整数入出力） */
/* 係数parcor_coefはorder+1個の配列 */
SLAPredictorApiResult SLALPCSynthesizer_PredictByParcorCoefInt32(
    struct SLALPCSynthesizer* lpc,
    const int32_t* data, uint32_t num_samples,
    const int32_t* parcor_coef, uint32_t order, int32_t* residual)
{
  uint32_t samp, ord;
  int64_t* forward_residual;
  int64_t* backward_residual;
  int64_t  mul_temp;          /* 乗算がオーバーフローする可能性があるため */
  const int32_t half = (1UL << 30);

  /* 引数チェック */
  if (lpc == NULL || data == NULL
      || parcor_coef == NULL || residual == NULL) {
    return SLAPREDICTOR_APIRESULT_INVALID_ARGUMENT;
  }

  /* 次数チェック */
  if (order > lpc->max_order) {
    return SLAPREDICTOR_APIRESULT_EXCEED_MAX_ORDER;
  }

  /* オート変数にポインタをコピー */
  forward_residual = (int64_t *)lpc->forward_residual_work;
  backward_residual = (int64_t *)lpc->backward_residual_work;

  /* 誤差をゼロ初期化 */
  for (ord = 0; ord < lpc->max_order + 1; ord++) {
    forward_residual[ord] = backward_residual[ord] = 0;
  }

  /* 誤差計算 */
  for (samp = 0; samp < num_samples; samp++) {
    /* 格子型フィルタにデータ入力 */
    forward_residual[0] = (int64_t)data[samp];
    /* 前向き誤差計算 */
    for (ord = 1; ord <= order; ord++) {
      mul_temp = SLAUTILITY_SHIFT_RIGHT_ARITHMETIC(parcor_coef[ord] * backward_residual[ord - 1] + half, 31);
      forward_residual[ord] = forward_residual[ord - 1] - mul_temp;
    }
    /* 後ろ向き誤差計算 */
    for (ord = order; ord >= 1; ord--) {
      mul_temp = SLAUTILITY_SHIFT_RIGHT_ARITHMETIC(parcor_coef[ord] * forward_residual[ord - 1] + half, 31);
      backward_residual[ord] = backward_residual[ord - 1] - mul_temp;
    }
    /* 後ろ向き誤差計算部にデータ入力 */
    backward_residual[0] = (int64_t)data[samp];
    /* 残差信号 */
    assert((forward_residual[order] <= INT32_MAX) && (forward_residual[order] >= INT32_MIN));
    residual[samp] = (int32_t)(forward_residual[order]);
    /* printf("res: %08x(%8d) \n", residual[samp], residual[samp]); */
  }

  return SLAPREDICTOR_APIRESULT_OK;
}

/* PARCOR係数により誤差信号から音声合成（32bit整数入出力） */
/* 係数parcor_coefはorder+1個の配列 */
SLAPredictorApiResult SLALPCSynthesizer_SynthesizeByParcorCoefInt32(
    struct SLALPCSynthesizer* lpc,
    const int32_t* residual, uint32_t num_samples,
    const int32_t* parcor_coef, uint32_t order, int32_t* output)
{
  uint32_t ord, samp;
  int64_t* forward_residual;
  int64_t* backward_residual;
  int64_t  mul_temp;                /* 乗算がオーバーフローする可能性があるため */
  const int32_t half = (1UL << 30); /* 丸め誤差軽減のための加算定数 = 0.5 */

  /* 引数チェック */
  if (lpc == NULL || residual == NULL
      || parcor_coef == NULL || output == NULL) {
    return SLAPREDICTOR_APIRESULT_INVALID_ARGUMENT;
  }

  /* オート変数にポインタをコピー */
  forward_residual = (int64_t *)lpc->forward_residual_work;
  backward_residual = (int64_t *)lpc->backward_residual_work;

  /* 次数チェック */
  if (order > lpc->max_order) {
    return SLAPREDICTOR_APIRESULT_EXCEED_MAX_ORDER;
  }

  /* 誤差をゼロ初期化 */
  for (ord = 0; ord < lpc->max_order + 1; ord++) {
    forward_residual[ord] = backward_residual[ord] = 0;
  }

  /* TODO: 1乗算型に変える */
  /* 格子型フィルタによる音声合成 */
  for (samp = 0; samp < num_samples; samp++) {
    /* 誤差入力 */
    forward_residual[order] = (int64_t)residual[samp];
    for (ord = order; ord >= 1; ord--) {
      /* 前向き誤差計算 */
      mul_temp = SLAUTILITY_SHIFT_RIGHT_ARITHMETIC(parcor_coef[ord] * backward_residual[ord - 1] + half, 31);
      forward_residual[ord - 1] = forward_residual[ord] + mul_temp;
      /* 後ろ向き誤差計算 */
      mul_temp = SLAUTILITY_SHIFT_RIGHT_ARITHMETIC(parcor_coef[ord] * forward_residual[ord - 1] + half, 31);
      backward_residual[ord] = backward_residual[ord - 1] - mul_temp;
    }
    /* 合成信号 */
    assert((forward_residual[0] <= INT32_MAX) && (forward_residual[0] >= INT32_MIN));
    output[samp] = (int32_t)(forward_residual[0]);
    /* 後ろ向き誤差計算部にデータ入力 */
    backward_residual[0] = forward_residual[0];
    /* printf("out: %08x(%8d) \n", output[samp], output[samp]); */
  }

  return SLAPREDICTOR_APIRESULT_OK;
}

/* ロングターム計算ハンドルの作成 */
struct SLALongTermCalculator* SLALongTermCalculator_Create(
    uint32_t fft_size, uint32_t max_pitch_period, uint32_t max_num_pitch_candidates)
{
  struct SLALongTermCalculator* ltm;

  /* 2のべき乗数になっているかチェック */
  if (fft_size & (fft_size - 1)) {
    return NULL;
  }

  ltm = (struct SLALongTermCalculator *)malloc(sizeof(struct SLALongTermCalculator));

  ltm->fft_size                 = fft_size;
  ltm->auto_corr                = (double *)malloc(sizeof(double) * fft_size);
  ltm->max_num_pitch_candidates = max_num_pitch_candidates;
  ltm->max_pitch_period         = max_pitch_period;
  ltm->pitch_candidate          = (uint32_t *)malloc(sizeof(uint32_t) * max_num_pitch_candidates);

  return ltm;
}

/* ロングターム計算ハンドルの破棄 */
void SLALongTermCalculator_Destroy(struct SLALongTermCalculator* ltm_calculator)
{
  if (ltm_calculator != NULL) {
    NULLCHECK_AND_FREE(ltm_calculator->auto_corr);
    NULLCHECK_AND_FREE(ltm_calculator->pitch_candidate);
    free(ltm_calculator);
  }
}

/* ロングターム係数の計算（内部的にピッチ解析が走る） */
SLAPredictorApiResult SLALongTermCalculator_CalculateCoef(
	struct SLALongTermCalculator* ltm_calculator,
	const int32_t* data, uint32_t num_samples,
	uint32_t* pitch_period, double* ltm_coef, uint32_t num_taps)
{
  uint32_t  i, fft_size, num_peak;
  double*   auto_corr;
  double    max_peak;

  /* 引数チェック */
  if ((ltm_calculator == NULL) || (data == NULL)
      || (pitch_period == NULL) || (ltm_coef == NULL)) {
    return SLAPREDICTOR_APIRESULT_INVALID_ARGUMENT;
  }

  /* タップ数は奇数であることを要求 */
  if (!(num_taps & 1)) {
    return SLAPREDICTOR_APIRESULT_INVALID_ARGUMENT;
  }

  /* 最大のタップ数を越えている */
  if (num_taps > LPC_LONGTERM_MAX_NUM_TAP) {
    return SLAPREDICTOR_APIRESULT_EXCEED_MAX_ORDER;
  }

  /* FFTのサイズを越えている */
  /* 巡回の影響を小さくしたいため、サンプル数はfft_sizeの半分以下を要求 */
  if (2 * num_samples > ltm_calculator->fft_size) {
    return SLAPREDICTOR_APIRESULT_INVALID_ARGUMENT;
  }

  /* ローカル変数に受ける */
  fft_size  = ltm_calculator->fft_size;
  auto_corr = ltm_calculator->auto_corr;

  /* 自己相関をFFTで計算（ウィーナ-ヒンチンの定理） */
  /* データをセット */
  for (i = 0; i < fft_size; i++) {
    if (i < num_samples) {
      auto_corr[i] = data[i] * pow(2, -31);
    } else {
      /* 後ろは0埋め */
      auto_corr[i] = 0.0f;
    }
  }
  /* FFT */
  SLAUtility_FFT(auto_corr, fft_size, 1);
  /* 直流のパワー */
  auto_corr[0] *= auto_corr[0]; 
  /* ナイキスト周波数のパワー */
  auto_corr[1] *= auto_corr[1];
  /* パワー（絶対値の2乗）を計算 */
  for (i = 1; i < fft_size / 2; i++) {
    double re, im;
    re = auto_corr[2 * i];
    im = auto_corr[2 * i + 1];
    auto_corr[2 * i]      = re * re + im * im;
    /* 虚数部は全て0 */
    auto_corr[2 * i + 1]  = 0.0f;
  }
  /* パワースペクトルを信号と見做してIFFT -> 自己相関が得られる */
  SLAUtility_FFT(auto_corr, fft_size, -1);

  /* 無音フレーム */
  if (fabs(auto_corr[0]) <= FLT_MIN) {
    *pitch_period = 0;
    for (i = 0; i < num_taps; i++) {
      ltm_coef[i] = 0.0f;
    }
    return SLAPREDICTOR_APIRESULT_OK;
  }

  /* 無音フレームでなければピッチ検出 */

  /* 簡易ピッチ検出 */
  max_peak = 0.0f;
  num_peak = 0;
  i = 1;
  while ((i < ltm_calculator->max_pitch_period)
      && (num_peak < ltm_calculator->max_num_pitch_candidates)) {
    uint32_t  start, end, j, local_peak_index;
    double    local_peak;

    /* 負 -> 正 のゼロクロス点を検索 */
    for (start = i; start < ltm_calculator->max_pitch_period; start++) {
      if ((auto_corr[start - 1] < 0.0f) && (auto_corr[start] > 0.0f)) {
        break;
      }
    }

    /* 正 -> 負 のゼロクロス点を検索 */
    for (end = start + 1; end < ltm_calculator->max_pitch_period; end++) {
      if ((auto_corr[end] > 0.0f) && (auto_corr[end + 1] < 0.0f)) {
        break;
      }
    }

    /* ローカルピークの探索 */
    /* start, end 間で最大のピークを検索 */
    local_peak_index = 0; local_peak = 0.0f;
    for (j = start; j <= end; j++) {
      if ((auto_corr[j] > auto_corr[j - 1]) && (auto_corr[j] > auto_corr[j + 1])) {
        if (auto_corr[j] > local_peak) {
          local_peak_index  = j;
          local_peak        = auto_corr[j];
        }
      }
    }
    /* ローカルピーク（ピッチ候補）があった */
    if (local_peak_index != 0) {
      ltm_calculator->pitch_candidate[num_peak] = local_peak_index;
      num_peak++;
      /* 最大ピーク値の更新 */
      if (local_peak > max_peak) {
        max_peak = local_peak;
      }
    }

    i = end + 1;
  }

  /* ピッチ候補を1つも発見できず */
  if (num_peak == 0) {
    return SLAPREDICTOR_APIRESULT_FAILED_TO_CALCULATION;
  }

  /* ピッチ候補を先頭から見て、最大ピーク値の一定割合以上の値を持つ最初のピークがピッチ */
  for (i = 0; i < num_peak; i++) {
    if (auto_corr[ltm_calculator->pitch_candidate[i]] >= LPC_LONGTERM_PITCH_RATIO_VS_MAX_THRESHOULD * max_peak) {
      break;
    }
  }

  /* 制限: 今はタップ1つのみに制限 */
  assert(num_taps == 1);
  /* ピッチに該当するインデックス */
  *pitch_period = ltm_calculator->pitch_candidate[i];
  /* ロングターム係数 */
  ltm_coef[0]   = auto_corr[ltm_calculator->pitch_candidate[i]] / auto_corr[0];

  return SLAPREDICTOR_APIRESULT_OK;
}

/* ロングタームを使用して残差信号の計算 */
SLAPredictorApiResult SLALongTerm_PredictInt32(
	const int32_t* data, uint32_t num_samples,
	uint32_t pitch_period, 
	const int32_t* ltm_coef, uint32_t num_taps, int32_t* residual)
{
  uint32_t      i, j;
  const int32_t half    = (1UL << 30); /* 丸め用定数(0.5) */
  int32_t       predict;

  /* 引数チェック */
  if ((data == NULL) || (ltm_coef == NULL) || (residual == NULL)) {
    return SLAPREDICTOR_APIRESULT_INVALID_ARGUMENT;
  }

  /* ピッチ周期0は予測せず、そのまま誤差とする */
  if (pitch_period == 0) {
    memcpy(residual, data, sizeof(int32_t) * num_samples);
    return SLAPREDICTOR_APIRESULT_OK;
  }

  /* 予測が始まるまでのサンプルは単純コピー */
  for (i = 0; i < pitch_period + num_taps / 2; i++) {
      residual[i] = data[i];
  }

  /* ロングターム予測 */
  for (; i < num_samples; i++) {
    predict = 0;
    for (j = 0; j < num_taps; j++) {
      predict += (int32_t)SLAUTILITY_SHIFT_RIGHT_ARITHMETIC(
          (int64_t)ltm_coef[j] * data[i + j - pitch_period - num_taps / 2] + half, 31);
    }
    residual[i] = data[i] - predict;
  }

  return SLAPREDICTOR_APIRESULT_OK;
}
	
/* ロングターム誤差信号から音声合成 */
SLAPredictorApiResult SLALongTerm_SynthesizeInt32(
	const int32_t* residual, uint32_t num_samples,
	uint32_t pitch_period,
	const int32_t* ltm_coef, uint32_t num_taps, int32_t* output)
{
  uint32_t      i, j;
  const int32_t half    = (1UL << 30); /* 丸め用定数(0.5) */
  int32_t       predict;

  /* 引数チェック */
  if ((residual == NULL) || (ltm_coef == NULL) || (output == NULL)) {
    return SLAPREDICTOR_APIRESULT_INVALID_ARGUMENT;
  }

  /* ピッチ周期0は予測されていない */
  if (pitch_period == 0) {
    memcpy(output, residual, sizeof(int32_t) * num_samples);
    return SLAPREDICTOR_APIRESULT_OK;
  }

  /* ピッチまでのサンプルは単純コピー */
  for (i = 0; i < pitch_period + num_taps / 2; i++) {
      output[i] = residual[i];
  }

  /* ロングターム予測 */
  for (; i < num_samples; i++) {
    predict = 0;
    for (j = 0; j < num_taps; j++) {
      predict += (int32_t)SLAUTILITY_SHIFT_RIGHT_ARITHMETIC(
          (int64_t)ltm_coef[j] * output[i + j - pitch_period - num_taps / 2] + half, 31);
    }
    output[i] = residual[i] + predict;
  }

  return SLAPREDICTOR_APIRESULT_OK;
}

/* NLMS計算ハンドルの作成 */
struct SLANLMSCalculator* SLANLMSCalculator_Create(uint32_t max_num_coef)
{
  struct SLANLMSCalculator* nlms;

  nlms = malloc(sizeof(struct SLANLMSCalculator));
  nlms->max_num_coef  = max_num_coef;

  nlms->coef = malloc(sizeof(int64_t) * max_num_coef);

  return nlms;
}

/* NLMS計算ハンドルの破棄 */
void SLANLMSCalculator_Destroy(struct SLANLMSCalculator* nlms)
{
  if (nlms != NULL) {
    NULLCHECK_AND_FREE(nlms->coef);
    free(nlms);
  }
}

/* NLMS処理のコア処理 */
static SLAPredictorApiResult SLANLMSCalculator_ProcessCore(
    struct SLANLMSCalculator* nlms, uint32_t num_coef,
    int32_t* original_signal, int32_t* residual,
    uint32_t num_samples, uint8_t is_predict)
{
  uint32_t smpl, i;
  int64_t predict, input_power, mul_const;

  /* 引数チェック */
  if (nlms == NULL || original_signal == NULL || residual == NULL) {
    return SLAPREDICTOR_APIRESULT_INVALID_ARGUMENT;
  }

  /* 次数チェック */
  if (num_coef > nlms->max_num_coef) {
    return SLAPREDICTOR_APIRESULT_EXCEED_MAX_ORDER;
  }

  /* 係数を0クリア */
  memset(nlms->coef, 0, sizeof(int64_t) * num_coef);

  /* 一旦全部コピー */
  /* （残差のときは予測分で引くだけ、合成のときは足すだけで良くなる） */
  if (is_predict == 1) {
    memcpy(residual, original_signal, sizeof(int32_t) * num_samples);
  } else {
    memcpy(original_signal, residual, sizeof(int32_t) * num_samples);
  }

  for (smpl = num_coef; smpl < num_samples; smpl++) {
    /* 予測（同時に入力パワーを計算） */
    input_power   = (1UL << 31);  /* 最低でも1を保証 */
    predict       = (1UL << 30);  /* 丸め誤差回避 */
    for (i = 0; i < num_coef; i++) {
      const int64_t signal = original_signal[smpl - i - 1];
      predict     += nlms->coef[i] * signal;
      input_power += signal * signal;
      /* 値域チェック */
      assert(SLAUTILITY_SHIFT_RIGHT_ARITHMETIC(predict, 31) <= (int64_t)INT32_MAX);
      assert(SLAUTILITY_SHIFT_RIGHT_ARITHMETIC(predict, 31) >= (int64_t)INT32_MIN);
      assert((input_power  >> 31) <= (int64_t)INT32_MAX);
      assert((input_power  >> 31) >= (int64_t)INT32_MIN);
    }
    predict = SLAUTILITY_SHIFT_RIGHT_ARITHMETIC(predict, 31);
    input_power >>= 31; /* 必ず正になるので何も考えず右シフトでOK */

    if (is_predict == 1) {
      /* 残差出力 */
      residual[smpl]        -= (int32_t)predict;
    } else {
      /* 合成出力 */
      original_signal[smpl] += (int32_t)predict;
    }
    /* printf("%8d, %8d, %8d \n", residual[smpl], original_signal[smpl], (int32_t)predict); */

    /* 係数更新 */
    /* 除算の負荷が高いため、係数更新は乗算のみに変形 */
    mul_const = ((int64_t)residual[smpl] << 31) / input_power;
    for (i = 0; i < num_coef; i++) {
      /* 補足）mul_coefを展開すると定義式通りになるので、右シフト時の丸めは考慮不要。 */
      nlms->coef[i] += SLAUTILITY_SHIFT_RIGHT_ARITHMETIC(mul_const * original_signal[smpl - i - 1], 31);
    }
  }

  return SLAPREDICTOR_APIRESULT_OK;
}

/* NLMS予測 */
SLAPredictorApiResult SLANLMSCalculator_PredictInt32(
    struct SLANLMSCalculator* nlms, uint32_t num_coef,
    const int32_t* data, uint32_t num_samples, int32_t* residual)
{
  return SLANLMSCalculator_ProcessCore(nlms,
      num_coef, (int32_t *)data, residual, num_samples, 1);
}
    
/* NLMS合成 */
SLAPredictorApiResult SLANLMSCalculator_SynthesizeInt32(
    struct SLANLMSCalculator* nlms, uint32_t num_coef,
    const int32_t* residual, uint32_t num_samples, int32_t* output)
{
  return SLANLMSCalculator_ProcessCore(nlms,
      num_coef, output, (int32_t *)residual, num_samples, 0);
}
