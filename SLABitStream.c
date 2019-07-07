#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "SLABitStream.h"
#include "SLAInternal.h"

/* アラインメント */
#define SLABITSTREAM_ALIGNMENT                   16
/* 読みモードか？（0で書きモード） */
#define SLABITSTREAM_FLAGS_FILEOPENMODE_READ     (1 << 0)
/* メモリ操作モードか否か？ */
#define SLABITSTREAM_FLAGS_READWRITE_ON_MEMORY   (1 << 1)
/* メモリはワーク渡しか？（1:ワーク渡し, 0:mallocで自前確保） */
#define SLABITSTREAM_FLAGS_MEMORYALLOC_BYWORK    (1 << 2)

/* 下位n_bitsを取得 */
/* 補足）((1UL << (n_bits)) - 1)は下位の数値だけ取り出すマスクになる */
#define SLABITSTREAM_GETLOWERBITS(n_bits, val) ((val) & st_lowerbits_mask[(n_bits)])

/* 内部エラー型 */
typedef enum SLABitStreamErrorTag {
  SLABITSTREAM_ERROR_OK = 0,
  SLABITSTREAM_ERROR_NG,
  SLABITSTREAM_ERROR_EOS,
  SLABITSTREAM_ERROR_IOERROR
} SLABitStreamError;

/* ストリーム */
typedef union StreamTag {
  FILE* fp;                   /* ファイル構造体   */
  struct {
    uint8_t*  memory_image;   /* メモリイメージ   */
    uint64_t  memory_size;    /* メモリサイズ     */
    uint64_t  memory_p;       /* 読み書き参照位置 */
  } mem;
} Stream;

/* ストリームに対する操作インターフェース */
typedef struct SLABitStreamInterfaceTag {
  /* 1文字読み関数 */
  SLABitStreamError (*SGetChar)(Stream* strm, int32_t* character);
  /* 1文字書き関数 */
  SLABitStreamError (*SPutChar)(Stream* strm, int32_t  character);
  /* シーク関数 */
  SLABitStreamError (*SSeek)(Stream* strm, int32_t offset, int32_t wherefrom);
  /* tell関数 */
  SLABitStreamError (*STell)(const Stream* strm, int32_t* result);
} SLABitStreamInterface;

/* ビットストリーム構造体 */
struct SLABitStream {
  Stream                        stm;        /* ストリーム                 */
  const SLABitStreamInterface*  stm_if;     /* ストリームインターフェース */
  uint8_t                       flags;      /* 内部状態フラグ             */
  uint32_t                      bit_buffer; /* 内部ビット入出力バッファ   */
  uint32_t                      bit_count;  /* 内部ビット入出力カウント   */
  void*                         work_ptr;   /* ワーク領域先頭ポインタ     */
};

/* オープン関数の共通ルーチン */
static struct SLABitStream* SLABitStream_OpenCommon(
    const char* mode, void *work, int32_t work_size);

/* ファイルに対する一文字取得 */
static SLABitStreamError SLABitStream_FGet(Stream* strm, int32_t* ch);
/* ファイルに対する一文字書き込み */
static SLABitStreamError SLABitStream_FPut(Stream* strm, int32_t ch);
/* ファイルに対するSeek関数 */
static SLABitStreamError SLABitStream_FSeek(Stream* strm, int32_t offset, int32_t wherefrom);
/* ファイルに対するTell関数 */
static SLABitStreamError SLABitStream_FTell(const Stream* strm, int32_t* result);
/* メモリに対する一文字取得 */
static SLABitStreamError SLABitStream_MGet(Stream* strm, int32_t* ch);
/* メモリに対する一文字書き込み */
static SLABitStreamError SLABitStream_MPut(Stream* strm, int32_t ch);
/* メモリに対するSeek関数 */
static SLABitStreamError SLABitStream_MSeek(Stream* strm, int32_t offset, int32_t wherefrom);
/* メモリに対するTell関数 */
static SLABitStreamError SLABitStream_MTell(const Stream* strm, int32_t* result);

