#include "SLAEncoder.h"
#include "SLAUtility.h"
#include "SLAPredictor.h"
#include "SLACoder.h"
#include "SLABitStream.h"
#include "SLAByteArray.h"
#include "SLAInternal.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>

/* エンコーダハンドル */
struct SLAEncoder {
  struct SLAWaveFormat          wave_format;
  struct SLAEncodeParameter     encode_param;
  uint32_t                      max_num_channels;
  uint32_t                      max_num_block_samples;
  uint32_t                      max_parcor_order;
  uint32_t                      max_longterm_order;
  uint32_t                      max_lms_order_per_filter;
  struct SLABitStream*          strm;
  struct SLACoder*              coder;
  void*                         strm_work;
  struct SLALPCCalculator*        lpcc;   
  struct SLALongTermCalculator*   ltc;
  struct SLALPCSynthesizer**      lpcs;
  struct SLALongTermSynthesizer** ltms;
  struct SLALMSFilter**       nlmsc;
  struct SLAEmphasisFilter**      emp;
  struct SLAOptimalBlockPartitionEstimator* oee;
  SLAChannelProcessMethod	      ch_proc_method;
  SLAWindowFunctionType         window_type;
  double**                      input_double;
  int32_t**                     input_int32;
  double**                      parcor_coef;
  int32_t**                     parcor_coef_int32;
  int32_t**                     parcor_coef_code;
  uint32_t*                     parcor_rshift;
  double**                      longterm_coef;
  int32_t**                     longterm_coef_int32;
  uint32_t*                     pitch_period;
  double*                       window;
  SLABlockDataType              block_data_type;
  int32_t**                     residual;
  int32_t**                     tmp_residual;
  uint32_t*                     num_block_partition_samples;
  uint8_t                       verpose_flag;
};

/* エンコーダハンドルの作成 */
struct SLAEncoder* SLAEncoder_Create(const struct SLAEncoderConfig* config)
{
  struct SLAEncoder* encoder;
  uint32_t ch; 
  uint32_t max_num_channels, max_num_block_samples;

  /* 引数チェック */
  if (config == NULL) {
    return NULL;
  }

  /* 頻繁に参照する変数をオート変数に受ける */
  max_num_channels      = config->max_num_channels;
  max_num_block_samples = config->max_num_block_samples;

  encoder = malloc(sizeof(struct SLAEncoder));
  encoder->max_num_channels         = max_num_channels;
  encoder->max_num_block_samples    = max_num_block_samples;
  encoder->max_parcor_order         = config->max_parcor_order;
  encoder->max_longterm_order       = config->max_longterm_order;
  encoder->max_lms_order_per_filter = config->max_lms_order_per_filter;
  encoder->verpose_flag             = config->verpose_flag;

  /* 各種領域割当て */
  encoder->strm_work              = malloc((size_t)SLABitStream_CalculateWorkSize());
  encoder->input_double           = (double **)malloc(sizeof(double *) * max_num_channels);
  encoder->input_int32            = (int32_t **)malloc(sizeof(int32_t *) * max_num_channels);
  encoder->residual               = (int32_t **)malloc(sizeof(int32_t *) * max_num_channels);
  encoder->tmp_residual           = (int32_t **)malloc(sizeof(int32_t *) * max_num_channels);
  encoder->parcor_coef            = (double **)malloc(sizeof(double *) * max_num_channels);
  encoder->parcor_coef_int32      = (int32_t **)malloc(sizeof(int32_t *) * max_num_channels);
  encoder->parcor_coef_code       = (int32_t **)malloc(sizeof(int32_t *) * max_num_channels);
  encoder->parcor_rshift          = (uint32_t *)malloc(sizeof(uint32_t) * max_num_channels);
  encoder->longterm_coef          = (double **)malloc(sizeof(double *) * max_num_channels);
  encoder->longterm_coef_int32    = (int32_t **)malloc(sizeof(int32_t *) * max_num_channels);

  for (ch = 0; ch < max_num_channels; ch++) {
    encoder->input_double[ch]         = (double *)malloc(sizeof(double) * max_num_block_samples);
    encoder->input_int32[ch]          = (int32_t *)malloc(sizeof(int32_t) * max_num_block_samples);
    encoder->parcor_coef_code[ch]     = (int32_t *)malloc(sizeof(int32_t) * max_num_block_samples);
    encoder->residual[ch]             = (int32_t *)malloc(sizeof(int32_t) * max_num_block_samples);
    encoder->tmp_residual[ch]         = (int32_t *)malloc(sizeof(int32_t) * max_num_block_samples);
    encoder->parcor_coef[ch]          = (double *)malloc(sizeof(double) * (config->max_parcor_order + 1));
    encoder->parcor_coef_int32[ch]    = (int32_t *)malloc(sizeof(int32_t) * (config->max_parcor_order + 1));
    encoder->longterm_coef[ch]        = (double *)malloc(sizeof(double) * config->max_longterm_order);
    encoder->longterm_coef_int32[ch]  = (int32_t *)malloc(sizeof(int32_t) * config->max_longterm_order);
  }

  encoder->pitch_period                 = (uint32_t *)malloc(sizeof(uint32_t) * max_num_channels);
  encoder->window                       = (double *)malloc(sizeof(double) * max_num_block_samples);
  encoder->num_block_partition_samples  = (uint32_t *)malloc(sizeof(uint32_t) * SLAOptimalEncodeEstimator_CalculateMaxNumPartitions(config->max_num_block_samples, SLA_SEARCH_BLOCK_NUM_SAMPLES_DELTA));

