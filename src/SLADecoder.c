#include "SLADecoder.h"
#include "SLAUtility.h"
#include "SLAPredictor.h"
#include "SLACoder.h"
#include "SLABitStream.h"
#include "SLAByteArray.h"
#include "SLAInternal.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

/* デコーダ状態管理フラグ */
#define SLADECODER_STATUS_FLAG_SET_WAVE_FORMAT      (1 << 0)    /* 波形フォーマットセット済み     */
#define SLADECODER_STATUS_FLAG_SET_ENCODE_PARAMETER (1 << 1)    /* エンコードパラメータセット済み */

/* ブロックヘッダ */
struct SLABlockHeaderInfo {
  uint32_t  block_size;               /* ブロックサイズ                         */
  uint32_t  block_num_samples;        /* ブロックに含まれるchあたりのサンプル数 */
};

/* デコーダハンドル */
struct SLADecoder {
  struct SLAWaveFormat          wave_format;
  struct SLAEncodeParameter     encode_param;
  uint32_t                      max_num_channels;
  uint32_t                      max_num_block_samples;
  uint32_t                      max_parcor_order;
  uint32_t                      max_longterm_order;
  uint32_t                      max_lms_order_per_filter;
  uint8_t                       enable_crc_check;
  struct SLABitStream           strm;
  struct SLACoder*              coder;

  struct SLALPCSynthesizer**        lpcs;
  struct SLALongTermSynthesizer**   ltms;
  struct SLALMSFilter**             nlmsc;
  struct SLAEmphasisFilter**        emp;

  int32_t**                     parcor_coef;
  int32_t**                     longterm_coef;
  uint32_t*                     pitch_period;

  SLABlockDataType              block_data_type;
  int32_t**                     residual;
  int32_t**                     output;
  uint32_t                      status_flag;
  uint8_t                       verpose_flag;
};

/* ストリーミングデコードハンドル */
struct SLAStreamingDecoder {
  struct SLADecoder*            decoder_core;
  uint8_t*                      data_buffer;
  uint32_t                      data_buffer_size;
  uint32_t                      data_buffer_provided_size;
  uint32_t                      num_output_samples_per_decode;
  struct SLABlockHeaderInfo     current_block_header;
  uint32_t                      current_block_sample_offset;
  float                         estimated_bytes_per_sample;
  float                         decode_interval_hz;
  uint32_t                      max_bit_per_sample;
  struct SLADataPacketQueue*    queue;
};

/* デコーダハンドルの作成 */
struct SLADecoder* SLADecoder_Create(const struct SLADecoderConfig* config)
{
  struct SLADecoder* decoder;
  uint32_t ch;
  uint32_t max_num_channels, max_num_block_samples;

  /* 引数チェック */
  if (config == NULL) {
    return NULL;
  }

  /* 頻繁に参照する変数をオート変数に受ける */
  max_num_channels      = config->max_num_channels;
  max_num_block_samples = config->max_num_block_samples;

  decoder = malloc(sizeof(struct SLADecoder));
  decoder->max_num_channels         = config->max_num_channels;
  decoder->max_num_block_samples    = config->max_num_block_samples;
  decoder->max_parcor_order         = config->max_parcor_order;
  decoder->max_longterm_order       = config->max_longterm_order;
  decoder->max_lms_order_per_filter = config->max_lms_order_per_filter;
  decoder->enable_crc_check         = config->enable_crc_check;
  decoder->verpose_flag             = config->verpose_flag;

  /* 各種領域割り当て */
  decoder->parcor_coef   = (int32_t **)malloc(sizeof(int32_t*) * max_num_channels);
  decoder->longterm_coef = (int32_t **)malloc(sizeof(int32_t*) * max_num_channels);
  decoder->pitch_period  = (uint32_t *)malloc(sizeof(uint32_t) * max_num_channels);
  decoder->residual      = (int32_t **)malloc(sizeof(int32_t*) * max_num_channels);
  decoder->output        = (int32_t **)malloc(sizeof(int32_t*) * max_num_channels);
  for (ch = 0; ch < max_num_channels; ch++) {
    decoder->parcor_coef[ch]    = (int32_t *)malloc(sizeof(int32_t) * (config->max_parcor_order + 1));
    decoder->longterm_coef[ch]  = (int32_t *)malloc(sizeof(int32_t) * config->max_longterm_order);
    decoder->residual[ch]       = (int32_t *)malloc(sizeof(int32_t) * max_num_block_samples);
    decoder->output[ch]         = (int32_t *)malloc(sizeof(int32_t) * max_num_block_samples);
  }

  decoder->coder  = SLACoder_Create(config->max_num_channels, SLACODER_NUM_RECURSIVERICE_PARAMETER);

  /* 合成ハンドル作成 */
  decoder->lpcs   = (struct SLALPCSynthesizer **)malloc(sizeof(struct SLALPCSynthesizer *) * max_num_channels);
  decoder->ltms   = (struct SLALongTermSynthesizer **)malloc(sizeof(struct SLALongTermSynthesizer *) * max_num_channels);
  decoder->nlmsc  = (struct SLALMSFilter **)malloc(sizeof(struct SLALMSFilter *) * max_num_channels);
  decoder->emp    = (struct SLAEmphasisFilter **)malloc(sizeof(struct SLAEmphasisFilter *) * max_num_channels);
  for (ch = 0; ch < max_num_channels; ch++) {
    decoder->lpcs[ch]   = SLALPCSynthesizer_Create(config->max_parcor_order);
    decoder->ltms[ch]   = SLALongTermSynthesizer_Create(config->max_longterm_order, SLALONGTERM_MAX_PERIOD);
    decoder->nlmsc[ch]  = SLALMSFilter_Create(config->max_lms_order_per_filter);
    decoder->emp[ch]    = SLAEmphasisFilter_Create();
  }

  /* 状態管理フラグをすべて落とす */
  decoder->status_flag = 0;
   
  return decoder;
}

