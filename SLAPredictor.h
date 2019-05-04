#ifndef SLAPREDICTOR_H_INCLUDED
#define SLAPREDICTOR_H_INCLUDED

#include <stdint.h>

/* LPC係数計算ハンドル */
struct SLALPCCalculator;

/* LPC音声合成ハンドル */
struct SLALPCSynthesizer;

/* ロングターム計算ハンドル */
struct SLALongTermCalculator;

/* LMS計算ハンドル */
struct SLALMSCalculator;

/* 最適ブロック分割探索ハンドル */
struct SLAOptimalBlockPartitionEstimator;

/* API結果型 */
typedef enum SLAPredictorApiResultTag {
  SLAPREDICTOR_APIRESULT_OK,                     /* OK */
  SLAPREDICTOR_APIRESULT_NG,                     /* 分類不能なエラー */
  SLAPREDICTOR_APIRESULT_INVALID_ARGUMENT,       /* 不正な引数 */
  SLAPREDICTOR_APIRESULT_EXCEED_MAX_ORDER,       /* 最大次数を超えた */
  SLAPREDICTOR_APIRESULT_FAILED_TO_CALCULATION   /* 計算に失敗 */
} SLAPredictorApiResult;

#ifdef __cplusplus
extern "C" {
#endif

/* LPC係数計算ハンドルの作成 */
struct SLALPCCalculator* SLALPCCalculator_Create(uint32_t max_order);

/* LPC係数計算ハンドルの破棄 */
void SLALPCCalculator_Destroy(struct SLALPCCalculator* lpc);

/* Levinson-Durbin再帰計算によりPARCOR係数を求める（倍精度） */
/* 係数parcor_coefはorder+1個の配列 */
SLAPredictorApiResult SLALPCCalculator_CalculatePARCORCoefDouble(
    struct SLALPCCalculator* lpcc,
    const double* data, uint32_t num_samples,
    double* parcor_coef, uint32_t order);

/* 入力データとPARCOR係数からサンプルあたりの推定符号長を求める */
SLAPredictorApiResult SLALPCCalculator_EstimateCodeLength(
    const double* data, uint32_t num_samples, uint32_t bits_par_sample,
    const double* parcor_coef, uint32_t order, 
    double* length_per_sample);

/* 入力データとPARCOR係数から残差パワーを求める */
SLAPredictorApiResult SLALPCCalculator_CalculateResidualPower(
    const double* data, uint32_t num_samples,
    const double* parcor_coef, uint32_t order,
    double* residual_power);

/* LPC音声合成ハンドルの作成 */
struct SLALPCSynthesizer* SLALPCSynthesizer_Create(uint32_t max_order);

/* LPC音声合成ハンドルの破棄 */
void SLALPCSynthesizer_Destroy(struct SLALPCSynthesizer* lpc);

/* PARCOR係数により予測/誤差出力（32bit整数入出力） */
/* 係数parcor_coefはorder+1個の配列 */
SLAPredictorApiResult SLALPCSynthesizer_PredictByParcorCoefInt32(
    struct SLALPCSynthesizer* lpcs,
    const int32_t* data, uint32_t num_samples,
    const int32_t* parcor_coef, uint32_t order,
    int32_t* residual);

/* PARCOR係数により誤差信号から音声合成（32bit整数入出力） */
/* 係数parcor_coefはorder+1個の配列 */
SLAPredictorApiResult SLALPCSynthesizer_SynthesizeByParcorCoefInt32(
    struct SLALPCSynthesizer* lpcs,
    const int32_t* residual, uint32_t num_samples,
    const int32_t* parcor_coef, uint32_t order,
    int32_t* output);

/* ロングターム計算ハンドルの作成 */
struct SLALongTermCalculator* SLALongTermCalculator_Create(
    uint32_t fft_size, 
    uint32_t max_pitch_period, uint32_t max_num_pitch_candidates);

/* ロングターム計算ハンドルの破棄 */
void SLALongTermCalculator_Destroy(struct SLALongTermCalculator* ltm_calculator);

/* ロングターム係数の計算（内部的にピッチ解析が走る） */
SLAPredictorApiResult SLALongTermCalculator_CalculateCoef(
	struct SLALongTermCalculator* ltm_calculator,
	const int32_t* data, uint32_t num_samples,
	uint32_t* pitch_num_samples, double* ltm_coef, uint32_t num_taps);

/* ロングタームを使用して残差信号の計算 */
SLAPredictorApiResult SLALongTerm_PredictInt32(
	const int32_t* data, uint32_t num_samples,
	uint32_t pitch_period, 
	const int32_t* ltm_coef, uint32_t num_taps, int32_t* residual);

/* ロングターム誤差信号から音声合成 */
SLAPredictorApiResult SLALongTerm_SynthesizeInt32(
	const int32_t* residual, uint32_t num_samples,
	uint32_t pitch_period,
	const int32_t* ltm_coef, uint32_t num_taps, int32_t* output);

/* LMS計算ハンドルの作成 */
struct SLALMSCalculator* SLALMSCalculator_Create(uint32_t max_num_coef);

/* LMS計算ハンドルの破棄 */
void SLALMSCalculator_Destroy(struct SLALMSCalculator* nlms);

/* LMS予測 */
SLAPredictorApiResult SLALMSCalculator_PredictInt32(
    struct SLALMSCalculator* nlms, uint32_t num_coef,
    const int32_t* data, uint32_t num_samples, int32_t* residual);

/* LMS合成 */
SLAPredictorApiResult SLALMSCalculator_SynthesizeInt32(
    struct SLALMSCalculator* nlms, uint32_t num_coef,
    const int32_t* residual, uint32_t num_samples, int32_t* output);

/* 探索ハンドルの作成 */
struct SLAOptimalBlockPartitionEstimator* SLAOptimalEncodeEstimator_Create(
    uint32_t max_num_samples, uint32_t delta_num_samples);

/* 最適なブロック分割の探索ハンドルの作成 */
void SLAOptimalEncodeEstimator_Destroy(struct SLAOptimalBlockPartitionEstimator* oee);

/* 最適なブロック分割の探索 */
SLAPredictorApiResult SLAOptimalEncodeEstimator_SearchOptimalBlockPartitions(
    struct SLAOptimalBlockPartitionEstimator* oee, 
    struct SLALPCCalculator* lpcc,
    const double* const* data, uint32_t num_channels, uint32_t num_samples,
    uint32_t min_num_block_samples, uint32_t delta_num_samples, uint32_t max_num_block_samples,
    uint32_t bits_par_sample, uint32_t parcor_order, 
    uint32_t* optimal_num_partitions, uint32_t* optimal_block_partition);

/* 最大分割数の取得 */
uint32_t SLAOptimalEncodeEstimator_CalculateMaxNumPartitions(
    uint32_t max_num_samples, uint32_t delta_num_samples);

#ifdef __cplusplus
}
#endif

#endif /* SLAPREDICTOR_H_INCLUDED */