  /* ハンドル領域作成 */
  encoder->coder  = SLACoder_Create(config->max_num_channels, SLACODER_NUM_RECURSIVERICE_PARAMETER);
  encoder->lpcc   = SLALPCCalculator_Create(config->max_parcor_order);
  encoder->ltc    = SLALongTermCalculator_Create(SLAUtility_RoundUp2Powered(config->max_num_block_samples * 2), SLALONGTERM_MAX_PERIOD, SLALONGTERM_NUM_PITCH_CANDIDATES, config->max_longterm_order);
  encoder->oee    = SLAOptimalEncodeEstimator_Create(config->max_num_block_samples, SLA_SEARCH_BLOCK_NUM_SAMPLES_DELTA);

  encoder->lpcs     = (struct SLALPCSynthesizer **)malloc(sizeof(struct SLALPCSynthesizer *) * max_num_channels);
  encoder->ltms     = (struct SLALongTermSynthesizer **)malloc(sizeof(struct SLALongTermSynthesizer *) * max_num_channels);
  encoder->nlmsc    = (struct SLALMSFilter **)malloc(sizeof(struct SLALMSFilter *) * max_num_channels);
  encoder->emp      = (struct SLAEmphasisFilter **)malloc(sizeof(struct SLAEmphasisFilter *) * max_num_channels);
  for (ch = 0; ch < max_num_channels; ch++) {
    encoder->lpcs[ch]   = SLALPCSynthesizer_Create(config->max_parcor_order);
    encoder->ltms[ch]   = SLALongTermSynthesizer_Create(config->max_longterm_order, SLALONGTERM_MAX_PERIOD);
    encoder->nlmsc[ch]  = SLALMSFilter_Create(config->max_lms_order_per_filter);
    encoder->emp[ch]    = SLAEmphasisFilter_Create();
  }
  
  return encoder;
}

/* エンコーダハンドルの破棄 */
void SLAEncoder_Destroy(struct SLAEncoder* encoder)
{
  uint32_t ch;

  if (encoder != NULL) {
    for (ch = 0; ch < encoder->max_num_channels; ch++) {
      NULLCHECK_AND_FREE(encoder->input_double[ch]);
      NULLCHECK_AND_FREE(encoder->input_int32[ch]);
      NULLCHECK_AND_FREE(encoder->residual[ch]);
      NULLCHECK_AND_FREE(encoder->tmp_residual[ch]);
      NULLCHECK_AND_FREE(encoder->parcor_coef[ch]);
      NULLCHECK_AND_FREE(encoder->parcor_coef_int32[ch]);
      NULLCHECK_AND_FREE(encoder->parcor_coef_code[ch]);
      NULLCHECK_AND_FREE(encoder->longterm_coef[ch]);
      NULLCHECK_AND_FREE(encoder->longterm_coef_int32[ch]);
    }
    NULLCHECK_AND_FREE(encoder->input_double);
    NULLCHECK_AND_FREE(encoder->input_int32);
    NULLCHECK_AND_FREE(encoder->residual);
    NULLCHECK_AND_FREE(encoder->tmp_residual);
    NULLCHECK_AND_FREE(encoder->parcor_coef);
    NULLCHECK_AND_FREE(encoder->parcor_coef_int32);
    NULLCHECK_AND_FREE(encoder->parcor_coef_code);
    NULLCHECK_AND_FREE(encoder->longterm_coef);
    NULLCHECK_AND_FREE(encoder->longterm_coef_int32);
    NULLCHECK_AND_FREE(encoder->num_block_partition_samples);
    NULLCHECK_AND_FREE(encoder->parcor_rshift);
    SLALPCCalculator_Destroy(encoder->lpcc);
    SLALongTermCalculator_Destroy(encoder->ltc);
    SLAOptimalEncodeEstimator_Destroy(encoder->oee);
    for (ch = 0; ch < encoder->max_num_channels; ch++) {
      SLALPCSynthesizer_Destroy(encoder->lpcs[ch]);
      SLALongTermSynthesizer_Destroy(encoder->ltms[ch]);
      SLALMSFilter_Destroy(encoder->nlmsc[ch]);
      SLAEmphasisFilter_Destroy(encoder->emp[ch]);
    }
    SLACoder_Destroy(encoder->coder);
    NULLCHECK_AND_FREE(encoder->ltms);
    NULLCHECK_AND_FREE(encoder->nlmsc);
    NULLCHECK_AND_FREE(encoder->emp);
    /* Closeを呼ぶと意図せずFlushされるのでメモリ領域だけ開放する */
    NULLCHECK_AND_FREE(encoder->strm_work);
    free(encoder);
  }
}

/* 波形パラメータをエンコーダにセット */
SLAApiResult SLAEncoder_SetWaveFormat(struct SLAEncoder* encoder,
    const struct SLAWaveFormat* wave_format)
{
  /* 引数チェック */
  if (encoder == NULL || wave_format == NULL) {
    return SLA_APIRESULT_INVALID_ARGUMENT;
  }

  /* エンコーダの許容範囲か？ */
  if ((wave_format->num_channels > encoder->max_num_channels)
      || (wave_format->bit_per_sample > 32)) {
    return SLA_APIRESULT_EXCEED_HANDLE_CAPACITY;
  }

  /* パラメータをセット */
  encoder->wave_format = *wave_format;
  return SLA_APIRESULT_OK;
}

