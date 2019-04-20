#ifndef SLABITSTREAM_H_INCLUDED
#define SLABITSTREAM_H_INCLUDED

#include <stdint.h>
#include <stdio.h>

/* SLABitStream_Seek関数の探索コード */
#define SLABITSTREAM_SEEK_SET  SEEK_SET
#define SLABITSTREAM_SEEK_CUR  SEEK_CUR
#define SLABITSTREAM_SEEK_END  SEEK_END

/* ビットストリーム構造体 */
struct SLABitStream;

/* API結果型 */
typedef enum SLABitStreamApiResultTag {
  SLABITSTREAM_APIRESULT_OK = 0,           /* 成功                       */
  SLABITSTREAM_APIRESULT_NG,               /* 分類不能なエラー           */
  SLABITSTREAM_APIRESULT_INVALID_ARGUMENT, /* 不正な引数                 */
  SLABITSTREAM_APIRESULT_INVALID_MODE,     /* 不正なモード               */
  SLABITSTREAM_APIRESULT_IOERROR,          /* 分類不能なI/Oエラー        */
  SLABITSTREAM_APIRESULT_EOS               /* ストリーム終端に達している */
} SLABitStreamApiResult;

#ifdef __cplusplus
extern "C" {
#endif 

/* ビットストリーム構造体生成に必要なワークサイズ計算 */
int32_t SLABitStream_CalculateWorkSize(void);

/* ビットストリームのオープン */
struct SLABitStream* SLABitStream_Open(const char* filepath, 
    const char *mode, void *work, int32_t work_size);

/* メモリビットストリームのオープン */
struct SLABitStream* SLABitStream_OpenMemory(
    uint8_t* memory_image, uint64_t memory_size,
    const char* mode, void *work, int32_t work_size);

/* ビットストリームのクローズ */
void SLABitStream_Close(struct SLABitStream* stream);

/* シーク(fseek準拠)
 * 注意）バッファをクリアするので副作用がある */
SLABitStreamApiResult SLABitStream_Seek(struct SLABitStream* stream, int32_t offset, int32_t wherefrom);

/* 現在位置(ftell)準拠 */
SLABitStreamApiResult SLABitStream_Tell(struct SLABitStream* stream, int32_t* result);

/* 1bit出力 */
SLABitStreamApiResult SLABitStream_PutBit(struct SLABitStream* stream, uint8_t bit);

/*
 * valの右側（下位）n_bits 出力（最大64bit出力可能）
 * SLABitStream_PutBits(stream, 3, 6);は次と同じ:
 * SLABitStream_PutBit(stream, 1); SLABitStream_PutBit(stream, 1); SLABitStream_PutBit(stream, 0); 
 */
SLABitStreamApiResult SLABitStream_PutBits(struct SLABitStream* stream, uint32_t n_bits, uint64_t val);

/* 1bit取得 */
SLABitStreamApiResult SLABitStream_GetBit(struct SLABitStream* stream, uint8_t* bit);

/* n_bits 取得（最大64bit）し、その値を右詰めして出力 */
SLABitStreamApiResult SLABitStream_GetBits(struct SLABitStream* stream, uint32_t n_bits, uint64_t *val);

/* バッファにたまったビットをクリア */
SLABitStreamApiResult SLABitStream_Flush(struct SLABitStream* stream);

#ifdef __cplusplus
}
#endif 

#endif /* SLABITSTREAM_H_INCLUDED */