/* デコーダハンドルの破棄 */
void SLADecoder_Destroy(struct SLADecoder* decoder)
{
  uint32_t ch;

  if (decoder != NULL) {
    for (ch = 0; ch < decoder->max_num_channels; ch++) {
      NULLCHECK_AND_FREE(decoder->output[ch]);
      NULLCHECK_AND_FREE(decoder->residual[ch]);
      NULLCHECK_AND_FREE(decoder->parcor_coef[ch]);
      NULLCHECK_AND_FREE(decoder->longterm_coef[ch]);
    }
    NULLCHECK_AND_FREE(decoder->residual);
    NULLCHECK_AND_FREE(decoder->output);
    NULLCHECK_AND_FREE(decoder->parcor_coef);
    NULLCHECK_AND_FREE(decoder->longterm_coef);
    for (ch = 0; ch < decoder->max_num_channels; ch++) {
      SLALPCSynthesizer_Destroy(decoder->lpcs[ch]);
      SLALongTermSynthesizer_Destroy(decoder->ltms[ch]);
      SLALMSFilter_Destroy(decoder->nlmsc[ch]);
      SLAEmphasisFilter_Destroy(decoder->emp[ch]);
    }
    NULLCHECK_AND_FREE(decoder->lpcs);
    NULLCHECK_AND_FREE(decoder->ltms);
    NULLCHECK_AND_FREE(decoder->nlmsc);
    NULLCHECK_AND_FREE(decoder->emp);
    SLACoder_Destroy(decoder->coder);
    free(decoder);
  }
}

/* ヘッダデコード */
SLAApiResult SLADecoder_DecodeHeader(
    const uint8_t* data, uint32_t data_size, struct SLAHeaderInfo* header_info)
{
  const uint8_t* data_pos;
  uint32_t u32buf;
  uint16_t u16buf;
  uint8_t  u8buf;
  struct SLAHeaderInfo tmp_header;
  SLAApiResult ret;

  /* 引数チェック */
  if (data == NULL || header_info == NULL) {
    return SLA_APIRESULT_INVALID_ARGUMENT;
  }

  /* データサイズが足らない */
  if (data_size < SLA_HEADER_SIZE) {
    return SLA_APIRESULT_INSUFFICIENT_DATA_SIZE;
  }

  /* 読み出し用ポインタを設定 */
  data_pos = data;

  /* 読み出し結果は一旦OKとする */
  ret = SLA_APIRESULT_OK;

  /* シグネチャ取得 */
  {
    uint8_t signature[4];
    SLAByteArray_GetUint8(data_pos, &signature[0]);
    SLAByteArray_GetUint8(data_pos, &signature[1]);
    SLAByteArray_GetUint8(data_pos, &signature[2]);
    SLAByteArray_GetUint8(data_pos, &signature[3]);
    /* 不正なシグネチャの場合は無条件でエラー */
    if (   (signature[0] != 'S') || (signature[1] != 'L')
        || (signature[2] != '*') || (signature[3] != '\1')) {
      return SLA_APIRESULT_INVALID_HEADER_FORMAT;
    }
  }

  /* 一番最初のデータブロックまでのオフセット */
  SLAByteArray_GetUint32(data_pos, &u32buf);
  /* これ以降のフィールドで、ヘッダ末尾までのCRC16 */
  SLAByteArray_GetUint16(data_pos, &u16buf);
  /* CRC16計算 */
  if (u16buf != SLAUtility_CalculateCRC16(
        &data[SLA_HEADER_CRC16_CALC_START_OFFSET], SLA_HEADER_SIZE - SLA_HEADER_CRC16_CALC_START_OFFSET)) {
    /* 不一致を検出: 返り値をデータ破損検出とする */
    ret = SLA_APIRESULT_DETECT_DATA_CORRUPTION;
  }
  /* フォーマットバージョン */
  SLAByteArray_GetUint32(data_pos, &u32buf);
  /* 今のところはバージョン不一致の場合は無条件でエラー */
  if (u32buf != SLA_FORMAT_VERSION) {
    return SLA_APIRESULT_INVALID_HEADER_FORMAT;
  }
  /* チャンネル数 */
  SLAByteArray_GetUint8(data_pos, &u8buf);
  tmp_header.wave_format.num_channels       = (uint32_t)u8buf;
  /* サンプル数 */
  SLAByteArray_GetUint32(data_pos, &tmp_header.num_samples);
  /* サンプリングレート */
  SLAByteArray_GetUint32(data_pos, &tmp_header.wave_format.sampling_rate);
  /* サンプルあたりビット数 */
  SLAByteArray_GetUint8(data_pos, &u8buf);
  tmp_header.wave_format.bit_per_sample     = (uint32_t)u8buf;
  /* オフセット分の左シフト量 */
  SLAByteArray_GetUint8(data_pos, &tmp_header.wave_format.offset_lshift);
  /* PARCOR係数次数 */
  SLAByteArray_GetUint8(data_pos, &u8buf);
  tmp_header.encode_param.parcor_order      = (uint32_t)u8buf;
  /* ロングターム係数次数 */
  SLAByteArray_GetUint8(data_pos, &u8buf);
  tmp_header.encode_param.longterm_order    = (uint32_t)u8buf;
  /* LMS次数 */
  SLAByteArray_GetUint8(data_pos, &u8buf);
  tmp_header.encode_param.lms_order_per_filter = (uint32_t)u8buf;
  /* チャンネル毎の処理法 */
  SLAByteArray_GetUint8(data_pos, &u8buf);
  tmp_header.encode_param.ch_process_method = (uint32_t)u8buf;
  /* SLAブロック数 */
  SLAByteArray_GetUint32(data_pos, &u32buf);
  tmp_header.num_blocks = u32buf;
  /* SLAブロックあたり最大サンプル数 */
  SLAByteArray_GetUint16(data_pos, &u16buf);
  tmp_header.encode_param.max_num_block_samples = (uint32_t)u16buf;
  /* 最大ブロックサイズ */
  SLAByteArray_GetUint32(data_pos, &tmp_header.max_block_size);
  /* 最大bps */
  SLAByteArray_GetUint32(data_pos, &tmp_header.max_bit_per_second);

  /* ヘッダサイズチェック */
  SLA_Assert((data_pos - data) == SLA_HEADER_SIZE);

  /* 出力に書き込むが、ステータスは破壊検知の場合もある */
  *header_info = tmp_header;
  return ret;
}

