#include "SLAPredictor.h"
#include "SLAUtility.h"
#include "SLAInternal.h"

#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <float.h>

/* 最大自己相関値からどの比率のピークをピッチとして採用するか */
/* TODO:ピッチが取りたいのではなく純粋に相関除去したいから1.0fでいいかも */
#define LPC_LONGTERM_PITCH_RATIO_VS_MAX_THRESHOULD    (1.0f)

/* ダイクストラ法使用時の巨大な重み */
#define SLAOPTIMALENCODEESTIMATOR_DIJKSTRA_BIGWEIGHT  (1UL << 24)

/* ブロックヘッダサイズの推定値 */
/* TODO:真値に置き換える */
#define SLAOPTIMALENCODEESTIMATOR_ESTIMATE_BLOCK_SIZE (50)

/* ブロック探索に必要なノード数の計算 */
#define SLAOPTIMALENCODEESTIMATOR_CALCULATE_NUM_NODES(num_samples, delta_num_samples) \
  ((((num_samples) + ((delta_num_samples) - 1)) / (delta_num_samples)) + 1)

/* sign(x) * log2ceil(|x| + 1) の計算 */
#define SLALMS_SIGNED_LOG2CEIL(x) (SLAUTILITY_SIGN(x) * (int32_t)SLAUtility_Log2Ceil((uint32_t)SLAUTILITY_ABS(x) + 1))

/* 内部エラー型 */
typedef enum SLAPredictorErrorTag {
  SLAPREDICTOR_ERROR_OK,
  SLAPREDICTOR_ERROR_NG,
  SLAPREDICTOR_ERROR_INVALID_ARGUMENT
} SLAPredictorError;

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
  uint32_t  max_order;            /* 最大次数     */
  int32_t*  forward_residual;     /* 前向き誤差   */
  int32_t*  backward_residual;    /* 後ろ向き誤差 */
};

/* ロングターム計算ハンドル */
struct SLALongTermCalculator {
  uint32_t            fft_size;                 /* FFTサイズ                  */
  uint32_t            max_num_taps;             /* 最大タップ数               */
  double*             auto_corr;                /* 自己相関                   */
  uint32_t            max_num_pitch_candidates; /* 最大のピッチ候補数         */
  uint32_t            max_pitch_period;         /* 最大ピッチ                 */
  uint32_t*           pitch_candidate;          /* ピッチ候補配列             */
  struct SLALESolver* lesolver;                 /* 連立一次方程式ソルバー     */
  double**            R_mat;                    /* 自己相関行列               */
  double*             ltm_coef_vec;             /* ロングターム係数ベクトル   */
};

/* ロングターム予測合成ハンドル */
struct SLALongTermSynthesizer {
  uint8_t             is_first_process;         /* リセット後最初の処理か否か */
};

/* LMS計算ハンドル */
struct SLALMSCalculator {
	int64_t* 	fir_coef;			            /* FIR係数 */
	int64_t* 	iir_coef;			            /* IIR係数 */
	uint32_t	max_num_coef;	            /* 最大の係数個数 */
  int32_t*  fir_sign_buffer;          /* 入力信号の符号を記録したバッファ */
  int32_t*  iir_sign_buffer;          /* 予測信号の符号を記録したバッファ */
  int32_t*  iir_buffer;               /* 予測信号バッファ */
  uint32_t  signal_sign_buffer_size;  /* バッファサイズ */
  uint32_t  buffer_pos;               /* バッファ参照位置 */
  uint8_t   is_first_process;         /* リセット後最初の処理か否か */
};

/* 最適ブロック分割探索ハンドル */
struct SLAOptimalBlockPartitionEstimator {
  uint32_t  max_num_nodes;      /* ノード数                 */
  double**  adjacency_matrix;   /* 隣接行列                 */
  double*   cost;               /* 最小コスト               */
  uint32_t* path;               /* パス経路                 */
  uint8_t*  used_flag;          /* 各ノードの使用状態フラグ */
};

/*（標本）自己相関の計算 */
static SLAPredictorError LPC_CalculateAutoCorrelation(
    const double* data, uint32_t num_samples,
    double* auto_corr, uint32_t order);

/* Levinson-Durbin再帰計算 */
static SLAPredictorError LPC_LevinsonDurbinRecursion(
    struct SLALPCCalculator* lpc, const double* auto_corr,
    double* lpc_coef, double* parcor_coef, uint32_t order);

/* 係数計算の共通関数 */
static SLAPredictorError LPC_CalculateCoef(
    struct SLALPCCalculator* lpc, 
    const double* data, uint32_t num_samples, uint32_t order);

/* LMSの更新量テーブル */
/* 補足）更新量はlog2(残差), 残差符号, 入力信号符号の3つで決まるから更新量を全てキャッシュする */
#define DEFINE_LMS_DELTA_ENTRY(log2res, signres) \
    { -(signres) * ((log2res) << (31 - SLALMS_DELTA_WEIGHT_SHIFT)), 0, (signres) * ((log2res) << (31 - SLALMS_DELTA_WEIGHT_SHIFT)) }
