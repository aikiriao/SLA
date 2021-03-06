#ifndef SLAUTILITY_H_INCLUDED
#define SLAUTILITY_H_INCLUDED

#include "SLAStdint.h"

#if defined(__SSE4_1__)
/* SSE命令を使用した最適化コードを使用する */
#define USE_SSE
#include <x86intrin.h>
#endif

/* 円周率 */
#define SLA_PI              3.1415926535897932384626433832795029

/* 未使用引数 */
#define SLAUTILITY_UNUSED_ARGUMENT(arg)  ((void)(arg))
/* 算術右シフト */
#if ((((int32_t)-1) >> 1) == ((int32_t)-1))
/* 算術右シフトが有効な環境では、そのまま右シフト */
#define SLAUTILITY_SHIFT_RIGHT_ARITHMETIC(sint32, rshift) ((sint32) >> (rshift))
#else
/* 算術右シフトが無効な環境では、自分で定義する ハッカーのたのしみのより引用 */
/* 注意）有効範囲:0 <= rshift <= 32 */
#define SLAUTILITY_SHIFT_RIGHT_ARITHMETIC(sint32, rshift) ((((uint64_t)(sint32) + 0x80000000UL) >> (rshift)) - (0x80000000UL >> (rshift)))
#endif
/* 符号関数 ハッカーのたのしみより引用 補足）val==0の時は0を返す */
#define SLAUTILITY_SIGN(val)  (int32_t)((-(((uint32_t)(val)) >> 31)) | (((uint32_t)-(val)) >> 31))
/* 最大値の取得 */
#define SLAUTILITY_MAX(a,b) (((a) > (b)) ? (a) : (b))
/* 最小値の取得 */
#define SLAUTILITY_MIN(a,b) (((a) < (b)) ? (a) : (b))
/* 最小値以上最小値以下に制限 */
#define SLAUTILITY_INNER_VALUE(val, min, max) (SLAUTILITY_MIN((max), SLAUTILITY_MAX((min), (val))))
/* 2の冪乗か？ */
#define SLAUTILITY_IS_POWERED_OF_2(val) (!((val) & ((val) - 1)))
/* 符号付き32bit数値を符号なし32bit数値に一意変換 */
#define SLAUTILITY_SINT32_TO_UINT32(sint) (((int32_t)(sint) < 0) ? ((uint32_t)((-((sint) << 1)) - 1)) : ((uint32_t)(((sint) << 1))))
/* 符号なし32bit数値を符号付き32bit数値に一意変換 */
#define SLAUTILITY_UINT32_TO_SINT32(uint) ((int32_t)((uint) >> 1) ^ -(int32_t)((uint) & 1))
/* 絶対値の取得 */
#define SLAUTILITY_ABS(val)               (((val) > 0) ? (val) : -(val))
/* 32bit整数演算のための右シフト量を計算 */
#define SLAUTILITY_CALC_RSHIFT_FOR_SINT32(bitwidth) (((bitwidth) > 16) ? ((bitwidth) - 16) : 0)

/* NLZ（最上位ビットから1に当たるまでのビット数）の計算 */
#if defined(__GNUC__)
/* ビルトイン関数を使用 */
#define SLAUTILITY_NLZ(x) (((x) > 0) ? (uint32_t)__builtin_clz(x) : 32U)
#else
/* ソフトウェア実装を使用 */
#define SLAUTILITY_NLZ(x) SLAUtility_NLZSoft(x)
#endif

/* ceil(log2(val))の計算 */
#define SLAUTILITY_LOG2CEIL(x) (32U - SLAUTILITY_NLZ((uint32_t)((x) - 1U)))
/* floor(log2(val))の計算 */
#define SLAUTILITY_LOG2FLOOR(x) (31U - SLAUTILITY_NLZ(x))

/* 2の冪乗数(1,2,4,8,16,...)への切り上げ */
#if defined(__GNUC__)
/* ビルトイン関数を使用 */
#define SLAUTILITY_ROUNDUP2POWERED(x) (1U << SLAUTILITY_LOG2CEIL(x))
#else 
/* ソフトウェア実装を使用 */
#define SLAUTILITY_ROUNDUP2POWERED(x) SLAUtility_RoundUp2PoweredSoft(x)
#endif