/* 波形パラメータをデコーダにセット */
SLAApiResult SLADecoder_SetWaveFormat(struct SLADecoder* decoder,
    const struct SLAWaveFormat* wave_format)
{
  /* 引数チェック */
  if (decoder == NULL || wave_format == NULL) {
    return SLA_APIRESULT_INVALID_ARGUMENT;
  }

  /* デコーダの許容範囲か？ */
  if ((wave_format->num_channels > decoder->max_num_channels)
      || (wave_format->bit_per_sample > 32)) {
    return SLA_APIRESULT_EXCEED_HANDLE_CAPACITY;
  }

  /* パラメータをセット */
  decoder->wave_format = *wave_format;

  /* 波形フォーマットセット済みに設定 */
  decoder->status_flag |= SLADECODER_STATUS_FLAG_SET_WAVE_FORMAT;

  return SLA_APIRESULT_OK;
}

/* エンコードパラメータをデコーダにセット */
SLAApiResult SLADecoder_SetEncodeParameter(struct SLADecoder* decoder,
    const struct SLAEncodeParameter* encode_param)
{
  /* 引数チェック */
  if (decoder == NULL || encode_param == NULL) {
    return SLA_APIRESULT_INVALID_ARGUMENT;
  }

  /* デコーダの許容範囲か？ */
  if ((encode_param->parcor_order > decoder->max_parcor_order)
      || (encode_param->longterm_order > decoder->max_longterm_order)
      || (encode_param->lms_order_per_filter > decoder->max_lms_order_per_filter)
      || (encode_param->max_num_block_samples > decoder->max_num_block_samples)
      || (encode_param->max_num_block_samples < SLA_MIN_BLOCK_NUM_SAMPLES)) {
    return SLA_APIRESULT_EXCEED_HANDLE_CAPACITY;
  }

  /* パラメータをセット */
  decoder->encode_param = *encode_param;

  /* パラメータセット済みに設定 */
  decoder->status_flag |= SLADECODER_STATUS_FLAG_SET_ENCODE_PARAMETER;

  return SLA_APIRESULT_OK;
}

/* ブロックヘッダ（ヘッダ+係数パラメータ）のデコード */
/* FIXME: この関数内だけでストリームオープンとクローズを完結させたかったができていない */
static SLAApiResult SLADecoder_DecodeBlockHeader(struct SLADecoder* decoder, 
    const uint8_t* data, uint32_t data_size, 
    struct SLABlockHeaderInfo* block_header_info, uint32_t* block_header_size)
{
  uint64_t bitsbuf;
  uint16_t crc16;
  uint32_t ch, ord;

  /* 引数チェック */
  if ((decoder == NULL) || (data == NULL) || (block_header_info == NULL)) {
    return SLA_APIRESULT_INVALID_ARGUMENT;
  }

  /* デコードに必要なパラメータがセットされていない */
  if ((!(decoder->status_flag & SLADECODER_STATUS_FLAG_SET_WAVE_FORMAT))
      || (!(decoder->status_flag & SLADECODER_STATUS_FLAG_SET_ENCODE_PARAMETER))) {
    return SLA_APIRESULT_PARAMETER_NOT_SET;
  }

  /* 最小のサイズに満たない */
  if (data_size < SLA_MINIMUM_BLOCK_HEADER_SIZE) {
    return SLA_APIRESULT_INSUFFICIENT_DATA_SIZE;
  }

  /* 同期コード */
  SLABitReader_GetBits(&decoder->strm, &bitsbuf, 16);
  if (bitsbuf != SLA_BLOCK_SYNC_CODE) {
    return SLA_APIRESULT_FAILED_TO_FIND_SYNC_CODE;
  }
  /* 次のブロックまでのオフセット */
  SLABitReader_GetBits(&decoder->strm, &bitsbuf, 32);
  /* ブロックサイズに変換 */
  block_header_info->block_size = (uint32_t)bitsbuf + 2 + 4;  /* 同期コード, オフセットを抜いた分足す */
  /* これ以降のフィールドのCRC16値 */
  SLABitReader_GetBits(&decoder->strm, &bitsbuf, 16);
  crc16 = (uint16_t)bitsbuf;
  /* CRC16チェック: データ長がブロックサイズ分ないときはチェックできない */
  if ((decoder->enable_crc_check == 1) && (data_size >= block_header_info->block_size)) {
    uint16_t calc_crc16;
    SLA_Assert(block_header_info->block_size >= SLA_BLOCK_CRC16_CALC_START_OFFSET);
    calc_crc16 = SLAUtility_CalculateCRC16(
        &data[SLA_BLOCK_CRC16_CALC_START_OFFSET], block_header_info->block_size - SLA_BLOCK_CRC16_CALC_START_OFFSET);
    if (calc_crc16 != crc16) {
      /* 不一致を検出 */
      return SLA_APIRESULT_DETECT_DATA_CORRUPTION;
    }
  }
  /* ブロックサンプル数 */
  SLABitReader_GetBits(&decoder->strm, &bitsbuf, 16);
  block_header_info->block_num_samples = (uint32_t)bitsbuf;
  /* printf("next:%d crc16:%04X nsmpl:%d \n", next_block_offset, crc16, block_samples); */
  /* ブロックタイプ */
  SLABitReader_GetBits(&decoder->strm, &bitsbuf, 2);
  decoder->block_data_type = (SLABlockDataType)bitsbuf;

