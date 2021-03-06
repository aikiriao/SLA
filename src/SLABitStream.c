#include "SLABitStream.h"
#include "SLAStdint.h"
#include "SLAInternal.h"
#include "SLAUtility.h"

#include <string.h>
#include <stdlib.h>

/* 下位ビットを取り出すマスク 32bitまで */
const uint32_t g_sla_bitstream_lower_bits_mask[33] = {
  0x00000000UL,
  0x00000001UL, 0x00000003UL, 0x00000007UL, 0x0000000FUL,
  0x0000001FUL, 0x0000003FUL, 0x0000007FUL, 0x000000FFUL,
  0x000001FFUL, 0x000003FFUL, 0x000007FFUL, 0x00000FFFUL,
  0x00001FFFUL, 0x00003FFFUL, 0x00007FFFUL, 0x0000FFFFUL,
  0x0001FFFFUL, 0x0003FFFFUL, 0x0007FFFFUL, 0x000FFFFFUL, 
  0x001FFFFFUL, 0x000FFFFFUL, 0x007FFFFFUL, 0x00FFFFFFUL,
  0x01FFFFFFUL, 0x03FFFFFFUL, 0x07FFFFFFUL, 0x0FFFFFFFUL, 
  0x1FFFFFFFUL, 0x3FFFFFFFUL, 0x7FFFFFFFUL, 0xFFFFFFFFUL
};