/* エンコードパラメータをエンコーダにセット */
SLAApiResult SLAEncoder_SetEncodeParameter(struct SLAEncoder* encoder,
    const struct SLAEncodeParameter* encode_param)
{
  /* 引数チェック */
  if (encoder == NULL || encode_param == NULL) {
    return SLA_APIRESULT_INVALID_ARGUMENT;
  }

  /* エンコーダの許容範囲か？ */
  if ((encode_param->parcor_order > encoder->max_parcor_order)
      || (encode_param->longterm_order > encoder->max_longterm_order)
      || (encode_param->lms_order_per_filter > encoder->max_lms_order_per_filter)
      || (encode_param->max_num_block_samples > encoder->max_num_block_samples)
      || (encode_param->max_num_block_samples < SLA_MIN_BLOCK_NUM_SAMPLES)) {
    return SLA_APIRESULT_EXCEED_HANDLE_CAPACITY;
  }

  /* パラメータをセット */
  encoder->encode_param = *encode_param;
  return SLA_APIRESULT_OK;
}

/* ヘッダ書き出し */
SLAApiResult SLAEncoder_EncodeHeader(
    const struct SLAHeaderInfo* header, uint8_t* data, uint32_t data_size)
{
  uint16_t  crc16;
  uint8_t*  data_pos = data;

  /* 引数チェック */
  if (header == NULL || data == NULL) {
    return SLA_APIRESULT_INVALID_ARGUMENT;
  }

  /* データサイズチェック */
  if (data_size < SLA_HEADER_SIZE) {
    return SLA_APIRESULT_INSUFFICIENT_BUFFER_SIZE;
  }

  /* シグネチャ */
  SLAByteArray_PutUint8(data_pos, (uint8_t)'S');
  SLAByteArray_PutUint8(data_pos, (uint8_t)'L');
  SLAByteArray_PutUint8(data_pos, (uint8_t)'*');
  SLAByteArray_PutUint8(data_pos, (uint8_t)'\1');

  /* 一番最初のデータブロックまでのオフセット */
  SLAByteArray_PutUint32(data_pos, SLA_HEADER_SIZE - 8);
  /* これ以降のフィールドで、ヘッダ末尾までのCRC16（仮値で埋めておく） */
  SLAByteArray_PutUint16(data_pos, 0);
  /* フォーマットバージョン */
  SLAByteArray_PutUint32(data_pos, SLA_FORMAT_VERSION);
  /* チャンネル数 */
  SLAByteArray_PutUint8(data_pos,  (uint8_t)header->wave_format.num_channels);
  /* サンプル数 */
  SLAByteArray_PutUint32(data_pos, header->num_samples);
  /* サンプリングレート */
  SLAByteArray_PutUint32(data_pos, header->wave_format.sampling_rate);
  /* サンプルあたりbit数 */
  SLAByteArray_PutUint8(data_pos,  (uint8_t)header->wave_format.bit_per_sample);
  /* オフセット分の左シフト量 */
  SLAByteArray_PutUint8(data_pos,  header->wave_format.offset_lshift);
  /* PARCOR係数次数 */
  SLAByteArray_PutUint8(data_pos,  (uint8_t)header->encode_param.parcor_order);
  /* ロングターム係数次数 */
  SLAByteArray_PutUint8(data_pos,  (uint8_t)header->encode_param.longterm_order);
  /* LMS次数 */
  SLAByteArray_PutUint8(data_pos,  (uint8_t)header->encode_param.lms_order_per_filter);
  /* チャンネル毎の処理法 */
  SLAByteArray_PutUint8(data_pos,  (uint8_t)header->encode_param.ch_process_method);
  /* SLAブロック数 */
  SLAByteArray_PutUint32(data_pos, header->num_blocks);
  /* SLAブロックあたりサンプル数 */
  SLAByteArray_PutUint16(data_pos, (uint16_t)header->encode_param.max_num_block_samples);
  /* 最大ブロックサイズ */
  SLAByteArray_PutUint32(data_pos, header->max_block_size);
  /* 最大bps */
  SLAByteArray_PutUint32(data_pos, header->max_bit_per_second);

  /* ヘッダサイズチェック */
  SLA_Assert((data_pos - data) == SLA_HEADER_SIZE);

  /* ヘッダCRC16計算 */
  crc16 = SLAUtility_CalculateCRC16(
      &data[SLA_HEADER_CRC16_CALC_START_OFFSET], SLA_HEADER_SIZE - SLA_HEADER_CRC16_CALC_START_OFFSET);
  /* CRC16のフィールドに記録 */
  SLAByteArray_WriteUint16(&data[SLA_HEADER_CRC16_CALC_START_OFFSET - 2], crc16);

  return SLA_APIRESULT_OK;
}

/* 指定されたサンプル数で窓を作成 */
static SLAApiResult SLAEncoder_MakeWindow(struct SLAEncoder* encoder, uint32_t num_samples)
{
  SLA_Assert(encoder != NULL);
  SLA_Assert(num_samples <= encoder->encode_param.max_num_block_samples);

  switch (encoder->encode_param.window_function_type) {
    case SLA_WINDOWFUNCTIONTYPE_RECTANGULAR:
      break;
    case SLA_WINDOWFUNCTIONTYPE_SIN:
      SLAUtility_MakeSinWindow(encoder->window, num_samples);
      break;
    case SLA_WINDOWFUNCTIONTYPE_HANN:
      SLAUtility_MakeHannWindow(encoder->window, num_samples);
      break;
    case SLA_WINDOWFUNCTIONTYPE_BLACKMAN:
      SLAUtility_MakeBlackmanWindow(encoder->window, num_samples);
      break;
    case SLA_WINDOWFUNCTIONTYPE_VORBIS:
      SLAUtility_MakeVorbisWindow(encoder->window, num_samples);
      break;
    default:
      return SLA_APIRESULT_INVALID_WINDOWFUNCTION_TYPE;
      break;
  }

  return SLA_APIRESULT_OK;
}