  /* 各チャンネルの係数取得 */
  for (ch = 0; ch < decoder->wave_format.num_channels; ch++) {
    uint32_t rshift;

    /* 圧縮されたデータ以外ではスキップ */
    if (decoder->block_data_type != SLA_BLOCK_DATA_TYPE_COMPRESSDATA) {
      break;
    }

    /* PARCOR係数読み取り */
    /* 右シフト量 */
    SLABitReader_GetBits(&decoder->strm, &bitsbuf, 4);
    rshift = (uint32_t)bitsbuf;
    /* 0次は0で確定 */
    decoder->parcor_coef[ch][0] = 0;
    for (ord = 1; ord < decoder->encode_param.parcor_order + 1; ord++) {
      /* 量子化ビット数 */
      uint32_t qbits = (uint32_t)SLA_GET_PARCOR_QUANTIZE_BIT_WIDTH(ord);
      /* PARCOR係数 */
      SLABitReader_GetBits(&decoder->strm, &bitsbuf, qbits);
      decoder->parcor_coef[ch][ord] = SLAUTILITY_UINT32_TO_SINT32(bitsbuf);
      /* 16bitをベースにシフト */
      decoder->parcor_coef[ch][ord] <<= (16U - qbits);
      /* オーバーフローを防ぐための右シフト */
      decoder->parcor_coef[ch][ord] 
        = SLAUTILITY_SHIFT_RIGHT_ARITHMETIC(decoder->parcor_coef[ch][ord], rshift);
    }

    /* ロングターム係数読み取り */
    SLABitReader_GetBits(&decoder->strm, &bitsbuf, 1);
    if (bitsbuf == 0) {
      /* ピッチ周期無効値の0でマーク */
      decoder->pitch_period[ch] = 0;
    } else {
      SLABitReader_GetBits(&decoder->strm, &bitsbuf, SLALONGTERM_PERIOD_NUM_BITS);
      decoder->pitch_period[ch] = (uint32_t)bitsbuf;
      for (ord = 0; ord < decoder->encode_param.longterm_order; ord++) {
        SLABitReader_GetBits(&decoder->strm, &bitsbuf, 16);
        decoder->longterm_coef[ch][ord] = SLAUTILITY_UINT32_TO_SINT32(bitsbuf);
        decoder->longterm_coef[ch][ord] <<= 16;
      }
    }

    /* 再帰的ライスパラメータ復号 */
    SLACoder_GetInitialRecursiveRiceParameter(decoder->coder, &decoder->strm,
        SLACODER_NUM_RECURSIVERICE_PARAMETER, 
        decoder->wave_format.bit_per_sample, ch);
  }

  /* バイト境界に揃える */
  SLABitStream_Flush(&decoder->strm);

  /* ブロックヘッダサイズの取得 */
  SLABitStream_Tell(&decoder->strm, (int32_t *)block_header_size);

  return SLA_APIRESULT_OK;
}

/* ブロックデータ（波形データ）のデコード */
/* FIXME: この関数内だけでストリームオープンとクローズを完結させたかったができていない */
/* 注意）data_sizeは消費したサイズを返すがバイト境界上にあるとは限らない */
static SLAApiResult SLADecoder_DecodeWaveData(struct SLADecoder* decoder, 
    int32_t** buffer, uint32_t num_decode_saples, uint32_t* output_data_size)
{
  uint32_t ch, smpl;
  uint32_t num_channels;
  uint64_t bitsbuf;
  int32_t  start_data_offset, end_data_offset; 

  /* 引数チェック */
  if ((decoder == NULL) || (buffer == NULL)) {
    return SLA_APIRESULT_INVALID_ARGUMENT;
  }

  /* デコードに必要なパラメータがセットされていない */
  if ((!(decoder->status_flag & SLADECODER_STATUS_FLAG_SET_WAVE_FORMAT))
      || (!(decoder->status_flag & SLADECODER_STATUS_FLAG_SET_ENCODE_PARAMETER))) {
    return SLA_APIRESULT_PARAMETER_NOT_SET;
  }

  /* 開始オフセットの記録 */
  SLABitStream_Tell(&decoder->strm, &start_data_offset);

  /* 頻繁に参照する変数をオート変数に受ける */
  num_channels = decoder->wave_format.num_channels;

  /* 残差復号 */
  switch (decoder->block_data_type) {
    case SLA_BLOCK_DATA_TYPE_SILENT:
      /* 無音で埋める */
      for (ch = 0; ch < num_channels; ch++) {
        memset(decoder->output[ch], 0, sizeof(int32_t) * num_decode_saples);
      }
      break;
    case SLA_BLOCK_DATA_TYPE_RAWDATA:
      /* 生データ取得 */
      {
        uint32_t input_bits[SLA_MAX_CHANNELS];
        for (ch = 0; ch < num_channels; ch++) {
          /* 左シフト量だけ減らして取得 */
          SLA_Assert(decoder->wave_format.bit_per_sample > decoder->wave_format.offset_lshift);
          input_bits[ch] = decoder->wave_format.bit_per_sample - decoder->wave_format.offset_lshift;
          /* MS処理を行った場合の2ch目はL-Rが入っているのでビット幅が1増える */
          if ((ch == 1)
              && (decoder->encode_param.ch_process_method == SLA_CHPROCESSMETHOD_STEREO_MS)) {
            input_bits[ch] += 1;
          }
        }
        /* チャンネルインターリーブしつつ復号 */
        for (smpl = 0; smpl < num_decode_saples; smpl++) {
          for (ch = 0; ch < num_channels; ch++) {
            SLABitReader_GetBits(&decoder->strm, &bitsbuf, input_bits[ch]);
            decoder->output[ch][smpl] = SLAUTILITY_UINT32_TO_SINT32((uint32_t)bitsbuf);
          }
        }
      }
      break;
    case SLA_BLOCK_DATA_TYPE_COMPRESSDATA:
      /* 残差復号 */
      SLACoder_GetDataArray(decoder->coder, &decoder->strm, 
          SLACODER_NUM_RECURSIVERICE_PARAMETER,
          decoder->residual, num_channels, num_decode_saples);
      break;
    default:
      /* ここに入ってきたらプログラミングミス */
      SLA_Assert(0);
      break;
  }

  /* チャンネル毎に音声合成 */
  for (ch = 0; ch < num_channels; ch++) {
    /* 圧縮されたブロック以外ではスキップ */
    if (decoder->block_data_type != SLA_BLOCK_DATA_TYPE_COMPRESSDATA) {
      break;
    }

    /* LMSの残差分を合成 */
    if (SLALMSFilter_SynthesizeInt32(decoder->nlmsc[ch],
          decoder->encode_param.lms_order_per_filter,
          decoder->residual[ch], num_decode_saples,
          decoder->output[ch]) != SLAPREDICTOR_APIRESULT_OK) {
      return SLA_APIRESULT_FAILED_TO_SYNTHESIZE;
    }
    /* 合成した信号で残差を差し替え */
    memcpy(decoder->residual[ch], decoder->output[ch], sizeof(int32_t) * num_decode_saples);

    /* ロングタームの残差分を合成 */
    if (decoder->pitch_period[ch] != 0) {
      if (SLALongTermSynthesizer_SynthesizeInt32(
            decoder->ltms[ch],
            decoder->residual[ch], num_decode_saples,
            decoder->pitch_period[ch], decoder->longterm_coef[ch],
            decoder->encode_param.longterm_order, decoder->output[ch]) != SLAPREDICTOR_APIRESULT_OK) {
        return SLA_APIRESULT_FAILED_TO_SYNTHESIZE;
      }
      /* 合成した信号で残差を差し替え */
      memcpy(decoder->residual[ch], decoder->output[ch], sizeof(int32_t) * num_decode_saples);
    }

    /* PARCORの残差分を合成 */
    if (SLALPCSynthesizer_SynthesizeByParcorCoefInt32(decoder->lpcs[ch],
          decoder->residual[ch], num_decode_saples,
          decoder->parcor_coef[ch], decoder->encode_param.parcor_order,
          decoder->output[ch]) != SLAPREDICTOR_APIRESULT_OK) {
      return SLA_APIRESULT_FAILED_TO_SYNTHESIZE;
    }

    /* デエンファシス */
    if (SLAEmphasisFilter_DeEmphasisInt32(decoder->emp[ch], 
          decoder->output[ch], num_decode_saples, 
          SLA_PRE_EMPHASIS_COEFFICIENT_SHIFT) != SLAPREDICTOR_APIRESULT_OK) {
      return SLA_APIRESULT_FAILED_TO_SYNTHESIZE;
    }
  }

  /* チャンネル毎の処理をしていたら元に戻す */
  switch (decoder->encode_param.ch_process_method) {
    case SLA_CHPROCESSMETHOD_STEREO_MS:
      SLAUtility_MStoLRInt32(decoder->output, decoder->wave_format.num_channels, num_decode_saples);
      break;
    default:
      break;
  }

  /* 最終結果をバッファにコピー */
  SLA_Assert(decoder->wave_format.bit_per_sample > decoder->wave_format.offset_lshift);
  SLA_Assert((decoder->wave_format.bit_per_sample - decoder->wave_format.offset_lshift) < 32);
  for (ch = 0; ch < num_channels; ch++) {
    for (smpl = 0; smpl < num_decode_saples; smpl++) {
      buffer[ch][smpl]
        = decoder->output[ch][smpl] << (32 - decoder->wave_format.bit_per_sample + decoder->wave_format.offset_lshift);
    }
  }

  /* 終了オフセットの記録 */
  SLABitStream_Tell(&decoder->strm, &end_data_offset);

  /* 消費データサイズの計算 */
  SLA_Assert(end_data_offset >= start_data_offset);
  (*output_data_size) = (uint32_t)(end_data_offset - start_data_offset);

  return SLA_APIRESULT_OK;
}