static const int32_t logsignlms_delta_table[64][3] = {
  DEFINE_LMS_DELTA_ENTRY(32, -1), DEFINE_LMS_DELTA_ENTRY(31, -1), DEFINE_LMS_DELTA_ENTRY(30, -1), DEFINE_LMS_DELTA_ENTRY(29, -1), 
  DEFINE_LMS_DELTA_ENTRY(28, -1), DEFINE_LMS_DELTA_ENTRY(27, -1), DEFINE_LMS_DELTA_ENTRY(26, -1), DEFINE_LMS_DELTA_ENTRY(25, -1), 
  DEFINE_LMS_DELTA_ENTRY(24, -1), DEFINE_LMS_DELTA_ENTRY(23, -1), DEFINE_LMS_DELTA_ENTRY(22, -1), DEFINE_LMS_DELTA_ENTRY(21, -1), 
  DEFINE_LMS_DELTA_ENTRY(20, -1), DEFINE_LMS_DELTA_ENTRY(19, -1), DEFINE_LMS_DELTA_ENTRY(18, -1), DEFINE_LMS_DELTA_ENTRY(17, -1), 
  DEFINE_LMS_DELTA_ENTRY(16, -1), DEFINE_LMS_DELTA_ENTRY(15, -1), DEFINE_LMS_DELTA_ENTRY(14, -1), DEFINE_LMS_DELTA_ENTRY(13, -1), 
  DEFINE_LMS_DELTA_ENTRY(12, -1), DEFINE_LMS_DELTA_ENTRY(11, -1), DEFINE_LMS_DELTA_ENTRY(10, -1), DEFINE_LMS_DELTA_ENTRY( 9, -1), 
  DEFINE_LMS_DELTA_ENTRY( 8, -1), DEFINE_LMS_DELTA_ENTRY( 7, -1), DEFINE_LMS_DELTA_ENTRY( 6, -1), DEFINE_LMS_DELTA_ENTRY( 5, -1), 
  DEFINE_LMS_DELTA_ENTRY( 4, -1), DEFINE_LMS_DELTA_ENTRY( 3, -1), DEFINE_LMS_DELTA_ENTRY( 2, -1), DEFINE_LMS_DELTA_ENTRY( 1, -1), 
  DEFINE_LMS_DELTA_ENTRY( 0,  1), DEFINE_LMS_DELTA_ENTRY( 1,  1), DEFINE_LMS_DELTA_ENTRY( 2,  1), DEFINE_LMS_DELTA_ENTRY( 3,  1),
  DEFINE_LMS_DELTA_ENTRY( 4,  1), DEFINE_LMS_DELTA_ENTRY( 5,  1), DEFINE_LMS_DELTA_ENTRY( 6,  1), DEFINE_LMS_DELTA_ENTRY( 7,  1),
  DEFINE_LMS_DELTA_ENTRY( 8,  1), DEFINE_LMS_DELTA_ENTRY( 9,  1), DEFINE_LMS_DELTA_ENTRY(10,  1), DEFINE_LMS_DELTA_ENTRY(11,  1),
  DEFINE_LMS_DELTA_ENTRY(12,  1), DEFINE_LMS_DELTA_ENTRY(13,  1), DEFINE_LMS_DELTA_ENTRY(14,  1), DEFINE_LMS_DELTA_ENTRY(15,  1),
  DEFINE_LMS_DELTA_ENTRY(16,  1), DEFINE_LMS_DELTA_ENTRY(17,  1), DEFINE_LMS_DELTA_ENTRY(18,  1), DEFINE_LMS_DELTA_ENTRY(19,  1),
  DEFINE_LMS_DELTA_ENTRY(20,  1), DEFINE_LMS_DELTA_ENTRY(21,  1), DEFINE_LMS_DELTA_ENTRY(22,  1), DEFINE_LMS_DELTA_ENTRY(23,  1),
  DEFINE_LMS_DELTA_ENTRY(24,  1), DEFINE_LMS_DELTA_ENTRY(25,  1), DEFINE_LMS_DELTA_ENTRY(26,  1), DEFINE_LMS_DELTA_ENTRY(27,  1),
  DEFINE_LMS_DELTA_ENTRY(28,  1), DEFINE_LMS_DELTA_ENTRY(29,  1), DEFINE_LMS_DELTA_ENTRY(30,  1), DEFINE_LMS_DELTA_ENTRY(31,  1),
};

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
    const double* data, uint32_t num_samples,
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
  /* parcor_coef と lpc->parcor_coef が同じ場所を指しているときもあるのでmemmove */
  memmove(parcor_coef, lpc->parcor_coef, sizeof(double) * (order + 1));

  return SLAPREDICTOR_APIRESULT_OK;
}

/* 係数計算の共通関数 */
static SLAPredictorError LPC_CalculateCoef(
    struct SLALPCCalculator* lpc, 
    const double* data, uint32_t num_samples, uint32_t order)
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

  /* 入力サンプル数が少ないときは、係数が発散することが多数
   * => 無音データとして扱い、係数はすべて0とする */
  if (num_samples < order) {
    uint32_t ord;
    for (ord = 0; ord < order + 1; ord++) {
      lpc->lpc_coef[ord] = lpc->parcor_coef[ord] = 0.0f;
    }
    return SLAPREDICTOR_ERROR_OK;
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
    SLA_Assert(e_vec[delay] >= 0.0f);

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
    SLA_Assert(fabs(gamma) < 1.0f);
  }

  /* 結果を取得 */
  memcpy(lpc_coef, a_vec, sizeof(double) * (order + 1));

  return SLAPREDICTOR_ERROR_OK;
}

