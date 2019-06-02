#include "SLADecoder.h"
#include "SLAUtility.h"
#include "SLAPredictor.h"
#include "SLACoder.h"
#include "SLABitStream.h"
#include "SLAByteArray.h"
#include "SLAInternal.h"

#include <stdlib.h>
#include <string.h>

/* デコーダハンドル */
struct SLADecoder {
  struct SLAWaveFormat          wave_format;
  struct SLAEncodeParameter     encode_param;
  uint32_t                      max_num_channels;
  uint32_t                      max_num_block_samples;
  uint32_t                      max_parcor_order;
  uint32_t                      max_longterm_order;
  uint32_t                      max_lms_order_par_filter;
  uint8_t                       enable_crc_check;
  struct SLABitStream*          strm;
  void*                         strm_work;

  struct SLALPCSynthesizer*     lpcs;
  struct SLALMSCalculator*      nlmsc;

  int32_t**                     parcor_coef;
  int32_t**                     longterm_coef;
  uint32_t*                     pitch_period;

  uint8_t*                      is_silence_block;
  int32_t**                     residual;
  int32_t**                     output;
  uint8_t                       verpose_flag;
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
  decoder->max_lms_order_par_filter = config->max_lms_order_par_filter;
  decoder->enable_crc_check         = config->enable_crc_check;
  decoder->verpose_flag             = config->verpose_flag;

  /* 各種領域割り当て */
  decoder->strm_work     = malloc((size_t)SLABitStream_CalculateWorkSize());
  decoder->parcor_coef   = (int32_t **)malloc(sizeof(int32_t*) * max_num_channels);
  decoder->longterm_coef = (int32_t **)malloc(sizeof(int32_t*) * max_num_channels);
  decoder->pitch_period  = (uint32_t *)malloc(sizeof(uint32_t) * max_num_channels);
  decoder->residual      = (int32_t **)malloc(sizeof(int32_t*) * max_num_channels);
  decoder->output        = (int32_t **)malloc(sizeof(int32_t*) * max_num_channels);
  decoder->is_silence_block     = (uint8_t *)malloc(sizeof(uint8_t) * max_num_channels);
  for (ch = 0; ch < max_num_channels; ch++) {
    decoder->parcor_coef[ch]   = (int32_t *)malloc(sizeof(int32_t) * (config->max_parcor_order + 1));
    decoder->longterm_coef[ch] = (int32_t *)malloc(sizeof(int32_t) * config->max_longterm_order);
    decoder->residual[ch]      = (int32_t *)malloc(sizeof(int32_t) * max_num_block_samples);
    decoder->output[ch]        = (int32_t *)malloc(sizeof(int32_t) * max_num_block_samples);
  }

  /* 合成ハンドル作成 */
  decoder->lpcs   = SLALPCSynthesizer_Create(config->max_parcor_order);
  decoder->nlmsc  = SLALMSCalculator_Create(config->max_lms_order_par_filter);
   
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
    NULLCHECK_AND_FREE(decoder->is_silence_block);
    SLALPCSynthesizer_Destroy(decoder->lpcs);
    SLALMSCalculator_Destroy(decoder->nlmsc);
    NULLCHECK_AND_FREE(decoder->strm_work);
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
        || (signature[2] != '*') || (signature[3] != ' ')) {
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
  /* PARCOR係数次数 */
  SLAByteArray_GetUint8(data_pos, &u8buf);
  tmp_header.encode_param.parcor_order      = (uint32_t)u8buf;
  /* ロングターム係数次数 */
  SLAByteArray_GetUint8(data_pos, &u8buf);
  tmp_header.encode_param.longterm_order    = (uint32_t)u8buf;
  /* LMS次数 */
  SLAByteArray_GetUint8(data_pos, &u8buf);
  tmp_header.encode_param.lms_order_par_filter = (uint32_t)u8buf;
  /* LMSカスケード数 */
  SLAByteArray_GetUint8(data_pos, &u8buf);
  tmp_header.encode_param.num_lms_filter_cascade = (uint32_t)u8buf;
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
      || (encode_param->lms_order_par_filter > decoder->max_lms_order_par_filter)
      || (encode_param->max_num_block_samples > decoder->max_num_block_samples)
      || (encode_param->max_num_block_samples < SLA_MIN_BLOCK_NUM_SAMPLES)) {
    return SLA_APIRESULT_EXCEED_HANDLE_CAPACITY;
  }

  /* パラメータをセット */
  decoder->encode_param = *encode_param;
  return SLA_APIRESULT_OK;
}