/* チャンネル毎の処理を実行 */
static SLAApiResult SLAEncoder_ApplyChProcessing(struct SLAEncoder* encoder, uint32_t num_samples)
{
  SLA_Assert(encoder != NULL);
  SLA_Assert(num_samples <= encoder->encode_param.max_num_block_samples);

  /* チャンネル毎の処理が行えるかチェック */
  switch (encoder->encode_param.ch_process_method) {
    case SLA_CHPROCESSMETHOD_STEREO_MS:
      /* MS処理 */
      if (encoder->wave_format.num_channels != 2) {
        return SLA_APIRESULT_INVAILD_CHPROCESSMETHOD;
      }
      break;
    default:
      break;
  }

  switch (encoder->encode_param.ch_process_method) {
    case SLA_CHPROCESSMETHOD_STEREO_MS:
      /* MS処理 */
      SLAUtility_LRtoMSDouble(encoder->input_double, encoder->wave_format.num_channels, num_samples);
      SLAUtility_LRtoMSInt32(encoder->input_int32, encoder->wave_format.num_channels, num_samples);
      break;
    default:
      break;
  }

  return SLA_APIRESULT_OK;
}

/* 最適なブロック分割の探索 */
static SLAApiResult SLAEncoder_SearchOptimalBlockPartitions(struct SLAEncoder* encoder,
    const int32_t* const* input, uint32_t num_samples,
    uint32_t min_num_block_samples, uint32_t delta_num_samples, uint32_t max_num_block_samples,
    uint32_t *optimal_num_partitions, uint32_t *optimal_num_block_samples)
{
  uint32_t ch, smpl;
  uint32_t num_channels, parcor_order;

  /* 引数チェック */
  if (encoder == NULL || input == NULL
      || optimal_num_partitions == NULL || optimal_num_block_samples == NULL) {
    return SLA_APIRESULT_INVALID_ARGUMENT;
  }

  /* サンプル数が最小サンプル数より小さい */
  if (max_num_block_samples < min_num_block_samples) {
    return SLA_APIRESULT_INVALID_ARGUMENT;
  }

  /* 頻繁に参照する変数をオート変数に受ける */
  num_channels  = encoder->wave_format.num_channels;
  parcor_order  = encoder->encode_param.parcor_order;

  /* データを設定 */
  for (ch = 0; ch < num_channels; ch++) {
    for (smpl = 0; smpl < num_samples; smpl++) {
      encoder->input_double[ch][smpl] = (double)input[ch][smpl] * pow(2, -31);
      encoder->input_int32[ch][smpl]  = input[ch][smpl] >> (32 - encoder->wave_format.bit_per_sample);
    }
  }
  /* チャンネル毎の処理実行 */
  SLAEncoder_ApplyChProcessing(encoder, max_num_block_samples);

  /* 無音判定 */
  for (smpl = 0; smpl < num_samples; smpl++) {
    for (ch = 0; ch < num_channels; ch++) {
      if (encoder->input_int32[ch][smpl] != 0) {
        goto DETECT_NOT_SILENCE;
      }
    }
  }

DETECT_NOT_SILENCE:
  /* 最小ブロックサイズ以上の無音が見られたらそれを最適なブロックサイズとする */
  /* 分割数は1 */
  if (smpl >= min_num_block_samples) {
    *optimal_num_partitions        = 1;
    optimal_num_block_samples[0]   = smpl;
    return SLA_APIRESULT_OK;
  }

  /* 最適ブロック分割の探索 */
  if (SLAOptimalEncodeEstimator_SearchOptimalBlockPartitions(
        encoder->oee, encoder->lpcc,
        (const double* const*)encoder->input_double, 
        num_channels, num_samples,
        min_num_block_samples, delta_num_samples, max_num_block_samples,
        encoder->wave_format.bit_per_sample, 
        parcor_order, optimal_num_partitions, optimal_num_block_samples) != SLAPREDICTOR_APIRESULT_OK) {
    return SLA_APIRESULT_FAILED_TO_CALCULATE_COEF;
  }

  return SLA_APIRESULT_OK;
}

/* オフセット分の左シフト量を計算 */
static uint32_t SLAEncoder_CalculateLeftShiftOffset(
    struct SLAEncoder* encoder, const int32_t* const* input, uint32_t num_samples)
{
  uint32_t ch, smpl, minabs_bits;
  uint32_t mask = 0;

  SLA_Assert(encoder != NULL);
  SLA_Assert(input != NULL);

  /* 使用されているビットを検査 */
  for (ch = 0; ch < encoder->wave_format.num_channels; ch++) {
    for (smpl = 0; smpl < num_samples; smpl++) {
      mask |= (uint32_t)input[ch][smpl];
    }
  }

  /* 全入力が0 */
  if (mask == 0) {
    return 0;
  }

  /* ntz（末尾へ続く0の個数）を計算
   * ntz(x) = 32 - nlz(~x & (x-1)), nlz(x) = 31 - log2ceil(x)を使用 */
  minabs_bits = 1 + SLAUtility_Log2Floor(~mask & (mask - 1));
  SLA_Assert(minabs_bits <= 31);

  /* (32-minabs_bits)はダイナミックレンジbitを示す */
  /* 元のビット幅から引くことで元の左シフト量が求まる */
  SLA_Assert(encoder->wave_format.bit_per_sample >= (32 - minabs_bits));
  return encoder->wave_format.bit_per_sample - (32 - minabs_bits);
}