/* 全音声合成モジュールのリセット */
static void SLADecoder_ResetAllSynthesizer(struct SLADecoder* decoder)
{
  uint32_t ch;

  SLA_Assert(decoder != NULL);

  for (ch = 0; ch < decoder->wave_format.num_channels; ch++) {
    (void)SLAEmphasisFilter_Reset(decoder->emp[ch]);
    (void)SLALPCSynthesizer_Reset(decoder->lpcs[ch]);
    (void)SLALongTermSynthesizer_Reset(decoder->ltms[ch]);
    (void)SLALMSFilter_Reset(decoder->nlmsc[ch]);
  }
}

/* 1ブロックデコード */
static SLAApiResult SLADecoder_DecodeBlock(struct SLADecoder* decoder,
    const uint8_t* data, uint32_t data_size,
    int32_t** buffer, uint32_t buffer_num_samples,
    uint32_t* output_block_size, uint32_t* output_num_samples)
{
  uint32_t block_header_size;
  uint32_t tmp_data_size;
  SLAApiResult ret;
  struct SLABlockHeaderInfo block_header;

  /* 引数チェック */
  if (decoder == NULL || data == NULL || buffer == NULL
      || output_block_size == NULL || output_num_samples == NULL) {
    return SLA_APIRESULT_INVALID_ARGUMENT;
  }

  /* デコードに必要なパラメータがセットされていない */
  if ((!(decoder->status_flag & SLADECODER_STATUS_FLAG_SET_WAVE_FORMAT))
      || (!(decoder->status_flag & SLADECODER_STATUS_FLAG_SET_ENCODE_PARAMETER))) {
    return SLA_APIRESULT_PARAMETER_NOT_SET;
  }

  /* チャンネル毎の処理に矛盾がないかチェック */
  switch (decoder->encode_param.ch_process_method) {
    case SLA_CHPROCESSMETHOD_STEREO_MS:
      if (decoder->wave_format.num_channels != 2) {
        return SLA_APIRESULT_INVAILD_CHPROCESSMETHOD;
      }
      break;
    default:
      break;
  }

  /* ビットストリーム作成 */
  SLABitReader_Open(&decoder->strm, (uint8_t *)data, data_size);
  SLABitStream_Seek(&decoder->strm, 0, SLABITSTREAM_SEEK_SET);

  /* ブロックヘッダを復号 */
  if ((ret = SLADecoder_DecodeBlockHeader(decoder,
          data, data_size, &block_header, &block_header_size)) != SLA_APIRESULT_OK) {
    return ret;
  }

  /* データサイズ不足 */
  if (block_header.block_size > data_size) {
    return SLA_APIRESULT_INSUFFICIENT_DATA_SIZE;
  }

  /* バッファサイズ不足 */
  if (block_header.block_num_samples > buffer_num_samples) {
    return SLA_APIRESULT_INSUFFICIENT_BUFFER_SIZE;
  }

  /* 全音声合成モジュールをリセット */
  /* 補足）ブロック先頭でリセット必須 */
  SLADecoder_ResetAllSynthesizer(decoder);

  /* データ復号 */
  if ((ret = SLADecoder_DecodeWaveData(decoder,
          buffer, block_header.block_num_samples, &tmp_data_size)) != SLA_APIRESULT_OK) {
    return ret;
  }

  /* 出力サンプル数の取得 */
  (*output_num_samples) = block_header.block_num_samples;

  /* 出力サイズの取得 */
  SLABitStream_Tell(&decoder->strm, (int32_t *)output_block_size);

  /* ビットストリームクローズ */
  SLABitStream_Close(&decoder->strm);

  return SLA_APIRESULT_OK;
}