/* 1ブロックデコード */
SLAApiResult SLADecoder_DecodeBlock(struct SLADecoder* decoder,
    const uint8_t* data, uint32_t data_size,
    int32_t** buffer, uint32_t buffer_num_samples,
    uint32_t* output_block_size, uint32_t* output_num_samples)
{
  uint8_t  bit;
  uint64_t bitsbuf;
  uint32_t ch, ord, smpl;
  uint32_t block_samples, num_channels, parcor_order, longterm_order;
  uint32_t next_block_offset;
  uint16_t crc16;

  /* 引数チェック */
  if (decoder == NULL || data == NULL || buffer == NULL
      || output_block_size == NULL || output_num_samples == NULL) {
    return SLA_APIRESULT_INVALID_ARGUMENT;
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

  /* 頻繁に参照する変数をオート変数に受ける */
  num_channels    = decoder->wave_format.num_channels;
  parcor_order    = decoder->encode_param.parcor_order;
  longterm_order  = decoder->encode_param.longterm_order;

  /* ビットストリーム作成 */
  decoder->strm = SLABitStream_OpenMemory((uint8_t *)data,
      data_size, "r", decoder->strm_work, SLABitStream_CalculateWorkSize());
  SLABitStream_Seek(decoder->strm, 0, SLABITSTREAM_SEEK_SET);

  /* 同期コード */
  SLABitStream_GetBits(decoder->strm, 16, &bitsbuf);
  if (bitsbuf != 0xFFFF) {
    return SLA_APIRESULT_FAILED_TO_FIND_SYNC_CODE;
  }
  /* 次のブロックまでのオフセット */
  SLABitStream_GetBits(decoder->strm, 32, &bitsbuf);
  next_block_offset = (uint32_t)bitsbuf;
  /* これ以降のフィールドのCRC16値 */
  SLABitStream_GetBits(decoder->strm, 16, &bitsbuf);
  crc16 = (uint16_t)bitsbuf;
  /* CRC16チェック */
  if (decoder->enable_crc_check == 1) {
    uint16_t calc_crc16 = SLAUtility_CalculateCRC16(
        &data[SLA_BLOCK_CRC16_CALC_START_OFFSET], next_block_offset - 2); /* CRC16を記録したフィールドを除く */
    if (calc_crc16 != crc16) {
      /* 不一致を検出 */
      return SLA_APIRESULT_DETECT_DATA_CORRUPTION;
    }
  }
  /* ブロックサンプル数 */
  SLABitStream_GetBits(decoder->strm, 16, &bitsbuf);
  block_samples = (uint32_t)bitsbuf;
  if (block_samples > buffer_num_samples) {
    return SLA_APIRESULT_INSUFFICIENT_DATA_SIZE;
  }
  /* printf("next:%d crc16:%04X nsmpl:%d \n", next_block_offset, crc16, block_samples); */

  /* 各チャンネルの無音フラグ/係数取得 */
  for (ch = 0; ch < num_channels; ch++) {
    uint32_t rshift;

    /* 無音フラグ取得 */
    SLABitStream_GetBit(decoder->strm, &decoder->is_silence_block[ch]);

    /* 無音ブロックを実際に無音で埋め、以降の係数取得をスキップ */
    if (decoder->is_silence_block[ch] == 1) {
      memset(decoder->output[ch], 0, sizeof(int32_t) * block_samples);
      continue;
    }

    /* PARCOR係数読み取り */
    /* 右シフト量 */
    SLABitStream_GetBits(decoder->strm, 3, &bitsbuf);
    rshift = (uint32_t)bitsbuf;
    /* 0次は0で確定 */
    decoder->parcor_coef[ch][0] = 0;
    for (ord = 1; ord < parcor_order + 1; ord++) {
      uint32_t qbits;
      if (ord < SLAPARCOR_COEF_LOW_ORDER_THRESHOULD) {
        qbits = 16;
      } else {
        qbits = 8;
      }
      /* PARCOR係数 */
      SLABitStream_GetBits(decoder->strm, qbits, &bitsbuf);
      decoder->parcor_coef[ch][ord] = SLAUTILITY_UINT32_TO_SINT32(bitsbuf);
      /* 16bitをベースに右シフト */
      decoder->parcor_coef[ch][ord] <<= (16U - qbits);
      decoder->parcor_coef[ch][ord] 
        = SLAUTILITY_SHIFT_RIGHT_ARITHMETIC(decoder->parcor_coef[ch][ord], rshift);
    }

    /* ロングターム係数読み取り */
    SLABitStream_GetBit(decoder->strm, &bit);
    if (bit == 0) {
      /* ピッチ周期無効値の0でマーク */
      decoder->pitch_period[ch] = 0;
    } else {
      SLABitStream_GetBits(decoder->strm, SLALONGTERM_PERIOD_NUM_BITS, &bitsbuf);
      decoder->pitch_period[ch] = (uint32_t)bitsbuf;
      for (ord = 0; ord < longterm_order; ord++) {
        SLABitStream_GetBits(decoder->strm, 16, &bitsbuf);
        decoder->longterm_coef[ch][ord] = SLAUTILITY_UINT32_TO_SINT32(bitsbuf);
        decoder->longterm_coef[ch][ord] <<= 16;
      }
    }
  }

  /* チャンネル毎に残差を復号/音声合成 */
  for (ch = 0; ch < num_channels; ch++) {
    /* 無音ブロックはスキップ */
    if (decoder->is_silence_block[ch] == 1) { continue; }

    /* 残差信号をデコード */
    SLACoder_GetDataArray(decoder->strm, decoder->residual[ch], block_samples);

    /* LMSの残差分を合成 */
    for (ord = 0; ord < decoder->encode_param.num_lms_filter_cascade; ord++) {
      if (SLALMSCalculator_SynthesizeInt32(decoder->nlmsc,
            decoder->encode_param.lms_order_par_filter,
            decoder->residual[ch], block_samples,
            decoder->output[ch]) != SLAPREDICTOR_APIRESULT_OK) {
        return SLA_APIRESULT_FAILED_TO_SYNTHESIZE;
      }
      /* 合成した信号で残差を差し替え */
      memcpy(decoder->residual[ch], 
          decoder->output[ch], sizeof(int32_t) * block_samples);
    }

    /* ロングタームの残差分を合成 */
    if (decoder->pitch_period[ch] != 0) {
      if (SLALongTerm_SynthesizeInt32(
            decoder->residual[ch], block_samples,
            decoder->pitch_period[ch], decoder->longterm_coef[ch],
            longterm_order, decoder->output[ch]) != SLAPREDICTOR_APIRESULT_OK) {
        return SLA_APIRESULT_FAILED_TO_SYNTHESIZE;
      }
      /* 合成した信号で残差を差し替え */
      memcpy(decoder->residual[ch], 
          decoder->output[ch], sizeof(int32_t) * block_samples);
    }

    /* PARCORの残差分を合成 */
    if (SLALPCSynthesizer_SynthesizeByParcorCoefInt32(decoder->lpcs,
          decoder->residual[ch], block_samples,
          decoder->parcor_coef[ch], parcor_order,
          decoder->output[ch]) != SLAPREDICTOR_APIRESULT_OK) {
      return SLA_APIRESULT_FAILED_TO_SYNTHESIZE;
    }

    /* デエンファシス */
    SLAUtility_DeEmphasisInt32(decoder->output[ch], block_samples, SLA_PRE_EMPHASIS_COEFFICIENT_SHIFT);
  }

  /* チャンネル毎の処理をしていたら元に戻す */
  switch (decoder->encode_param.ch_process_method) {
    case SLA_CHPROCESSMETHOD_STEREO_MS:
      SLAUtility_MStoLRInt32(decoder->output, decoder->wave_format.num_channels, block_samples);
      break;
    default:
      break;
  }

  /* 最終結果をバッファにコピー */
  for (ch = 0; ch < num_channels; ch++) {
    for (smpl = 0; smpl < block_samples; smpl++) {
      buffer[ch][smpl] = decoder->output[ch][smpl] << (32 - decoder->wave_format.bit_per_sample);
    }
  }

  /* 出力サンプル数の取得 */
  *output_num_samples = block_samples;
  /* 出力サイズの取得 */
  SLABitStream_Tell(decoder->strm, (int32_t *)output_block_size);

  /* ビットストリームクローズ */
  SLABitStream_Close(decoder->strm);

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
      printf("progress:%d%% \r", (100 * decode_offset_byte) / data_size);
      fflush(stdout);
    }
  }

  /* 出力サンプル数を記録 */
  *output_num_samples = decode_offset_sample;

  return SLA_APIRESULT_OK;
}