/* 1ブロックエンコード */
SLAApiResult SLAEncoder_EncodeBlock(struct SLAEncoder* encoder,
    const int32_t* const* input, uint32_t num_samples,
    uint8_t* data, uint32_t data_size, uint32_t* output_size)
{
  uint32_t              ch, smpl, ord;
  uint32_t              num_channels, parcor_order, longterm_order;
  uint32_t              bitwidth;
  uint16_t              crc16;
  double                estimated_code_length;
  SLAPredictorApiResult predictor_ret;
  SLAApiResult          api_ret;

  /* 引数チェック */
  if (encoder == NULL || input == NULL
      || data == NULL || output_size == NULL) {
    return SLA_APIRESULT_INVALID_ARGUMENT;
  }

  /* 許容サンプル数を超えている */
  if (num_samples > encoder->max_num_block_samples) {
    return SLA_APIRESULT_EXCEED_HANDLE_CAPACITY;
  }

  /* ブロックヘッダを書く余裕すらない */
  if (data_size <= SLA_BLOCK_HEADER_SIZE) {
    return SLA_APIRESULT_INSUFFICIENT_DATA_SIZE;
  }

  /* 頻繁に参照する変数をオート変数に受ける */
  num_channels    = encoder->wave_format.num_channels;
  parcor_order    = encoder->encode_param.parcor_order;
  longterm_order  = encoder->encode_param.longterm_order;

  /* 窓関数の作成 */
  if ((api_ret = SLAEncoder_MakeWindow(encoder, num_samples)) != SLA_APIRESULT_OK) {
    return api_ret;
  }

  /* 入力をdouble化/情報が失われない程度に右シフト */
  SLA_Assert(encoder->wave_format.bit_per_sample > encoder->wave_format.offset_lshift);
  SLA_Assert((encoder->wave_format.bit_per_sample - encoder->wave_format.offset_lshift) < 32);
  for (ch = 0; ch < num_channels; ch++) {
    for (smpl = 0; smpl < num_samples; smpl++) {
      encoder->input_double[ch][smpl] = (double)input[ch][smpl] * pow(2, -31);
      encoder->input_int32[ch][smpl]  
        = SLAUTILITY_SHIFT_RIGHT_ARITHMETIC(input[ch][smpl], 
            32 - encoder->wave_format.bit_per_sample + encoder->wave_format.offset_lshift);
    }
  }

  /* チャンネル毎の処理 */
  if ((api_ret = SLAEncoder_ApplyChProcessing(encoder, num_samples)) != SLA_APIRESULT_OK) {
    return api_ret;
  }

  /* 無音ブロック判定 */
  encoder->block_data_type = SLA_BLOCK_DATA_TYPE_SILENT;
  for (ch = 0; ch < num_channels; ch++) {
    for (smpl = 0; smpl < num_samples; smpl++) {
      if (encoder->input_int32[ch][smpl] != 0) {
        encoder->block_data_type = SLA_BLOCK_DATA_TYPE_COMPRESSDATA;
        break;
      }
    }
  }

  /* ch毎に残差信号を計算 */
  for (ch = 0; ch < num_channels; ch++) {
    /* 圧縮対象のブロックか否か？ */
    if (encoder->block_data_type != SLA_BLOCK_DATA_TYPE_COMPRESSDATA) {
      /* 以下の処理を全てスキップ */
      continue;
    }

    /* 窓掛け */
    /* 補足）窓掛けとプリエンファシスはほぼ順不同だが、先に窓をかけたほうが僅かに性能が良い */
    SLAUtility_ApplyWindow(encoder->window, encoder->input_double[ch], num_samples); 

    /* doubleデータに対してプリエンファシス処理 */
    SLAEmphasisFilter_PreEmphasisDouble(encoder->input_double[ch], num_samples, SLA_PRE_EMPHASIS_COEFFICIENT_SHIFT);

    /* PARCOR係数を求める */
    if (SLALPCCalculator_CalculatePARCORCoefDouble(encoder->lpcc, 
          encoder->input_double[ch], num_samples,
          encoder->parcor_coef[ch], parcor_order) != SLAPREDICTOR_APIRESULT_OK) {
      return SLA_APIRESULT_FAILED_TO_CALCULATE_COEF;
    }

    /* サンプルあたり推定符号長を計算 */
    if (SLALPCCalculator_EstimateCodeLength(
          encoder->input_double[ch], num_samples, encoder->wave_format.bit_per_sample,
          encoder->parcor_coef[ch], parcor_order, &estimated_code_length) != SLAPREDICTOR_APIRESULT_OK) {
      return SLA_APIRESULT_FAILED_TO_CALCULATE_COEF;
    }
    /* 推定圧縮率（=推定符号長/元の符号長）に変換 */
    estimated_code_length = (8 * estimated_code_length) / encoder->wave_format.bit_per_sample;

    /* 推定圧縮率が閾値以上ならば、予測を諦めて生データ出力を行う */
    if (estimated_code_length >= SLA_ESTIMATE_CODELENGTH_THRESHOLD) {
      encoder->block_data_type = SLA_BLOCK_DATA_TYPE_RAWDATA;
      break;
    }

    /* データのビット幅 */
    bitwidth = SLAUtility_GetDataBitWidth(encoder->input_int32[ch], num_samples);
    /* 係数右シフト量の計算 */
    encoder->parcor_rshift[ch] = SLAUTILITY_CALC_RSHIFT_FOR_SINT32(bitwidth);

    /* 係数量子化 */
    encoder->parcor_coef_int32[ch][0] = 0; /* PARCOR係数の0次成分は0.0で確定だから飛ばす */
    SLA_Assert(encoder->parcor_coef[ch][0] == 0.0f);
    for (ord = 1; ord < parcor_order + 1; ord++) {
      uint32_t qbits = SLA_GET_PARCOR_QUANTIZE_BIT_WIDTH(ord);     /* 量子化ビット数 */
      /* 整数化（符号化する係数） */
      encoder->parcor_coef_code[ch][ord]
        = (int32_t)SLAUtility_Round(encoder->parcor_coef[ch][ord] * pow(2.0f, (qbits - 1)));
      /* roundによる丸めによりビット幅をはみ出てしまうことがある クリップにより対処 */
      encoder->parcor_coef_code[ch][ord] 
        = SLAUTILITY_INNER_VALUE(encoder->parcor_coef_code[ch][ord], -(1 << (qbits - 1)), (1 << (qbits - 1)) - 1);
      /* 16bit幅をベースにシフト */
      encoder->parcor_coef_int32[ch][ord] 
        = encoder->parcor_coef_code[ch][ord] << (16U - qbits);
      /* オーバーフローを防ぐための右シフト */
      encoder->parcor_coef_int32[ch][ord]
        = SLAUTILITY_SHIFT_RIGHT_ARITHMETIC(encoder->parcor_coef_int32[ch][ord], encoder->parcor_rshift[ch]);
    } 

    /* 残差を求める */

    /* doubleデータに適用したものと同様のプリエンファシスフィルタを適用 */
    SLAEmphasisFilter_Reset(encoder->emp[ch]);
    memcpy(encoder->tmp_residual[ch], encoder->input_int32[ch], sizeof(int32_t) * num_samples);
    SLAEmphasisFilter_PreEmphasisInt32(encoder->emp[ch], encoder->tmp_residual[ch], num_samples, SLA_PRE_EMPHASIS_COEFFICIENT_SHIFT);
    /* 残差をプリエンファシス予測による残差に差し替え */
    memcpy(encoder->residual[ch], encoder->tmp_residual[ch], sizeof(int32_t) * num_samples);

    /* PARCORで残差計算 */
    SLALPCSynthesizer_Reset(encoder->lpcs[ch]);
    if (SLALPCSynthesizer_PredictByParcorCoefInt32(encoder->lpcs[ch],
          encoder->residual[ch], num_samples,
          encoder->parcor_coef_int32[ch], parcor_order,
          encoder->tmp_residual[ch]) != SLAPREDICTOR_APIRESULT_OK) {
      return SLA_APIRESULT_FAILED_TO_PREDICT;
    }
    /* 残差をPARCOR予測による残差に差し替え */
    memcpy(encoder->residual[ch], encoder->tmp_residual[ch], sizeof(int32_t) * num_samples);

    /* 残差信号に対してロングターム係数計算 */
    predictor_ret = SLALongTermCalculator_CalculateCoef(encoder->ltc,
          encoder->residual[ch], num_samples,
          &encoder->pitch_period[ch], encoder->longterm_coef[ch],
          longterm_order);
    if ((predictor_ret != SLAPREDICTOR_APIRESULT_OK)
        && (predictor_ret != SLAPREDICTOR_APIRESULT_FAILED_TO_CALCULATION)) {
      return SLA_APIRESULT_FAILED_TO_CALCULATE_COEF;
    }
    /* 計算に失敗している場合はロングターム未使用 */
    if ((predictor_ret == SLAPREDICTOR_APIRESULT_FAILED_TO_CALCULATION)
        || (encoder->pitch_period[ch] >= SLALONGTERM_MAX_PERIOD)) {
      encoder->pitch_period[ch] = 0;
    }

    /* ロングターム係数を符号化/量子化 */
    for (ord = 0; ord < longterm_order; ord++) {
      encoder->longterm_coef_int32[ch][ord] 
        = (int32_t)SLAUtility_Round(encoder->longterm_coef[ch][ord] * pow(2.0f, 15));
      /* 計算自体は31bit精度で行う */
      encoder->longterm_coef_int32[ch][ord] <<= 16;
    }

    /* ロングタームで残差計算 */
    if (encoder->pitch_period[ch] >= SLALONGTERM_MIN_PITCH_THRESHOULD) {
      SLALongTermSynthesizer_Reset(encoder->ltms[ch]);
      if (SLALongTermSynthesizer_PredictInt32(
            encoder->ltms[ch],
            encoder->residual[ch], num_samples,
            encoder->pitch_period[ch], encoder->longterm_coef_int32[ch], longterm_order,
            encoder->tmp_residual[ch]) != SLAPREDICTOR_APIRESULT_OK) {
        return SLA_APIRESULT_FAILED_TO_PREDICT;
      }
      /* 残差をロングタームによる残差に差し替え */
      memcpy(encoder->residual[ch], encoder->tmp_residual[ch], sizeof(int32_t) * num_samples);
    }

    /* LMSで残差計算 */
    SLALMSFilter_Reset(encoder->nlmsc[ch]);
    if (SLALMSFilter_PredictInt32(encoder->nlmsc[ch],
          encoder->encode_param.lms_order_per_filter,
          encoder->residual[ch], num_samples,
          encoder->tmp_residual[ch]) == SLAPREDICTOR_APIRESULT_OK) {
      /* 残差をLMSによる残差に差し替え */
      memcpy(encoder->residual[ch], encoder->tmp_residual[ch], sizeof(int32_t) * num_samples);
    } else {
      return SLA_APIRESULT_FAILED_TO_PREDICT;
    }

  }

  /* 初期パラメータの計算 */
  SLACoder_CalculateInitialRecursiveRiceParameter(encoder->coder, 
      SLACODER_NUM_RECURSIVERICE_PARAMETER, 
      (const int32_t **)encoder->residual, num_channels, num_samples);

  /* 符号化 */

  /* ビットストリーム作成 */
  encoder->strm = SLABitStream_OpenMemory(data,
      data_size, "w", encoder->strm_work, SLABitStream_CalculateWorkSize());
  SLABitStream_Seek(encoder->strm, 0, SLABITSTREAM_SEEK_SET);

  /* ブロックヘッダ書き出し */
  /* 同期コード */
  SLABitStream_PutBits(encoder->strm, 16, SLA_BLOCK_SYNC_CODE);
  /* ブロックサイズ:一旦飛ばす */
  SLABitStream_PutBits(encoder->strm, 32, 0);
  /* CRC16:一旦飛ばす */
  SLABitStream_PutBits(encoder->strm, 16, 0);
  /* ブロックのサンプル数 */
  SLABitStream_PutBits(encoder->strm, 16, num_samples);
  /* ブロックデータタイプ */
  SLABitStream_PutBits(encoder->strm,  2, encoder->block_data_type);

  /* ch毎に係数情報を符号化 */
  for (ch = 0; ch < num_channels; ch++) {
    /* 圧縮データ以外では係数出力をスキップ */
    if (encoder->block_data_type != SLA_BLOCK_DATA_TYPE_COMPRESSDATA) {
      break;
    }

    /* PARCOR係数 */
    /* 右シフト量を記録 */
    SLA_Assert(encoder->parcor_rshift[ch] < (1UL << 4));
    SLABitStream_PutBits(encoder->strm, 4, encoder->parcor_rshift[ch]);
    /* 0次は0.0だから符号化せず飛ばす */
    for (ord = 1; ord < parcor_order + 1; ord++) {
      /* 符号なしで符号化 */
      SLABitStream_PutBits(encoder->strm, SLA_GET_PARCOR_QUANTIZE_BIT_WIDTH(ord), 
          SLAUTILITY_SINT32_TO_UINT32(encoder->parcor_coef_code[ch][ord]));
    }

    /* ピッチ周期/ロングターム係数 */
    if (encoder->pitch_period[ch] >= SLALONGTERM_MIN_PITCH_THRESHOULD) {
      SLABitStream_PutBit(encoder->strm, 1);
      SLABitStream_PutBits(encoder->strm, 10, encoder->pitch_period[ch]);
      for (ord = 0; ord < longterm_order; ord++) {
        SLABitStream_PutBits(encoder->strm, 16, 
            SLAUTILITY_SINT32_TO_UINT32(
              SLAUTILITY_SHIFT_RIGHT_ARITHMETIC(encoder->longterm_coef_int32[ch][ord], 16)));
      }
    } else {
      /* ロングターム未使用であることをマーク */
      SLABitStream_PutBit(encoder->strm, 0);
    }

    /* 再帰的ライス符号パラメータを符号化 */
    SLACoder_PutInitialRecursiveRiceParameter(encoder->coder,
        encoder->strm, SLACODER_NUM_RECURSIVERICE_PARAMETER,
        encoder->wave_format.bit_per_sample, ch);
  }

  /* ここまでがブロックヘッダ. バイト境界に揃える */
  SLABitStream_Flush(encoder->strm);

  /* データ符号化 */
  switch (encoder->block_data_type) {
    case SLA_BLOCK_DATA_TYPE_RAWDATA:
      /* 圧縮を断念している。生データを符号なし整数化して書き出す */
      {
        uint32_t output_bits[SLA_MAX_CHANNELS];
        /* ビット幅を設定 */
        for (ch = 0; ch < num_channels; ch++) {
          /* 左シフトしている場合があるのでその分は書き出しビット幅を減らす */
          SLA_Assert(encoder->wave_format.bit_per_sample > encoder->wave_format.offset_lshift);
          output_bits[ch] = encoder->wave_format.bit_per_sample - encoder->wave_format.offset_lshift;
          /* MS処理を行った場合の2ch目はL-Rが入っているのでビット幅が1増える */
          if ((ch == 1)
              && (encoder->encode_param.ch_process_method == SLA_CHPROCESSMETHOD_STEREO_MS)) {
            output_bits[ch] += 1;
          }
        }
        /* チャンネルインターリーブしつつ符号化 */
        for (smpl = 0; smpl < num_samples; smpl++) {
          for (ch = 0; ch < num_channels; ch++) {
            SLABitStream_PutBits(encoder->strm, output_bits[ch],
                SLAUTILITY_SINT32_TO_UINT32(encoder->input_int32[ch][smpl]));
          }
        }
      }
      break;
    case SLA_BLOCK_DATA_TYPE_COMPRESSDATA:
      /* 残差符号化 */
      SLACoder_PutDataArray(encoder->coder, encoder->strm, 
          SLACODER_NUM_RECURSIVERICE_PARAMETER,
          (const int32_t **)encoder->residual, num_channels, num_samples);
      break;
    case SLA_BLOCK_DATA_TYPE_SILENT:
      /* 無音の場合は何もしない */
      break;
    default:
      /* ここに入ってきたらプログラミングミス */
      SLA_Assert(0);
      break;
  }

  /* バイト境界に揃える */
  SLABitStream_Flush(encoder->strm);

  /* 出力サイズの取得 */
  SLABitStream_Tell(encoder->strm, (int32_t *)output_size);

  /* ブロックCRC16計算 */
  crc16 = SLAUtility_CalculateCRC16(
      &data[SLA_BLOCK_CRC16_CALC_START_OFFSET],
      (*output_size) - SLA_BLOCK_CRC16_CALC_START_OFFSET);

  /* オフセット / CRC16値の書き込み */
  SLABitStream_Seek(encoder->strm, 
      SLABITSTREAM_SEEK_SET, SLA_BLOCK_CRC16_CALC_START_OFFSET - 2 - 4);  /* オフセットの書き込み位置に移動 */
  SLABitStream_PutBits(encoder->strm, 32, (*output_size) - 2 - 4);        /* 同期コード(2byte)とオフセット自体(4byte)を除く */
  SLABitStream_PutBits(encoder->strm, 16, crc16);

  /* ビットストリーム破棄 */
  SLABitStream_Close(encoder->strm);

  return SLA_APIRESULT_OK;
}