/* ヘッダを含めて全ブロックデコード */
SLAApiResult SLADecoder_DecodeWhole(struct SLADecoder* decoder,
    const uint8_t* data, uint32_t data_size,
    int32_t** buffer, uint32_t buffer_num_samples, uint32_t* output_num_samples)
{
  uint32_t ch;
  uint32_t decode_offset_byte, decode_offset_sample;
  uint32_t block_num_samples, block_size;
  int32_t* output_ptr[SLA_MAX_CHANNELS];
  struct SLAHeaderInfo header;
  SLAApiResult api_ret;

  /* 引数チェック */
  if (decoder == NULL || buffer == NULL
      || data == NULL || output_num_samples == NULL) {
    return SLA_APIRESULT_INVALID_ARGUMENT;
  }

  /* ヘッダ読み出し */
  if ((api_ret = SLADecoder_DecodeHeader(data, data_size, &header))
      != SLA_APIRESULT_OK) {
    return api_ret;
  }

  /* ヘッダから読み取った情報をハンドルにセット */
  if ((api_ret = SLADecoder_SetWaveFormat(decoder,
          &header.wave_format)) != SLA_APIRESULT_OK) {
    return api_ret;
  }
  if ((api_ret = SLADecoder_SetEncodeParameter(decoder,
          &header.encode_param)) != SLA_APIRESULT_OK) {
    return api_ret;
  }

  /* 全ブロックを逐次デコード */
  decode_offset_byte   = SLA_HEADER_SIZE;
  decode_offset_sample = 0;
  /* FIXME: サンプル数が無効値だと偉いことになる */
  while (decode_offset_sample < header.num_samples) {
    /* 出力データサイズが足らない */
    if (decode_offset_byte > data_size) {
      return SLA_APIRESULT_INSUFFICIENT_DATA_SIZE;
    }

    /* 出力信号のポインタをセット */
    for (ch = 0; ch < decoder->wave_format.num_channels; ch++) {
      output_ptr[ch] = &buffer[ch][decode_offset_sample];
    }

    /* ブロックデコード */
    if ((api_ret = SLADecoder_DecodeBlock(decoder,
            &data[decode_offset_byte], data_size - decode_offset_byte,
            output_ptr, buffer_num_samples - decode_offset_sample,
            &block_size, &block_num_samples)) != SLA_APIRESULT_OK) {
      return api_ret;
    }

    /* 出力データサイズの更新 */
    decode_offset_byte    += block_size;
    /* デコードしたサンプル数の更新 */
    decode_offset_sample  += block_num_samples;

    /* 進捗の表示 */
    if (decoder->verpose_flag != 0) {
      printf("progress:%2u%% \r", (unsigned int)((100 * decode_offset_byte) / data_size));
      fflush(stdout);
    }
  }

  /* 出力サンプル数を記録 */
  *output_num_samples = decode_offset_sample;

  return SLA_APIRESULT_OK;
}

/* ストリーミングデコーダの内部状態リセット */
static SLAApiResult SLAStreamingDecoder_Reset(struct SLAStreamingDecoder* decoder)
{
  if (decoder == NULL) {
    return SLA_APIRESULT_INVALID_ARGUMENT;
  }

  /* ブロックオフセットをリセット */
  decoder->current_block_sample_offset = 0;

  /* データバッファをリセット */
  memset(decoder->data_buffer, 0, sizeof(uint8_t) * decoder->data_buffer_size);
  decoder->data_buffer_provided_size = 0;

  return SLA_APIRESULT_OK;
}

/* ストリーミングデコーダの作成 */
struct SLAStreamingDecoder* SLAStreamingDecoder_Create(const struct SLAStreamingDecoderConfig* config)
{
  struct SLAStreamingDecoder* decoder;

  /* 引数チェック */
  if (config == NULL) {
    return NULL;
  }

  /* デコード処理間隔が異常 */
  if (config->decode_interval_hz <= 0.0f) {
    return NULL;
  }

  /* ハンドルの領域割当て */
  decoder = (struct SLAStreamingDecoder *)malloc(sizeof(struct SLAStreamingDecoder));

  /* デコーダコアハンドルの作成 */
  decoder->decoder_core = SLADecoder_Create(&config->core_config);
  if (decoder->decoder_core == NULL) {
    free(decoder);
    return NULL;
  }

  /* データパケットキューの作成 */
  decoder->queue = SLADataPacketQueue_Create(SLA_STREAMING_DECODE_MAX_NUM_PACKETS);
  if (decoder->queue == NULL) {
    free(decoder->decoder_core);
    free(decoder);
    return NULL;
  }

  /* コンフィグをメンバに記録 */
  decoder->decode_interval_hz = config->decode_interval_hz;
  decoder->max_bit_per_sample = config->max_bit_per_sample;

  /* バッファサイズ計算: ブロックをまたぐことがあるので、2ブロック分確保 */
  decoder->data_buffer_size
    = 2 * SLA_CalculateSufficientBlockSize(config->core_config.max_num_channels,
        config->core_config.max_num_block_samples, config->max_bit_per_sample);

  /* バッファ確保 */
  decoder->data_buffer = (uint8_t *)malloc(sizeof(uint8_t) * decoder->data_buffer_size);

  /* 推定サンプルあたりバイト数を最悪値で見積もる */
  decoder->estimated_bytes_per_sample = (float)((double)config->core_config.max_num_channels * (config->max_bit_per_sample / 8));

  /* 内部状態リセット */
  if (SLAStreamingDecoder_Reset(decoder) != SLA_APIRESULT_OK) {
    free(decoder->decoder_core);
    free(decoder->data_buffer);
    free(decoder->queue);
    free(decoder);
    return NULL;
  }

  return decoder;
}

/* ストリーミングデコーダの破棄 */
void SLAStreamingDecoder_Destroy(struct SLAStreamingDecoder* decoder)
{
  if (decoder != NULL) {
    SLADecoder_Destroy(decoder->decoder_core);
    SLADataPacketQueue_Destroy(decoder->queue);
    NULLCHECK_AND_FREE(decoder->data_buffer);
    NULLCHECK_AND_FREE(decoder);
  }
}

