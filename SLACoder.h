#ifndef SLACODER_H_INCLUDED
#define SLACODER_H_INCLUDED

#include "SLABitStream.h"
#include <stdint.h>

/* 再帰的ライス符号パラメータ型 */
typedef uint64_t SLARecursiveRiceParameter;

#ifdef __cplusplus
extern "C" {
#endif 

/* 初期パラメータの計算 */
void SLACoder_CalculateInitialRecursiveRiceParameter(
    SLARecursiveRiceParameter* rice_parameter, uint32_t num_parameters,
    const int32_t* data, uint32_t num_data);

/* 再帰的ライス符号のパラメータを符号化 */
void SLACoder_PutRecursiveRiceParameter(
    struct SLABitStream* strm,
    SLARecursiveRiceParameter* rice_parameter, uint32_t num_parameters,
    uint32_t bitwidth);

/* 再帰的ライス符号のパラメータを復号 */
void SLACoder_GetRecursiveRiceParameter(
    struct SLABitStream* strm,
    SLARecursiveRiceParameter* rice_parameter, uint32_t num_parameters,
    uint32_t bitwidth);

/* 符号付き整数配列の符号化 */
void SLACoder_PutDataArray(
    struct SLABitStream* strm, 
    SLARecursiveRiceParameter** rice_parameter, uint32_t num_parameters,
    const int32_t** data, uint32_t num_channels, uint32_t num_samples);

/* 符号付き整数配列の復号 */
void SLACoder_GetDataArray(
    struct SLABitStream* strm, 
    SLARecursiveRiceParameter** rice_parameter, uint32_t num_parameters,
    int32_t** data, uint32_t num_channels, uint32_t num_samples);

#ifdef __cplusplus
}
#endif 

#endif /* SLACODER_H_INCLUDED */