/* ファイルIO用のインターフェース */
static const SLABitStreamInterface st_filestream_io_if = {
   SLABitStream_FGet,
   SLABitStream_FPut,
   SLABitStream_FSeek,
   SLABitStream_FTell,
};

/* メモリIO用のインターフェース */
static const SLABitStreamInterface st_memstream_io_if = {
   SLABitStream_MGet,
   SLABitStream_MPut,
   SLABitStream_MSeek,
   SLABitStream_MTell,
};

/* 下位ビットを取り出すマスク 32bitまで */
static const uint32_t st_lowerbits_mask[33] = {
  0x00000000UL,
  0x00000001UL, 0x00000003UL, 0x00000007UL, 0x0000000FUL,
  0x0000001FUL, 0x0000003FUL, 0x0000007FUL, 0x000000FFUL,
  0x000001FFUL, 0x000003FFUL, 0x000007FFUL, 0x00000FFFUL,
  0x00001FFFUL, 0x00003FFFUL, 0x00007FFFUL, 0x0000FFFFUL,
  0x0001FFFFUL, 0x0003FFFFUL, 0x0007FFFFUL, 0x000FFFFFUL, 
  0x001FFFFFUL, 0x300FFFFFUL, 0x007FFFFFUL, 0x00FFFFFFUL,
  0x01FFFFFFUL, 0x03FFFFFFUL, 0x07FFFFFFUL, 0x0FFFFFFFUL, 
  0x1FFFFFFFUL, 0x3FFFFFFFUL, 0x7FFFFFFFUL, 0xFFFFFFFFUL
};