/* ヘッダを含めて全ブロックエンコード */
SLAApiResult SLAEncoder_EncodeWhole(struct SLAEncoder* encoder,
    const int32_t* const* input, uint32_t num_samples,
    uint8_t* data, uint32_t data_size, uint32_t* output_size)
{
  uint32_t              ch, part;
  uint32_t              num_partitions;
  uint32_t              encode_offset_sample, num_remain_samples;
  uint32_t              cur_output_size, block_size, max_block_size, num_blocks;
  const int32_t*        input_ptr[SLA_MAX_CHANNELS];
  struct SLAHeaderInfo  header;
  SLAApiResult          api_ret;
  uint32_t              max_bit_per_second;

  /* 引数チェック */
  if (encoder == NULL || input == NULL || data == NULL || output_size == NULL) {
    return SLA_APIRESULT_INVALID_ARGUMENT;
  }

  /* ヘッダ情報設定 */
  header.wave_format    = encoder->wave_format;
  header.encode_param   = encoder->encode_param;
  header.num_samples    = num_samples;
  header.max_block_size = SLA_MAX_BLOCK_SIZE_INVAILD; /* ひとまず未知とする */

  /* 仮のヘッダの書き出し */
  if ((api_ret = SLAEncoder_EncodeHeader(&header, data, data_size))
      != SLA_APIRESULT_OK) {
    return api_ret;
  }

  /* オフセット分の左シフト量を解析 */
  header.wave_format.offset_lshift
    = encoder->wave_format.offset_lshift
    = (uint8_t)SLAEncoder_CalculateLeftShiftOffset(encoder, input, num_samples);
  SLA_Assert(encoder->wave_format.bit_per_sample > encoder->wave_format.offset_lshift);

  /* 全ブロックを逐次エンコード */
  cur_output_size = SLA_HEADER_SIZE;
  max_block_size = 0;
  encode_offset_sample = 0;
  num_blocks = 0;
  max_bit_per_second = 0;
  while (encode_offset_sample < num_samples) {
    /* 出力バッファサイズが足らない */
    if (cur_output_size >= data_size) {
      return SLA_APIRESULT_INSUFFICIENT_BUFFER_SIZE;
    }

    /* 入力信号のポインタをセット */
    for (ch = 0; ch < encoder->wave_format.num_channels; ch++) {
      input_ptr[ch] = &input[ch][encode_offset_sample];
    }

    /* 残りサンプル */
    num_remain_samples = num_samples - encode_offset_sample;

    /* 最適なブロック分割の探索 */
    if ((api_ret = SLAEncoder_SearchOptimalBlockPartitions(encoder,
            input_ptr,
            SLAUTILITY_MIN(encoder->encode_param.max_num_block_samples, num_remain_samples),
            SLAUTILITY_MIN(SLA_MIN_BLOCK_NUM_SAMPLES, num_remain_samples),
            SLA_SEARCH_BLOCK_NUM_SAMPLES_DELTA,
            SLAUTILITY_MIN(encoder->encode_param.max_num_block_samples, num_remain_samples),
            &num_partitions, encoder->num_block_partition_samples)) != SLA_APIRESULT_OK) {
      return api_ret;
    }

    /* 分割に従ってエンコード */
    for (part = 0; part < num_partitions; part++) {
      uint32_t  block_bit_per_second;
      uint32_t  num_encode_samples = encoder->num_block_partition_samples[part];
      /* 入力信号のポインタをセット */
      for (ch = 0; ch < encoder->wave_format.num_channels; ch++) {
        input_ptr[ch] = &input[ch][encode_offset_sample];
      }
      /* ブロックエンコード */
      if ((api_ret = SLAEncoder_EncodeBlock(encoder,
              input_ptr, num_encode_samples,
              &data[cur_output_size], data_size - cur_output_size,
              &block_size)) != SLA_APIRESULT_OK) {
        return api_ret;
      }
      /* 出力データサイズの更新 */
      cur_output_size += block_size;
      /* エンコードしたサンプル数の更新 */
      encode_offset_sample += num_encode_samples;
      /* 最大ブロックサイズの記録 */
      if (block_size > max_block_size) {
        max_block_size = block_size;
      }
      /* 最大bpsの計算 */
      block_bit_per_second = (8 * block_size * encoder->wave_format.sampling_rate) / num_encode_samples;
      if (block_bit_per_second > max_bit_per_second) {
        max_bit_per_second = block_bit_per_second;
      }
      /* ブロック数増加 */
      num_blocks++;
    }

    /* 進捗表示 */
    if (encoder->verpose_flag != 0) {
      uint32_t output_original_size 
        = encode_offset_sample * encoder->wave_format.num_channels * encoder->wave_format.bit_per_sample / 8;
      printf("progress:%2d%% (compress ratio:%3.1f %%)\r",
          (100 * encode_offset_sample) / num_samples,
          ((double)cur_output_size / output_original_size) * 100);
      fflush(stdout);
    }
  }

  /* 最終ブロックを書き出したところでオーバーランを検知 */
  if (cur_output_size > data_size) {
    return SLA_APIRESULT_INSUFFICIENT_DATA_SIZE;
  }

  /* ブロック数, 最大ブロックサイズ, 最大bpsを反映（ヘッダの再度書き込み） */
  header.num_blocks         = num_blocks;
  header.max_block_size     = max_block_size;
  header.max_bit_per_second = max_bit_per_second;
  if ((api_ret = SLAEncoder_EncodeHeader(&header, data, data_size))
      != SLA_APIRESULT_OK) {
    return api_ret;
  }

  /* 出力サイズの書き込み */
  *output_size = cur_output_size;

  return SLA_APIRESULT_OK;
}