/*（標本）自己相関の計算 */
static SLAPredictorError LPC_CalculateAutoCorrelation(
    const double* data, uint32_t num_samples,
    double* auto_corr, uint32_t order)
{
  uint32_t i, lag;

  /* 引数チェック */
  if (data == NULL || auto_corr == NULL) {
    return SLAPREDICTOR_ERROR_INVALID_ARGUMENT;
  }

  /* 次数（最大ラグ）がサンプル数を超えている */
  /* -> 次数をサンプル数に制限 */
  if (order > num_samples) {
    order = num_samples;
  }

  /* 自己相関初期化 */
  for (i = 0; i < order; i++) {
    auto_corr[i] = 0.0f;
  }

  /* 0次は係数は単純計算 */
  for (i = 0; i < num_samples; i++) {
    auto_corr[0] += data[i] * data[i];
  }

  /* 1次以降の係数 */
  for (lag = 1; lag < order; lag++) {
    uint32_t i, l, L;
    uint32_t Llag2;
    const uint32_t lag2 = lag << 1;

    /* 被乗数が重複している連続した項の集まりの数 */
    if ((3 * lag) < num_samples) {
      L = 1 + (num_samples - (3 * lag)) / lag2;
    } else {
      L = 0;
    }
    Llag2 = L * lag2;

    /* 被乗数が重複している分を積和 */
    for (i = 0; i < lag; i++) {
      for (l = 0; l < Llag2; l += lag2) {
        /* 一般的に lag < L なので、ループはこの順 */
        auto_corr[lag] += data[l + lag + i] * (data[l + i] + data[l + lag2 + i]);
      }
    }

    /* 残りの項を単純に積和（TODO:この中でも更にまとめることはできる...） */
    for (i = 0; i < (num_samples - Llag2 - lag); i++) {
      auto_corr[lag] += data[Llag2 + lag + i] * data[Llag2 + i];
    }

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

/* 入力データとPARCOR係数からサンプルあたりの推定符号長を求める */
SLAPredictorApiResult SLALPCCalculator_EstimateCodeLength(
    const double* data, uint32_t num_samples, uint32_t bits_par_sample,
    const double* parcor_coef, uint32_t order,
    double* length_per_sample)
{
  uint32_t smpl, ord;
  double log2_mean_res_power, log2_var_ratio;

  /* 定数値 */
#define BETA_CONST_FOR_LAPLACE_DIST   (1.9426950408889634)  /* sqrt(2 * E * E) */
#define BETA_CONST_FOR_GAUSS_DIST     (2.047095585180641)   /* sqrt(2 * E * PI) */
  /* 引数チェック */
  if (data == NULL || parcor_coef == NULL || length_per_sample == NULL) {
    return SLAPREDICTOR_APIRESULT_INVALID_ARGUMENT;
  }

  /* log2(パワー平均)の計算 */
  log2_mean_res_power = 0.0f;
  for (smpl = 0; smpl < num_samples; smpl++) {
    log2_mean_res_power += data[smpl] * data[smpl];
  }
  /* 整数PCMの振幅に変換（doubleの密度保障） */
  log2_mean_res_power *= pow(2, 2 * (bits_par_sample - 1));
  if (fabs(log2_mean_res_power) <= FLT_MIN) {
    /* ほぼ無音だった場合は符号長を0とする */
    *length_per_sample = 0.0;
    return SLAPREDICTOR_APIRESULT_OK;
  } 
  log2_mean_res_power = SLAUtility_Log2(log2_mean_res_power) - SLAUtility_Log2(num_samples);

  /* sum(log2(1-parcor * parcor))の計算 */
  /* 1次の係数は0で確定だから飛ばす */
  log2_var_ratio = 0.0f;
  for (ord = 1; ord <= order; ord++) {
    log2_var_ratio += SLAUtility_Log2(1.0f - parcor_coef[ord] * parcor_coef[ord]);
  }

  /* エントロピー計算 */
  /* →サンプルあたりの最小のビット数が得られる */
  *length_per_sample
    = BETA_CONST_FOR_LAPLACE_DIST + 0.5f * (log2_mean_res_power + log2_var_ratio);
  /* デバッグのしやすさのため、8で割ってバイト単位に換算 */
  *length_per_sample /= 8;

  /* 推定ビット数が負値の場合は、1サンプルあたり1ビットで符号化できることを期待する */
  /* 補足）このケースは入力音声パワーが非常に低い */
  if ((*length_per_sample) <= 0) {
    (*length_per_sample) = 1.0f / 8;
    return SLAPREDICTOR_APIRESULT_OK;
  }
  
  return SLAPREDICTOR_APIRESULT_OK;
}

/* 入力データとPARCOR係数から残差パワーを求める */
SLAPredictorApiResult SLALPCCalculator_CalculateResidualPower(
    const double* data, uint32_t num_samples,
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
  lpcs->forward_residual  = malloc(sizeof(int32_t) * (max_order + 1));
  lpcs->backward_residual = malloc(sizeof(int32_t) * (max_order + 1));

  /* 状態リセット */
  SLALPCSynthesizer_Reset(lpcs);

  return lpcs;
}

/* LPC音声合成ハンドルの破棄 */
void SLALPCSynthesizer_Destroy(struct SLALPCSynthesizer* lpc)
{
  if (lpc != NULL) {
    NULLCHECK_AND_FREE(lpc->forward_residual);
    NULLCHECK_AND_FREE(lpc->backward_residual);
    free(lpc);
  }
}

/* LPC音声合成ハンドルのリセット */
SLAPredictorApiResult SLALPCSynthesizer_Reset(struct SLALPCSynthesizer* lpc)
{
  uint32_t ord;

  /* 引数チェック */
  if (lpc == NULL) {
    return SLAPREDICTOR_APIRESULT_INVALID_ARGUMENT;
  }

  /* 誤差をゼロ初期化 */
  for (ord = 0; ord < lpc->max_order + 1; ord++) {
    lpc->forward_residual[ord] = lpc->backward_residual[ord] = 0;
  }

  return SLAPREDICTOR_APIRESULT_OK;
}

/* PARCOR係数により予測/誤差出力（32bit整数入出力）: 乗算時に32bit幅になるように修正 */
SLAPredictorApiResult SLALPCSynthesizer_PredictByParcorCoefInt32(
    struct SLALPCSynthesizer* lpc,
    const int32_t* data, uint32_t num_samples,
    const int32_t* parcor_coef, uint32_t order, int32_t* residual)
{
  uint32_t      samp, ord;
  int32_t*      forward_residual;
  int32_t*      backward_residual;
  int32_t       mul_temp;
  const int32_t half = (1UL << 14);

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
  forward_residual  = lpc->forward_residual;
  backward_residual = lpc->backward_residual;

  /* 誤差計算 */
  for (samp = 0; samp < num_samples; samp++) {
    /* 格子型フィルタにデータ入力 */
    forward_residual[0] = data[samp];
    /* 前向き誤差計算 */
    for (ord = 1; ord <= order; ord++) {
      mul_temp 
        = (int32_t)SLAUTILITY_SHIFT_RIGHT_ARITHMETIC(parcor_coef[ord] * backward_residual[ord - 1] + half, 15);
      forward_residual[ord] = forward_residual[ord - 1] - mul_temp;
    }
    /* 後ろ向き誤差計算 */
    for (ord = order; ord >= 1; ord--) {
      mul_temp 
        = (int32_t)SLAUTILITY_SHIFT_RIGHT_ARITHMETIC(parcor_coef[ord] * forward_residual[ord - 1] + half, 15);
      backward_residual[ord] = backward_residual[ord - 1] - mul_temp;
    }
    /* 後ろ向き誤差計算部にデータ入力 */
    backward_residual[0] = data[samp];
    /* 残差信号 */
    residual[samp] = forward_residual[order];
    /* printf("res: %08x(%8d) \n", residual[samp], residual[samp]); */
  }

  return SLAPREDICTOR_APIRESULT_OK;
}

/* PARCOR係数により誤差信号から音声合成（32bit整数入出力） */
SLAPredictorApiResult SLALPCSynthesizer_SynthesizeByParcorCoefInt32(
    struct SLALPCSynthesizer* lpc,
    const int32_t* residual, uint32_t num_samples,
    const int32_t* parcor_coef, uint32_t order, int32_t* output)
{
  uint32_t      ord, samp;
  int32_t       forward_residual;   /* 合成時は記憶領域を持つ必要なし */
  int32_t*      backward_residual;
  int32_t       mul_temp;
  const int32_t half = (1UL << 14); /* 丸め誤差軽減のための加算定数 = 0.5 */

  /* 引数チェック */
  if (lpc == NULL || residual == NULL
      || parcor_coef == NULL || output == NULL) {
    return SLAPREDICTOR_APIRESULT_INVALID_ARGUMENT;
  }

  /* 次数チェック */
  if (order > lpc->max_order) {
    return SLAPREDICTOR_APIRESULT_EXCEED_MAX_ORDER;
  }

  /* オート変数にポインタをコピー */
  backward_residual = lpc->backward_residual;

  /* 格子型フィルタによる音声合成 */
  for (samp = 0; samp < num_samples; samp++) {
    /* 誤差入力 */
    forward_residual = residual[samp];
    for (ord = order; ord >= 1; ord--) {
      /* 前向き誤差計算 */
      forward_residual
        += (int32_t)SLAUTILITY_SHIFT_RIGHT_ARITHMETIC(parcor_coef[ord] * backward_residual[ord - 1] + half, 15);
      /* 後ろ向き誤差計算 */
      mul_temp
        = (int32_t)SLAUTILITY_SHIFT_RIGHT_ARITHMETIC(parcor_coef[ord] * forward_residual + half, 15);
      backward_residual[ord] = backward_residual[ord - 1] - mul_temp;
    }
    /* 合成信号 */
    output[samp] = forward_residual;
    /* 後ろ向き誤差計算部にデータ入力 */
    backward_residual[0] = forward_residual;
    /* printf("out: %08x(%8d) \n", output[samp], output[samp]); */
  }

  return SLAPREDICTOR_APIRESULT_OK;
}

/* ロングターム計算ハンドルの作成 */
struct SLALongTermCalculator* SLALongTermCalculator_Create(
    uint32_t fft_size, uint32_t max_pitch_period, 
    uint32_t max_num_pitch_candidates, uint32_t max_num_taps)
{
  uint32_t dim;
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
  ltm->max_num_taps             = max_num_taps;
  ltm->lesolver                 = SLALESolver_Create(max_num_taps);
  ltm->ltm_coef_vec             = (double *)malloc(sizeof(double) * max_num_taps);
  ltm->R_mat                    = (double **)malloc(sizeof(double *) * max_num_taps);
  for (dim = 0; dim < max_num_taps; dim++) {
    ltm->R_mat[dim] = (double *)malloc(sizeof(double) * max_num_taps);
  }

  return ltm;
}

/* ロングターム計算ハンドルの破棄 */
void SLALongTermCalculator_Destroy(struct SLALongTermCalculator* ltm_calculator)
{
  if (ltm_calculator != NULL) {
    uint32_t dim;
    NULLCHECK_AND_FREE(ltm_calculator->auto_corr);
    NULLCHECK_AND_FREE(ltm_calculator->pitch_candidate);
    SLALESolver_Destroy(ltm_calculator->lesolver);
    NULLCHECK_AND_FREE(ltm_calculator->ltm_coef_vec);
    for (dim = 0; dim < ltm_calculator->max_num_taps; dim++) {
      NULLCHECK_AND_FREE(ltm_calculator->R_mat[dim]);
    }
    NULLCHECK_AND_FREE(ltm_calculator->R_mat);
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
  if (num_taps > ltm_calculator->max_num_taps) {
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

  /* ロングターム係数の導出 */
  {
    uint32_t  tmp_pitch_period = ltm_calculator->pitch_candidate[i];
    uint32_t  j, k;
    double    ltm_coef_sum;

    /* 自己相関のギャップが対称にならんだ行列 */
    /* (i,j)要素にギャップ|i-j|の自己相関値が入っている */
    for (j = 0; j < num_taps; j++) {
      for (k = 0; k < num_taps; k++) {
        uint32_t lag = (j >= k) ? (j - k) : (k - j);
        ltm_calculator->R_mat[j][k] = auto_corr[lag];
      }
    }

    /* 中心においてピッチ周期の自己相関が入ったベクトル */
    for (j = 0; j < num_taps; j++) {
      ltm_calculator->ltm_coef_vec[j]
        = auto_corr[j + tmp_pitch_period - num_taps / 2];
    }

    /* 求解 */
    /* 精度を求めるため、2回反復改良 */
    if (SLALESolver_Solve(ltm_calculator->lesolver,
          (const double **)ltm_calculator->R_mat, ltm_calculator->ltm_coef_vec, num_taps, 2) != 0) {
      return SLAPREDICTOR_APIRESULT_FAILED_TO_CALCULATION;
    }

    /* 得られた係数の収束条件を確認 */
    ltm_coef_sum = 0.0f;
    for (j = 0; j < num_taps; j++) {
      ltm_coef_sum += fabs(ltm_calculator->ltm_coef_vec[j]);
    }
    if (ltm_coef_sum >= 1.0f) {
      /* 確実に安定する係数をセット: タップ数1と同様の状態に修正 */
      for (j = 0; j < num_taps; j++) {
        ltm_calculator->ltm_coef_vec[j] = 0.0f;
      }
       ltm_calculator->ltm_coef_vec[num_taps / 2]
         = auto_corr[tmp_pitch_period] / auto_corr[0];
    }

    /* 結果を出力にセット */
    *pitch_period = tmp_pitch_period;
    for (j = 0; j < num_taps; j++) {
      ltm_coef[j] = ltm_calculator->ltm_coef_vec[j];
    }
  }

  return SLAPREDICTOR_APIRESULT_OK;
}

/* ロングターム予測合成ハンドル作成 */
struct SLALongTermSynthesizer* SLALongTermSynthesizer_Create(void)
{
  struct SLALongTermSynthesizer* ltm;

  ltm = malloc(sizeof(struct SLALongTermSynthesizer));
  
  SLALongTermSynthesizer_Reset(ltm);

  return ltm;
}

/* ロングターム予測合成ハンドル破棄 */
void SLALongTermSynthesizer_Destroy(struct SLALongTermSynthesizer* ltm)
{
  NULLCHECK_AND_FREE(ltm);
}

/* ロングターム予測合成ハンドルリセット */
SLAPredictorApiResult SLALongTermSynthesizer_Reset(struct SLALongTermSynthesizer* ltm)
{
  if (ltm == NULL) {
    return SLAPREDICTOR_APIRESULT_INVALID_ARGUMENT;
  }

  /* 次の予測合成処理が最初の処理 */
  ltm->is_first_process = 1;

  return SLAPREDICTOR_APIRESULT_OK;
}

/* ロングタームを使用して残差信号の計算 */
SLAPredictorApiResult SLALongTermSynthesizer_PredictInt32(
  struct SLALongTermSynthesizer* ltm,
	const int32_t* data, uint32_t num_samples,
	uint32_t pitch_period, 
	const int32_t* ltm_coef, uint32_t num_taps, int32_t* residual)
{
  uint32_t      i, j;
  const int32_t half    = (1UL << 30); /* 丸め用定数(0.5) */
  int64_t       predict;

  /* 引数チェック */
  if ((ltm == NULL) || (data == NULL) || (ltm_coef == NULL) || (residual == NULL)) {
    return SLAPREDICTOR_APIRESULT_INVALID_ARGUMENT;
  }

  /* ピッチ周期0は予測せず、そのまま誤差とする */
  if (pitch_period == 0) {
    memcpy(residual, data, sizeof(int32_t) * num_samples);
    return SLAPREDICTOR_APIRESULT_OK;
  }

  /* 予測が始まるまでのサンプルは単純コピー */
  if (ltm->is_first_process == 1) {
    for (i = 0; i < pitch_period + num_taps / 2; i++) {
      residual[i] = data[i];
    }
    ltm->is_first_process = 0;
  } else {
    i = 0;
  }

  /* ロングターム予測 */
  for (; i < num_samples; i++) {
    predict = half;
    for (j = 0; j < num_taps; j++) {
      predict += (int64_t)ltm_coef[j] * data[i + j - pitch_period - num_taps / 2];
    }
    predict = SLAUTILITY_SHIFT_RIGHT_ARITHMETIC(predict, 31);
    residual[i] = data[i] - (int32_t)predict;
  }

  return SLAPREDICTOR_APIRESULT_OK;
}
	
/* ロングターム誤差信号から音声合成 */
SLAPredictorApiResult SLALongTermSynthesizer_SynthesizeInt32(
  struct SLALongTermSynthesizer* ltm,
	const int32_t* residual, uint32_t num_samples,
	uint32_t pitch_period,
	const int32_t* ltm_coef, uint32_t num_taps, int32_t* output)
{
  uint32_t      i, j;
  const int32_t half    = (1UL << 30); /* 丸め用定数(0.5) */
  int64_t       predict;

  /* 引数チェック */
  if ((ltm == NULL) || (residual == NULL) || (ltm_coef == NULL) || (output == NULL)) {
    return SLAPREDICTOR_APIRESULT_INVALID_ARGUMENT;
  }

  /* ピッチ周期0は予測されていない */
  if (pitch_period == 0) {
    memcpy(output, residual, sizeof(int32_t) * num_samples);
    return SLAPREDICTOR_APIRESULT_OK;
  }

  /* 合成処理が始まるまでのサンプルは単純コピー */
  if (ltm->is_first_process == 1) {
    for (i = 0; i < pitch_period + num_taps / 2; i++) {
      output[i] = residual[i];
    }
    ltm->is_first_process = 0;
  } else {
    i = 0;
  }

  /* ロングターム予測 */
  for (; i < num_samples; i++) {
    predict = half;
    for (j = 0; j < num_taps; j++) {
      predict += (int64_t)ltm_coef[j] * output[i + j - pitch_period - num_taps / 2];
    }
    predict = SLAUTILITY_SHIFT_RIGHT_ARITHMETIC(predict, 31);
    output[i] = residual[i] + (int32_t)predict;
  }

  return SLAPREDICTOR_APIRESULT_OK;
}

/* LMS計算ハンドルの作成 */
struct SLALMSCalculator* SLALMSCalculator_Create(uint32_t max_num_coef)
{
  struct SLALMSCalculator* nlms;

  nlms = malloc(sizeof(struct SLALMSCalculator));
  nlms->max_num_coef            = max_num_coef;
  nlms->signal_sign_buffer_size = SLAUtility_RoundUp2Powered(max_num_coef);

  nlms->fir_coef                = malloc(sizeof(int64_t) * max_num_coef);
  nlms->iir_coef                = malloc(sizeof(int64_t) * max_num_coef);
  /* バッファアクセスの高速化のため2倍確保 */
  nlms->fir_sign_buffer         = malloc(sizeof(int32_t) * 2 * max_num_coef);
  nlms->iir_sign_buffer         = malloc(sizeof(int32_t) * 2 * max_num_coef);
  nlms->iir_buffer              = malloc(sizeof(int32_t) * 2 * max_num_coef);

  SLALMSCalculator_Reset(nlms);

  return nlms;
}

/* LMS計算ハンドルの破棄 */
void SLALMSCalculator_Destroy(struct SLALMSCalculator* nlms)
{
  if (nlms != NULL) {
    NULLCHECK_AND_FREE(nlms->fir_coef);
    NULLCHECK_AND_FREE(nlms->iir_coef);
    NULLCHECK_AND_FREE(nlms->fir_sign_buffer);
    NULLCHECK_AND_FREE(nlms->iir_sign_buffer);
    NULLCHECK_AND_FREE(nlms->iir_buffer);
    free(nlms);
  }
}

/* LMS計算ハンドルのリセット */
SLAPredictorApiResult SLALMSCalculator_Reset(struct SLALMSCalculator* nlms)
{
  /* 引数チェック */
  if (nlms == NULL) {
    return SLAPREDICTOR_APIRESULT_INVALID_ARGUMENT;
  }

  /* 初回処理フラグを立てる */
  nlms->is_first_process = 1;

  /* バッファ参照位置の初期化 */
  nlms->buffer_pos = 0;

  return SLAPREDICTOR_APIRESULT_OK;
}

/* LMS処理のコア処理 */
static SLAPredictorApiResult SLALMSCalculator_ProcessCore(
    struct SLALMSCalculator* nlms, uint32_t num_coef,
    int32_t* original_signal, int32_t* residual,
    uint32_t num_samples, uint8_t is_predict)
{
  uint32_t        smpl, i;
  uint32_t        buffer_pos;
  int64_t         predict;
  const int32_t*  delta_table_p;

  /* 引数チェック */
  if (nlms == NULL || original_signal == NULL || residual == NULL) {
    return SLAPREDICTOR_APIRESULT_INVALID_ARGUMENT;
  }

  /* 次数チェック */
  if (num_coef > nlms->max_num_coef) {
    return SLAPREDICTOR_APIRESULT_EXCEED_MAX_ORDER;
  }
  
  /* 符号バッファの初期化 */
  if (nlms->is_first_process == 1) {
    for (smpl = 0; smpl < num_coef; smpl++) {
      /* 合成時は残差に符号情報が入っている */
      nlms->fir_sign_buffer[smpl]
        = nlms->fir_sign_buffer[smpl + num_coef]
        = nlms->iir_sign_buffer[smpl]
        = nlms->iir_sign_buffer[smpl + num_coef]
        = (is_predict == 1) 
        ? (SLAUTILITY_SIGN(original_signal[num_coef - smpl - 1]) + 1)
        : (SLAUTILITY_SIGN(residual[num_coef - smpl - 1]) + 1);
    }
  }

  /* 係数を0クリア */
  memset(nlms->fir_coef, 0, sizeof(int64_t) * num_coef);
  memset(nlms->iir_coef, 0, sizeof(int64_t) * num_coef);

  /* 一旦全部コピー */
  /* （残差のときは予測分で引くだけ、合成のときは足すだけで良くなる） */
  if (is_predict == 1) {
    memcpy(residual, original_signal, sizeof(int32_t) * num_samples);
  } else {
    memcpy(original_signal, residual, sizeof(int32_t) * num_samples);
  }

  /* 出力バッファの初期化: 先頭coef分は残差と元信号は同一 */
  if (nlms->is_first_process == 1) {
    for (smpl = 0; smpl < num_coef; smpl++) {
      nlms->iir_buffer[smpl]
        = nlms->iir_buffer[smpl + num_coef]
        = residual[num_coef - smpl - 1];
    }
    nlms->is_first_process = 0;
    buffer_pos = num_coef;
  } else {
    smpl = 0;
    buffer_pos = nlms->buffer_pos;
  }

  /* フィルタ処理実行 */
  for (; smpl < num_samples; smpl++) {
    /* 予測 */
    predict = (1UL << 30);  /* 丸め誤差回避 */
    for (i = 0; i < num_coef; i++) {
      predict += nlms->fir_coef[i] * original_signal[smpl - i - 1];
      predict += nlms->iir_coef[i] * nlms->iir_buffer[buffer_pos + i];
      /* 値域チェック */
      SLA_Assert(SLAUTILITY_SHIFT_RIGHT_ARITHMETIC(predict, 31) <= (int64_t)INT32_MAX);
      SLA_Assert(SLAUTILITY_SHIFT_RIGHT_ARITHMETIC(predict, 31) >= (int64_t)INT32_MIN);
    }
    predict = SLAUTILITY_SHIFT_RIGHT_ARITHMETIC(predict, 31);

    if (is_predict == 1) {
      /* 残差出力 */
      residual[smpl]        -= (int32_t)predict;
    } else {
      /* 合成出力 */
      original_signal[smpl] += (int32_t)predict;
    }
    /* printf("%8d, %8d, %8d \n", residual[smpl], original_signal[smpl], (int32_t)predict); */

    /* 係数更新 */
    /* 32を加算して [-32, 31] を [0, 63] の範囲にマップ */
    delta_table_p = logsignlms_delta_table[SLALMS_SIGNED_LOG2CEIL(residual[smpl]) + 32]; 
    for (i = 0; i < num_coef; i++) {
      int32_t table_index;
      table_index = nlms->fir_sign_buffer[buffer_pos + i];
      nlms->fir_coef[i] += delta_table_p[table_index];
      table_index = nlms->iir_sign_buffer[buffer_pos + i];
      nlms->iir_coef[i] += delta_table_p[table_index];
    }

    /* バッファ更新 */

    /* バッファ参照位置更新 */
    buffer_pos = (buffer_pos == 0) ? (num_coef - 1) : (buffer_pos - 1);

    /* 出力バッファに記録 */
    /* 補足）バッファアクセスの高速化のため係数分離れた場所にも記録 */
    nlms->iir_buffer[buffer_pos]
      = nlms->iir_buffer[buffer_pos + num_coef]
      = (int32_t)predict;

    /* 更新量テーブルのインデックスを計算 */
    nlms->iir_sign_buffer[buffer_pos]
      = nlms->iir_sign_buffer[buffer_pos + num_coef]
      = SLAUTILITY_SIGN(nlms->iir_buffer[buffer_pos]) + 1;
    nlms->fir_sign_buffer[buffer_pos]
      = nlms->fir_sign_buffer[buffer_pos + num_coef]
      = SLAUTILITY_SIGN(original_signal[smpl]) + 1;
  }

  /* バッファ参照位置の記録 */
  nlms->buffer_pos = buffer_pos;

  return SLAPREDICTOR_APIRESULT_OK;
}

/* LMS予測 */
SLAPredictorApiResult SLALMSCalculator_PredictInt32(
    struct SLALMSCalculator* nlms, uint32_t num_coef,
    const int32_t* data, uint32_t num_samples, int32_t* residual)
{
  return SLALMSCalculator_ProcessCore(nlms,
      num_coef, (int32_t *)data, residual, num_samples, 1);
}
    
/* LMS合成 */
SLAPredictorApiResult SLALMSCalculator_SynthesizeInt32(
    struct SLALMSCalculator* nlms, uint32_t num_coef,
    const int32_t* residual, uint32_t num_samples, int32_t* output)
{
  return SLALMSCalculator_ProcessCore(nlms,
      num_coef, output, (int32_t *)residual, num_samples, 0);
}

/* 最大分割数計算 */
/* 用語 "ノード" が外に出るのを嫌ったため関数化 */
uint32_t SLAOptimalEncodeEstimator_CalculateMaxNumPartitions(
    uint32_t max_num_samples, uint32_t delta_num_samples)
{
  return SLAOPTIMALENCODEESTIMATOR_CALCULATE_NUM_NODES(max_num_samples, delta_num_samples);
}

/* 探索ハンドルの作成 */
struct SLAOptimalBlockPartitionEstimator* SLAOptimalEncodeEstimator_Create(
    uint32_t max_num_samples, uint32_t delta_num_samples)
{
  uint32_t i, tmp_max_num_nodes;
  struct SLAOptimalBlockPartitionEstimator* oee;

  /* 引数チェック */
  if (max_num_samples < delta_num_samples) {
    return NULL;
  }

  oee = malloc(sizeof(struct SLAOptimalBlockPartitionEstimator));

  /* 最大ノード数の計算 */
  tmp_max_num_nodes 
    = SLAOPTIMALENCODEESTIMATOR_CALCULATE_NUM_NODES(max_num_samples, delta_num_samples);
  oee->max_num_nodes = tmp_max_num_nodes;

  /* 領域確保 */
  oee->adjacency_matrix = (double **)malloc(sizeof(double *) * tmp_max_num_nodes);
  oee->cost             = (double *)malloc(sizeof(double) * tmp_max_num_nodes);
  oee->path             = (uint32_t *)malloc(sizeof(uint32_t) * tmp_max_num_nodes);
  oee->used_flag        = (uint8_t *)malloc(sizeof(uint8_t) * tmp_max_num_nodes);
  for (i = 0; i < tmp_max_num_nodes; i++) {
    oee->adjacency_matrix[i] = (double *)malloc(sizeof(double) * tmp_max_num_nodes);
  }

  return oee;
}

/* 探索ハンドルの作成 */
void SLAOptimalEncodeEstimator_Destroy(struct SLAOptimalBlockPartitionEstimator* oee)
{
  uint32_t i;
  if (oee != NULL) {
    for (i = 0; i < oee->max_num_nodes; i++) {
      NULLCHECK_AND_FREE(oee->adjacency_matrix[i]);
    }
    NULLCHECK_AND_FREE(oee->adjacency_matrix);
    NULLCHECK_AND_FREE(oee->cost);
    NULLCHECK_AND_FREE(oee->path);
    NULLCHECK_AND_FREE(oee->used_flag);
    NULLCHECK_AND_FREE(oee);
  }
}

/* ダイクストラ法により最短経路を求める */
static SLAPredictorApiResult SLAOptimalEncodeEstimator_ApplyDijkstraMethod(
    struct SLAOptimalBlockPartitionEstimator* oee, 
    uint32_t num_nodes, uint32_t start_node, uint32_t goal_node,
    double* min_cost)
{
  uint32_t  i;
  double    min;
  uint32_t  target = 0;

  /* 引数チェック */
  if (oee == NULL || min_cost == NULL) {
    return SLAPREDICTOR_APIRESULT_INVALID_ARGUMENT;
  }
  if (num_nodes > oee->max_num_nodes) {
    return SLAPREDICTOR_APIRESULT_EXCEED_MAX_ORDER;
  }

  /* フラグと経路をクリア, 距離は巨大値に設定 */
  for (i = 0; i < oee->max_num_nodes; i++) {
    oee->used_flag[i] = 0;
    oee->path[i]      = 0xFFFFFFFFUL;
    oee->cost[i]      = SLAOPTIMALENCODEESTIMATOR_DIJKSTRA_BIGWEIGHT;
  }

  /* ダイクストラ法実行 */
  oee->cost[start_node] = 0.0f;
  while (1) {
    /* まだ未確定のノードから最も距離（重み）が小さいノードを
     * その地点の最小距離として確定 */
    min = SLAOPTIMALENCODEESTIMATOR_DIJKSTRA_BIGWEIGHT;
    for (i = 0; i < num_nodes; i++) {
      if ((oee->used_flag[i] == 0) && (oee->cost[i] < min)) {
        min     = oee->cost[i];
        target  = i;
      }
    }

    /* 最短経路が確定 */
    if (target == goal_node) {
      break;
    }

    /* 現在確定したノードから、直接繋がっており、かつ、未確定の
     * ノードに対して、現在確定したノードを経由した時の距離を計算し、
     * 今までの距離よりも小さければ距離と経路を修正 */
    for (i = 0; i < num_nodes; i++) {
      if (oee->cost[i] > (oee->adjacency_matrix[target][i] + oee->cost[target])) {
        oee->cost[i]  = oee->adjacency_matrix[target][i] + oee->cost[target];
        oee->path[i]  = target;
      }
    }

    /* 現在注目しているノードを確定に変更 */
    oee->used_flag[target] = 1;
  }

  /* 最小距離のセット */
  *min_cost = oee->cost[goal_node];

  return SLAPREDICTOR_APIRESULT_OK;
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

/* 最適なブロック分割の探索 */
SLAPredictorApiResult SLAOptimalEncodeEstimator_SearchOptimalBlockPartitions(
    struct SLAOptimalBlockPartitionEstimator* oee, 
    struct SLALPCCalculator* lpcc,
    const double* const* data, uint32_t num_channels, uint32_t num_samples,
    uint32_t min_num_block_samples, uint32_t delta_num_samples, uint32_t max_num_block_samples,
    uint32_t bits_par_sample, uint32_t parcor_order, 
    uint32_t* optimal_num_partitions, uint32_t* optimal_block_partition)
{
  uint32_t  i, j, ch;
  uint32_t  num_nodes;
  double    min_code_length;
  uint32_t  tmp_optimal_num_partitions, tmp_node;

  /* 引数チェック */
  if (oee == NULL || lpcc == NULL || data == NULL
      || optimal_num_partitions == NULL
      || optimal_block_partition == NULL) {
    return SLAPREDICTOR_APIRESULT_INVALID_ARGUMENT;
  }

  /* 隣接行列次元（ノード数）の計算 */
  num_nodes = SLAOPTIMALENCODEESTIMATOR_CALCULATE_NUM_NODES(num_samples, delta_num_samples);

  /* 最大ノード数を超えている */
  if (num_nodes > oee->max_num_nodes) {
    return SLAPREDICTOR_APIRESULT_EXCEED_MAX_ORDER;
  }

  /* 隣接行列のセット */
  /* (i,j)要素は、i * delta_num_samples から j * delta_num_samples まで
   * エンコードした時の推定符号長が入る */
  for (i = 0; i < num_nodes; i++) {
    for (j = 0; j < num_nodes; j++) {
      if (j > i) {
        double    estimated_code_length, code_length;
        uint32_t  num_block_samples  = (j - i) * delta_num_samples;
        uint32_t  sample_offset      = i * delta_num_samples;

        /* min_num_block_samplesでは端点で飛び出る場合があるので調節 */
        num_block_samples           = SLAUTILITY_MIN(num_block_samples, num_samples - sample_offset);

        /* ブロックサイズが範囲外だった場合は巨大値をセットして次へ */
        if ((num_block_samples < min_num_block_samples)
            || (num_block_samples > max_num_block_samples)) {
          oee->adjacency_matrix[i][j] = SLAOPTIMALENCODEESTIMATOR_DIJKSTRA_BIGWEIGHT;
          continue;
        }

        /* 全チャンネルの符号長を計算 */
        estimated_code_length = 0.0f;
        for (ch = 0; ch < num_channels; ch++) {
          /* PARCOR係数を求める */
          if (SLALPCCalculator_CalculatePARCORCoefDouble(lpcc, 
                &data[ch][sample_offset], num_block_samples,
                lpcc->parcor_coef, parcor_order) != SLAPREDICTOR_APIRESULT_OK) {
            return SLAPREDICTOR_APIRESULT_FAILED_TO_CALCULATION;
          }
          /* 1サンプルあたりの推定符号長の計算 */
          if (SLALPCCalculator_EstimateCodeLength(
                &data[ch][sample_offset],
                num_block_samples, bits_par_sample,
                lpcc->parcor_coef, parcor_order, &code_length) != SLAPREDICTOR_APIRESULT_OK) {
            return SLAPREDICTOR_APIRESULT_FAILED_TO_CALCULATION;
          }
          estimated_code_length += num_block_samples * code_length;
        }
        /* ブロックヘッダサイズを加算 */
        estimated_code_length += SLAOPTIMALENCODEESTIMATOR_ESTIMATE_BLOCK_SIZE;
        /* パス増加時のペナルティを付加 */
        /* 補足）重要。ブロックを小さく分割した場合のペナルティになる */
        estimated_code_length += SLAOPTIMALENCODEESTIMATOR_LONGPATH_PENALTY;

        /* 隣接行列にセット */
        oee->adjacency_matrix[i][j] = estimated_code_length;
      } else {
        /* その他の要素は巨大値で埋める */
        oee->adjacency_matrix[i][j] = SLAOPTIMALENCODEESTIMATOR_DIJKSTRA_BIGWEIGHT;
      }
    }
  }

  /* ダイクストラ法を実行 */
  if (SLAOptimalEncodeEstimator_ApplyDijkstraMethod(oee,
        num_nodes, 0, num_nodes - 1, &min_code_length) != SLAPREDICTOR_APIRESULT_OK) {
    return SLAPREDICTOR_APIRESULT_FAILED_TO_CALCULATION;
  }

  /* 結果の解釈 */
  /* ゴールから開始位置まで逆順に辿り、まずはパスの長さを求める */
  tmp_optimal_num_partitions = 0;
  tmp_node = num_nodes - 1;
  while (tmp_node != 0) {
    /* ノードの巡回順路は昇順になっているはず */
    SLA_Assert(tmp_node > oee->path[tmp_node]);
    tmp_node = oee->path[tmp_node];
    tmp_optimal_num_partitions++;
  }

  /* 再度辿り、分割サイズ情報をセットしていく */
  tmp_node = num_nodes - 1;
  for (i = 0; i < tmp_optimal_num_partitions; i++) {
    uint32_t  num_block_samples = (tmp_node - oee->path[tmp_node]) * delta_num_samples;
    uint32_t  sample_offset = oee->path[tmp_node] * delta_num_samples;
    num_block_samples = SLAUTILITY_MIN(num_block_samples, num_samples - sample_offset);

    /* クリッピングしたブロックサンプル数をセット */
    optimal_block_partition[tmp_optimal_num_partitions - i - 1] = num_block_samples;
    tmp_node = oee->path[tmp_node];
  }

  /* 分割数の設定 */
  *optimal_num_partitions = tmp_optimal_num_partitions;

  /*
  for (i = 0; i < *optimal_num_partitions; i++) {
    printf("%d: %d \n", i, optimal_block_partition[i]);
  }
  printf("Estimated length: %e \n", min_code_length);
  */

  return SLAPREDICTOR_APIRESULT_OK;
}