/* 0のラン長パターンテーブル（注意：上位ビットからのラン長） */
static const uint8_t st_zerobit_runlength_table[0x100] = {
  8, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4, 
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* ファイルに対する一文字取得 */
static SLABitStreamError SLABitStream_FGet(Stream* strm, int32_t* ch)
{
  int32_t ret;

  SLA_Assert(strm != NULL && ch != NULL);

  /* getcを発行 */
  if ((ret = getc(strm->fp)) == EOF) {
    return SLABITSTREAM_ERROR_EOS;
  }

  (*ch) = ret;
  return SLABITSTREAM_ERROR_OK;
}

/* ファイルに対する一文字書き込み */
static SLABitStreamError SLABitStream_FPut(Stream* strm, int32_t ch)
{
  SLA_Assert(strm != NULL);
  if (fputc(ch, strm->fp) == EOF) {
    return SLABITSTREAM_ERROR_IOERROR;
  }
  return SLABITSTREAM_ERROR_OK;
}

/* ファイルに対するシーク関数 */
static SLABitStreamError SLABitStream_FSeek(Stream* strm, int32_t offset, int32_t wherefrom)
{
  SLA_Assert(strm != NULL);
  return (fseek(strm->fp, offset, wherefrom) == 0) ? SLABITSTREAM_ERROR_OK : SLABITSTREAM_ERROR_IOERROR;
}

/* ファイルに対するTell関数 */
static SLABitStreamError SLABitStream_FTell(const Stream* strm, int32_t* result)
{
  SLA_Assert(strm != NULL);
  /* ftell実行 */
  *result = (int32_t)ftell(strm->fp);
  return ((*result != -1) ? SLABITSTREAM_ERROR_OK : SLABITSTREAM_ERROR_IOERROR);
}

/* メモリに対する一文字取得 */
static SLABitStreamError SLABitStream_MGet(Stream* strm, int32_t* ch)
{
  SLA_Assert(strm != NULL && ch != NULL);

  /* 終端に達している */
  if (strm->mem.memory_p >= strm->mem.memory_size) {
    return SLABITSTREAM_ERROR_EOS;
  }

  /* メモリから読み出し */
  (*ch) = strm->mem.memory_image[strm->mem.memory_p];
  strm->mem.memory_p++;
  return SLABITSTREAM_ERROR_OK;
}

/* メモリに対する一文字書き込み */
static SLABitStreamError SLABitStream_MPut(Stream* strm, int32_t ch)
{
  SLA_Assert(strm != NULL);

  /* 終端に達している */
  if (strm->mem.memory_p >= strm->mem.memory_size) {
    return SLABITSTREAM_ERROR_EOS;
  }

  /* メモリに書き出し */
  strm->mem.memory_image[strm->mem.memory_p] = (uint8_t)(ch & 0xFF);
  strm->mem.memory_p++;
  return SLABITSTREAM_ERROR_OK;
}

/* メモリに対するSeek関数 */
static SLABitStreamError SLABitStream_MSeek(Stream* strm, int32_t offset, int32_t wherefrom)
{
  int64_t pos;

  SLA_Assert(strm != NULL);

  /* オフセットをまず定める */
  switch (offset) {
    case SLABITSTREAM_SEEK_CUR:
      pos = (int64_t)strm->mem.memory_p;
      break;
    case SLABITSTREAM_SEEK_SET:
      pos = 0;
      break;
    case SLABITSTREAM_SEEK_END:
      pos = (int64_t)(strm->mem.memory_size - 1);
      break;
    default:
      return SLABITSTREAM_ERROR_IOERROR;
  }

  /* 動かす */
  pos += wherefrom;

  /* 範囲チェック */
  if (pos >= (int64_t)strm->mem.memory_size || pos < 0) {
    return SLABITSTREAM_ERROR_NG;
  }

  strm->mem.memory_p = (uint64_t)pos;
  return SLABITSTREAM_ERROR_OK;
}

/* メモリに対するTell関数 */
static SLABitStreamError SLABitStream_MTell(const Stream* strm, int32_t* result)
{
  SLA_Assert(strm != NULL && result != NULL);

  /* 位置を返すだけ */
  (*result) = (int32_t)strm->mem.memory_p;
  return SLABITSTREAM_ERROR_OK;
}

/* ワークサイズの取得 */
int32_t SLABitStream_CalculateWorkSize(void)
{
  return (sizeof(struct SLABitStream) + SLABITSTREAM_ALIGNMENT);
}

/* メモリビットストリームのオープン */
struct SLABitStream* SLABitStream_OpenMemory(
    uint8_t* memory_image, uint64_t memory_size,
    const char* mode, void *work, int32_t work_size)
{
  struct SLABitStream* stream;

  /* 引数チェック */
  if (memory_image == NULL) {
    return NULL;
  }

  /* 共通インスタンス作成ルーチンを通す */
  stream = SLABitStream_OpenCommon(mode, work, work_size);
  if (stream == NULL) {
    return NULL;
  }

  /* このストリームはメモリ上 */
  stream->flags     |= SLABITSTREAM_FLAGS_READWRITE_ON_MEMORY;

  /* インターフェースのセット */
  stream->stm_if    = &st_memstream_io_if;

  /* メモリセット */
  stream->stm.mem.memory_image = memory_image;
  stream->stm.mem.memory_size  = memory_size;

  /* 内部状態初期化 */
  stream->stm.mem.memory_p = 0;
  stream->bit_buffer  = 0;

  return stream;
}

/* ビットストリームのオープン */
struct SLABitStream* SLABitStream_Open(const char* filepath,
    const char* mode, void *work, int32_t work_size)
{
  struct SLABitStream* stream;
  FILE* tmp_fp;

  /* 共通インスタンス作成ルーチンを通す */
  stream = SLABitStream_OpenCommon(mode, work, work_size);
  if (stream == NULL) {
    return NULL;
  }

  /* このストリームはファイル上 */
  stream->flags     &= (uint8_t)(~SLABITSTREAM_FLAGS_READWRITE_ON_MEMORY);

  /* インターフェースのセット */
  stream->stm_if    = &st_filestream_io_if;

  /* ファイルオープン */
  tmp_fp = fopen(filepath, mode);
  if (tmp_fp == NULL) {
    return NULL;
  }
  stream->stm.fp = tmp_fp;

  /* 内部状態初期化 */
  fseek(stream->stm.fp, SEEK_SET, 0);
  stream->bit_buffer  = 0;

  return stream;
}

/* オープン関数の共通ルーチン */
static struct SLABitStream* SLABitStream_OpenCommon(
    const char* mode, void *work, int32_t work_size)
{
  struct SLABitStream* stream;
  int8_t            is_malloc_by_work = 0;
  uint8_t*          work_ptr = (uint8_t *)work;

  /* 引数チェック */
  if ((mode == NULL)
      || (work_size < 0)
      || ((work != NULL) && (work_size < SLABitStream_CalculateWorkSize()))) {
    return NULL;
  }

  /* ワーク渡しか否か？ */
  if ((work == NULL) && (work_size == 0)) {
    is_malloc_by_work = 0;
    work = (uint8_t *)malloc((size_t)SLABitStream_CalculateWorkSize());
    if (work == NULL) {
      return NULL;
    }
  } else {
    is_malloc_by_work = 1;
  }

  /* アラインメント切り上げ */
  work_ptr = (uint8_t *)(((uintptr_t)work + (SLABITSTREAM_ALIGNMENT-1)) & ~(uintptr_t)(SLABITSTREAM_ALIGNMENT-1));

  /* 構造体の配置 */
  stream            = (struct SLABitStream *)work_ptr;
  work_ptr          += sizeof(struct SLABitStream);
  stream->work_ptr  = work;
  stream->flags     = 0;

  /* モードの1文字目でオープンモードを確定
   * 内部カウンタもモードに合わせて設定 */
  switch (mode[0]) {
    case 'r':
      stream->flags     |= SLABITSTREAM_FLAGS_FILEOPENMODE_READ;
      stream->bit_count = 0;
      break;
    case 'w':
      stream->flags     &= (uint8_t)(~SLABITSTREAM_FLAGS_FILEOPENMODE_READ);
      stream->bit_count = 8;
      break;
    default:
      return NULL;
  }

  /* メモリアロケート方法を記録 */
  if (is_malloc_by_work != 0) {
    stream->flags |= SLABITSTREAM_FLAGS_MEMORYALLOC_BYWORK;
  }

  return stream;
}

/* ビットストリームのクローズ */
void SLABitStream_Close(struct SLABitStream* stream)
{
  /* 引数チェック */
  if (stream == NULL) {
    return;
  }

  /* バッファのクリア */
  SLABitStream_Flush(stream);

  if (!(stream->flags & SLABITSTREAM_FLAGS_READWRITE_ON_MEMORY)) {
    /* ファイルハンドルクローズ */
    fclose(stream->stm.fp);
  }

  /* 必要ならばメモリ解放 */
  if (!(stream->flags & SLABITSTREAM_FLAGS_MEMORYALLOC_BYWORK)) {
    free(stream->work_ptr);
    stream->work_ptr = NULL;
  }
}

/* シーク(fseek準拠) */
SLABitStreamApiResult SLABitStream_Seek(struct SLABitStream* stream, int32_t offset, int32_t wherefrom)
{
  /* 引数チェック */
  if (stream == NULL) {
    return SLABITSTREAM_APIRESULT_INVALID_ARGUMENT;
  }

  /* 内部バッファをクリア（副作用が起こる） */
  if (SLABitStream_Flush(stream) != SLABITSTREAM_APIRESULT_OK) {
    return SLABITSTREAM_APIRESULT_NG;
  }

  /* シーク実行 */
  if (stream->stm_if->SSeek(
        &(stream->stm), offset, wherefrom) != SLABITSTREAM_ERROR_OK) {
    return SLABITSTREAM_APIRESULT_NG;
  }

  return SLABITSTREAM_APIRESULT_OK;
}

/* 現在位置(ftell)準拠 */
SLABitStreamApiResult SLABitStream_Tell(struct SLABitStream* stream, int32_t* result)
{
  SLABitStreamError err;

  /* 引数チェック */
  if (stream == NULL || result == NULL) {
    return SLABITSTREAM_APIRESULT_INVALID_ARGUMENT;
  }

  /* ftell実行 */
  err = stream->stm_if->STell(&(stream->stm), result);

  return (err == SLABITSTREAM_ERROR_OK) ? SLABITSTREAM_APIRESULT_OK : SLABITSTREAM_APIRESULT_NG;
}

/* 1bit出力 */
SLABitStreamApiResult SLABitStream_PutBit(struct SLABitStream* stream, uint8_t bit)
{
  /* 引数チェック */
  if (stream == NULL) {
    return SLABITSTREAM_APIRESULT_INVALID_ARGUMENT;
  }

  /* 読み込みモードでは実行不可能 */
  if (stream->flags & SLABITSTREAM_FLAGS_FILEOPENMODE_READ) {
    return SLABITSTREAM_APIRESULT_INVALID_MODE;
  }

  /* バイト出力するまでのカウントを減らす */
  stream->bit_count--;

  /* ビット出力バッファに値を格納 */
  if (bit != 0) {
    stream->bit_buffer |= (uint8_t)(1 << stream->bit_count);
  }

  /* バッファ出力・更新 */
  if (stream->bit_count == 0) {
    if (stream->stm_if->SPutChar(
          &(stream->stm), (int32_t)stream->bit_buffer) != SLABITSTREAM_ERROR_OK) {
      return SLABITSTREAM_APIRESULT_IOERROR;
    }
    stream->bit_buffer = 0;
    stream->bit_count  = 8;
  }

  return SLABITSTREAM_APIRESULT_OK;
}

/*
 * valの右側（下位）n_bits 出力（最大64bit出力可能）
 * SLABitStream_PutBits(stream, 3, 6);は次と同じ:
 * SLABitStream_PutBit(stream, 1); SLABitStream_PutBit(stream, 1); SLABitStream_PutBit(stream, 0); 
 */
SLABitStreamApiResult SLABitStream_PutBits(struct SLABitStream* stream, uint32_t n_bits, uint64_t val)
{
  /* 引数チェック */
  if (stream == NULL) {
    return SLABITSTREAM_APIRESULT_INVALID_ARGUMENT;
  }

  /* 読み込みモードでは実行不可能 */
  if (stream->flags & SLABITSTREAM_FLAGS_FILEOPENMODE_READ) {
    return SLABITSTREAM_APIRESULT_INVALID_MODE;
  }

  /* 出力可能な最大ビット数を越えている */
  if (n_bits > sizeof(uint64_t) * 8) {
    return SLABITSTREAM_APIRESULT_INVALID_ARGUMENT;
  }

  /* 0ビット出力では何もしない */
  if (n_bits == 0) {
    return SLABITSTREAM_APIRESULT_OK;
  }

  /* valの上位ビットから順次出力
   * 初回ループでは端数（出力に必要なビット数）分を埋め出力
   * 2回目以降は8bit単位で出力 */
  while (n_bits >= stream->bit_count) {
    n_bits              = n_bits - stream->bit_count;
    stream->bit_buffer  |= (uint32_t)SLABITSTREAM_GETLOWERBITS(stream->bit_count, val >> n_bits);
    if (stream->stm_if->SPutChar(
          &(stream->stm), (int32_t)stream->bit_buffer) != SLABITSTREAM_ERROR_OK) {
      return SLABITSTREAM_APIRESULT_IOERROR;
    }
    stream->bit_buffer  = 0;
    stream->bit_count   = 8;
  }

  /* 端数ビットの処理:
   * 残った分をバッファの上位ビットにセット */
  SLA_Assert(n_bits <= 8);
  stream->bit_count   -= n_bits;
  stream->bit_buffer  |= (uint32_t)SLABITSTREAM_GETLOWERBITS(n_bits, val) << stream->bit_count;

  return SLABITSTREAM_APIRESULT_OK;
}

/* 1bit取得 */
SLABitStreamApiResult SLABitStream_GetBit(struct SLABitStream* stream, uint8_t* bit)
{
  int32_t ch;
  SLABitStreamError err;

  /* 引数チェック */
  if (stream == NULL || bit == NULL) {
    return SLABITSTREAM_APIRESULT_INVALID_ARGUMENT;
  }

  /* 読み込みモードでない場合は即時リターン */
  if (!(stream->flags & SLABITSTREAM_FLAGS_FILEOPENMODE_READ)) {
    return SLABITSTREAM_APIRESULT_INVALID_MODE;
  }

  /* 入力ビットカウントを1減らし、バッファの対象ビットを出力 */
  if (stream->bit_count > 0) {
    stream->bit_count--;
    (*bit) = (stream->bit_buffer >> stream->bit_count) & 1;
    /* (*bit) = (stream->bit_buffer & st_bit_mask[stream->bit_count]); */
    return SLABITSTREAM_APIRESULT_OK;
  }

  /* 1バイト読み込みとエラー処理 */
  if ((err = stream->stm_if->SGetChar(
          &(stream->stm), &ch)) != SLABITSTREAM_ERROR_OK) {
    switch (err) {
      case SLABITSTREAM_ERROR_EOS:
        return SLABITSTREAM_APIRESULT_EOS;
      default:
        return SLABITSTREAM_APIRESULT_IOERROR;
    }
  }
  SLA_Assert(ch >= 0);
  SLA_Assert(ch <= 0xFF);

  /* カウンタとバッファの更新 */
  stream->bit_count   = 7;
  stream->bit_buffer  = (uint32_t)ch;

  /* 取得したバッファの最上位ビットを出力 */
  (*bit) = (stream->bit_buffer >> 7) & 1;
  /* (*bit) = (stream->bit_buffer & st_bit_mask[7]); */

  return SLABITSTREAM_APIRESULT_OK;
}

/* n_bits 取得（最大64bit）し、その値を右詰めして出力 */
SLABitStreamApiResult SLABitStream_GetBits(struct SLABitStream* stream, uint32_t n_bits, uint64_t *val)
{
  int32_t  ch;
  uint64_t tmp = 0;
  SLABitStreamError err;

  /* 引数チェック */
  if (stream == NULL || val == NULL) {
    return SLABITSTREAM_APIRESULT_INVALID_ARGUMENT;
  }

  /* 読み込みモードでない場合は即時リターン */
  if (!(stream->flags & SLABITSTREAM_FLAGS_FILEOPENMODE_READ)) {
    return SLABITSTREAM_APIRESULT_INVALID_MODE;
  }

  /* 入力可能な最大ビット数を越えている */
  if (n_bits > sizeof(uint64_t) * 8) {
    return SLABITSTREAM_APIRESULT_INVALID_ARGUMENT;
  }

  /* 最上位ビットからデータを埋めていく
   * 初回ループではtmpの上位ビットにセット
   * 2回目以降は8bit単位で入力しtmpにセット */
  while (n_bits > stream->bit_count) {
    n_bits  -= stream->bit_count;
    tmp     |= SLABITSTREAM_GETLOWERBITS(stream->bit_count, stream->bit_buffer) << n_bits;
    /* 1バイト読み込みとエラー処理 */
    err = stream->stm_if->SGetChar(&(stream->stm), &ch);
    SLA_Assert(ch >= 0);
    SLA_Assert(ch <= 0xFF);
    switch (err) {
      case SLABITSTREAM_APIRESULT_OK:
        break;
      case SLABITSTREAM_ERROR_EOS:
        /* ファイル終端であればループを抜ける */
        goto END_OF_STREAM;
      case SLABITSTREAM_ERROR_IOERROR:  /* FALLTRHU */
      default:
        return SLABITSTREAM_APIRESULT_IOERROR;
    }
    stream->bit_buffer  = (uint32_t)ch;
    stream->bit_count   = 8;
  }

END_OF_STREAM:
  /* 端数ビットの処理 
   * 残ったビット分をtmpの最上位ビットにセット */
  stream->bit_count -= n_bits;
  tmp               |= (uint64_t)SLABITSTREAM_GETLOWERBITS(n_bits, (uint32_t)(stream->bit_buffer >> stream->bit_count));

  /* 正常終了 */
  *val = tmp;
  return SLABITSTREAM_APIRESULT_OK;
}

/* つぎの1にぶつかるまで読み込み、その間に読み込んだ0のランレングスを取得 */
SLABitStreamApiResult SLABitStream_GetZeroRunLength(struct SLABitStream* stream, uint32_t* runlength)
{
  uint32_t  run, rshift, mask, table_index;

  /* 引数チェック */
  if (stream == NULL || runlength == NULL) {
    return SLABITSTREAM_APIRESULT_INVALID_ARGUMENT;
  }

  /* ビットバッファに残っているデータを読み取るための
   * 右シフト量とマスクを作成 */
  rshift = 8 - stream->bit_count;
  mask   = (1U << rshift) - 1;

  /* テーブル参照 / ラン取得 */
  table_index         = (stream->bit_buffer << rshift) | mask;
  run                 = st_zerobit_runlength_table[0xFF & table_index];
  stream->bit_count   -= run;

  /* バッファが空の時 */
  while (stream->bit_count == 0) {
    /* 1バイト読み込みとエラー処理 */
    int32_t   ch = -1;
    uint32_t  tmp_run;
    SLABitStreamError err;
    err = stream->stm_if->SGetChar(&(stream->stm), &ch);
    switch (err) {
      case SLABITSTREAM_APIRESULT_OK:
        break;
      case SLABITSTREAM_ERROR_EOS:
        /* ファイル終端であればループを抜ける */
        goto END_OF_STREAM;
      case SLABITSTREAM_ERROR_IOERROR:  /* FALLTRHU */
      default:
        return SLABITSTREAM_APIRESULT_IOERROR;
    }

    SLA_Assert(ch >= 0);
    SLA_Assert(ch <= 0xFF);
    /* ビットバッファにセットし直して再度ランを計測 */
    stream->bit_buffer  = (uint32_t)ch;
    tmp_run             = st_zerobit_runlength_table[ch];
    stream->bit_count   = 8 - tmp_run;
    /* ランを加算 */
    run                 += tmp_run;
  }

  /* 続く1を空読み */
  stream->bit_count -= 1;

END_OF_STREAM:
  /* 正常終了 */
  *runlength = run;
  return SLABITSTREAM_APIRESULT_OK;
}

/* バッファにたまったビットをクリア */
SLABitStreamApiResult SLABitStream_Flush(struct SLABitStream* stream)
{
  /* 引数チェック */
  if (stream == NULL) {
    return SLABITSTREAM_APIRESULT_INVALID_ARGUMENT;
  }

  /* 既に先頭にあるときは何もしない */
  if (stream->bit_count == 8) {
    return SLABITSTREAM_APIRESULT_OK;
  }

  /* 読み込み位置を次のバイト先頭に */
  if (stream->flags & SLABITSTREAM_FLAGS_FILEOPENMODE_READ) {
    /* 残りビット分を空読み */
    uint64_t dummy;
    return SLABitStream_GetBits(stream, (uint32_t)stream->bit_count, &dummy);
  } else {
    /* バッファに余ったビットを強制出力 */
    return SLABitStream_PutBits(stream, (uint16_t)stream->bit_count, 0);
  }
}
