#ifndef SLAUTILITY_H_INCLUDED
#define SLAUTILITY_H_INCLUDED

#include <stdint.h>

/* 円周率 */
#define SLA_PI              3.1415926535897932384626433832795029

/* 算術右シフト */
#if ((((int32_t)-1) >> 1) == ((int32_t)-1))
/* 算術右シフトが有効な環境では、そのまま右シフト */
#define SLAUTILITY_SHIFT_RIGHT_ARITHMETIC(sint32, rshift) ((int64_t)(sint32) >> (rshift))
#else
/* 算術右シフトが無効な環境では、自分で定義する */
/* 注意）有効範囲:0 <= rshift <= 32 */
#define SLAUTILITY_SHIFT_RIGHT_ARITHMETIC(sint32, rshift) ((((uint64_t)(sint32) + 0x80000000UL) >> (rshift)) - (0x80000000UL >> (rshift)))
#endif
/* 符号関数 補足）val==0の時は0を返す */
#define SLAUTILITY_SIGN(val)  (int32_t)((-(((uint32_t)(val)) >> 31)) | (((uint32_t)-(val)) >> 31))
/* 最大値の取得 */
#define SLAUTILITY_MAX(a,b) (((a) > (b)) ? (a) : (b))
/* 最小値の取得 */
#define SLAUTILITY_MIN(a,b) (((a) < (b)) ? (a) : (b))
/* 2の冪乗か？ */
#define SLAUTILITY_IS_POWERED_OF_2(val) (!((val) & ((val) - 1)))
/* 符号付き32bit数値を符号なし32bit数値に一意変換 */
#define SLAUTILITY_SINT32_TO_UINT32(sint) (((int32_t)(sint) <= 0) ? ((uint32_t)(-((sint) << 1))) : ((uint32_t)(((sint) << 1) - 1)))
/* 符号なし32bit数値を符号付き32bit数値に一意変換 */
#define SLAUTILITY_UINT32_TO_SINT32(uint) (((uint32_t)(uint) & 1) ? ((int32_t)((uint) >> 1) + 1) : (-(int32_t)((uint) >> 1)))

#ifdef __cplusplus
extern "C" {
#endif

/* 窓の適用 */
void SLAUtility_ApplyWindow(const double* window, float* data, uint32_t num_samples);

/* ハン窓を作成 */
void SLAUtility_MakeHannWindow(double* window, uint32_t window_size);

/* ブラックマン窓を作成 */
void SLAUtility_MakeBlackmanWindow(double* window, uint32_t window_size);

/* サイン窓を作成 */
void SLAUtility_MakeSinWindow(double* window, uint32_t window_size);

/* Vorbis窓を作成 */
void SLAUtility_MakeVorbisWindow(double* window, uint32_t window_size);

/* Tukey窓を作成 */
void SLAUtility_MakeTukeyWindow(double* window, uint32_t window_size, float alpha);

/* FFT */
void SLAUtility_FFT(double* data, uint32_t n, int32_t sign);

/* CRC16(CRC-IBM)の計算 */
uint16_t SLAUtility_CalculateCRC16(const uint8_t* data, uint64_t data_size);

/* ceil(log2(val))の計算 */
uint32_t SLAUtility_Log2Ceil(uint32_t val);

/* 2の冪乗に切り上げる */
uint32_t SLAUtility_RoundUp2Powered(uint32_t val);

/* LR -> MS（float） */
void SLAUtility_LRtoMSFloat(float **data, uint32_t num_samples);

/* LR -> MS（int32_t） */
void SLAUtility_LRtoMSInt32(int32_t **data, uint32_t num_samples);

/* MS -> LR（int32_t） */
void SLAUtility_MStoLRInt32(int32_t **data, uint32_t num_samples);

/* round関数（C89で定義されてない） */
double SLAUtility_Round(double d);

/* log2関数（C89で定義されていない） */
double SLAUtility_Log2(double x);

/* プリエンファシス(float) */
void SLAUtility_PreEmphasisFloat(float* data, uint32_t num_samples, int32_t coef_shift);

/* プリエンファシス(int32) */
void SLAUtility_PreEmphasisInt32(int32_t* data, uint32_t num_samples, int32_t coef_shift);

/* デエンファシス(int32) */
void SLAUtility_DeEmphasisInt32(int32_t* data, uint32_t num_samples, int32_t coef_shift);

#ifdef __cplusplus
}
#endif

#endif /* SLAUTILITY_H_INCLUDED */