/* 波形パラメータをデコーダにセット */
SLAApiResult SLAStreamingDecoder_SetWaveFormat(struct SLAStreamingDecoder* decoder,
    const struct SLAWaveFormat* wave_format)
{
  SLAApiResult ret;

  /* 引数チェック */
  if (decoder == NULL) {
    return SLA_APIRESULT_INVALID_ARGUMENT;
  }

  /* コアAPIの実行 */
  if ((ret = SLADecoder_SetWaveFormat(decoder->decoder_core, wave_format)) != SLA_APIRESULT_OK) {
    return ret;
  }

  /* サンプルあたりビット数をチェック */
  if (wave_format->bit_per_sample > decoder->max_bit_per_sample) {
    return SLA_APIRESULT_EXCEED_HANDLE_CAPACITY;
  }

  /* デコード関数呼び出しあたりのサンプル数を確定 */
  decoder->num_output_samples_per_decode
    = (uint32_t)ceil(SLA_STREAMING_DECODE_NUM_SAMPLES_MARGIN * (float)wave_format->sampling_rate / decoder->decode_interval_hz);
  
  return SLA_APIRESULT_OK;
}

/* エンコードパラメータをデコーダにセット */
SLAApiResult SLAStreamingDecoder_SetEncodeParameter(struct SLAStreamingDecoder* decoder,
    const struct SLAEncodeParameter* encode_param)
{
  /* 引数チェック */
  if (decoder == NULL) {
    return SLA_APIRESULT_INVALID_ARGUMENT;
  }
  /* コアAPIの実行 */
  return SLADecoder_SetEncodeParameter(decoder->decoder_core, encode_param);
}

/* 最低限供給すべきデータサイズの推定値を取得 */
SLAApiResult SLAStreamingDecoder_EstimateMinimumNessesaryDataSize(struct SLAStreamingDecoder* decoder,
    uint32_t* estimate_data_size)
{
  uint32_t  tmp_estimate_data_size;

  /* 引数チェック */
  if ((decoder == NULL) || (estimate_data_size == NULL)) {
    return SLA_APIRESULT_INVALID_ARGUMENT;
  }

  /* デコード関数呼び出しあたりのバイト数に変換 
   * 安全に振るためバイト数を切り上げ */
  tmp_estimate_data_size = (uint32_t)ceil((double)decoder->estimated_bytes_per_sample * decoder->num_output_samples_per_decode);

  /* 少なくとも最小のブロックヘッダサイズ分は必要 */
  tmp_estimate_data_size = SLAUTILITY_MAX(tmp_estimate_data_size, SLA_MINIMUM_BLOCK_HEADER_SIZE);

  /* 出力に書き込み */
  (*estimate_data_size) = tmp_estimate_data_size;

  return SLA_APIRESULT_OK;
}

/* デコード可能なサンプル数の推定値を取得 */
SLAApiResult SLAStreamingDecoder_EstimateDecodableNumSamples(struct SLAStreamingDecoder* decoder,
    uint32_t* estimate_num_samples)
{
  SLAApiResult  api_ret;
  uint32_t      remain_data_size;
  float         tmp_estimate_num_samples;

  /* 引数チェック */
  if ((decoder == NULL) || (estimate_num_samples == NULL)) {
    return SLA_APIRESULT_INVALID_ARGUMENT;
  }

  /* 残りデータサイズを取得 */
  if ((api_ret = SLAStreamingDecoder_GetRemainDataSize(decoder, &remain_data_size)) != SLA_APIRESULT_OK) {
    return api_ret;
  }

  /* デコード可能なサンプル数推定値を計算 */
  tmp_estimate_num_samples = (float)remain_data_size / decoder->estimated_bytes_per_sample;

  /* 安全に振るため切り捨て */
  (*estimate_num_samples) = (uint32_t)floor(tmp_estimate_num_samples);

  return SLA_APIRESULT_OK;
}

/* デコード関数呼び出しあたりの出力サンプル数を取得 */
SLAApiResult SLAStreamingDecoder_GetOutputNumSamplesPerDecode(struct SLAStreamingDecoder* decoder,
    uint32_t* output_num_samples)
{
  /* 引数チェック */
  if ((decoder == NULL) || (output_num_samples == NULL)) {
    return SLA_APIRESULT_INVALID_ARGUMENT;
  }

  /* 結果に記録 */
  (*output_num_samples) = decoder->num_output_samples_per_decode;

  return SLA_APIRESULT_OK;
}

/* 残りデータサイズを取得 */
SLAApiResult SLAStreamingDecoder_GetRemainDataSize(struct SLAStreamingDecoder* decoder,
    uint32_t* remain_data_size)
{
  uint32_t queue_remain, data_buffer_remain;

  /* 引数チェック */
  if ((decoder == NULL) || (remain_data_size == NULL)) {
    return SLA_APIRESULT_INVALID_ARGUMENT;
  }

  /* キューの残りデータサイズを取得 */
  queue_remain = SLADataPacketQueue_GetRemainDataSize(decoder->queue);

  /* 連続バッファの残りサイズ */
  data_buffer_remain = decoder->data_buffer_provided_size;
  if (decoder->current_block_sample_offset > 0) {
    int32_t decoded_size;
    /* デコード済みサイズで減じる */
    SLABitStream_Tell(&decoder->decoder_core->strm, &decoded_size);
    SLA_Assert(decoder->data_buffer_provided_size >= (uint32_t)decoded_size);
    data_buffer_remain -= (uint32_t)decoded_size;
  }

  /* キューと連続バッファの合計が余りサイズ */
  (*remain_data_size) = queue_remain + data_buffer_remain;

  return SLA_APIRESULT_OK;
}

/* デコーダにデータを供給 */
SLAApiResult SLAStreamingDecoder_AppendDataFragment(struct SLAStreamingDecoder* decoder,
    const uint8_t* data, uint32_t data_size)
{
  const uint8_t* append_data;
  uint32_t append_data_size;

  /* 引数チェック */
  if ((decoder == NULL) || (data == NULL)) {
    return SLA_APIRESULT_INVALID_ARGUMENT;
  }

  /* キューにデータ挿入 */
  if (SLADataPacketQueue_EnqueueDataFragment(
        decoder->queue, data, data_size) != SLA_DATAPACKETQUEUE_APIRESULT_OK) {
    return SLA_APIRESULT_EXCEED_HANDLE_CAPACITY;
  }

  /* データ片を突っ込めるだけデコード領域にコピー */
  SLA_Assert(decoder->data_buffer_size >= decoder->data_buffer_provided_size);
  while (SLADataPacketQueue_GetDataFragment(
          decoder->queue, &append_data, &append_data_size,
          decoder->data_buffer_size - decoder->data_buffer_provided_size) != SLA_DATAPACKETQUEUE_APIRESULT_NO_DATA_FRAGMENTS) {
    memcpy(&decoder->data_buffer[decoder->data_buffer_provided_size], append_data, append_data_size);
    decoder->data_buffer_provided_size += append_data_size;
    SLA_Assert(decoder->data_buffer_size >= decoder->data_buffer_provided_size);
  }

  return SLA_APIRESULT_OK;
}