/* データパケットキューのAPI結果 */
typedef enum SLADataPacketQueueApiResultTag {
  SLA_DATAPACKETQUEUE_APIRESULT_OK = 0,
  SLA_DATAPACKETQUEUE_APIRESULT_NG,
  SLA_DATAPACKETQUEUE_APIRESULT_EXCEED_MAX_NUM_DATA_FRAGMENTS,
  SLA_DATAPACKETQUEUE_APIRESULT_NO_DATA_FRAGMENTS
} SLADataPacketQueueApiResult;

#ifdef __cplusplus
extern "C" {
#endif

/* 窓の適用 */
void SLAUtility_ApplyWindow(const double* window, double* data, uint32_t num_samples);

/* 矩形窓を作成 */
void SLAUtility_MakeRectangularWindow(double* window, uint32_t window_size);

/* ハン窓を作成 */
void SLAUtility_MakeHannWindow(double* window, uint32_t window_size);

/* ブラックマン窓を作成 */
void SLAUtility_MakeBlackmanWindow(double* window, uint32_t window_size);

/* サイン窓を作成 */
void SLAUtility_MakeSinWindow(double* window, uint32_t window_size);

/* Vorbis窓を作成 */
void SLAUtility_MakeVorbisWindow(double* window, uint32_t window_size);

/* Tukey窓を作成 */
void SLAUtility_MakeTukeyWindow(double* window, uint32_t window_size, double alpha);

/* FFT */
void SLAUtility_FFT(double* data, uint32_t n, int32_t sign);

/* CRC16(CRC-IBM)の計算 */
uint16_t SLAUtility_CalculateCRC16(const uint8_t* data, uint64_t data_size);

/* NLZ（最上位ビットから1に当たるまでのビット数）の計算 */
uint32_t SLAUtility_NLZSoft(uint32_t val);

/* 2の冪乗に切り上げる */
uint32_t SLAUtility_RoundUp2PoweredSoft(uint32_t val);

/* LR -> MS（double） */
void SLAUtility_LRtoMSDouble(double **data, uint32_t num_channels, uint32_t num_samples);

/* LR -> MS（int32_t） */
void SLAUtility_LRtoMSInt32(int32_t **data, uint32_t num_channels, uint32_t num_samples);

/* MS -> LR（int32_t） */
void SLAUtility_MStoLRInt32(int32_t **data, uint32_t num_channels, uint32_t num_samples);

/* round関数（C89で定義されてない） */
double SLAUtility_Round(double d);

/* log2関数（C89で定義されていない） */
double SLAUtility_Log2(double x);

/* 連立一次方程式ソルバーの作成 */
struct SLALESolver* SLALESolver_Create(uint32_t max_dim);

/* 連立一次方程式ソルバーの破棄 */
void SLALESolver_Destroy(struct SLALESolver* lesolver);

/* LU分解（反復改良付き） 計算成功時は0を、失敗時はそれ以外を返す */
/* A:係数行列(dim x dim), b:右辺ベクトル(dim) -> 出力は解ベクトル, 
 * dim:次元, iteration_count:反復改良回数 */
int32_t SLALESolver_Solve(
    struct SLALESolver* lesolver,
    const double** A, double* b, uint32_t dim, uint32_t itration_count);

/* 入力データをもれなく表現できるビット幅の取得 */
uint32_t SLAUtility_GetDataBitWidth(
    const int32_t* data, uint32_t num_samples);

/* パケットキューの作成 */
struct SLADataPacketQueue* SLADataPacketQueue_Create(uint32_t max_num_packets);

/* パケットキューの破棄 */
void SLADataPacketQueue_Destroy(struct SLADataPacketQueue* queue);

/* データ片の追加 */
SLADataPacketQueueApiResult SLADataPacketQueue_EnqueueDataFragment(
    struct SLADataPacketQueue* queue, const uint8_t* data, uint32_t data_size);

/* データ片の読み出し */
SLADataPacketQueueApiResult SLADataPacketQueue_GetDataFragment(
    struct SLADataPacketQueue* queue, const uint8_t** data_ptr, uint32_t* data_size, uint32_t max_data_size);

/* 消費済みデータ片の回収 */
SLADataPacketQueueApiResult SLADataPacketQueue_DequeueDataFragment(
    struct SLADataPacketQueue* queue, const uint8_t** data_ptr, uint32_t* data_size);

/* キューに余っているデータサイズの取得 */
uint32_t SLADataPacketQueue_GetRemainDataSize(const struct SLADataPacketQueue* queue);

#ifdef __cplusplus
}
#endif

#endif /* SLAUTILITY_H_INCLUDED */
