#ifndef SLACODER_H_INCLUDED
#define SLACODER_H_INCLUDED

#include "SLABitStream.h"
#include <stdint.h>

/* 符号化ハンドル */
struct SLACoder;

#ifdef __cplusplus
extern "C" {
#endif 

/* 符号化ハンドルの作成 */
struct SLACoder* SLACoder_Create(uint32_t max_num_channels, uint32_t max_num_parameters);

/* 符号化ハンドルの破棄 */
void SLACoder_Destroy(struct SLACoder* coder);

/* 初期パラメータの計算 */
void SLACoder_CalculateInitialRecursiveRiceParameter(
    struct SLACoder* coder, uint32_t num_parameters,
    const int32_t** data, uint32_t num_channels, uint32_t num_samples);

/* 再帰的ライス符号の初期パラメータを符号化 */
void SLACoder_PutInitialRecursiveRiceParameter(
    struct SLACoder* coder, struct SLABitStream* strm,
    uint32_t num_parameters, uint32_t bitwidth, uint32_t channel_index);

/* 再帰的ライス符号の初期パラメータを取得 */
void SLACoder_GetInitialRecursiveRiceParameter(
    struct SLACoder* coder, struct SLABitStream* strm,
    uint32_t num_parameters, uint32_t bitwidth, uint32_t channel_index);

/* 符号付き整数配列の符号化 */
void SLACoder_PutDataArray(
    struct SLACoder* coder, struct SLABitStream* strm,
    uint32_t num_parameters,
    const int32_t** data, uint32_t num_channels, uint32_t num_samples);

/* 符号付き整数配列の復号 */
void SLACoder_GetDataArray(
    struct SLACoder* coder, struct SLABitStream* strm,
    uint32_t num_parameters,
    int32_t** data, uint32_t num_channels, uint32_t num_samples);

#ifdef __cplusplus
}
#endif 

#endif /* SLACODER_H_INCLUDED */
