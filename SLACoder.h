#ifndef SLACODER_H_INCLUDED
#define SLACODER_H_INCLUDED

#include "SLABitStream.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif 

/* 符号付き整数配列の符号化 */
void SLACoder_PutDataArray(
    struct SLABitStream* strm, const int32_t* data, uint32_t num_data);

/* 符号付き整数配列の復号 */
void SLACoder_GetDataArray(
    struct SLABitStream* strm, int32_t* data, uint32_t num_data);

#ifdef __cplusplus
}
#endif 

#endif /* SLACODER_H_INCLUDED */