/* デコーダに残っているデータの回収 */
SLAApiResult SLAStreamingDecoder_CollectDataFragment(struct SLAStreamingDecoder* decoder,
    const uint8_t** data_ptr, uint32_t* data_size)
{
  /* 引数チェック */
  if ((decoder == NULL) || (data_ptr == NULL) || (data_size == NULL)) {
    return SLA_APIRESULT_INVALID_ARGUMENT;
  }

  /* データ回収 */
  if (SLADataPacketQueue_DequeueDataFragment(
        decoder->queue, data_ptr, data_size) != SLA_DATAPACKETQUEUE_APIRESULT_OK) {
    return SLA_APIRESULT_NO_DATA_FRAGMENTS;
  }

  return SLA_APIRESULT_OK;
}

/* ストリーミングデコードコア処理 */
static SLAApiResult SLAStreamingDecoder_DecodeCore(struct SLAStreamingDecoder* decoder,
    int32_t** buffer, uint32_t buffer_num_samples, uint32_t* num_output_samples)
{
  uint32_t      ch;
  uint32_t      num_decode_samples, goal_num_samples;
  uint32_t      sample_progress, decoded_data_progress;
  SLAApiResult  ret;
  int32_t*      buffer_ptr[SLA_MAX_CHANNELS];
  uint32_t      output_wavedata_size;
  static uint32_t      block_offset;

  /* 内部関数なので引数は非NULLを要求 */
  SLA_Assert((decoder != NULL) && (buffer != NULL) && (num_output_samples != NULL));
  SLA_Assert(buffer_num_samples > 0);

  /* デコードするべきサンプル数 */
  goal_num_samples = SLAUTILITY_MIN(buffer_num_samples, decoder->num_output_samples_per_decode);

  /* デコードサンプル数に達するまでデコードし続ける */
  sample_progress = 0;
  decoded_data_progress = 0;
  while (sample_progress < goal_num_samples) {
    /* ブロック先頭 */
    if (decoder->current_block_sample_offset == 0) {
      uint32_t block_header_size;
      /* ストリームを開く */
      SLABitReader_Open(&decoder->decoder_core->strm,
          decoder->data_buffer, decoder->data_buffer_size);
      /* ブロックヘッダ読み取り */
      if ((ret = SLADecoder_DecodeBlockHeader(decoder->decoder_core, 
              decoder->data_buffer, decoder->data_buffer_provided_size,
              &decoder->current_block_header, &block_header_size)) != SLA_APIRESULT_OK) {
        return ret;
      }
      decoded_data_progress += block_header_size;
      block_offset += block_header_size;
      /* サンプルあたりバイト数の更新 */
      decoder->estimated_bytes_per_sample
        = (float)((double)decoder->current_block_header.block_size / decoder->current_block_header.block_num_samples);
      /* 予測ハンドルをリセット（ブロック先頭で必須） */
      SLADecoder_ResetAllSynthesizer(decoder->decoder_core);
    }

    /* 現在のブロック内でデコードするサンプル数の確定 */
    num_decode_samples
      = SLAUTILITY_MIN(goal_num_samples - sample_progress,
          decoder->current_block_header.block_num_samples - decoder->current_block_sample_offset);

    /* バッファオーバーラン検知 */
    if (buffer_num_samples < (sample_progress + num_decode_samples)) {
      return SLA_APIRESULT_INSUFFICIENT_BUFFER_SIZE;
    }

    /* デコード実行 */
    for (ch = 0; ch < decoder->decoder_core->wave_format.num_channels; ch++) {
      buffer_ptr[ch] = &buffer[ch][sample_progress];
    }
    if ((ret = SLADecoder_DecodeWaveData(
            decoder->decoder_core, buffer_ptr, num_decode_samples, &output_wavedata_size)) != SLA_APIRESULT_OK) {
      return ret;
    }

    /* デコード進捗を進める */
    sample_progress                       += num_decode_samples;
    decoder->current_block_sample_offset  += num_decode_samples;
    decoded_data_progress                 += output_wavedata_size;

    /* ブロック末尾に達した */
    if (decoder->current_block_sample_offset >= decoder->current_block_header.block_num_samples) {
      /* ぴったりブロック末尾でなければならない */
      SLA_Assert(decoder->current_block_sample_offset == decoder->current_block_header.block_num_samples);
      SLA_Assert(decoder->data_buffer_provided_size >= decoder->current_block_header.block_size);
      /* ブロック末尾でのデータ折返し */
      memmove(decoder->data_buffer,
          &decoder->data_buffer[decoder->current_block_header.block_size],
          decoder->data_buffer_provided_size - decoder->current_block_header.block_size);
      decoder->data_buffer_provided_size -= decoder->current_block_header.block_size;
      /* ストリームを閉じる */
      SLABitStream_Close(&decoder->decoder_core->strm);
      /* ブロック内のオフセットを0に戻す */
      decoder->current_block_sample_offset = 0;
    }
  }

  /* 目標サンプル数きっちりデコードしなければならない */
  SLA_Assert(sample_progress == goal_num_samples);

  /* 出力データサイズの記録 */
  (*num_output_samples) = goal_num_samples;

  return SLA_APIRESULT_OK;
}

/* ストリーミングデコード */
SLAApiResult SLAStreamingDecoder_Decode(struct SLAStreamingDecoder* decoder,
    int32_t** buffer, uint32_t buffer_num_samples, uint32_t* num_output_samples)
{
  uint32_t      tmp_num_output_samples;
  SLAApiResult  ret;

  /* 引数チェック */
  if ((decoder == NULL) || (buffer == NULL)) {
    return SLA_APIRESULT_INVALID_ARGUMENT;
  }

  /* コア処理実行 */
  if ((ret = SLAStreamingDecoder_DecodeCore(decoder,
          buffer, buffer_num_samples, &tmp_num_output_samples))) {
    return ret;
  }

  /* 出力サンプル数の書き出し */
  (*num_output_samples) = tmp_num_output_samples;

  return SLA_APIRESULT_OK;
}