/* 0のラン長パターンテーブル（注意：上位ビットからのラン長） */
const uint32_t g_sla_bitstream_zerobit_runlength_table[0x100] = {
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

#if !defined(SLABITSTREAM_PROCESS_BY_MACRO) 

/* ビットリーダのオープン */
void SLABitReader_Open(struct SLABitStream* stream,
    uint8_t* memory_image, size_t memory_size)
{
  SLA_Assert(stream != NULL);
  SLA_Assert(memory_image != NULL);

  /* 内部状態リセット */
  stream->flags = 0;

  /* バッファ初期化 */
  stream->bit_count   = 0;
  stream->bit_buffer  = 0;

  /* メモリセット */
  stream->memory_image = memory_image;
  stream->memory_size  = memory_size;

  /* 読み出し位置は先頭に */
  stream->memory_p = memory_image;

  /* 読みモードとしてセット */
  stream->flags |= (uint8_t)SLABITSTREAM_FLAGS_MODE_READ;
}

/* ビットライタのオープン */
void SLABitWriter_Open(struct SLABitStream* stream,
    uint8_t* memory_image, size_t memory_size)
{
  SLA_Assert(stream != NULL);
  SLA_Assert(memory_image != NULL);

  /* 内部状態リセット */
  stream->flags = 0;

  /* バッファ初期化 */
  stream->bit_count   = 8;
  stream->bit_buffer  = 0;

  /* メモリセット */
  stream->memory_image = memory_image;
  stream->memory_size  = memory_size;

  /* 読み出し位置は先頭に */
  stream->memory_p = memory_image;

  /* 書きモードとしてセット */
  stream->flags &= (uint8_t)(~SLABITSTREAM_FLAGS_MODE_READ);
}

/* ビットストリームのクローズ */
void SLABitStream_Close(struct SLABitStream* stream)
{
  SLA_Assert(stream != NULL);

  /* 残ったデータをフラッシュ */
  SLABitStream_Flush(stream);

  /* バッファのクリア */
  stream->bit_buffer = 0;

  /* メモリ情報のクリア */
  stream->memory_image = NULL;
  stream->memory_size  = 0;

  /* 内部状態のクリア */
  stream->memory_p     = NULL;
  stream->flags        = 0;
}

/* シーク(fseek準拠) */
void SLABitStream_Seek(struct SLABitStream* stream, int32_t offset, int32_t origin)
{
  uint8_t* pos = NULL;

  /* 引数チェック */
  SLA_Assert(stream != NULL);

  /* 内部バッファをクリア（副作用が起こる） */
  SLABitStream_Flush(stream);

  /* 起点をまず定める */
  switch (origin) {
    case SLABITSTREAM_SEEK_CUR:
      pos = stream->memory_p;
      break;
    case SLABITSTREAM_SEEK_SET:
      pos = (uint8_t *)stream->memory_image;
      break;
    case SLABITSTREAM_SEEK_END:
      pos = (uint8_t *)(stream->memory_image + (stream->memory_size - 1));
      break;
    default:
      SLA_Assert(0);
  }

  /* オフセット分動かす */
  pos += offset;

  /* 範囲チェック */
  SLA_Assert(pos < (stream->memory_image + stream->memory_size));
  SLA_Assert(pos >= stream->memory_image);

  /* 結果の保存 */
  stream->memory_p = pos;
}

/* 現在位置(ftell)準拠 */
void SLABitStream_Tell(struct SLABitStream* stream, int32_t* result)
{
  /* 引数チェック */
  SLA_Assert((stream != NULL) && (result != NULL));

  /* アクセスオフセットを返す */
  (*result) = (int32_t)(stream->memory_p - stream->memory_image);
}

/* valの右側（下位）nbits 出力（最大64bit出力可能） */
void SLABitWriter_PutBits(struct SLABitStream* stream, uint64_t val, uint32_t nbits)
{
  uint32_t bitcount;

  /* 引数チェック */
  SLA_Assert(stream != NULL);

  /* 読み込みモードでは実行不可能 */
  SLA_Assert(!(stream->flags & SLABITSTREAM_FLAGS_MODE_READ));

  /* 出力可能な最大ビット数を越えている */
  SLA_Assert(nbits <= (sizeof(uint64_t) * 8));

  /* 0ビット出力は冗長なのでアサートで落とす */
  SLA_Assert(nbits > 0);

  /* valの上位ビットから順次出力
   * 初回ループでは端数（出力に必要なビット数）分を埋め出力
   * 2回目以降は8bit単位で出力 */
  bitcount = nbits;
  while (bitcount >= stream->bit_count) {
    bitcount -= stream->bit_count;
    stream->bit_buffer |= (uint32_t)SLABITSTREAM_GETLOWERBITS(val >> bitcount, stream->bit_count);

    /* 終端に達している */
    if (stream->memory_p >= (stream->memory_image + stream->memory_size)) {
      stream->flags |= SLABITSTREAM_FLAGS_EOS;
      break;
    }

    /* メモリに書き出し */
    (*stream->memory_p) = (stream->bit_buffer & 0xFF);
    stream->memory_p++;

    /* バッファをリセット */
    stream->bit_buffer  = 0;
    stream->bit_count   = 8;
  }

  /* 端数ビットの処理:
   * 残った分をバッファの上位ビットにセット */
  SLA_Assert(bitcount <= 8);
  stream->bit_count  -= bitcount;
  stream->bit_buffer |= (uint32_t)SLABITSTREAM_GETLOWERBITS(val, bitcount) << stream->bit_count;
}

/* nbits 取得（最大64bit）し、その値を右詰めして出力 */
void SLABitReader_GetBits(struct SLABitStream* stream, uint64_t* val, uint32_t nbits)
{
  uint8_t  ch;
  uint32_t bitcount;
  uint64_t tmp = 0;

  /* 引数チェック */
  SLA_Assert((stream != NULL) && (val != NULL));

  /* 読み込みモードでない場合はアサートで落とす */
  SLA_Assert(stream->flags & SLABITSTREAM_FLAGS_MODE_READ);

  /* 入力可能な最大ビット数を越えている */
  SLA_Assert(nbits <= sizeof(uint64_t) * 8);

  /* 最上位ビットからデータを埋めていく
   * 初回ループではtmpの上位ビットにセット
   * 2回目以降は8bit単位で入力しtmpにセット */
  bitcount = nbits;
  while (bitcount > stream->bit_count) {
    bitcount  -= stream->bit_count;
    tmp       |= SLABITSTREAM_GETLOWERBITS(stream->bit_buffer, stream->bit_count) << bitcount;

    /* 終端に達している */
    if (stream->memory_p >= (stream->memory_p + stream->memory_size)) {
      stream->flags |= (uint8_t)SLABITSTREAM_FLAGS_EOS;
      break;
    }

    /* メモリから読み出し */
    ch = (*stream->memory_p);
    stream->memory_p++;

    stream->bit_buffer  = ch;
    stream->bit_count   = 8;
  }

  /* 端数ビットの処理 
   * 残ったビット分をtmpの最上位ビットにセット */
  stream->bit_count -= bitcount;
  tmp               |= (uint64_t)SLABITSTREAM_GETLOWERBITS(stream->bit_buffer >> stream->bit_count, bitcount);

  /* 正常終了 */
  (*val) = tmp;
}

/* つぎの1にぶつかるまで読み込み、その間に読み込んだ0のランレングスを取得 */
void SLABitReader_GetZeroRunLength(struct SLABitStream* stream, uint32_t* runlength)
{
  uint32_t run;

  /* 引数チェック */
  SLA_Assert((stream != NULL) && (runlength != NULL));

  /* 上位ビットからの連続する0をNLZで計測 */
  /* (1 << (31 - stream->bit_count)) はラン長が伸びすぎないようにするためのビット */
  run = SLAUtility_NLZ(
      (uint32_t)((stream->bit_buffer << (32 - stream->bit_count)) | (1UL << (31 - stream->bit_count)))
      );

  /* 読み込んだ分カウントを減らす */
  stream->bit_count -= run;

  /* バッファが空の時 */
  while (stream->bit_count == 0) {
    /* 1バイト読み込みとエラー処理 */
    uint8_t   ch;
    uint32_t  tmp_run;

    /* 終端に達している */
    if (stream->memory_p >= (stream->memory_p + stream->memory_size)) {
      stream->flags |= (uint8_t)SLABITSTREAM_FLAGS_EOS;
      break;
    }

    /* メモリから読み出し */
    ch = (*stream->memory_p);
    stream->memory_p++;

    /* ビットバッファにセットし直して再度ランを計測 */
    stream->bit_buffer  = ch;
    /* テーブルによりラン長を取得 */
    tmp_run             = g_sla_bitstream_zerobit_runlength_table[stream->bit_buffer];
    /* TODO:32bitで入力するときはこちらを使う
    tmp_run             = SLAUtility_NLZ(stream->bit_buffer);
    */
    stream->bit_count   = 8 - tmp_run;
    /* ランを加算 */
    run                 += tmp_run;
  }

  /* 続く1を空読み */
  stream->bit_count -= 1;

  /* 正常終了 */
  (*runlength) = run;
}

/* バッファにたまったビットをクリア */
void SLABitStream_Flush(struct SLABitStream* stream)
{
  /* 引数チェック */
  SLA_Assert(stream != NULL);

  /* 既に先頭にあるときは何もしない */
  if (stream->bit_count < 8) {
    /* 読み込み位置を次のバイト先頭に */
    if (stream->flags & SLABITSTREAM_FLAGS_MODE_READ) {
      /* 残りビット分を空読み */
      uint64_t dummy;
      SLABitReader_GetBits(stream, &dummy, stream->bit_count);
    } else {
      /* バッファに余ったビットを強制出力 */
      SLABitWriter_PutBits(stream, 0, stream->bit_count);
    }
  }
}

#endif /* SLABITSTREAM_PROCESS_BY_MACRO */
