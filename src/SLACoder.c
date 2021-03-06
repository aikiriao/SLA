#include "SLACoder.h"
#include "SLAUtility.h"
#include "SLAInternal.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* 固定小数の小数部ビット数 */
#define SLACODER_NUM_FRACTION_PART_BITS         8
/* 固定小数の0.5 */
#define SLACODER_FIXED_FLOAT_0_5                (1UL << ((SLACODER_NUM_FRACTION_PART_BITS) - 1))
/* 符号なし整数を固定小数に変換 */
#define SLACODER_UINT32_TO_FIXED_FLOAT(u32)     ((u32) << (SLACODER_NUM_FRACTION_PART_BITS))
/* 固定小数を符号なし整数に変換 */
#define SLACODER_FIXED_FLOAT_TO_UINT32(fixed)   (uint32_t)(((fixed) + (SLACODER_FIXED_FLOAT_0_5)) >> (SLACODER_NUM_FRACTION_PART_BITS))
/* ゴロム符号パラメータ直接設定 */
#define SLACODER_PARAMETER_SET(param_array, order, val) {\
  ((param_array)[(order)]) = SLACODER_UINT32_TO_FIXED_FLOAT(val); \
}
/* ゴロム符号パラメータ取得 : 1以上であることを担保 */
#define SLACODER_PARAMETER_GET(param_array, order) \
  (SLAUTILITY_MAX(SLACODER_FIXED_FLOAT_TO_UINT32((param_array)[(order)]), 1UL))
/* Rice符号のパラメータ更新式 */
/* 指数平滑平均により平均値を推定 */
#define SLARICE_PARAMETER_UPDATE(param_array, order, code) {\
  (param_array)[(order)] = (SLARecursiveRiceParameter)(119 * (param_array)[(order)] + 9 * SLACODER_UINT32_TO_FIXED_FLOAT(code) + (1UL << 6)) >> 7; \
}
/* Rice符号のパラメータ計算 2 ** ceil(log2(E(x)/2)) = E(x)/2の2の冪乗切り上げ */
#define SLARICE_CALCULATE_RICE_PARAMETER(param_array, order) \
  SLAUTILITY_ROUNDUP2POWERED(SLAUTILITY_MAX(SLACODER_FIXED_FLOAT_TO_UINT32((param_array)[(order)] >> 1), 1UL))

/* 再帰的ライス符号パラメータ型 */
typedef uint64_t SLARecursiveRiceParameter;

/* 符号化ハンドル */
struct SLACoder {
  SLARecursiveRiceParameter** rice_parameter;
  SLARecursiveRiceParameter** init_rice_parameter;
  uint32_t                    max_num_channels;
  uint32_t                    max_num_parameters;
};

/* ゴロム符号化の出力 */
static void SLAGolomb_PutCode(struct SLABitStream* strm, uint32_t m, uint32_t val)
{
  uint32_t quot;
  uint32_t rest;
  uint32_t b, two_b;

  SLA_Assert(strm != NULL);
  SLA_Assert(m != 0);

  /* 商部分長と剰余部分の計算 */
  quot = val / m;
  rest = val % m;

  /* 前半部分の出力(unary符号) */
  while (quot > 0) {
    SLABitWriter_PutBits(strm, 0, 1);
    quot--;
  }
  SLABitWriter_PutBits(strm, 1, 1);

  /* 剰余部分の出力 */
  if (SLAUTILITY_IS_POWERED_OF_2(m)) {
    /* mが2の冪: ライス符号化 m == 1の時は剰余0だから何もしない */
    if (m > 1) {
      SLABitWriter_PutBits(strm, rest, SLAUTILITY_LOG2CEIL(m));
    }
    return;
  }

  /* ゴロム符号化 */
  b = SLAUTILITY_LOG2CEIL(m);
  two_b = (uint32_t)(1UL << b);
  if (rest < (two_b - m)) {
    SLABitWriter_PutBits(strm, rest, b - 1);
  } else {
    SLABitWriter_PutBits(strm, rest + two_b - m, b);
  }
}

/* ゴロム符号化の取得 */
static uint32_t SLAGolomb_GetCode(struct SLABitStream* strm, uint32_t m) 
{
  uint32_t quot;
  uint64_t rest;
  uint32_t b, two_b;

  SLA_Assert(strm != NULL);
  SLA_Assert(m != 0);

  /* 前半のunary符号部分を読み取り */
  SLABitReader_GetZeroRunLength(strm, &quot);

  /* 剰余部分の読み取り */
  if (SLAUTILITY_IS_POWERED_OF_2(m)) {
    /* mが2の冪: ライス符号化 */
    SLABitReader_GetBits(strm, &rest, SLAUTILITY_LOG2CEIL(m));
    return (uint32_t)(quot * m + rest);
  }

  /* ゴロム符号化 */
  b = SLAUTILITY_LOG2CEIL(m);
  two_b = (uint32_t)(1UL << b);
  SLABitReader_GetBits(strm, &rest, b - 1);
  if (rest < (two_b - m)) {
    return (uint32_t)(quot * m + rest);
  } else {
    uint64_t buf;
    rest <<= 1;
    SLABitReader_GetBits(strm, &buf, 1);
    rest += buf;
    return (uint32_t)(quot * m + rest - (two_b - m));
  }
}

/* ガンマ符号の出力 */
static void SLAGamma_PutCode(struct SLABitStream* strm, uint32_t val)
{
  uint32_t ndigit;

  SLA_Assert(strm != NULL);

  if (val == 0) {
    /* 符号化対象が0ならば1を出力して終了 */
    SLABitWriter_PutBits(strm, 1, 1);
    return;
  } 

  /* 桁数を取得 */
  ndigit = SLAUTILITY_LOG2CEIL(val + 2);
  /* 桁数-1だけ0を続ける */
  SLABitWriter_PutBits(strm, 0, ndigit - 1);
  /* 桁数を使用して符号語を2進数で出力 */
  SLABitWriter_PutBits(strm, val + 1, ndigit);
}

/* ガンマ符号の取得 */
static uint32_t SLAGamma_GetCode(struct SLABitStream* strm)
{
  uint32_t ndigit;
  uint64_t bitsbuf;

  SLA_Assert(strm != NULL);

  /* 桁数を取得 */
  /* 1が出現するまで桁数を増加 */
  SLABitReader_GetZeroRunLength(strm, &ndigit);
  /* 最低でも1のため下駄を履かせる */
  ndigit++;

  /* 桁数が1のときは0 */
  if (ndigit == 1) {
    return 0;
  }

  /* 桁数から符号語を出力 */
  SLABitReader_GetBits(strm, &bitsbuf, ndigit - 1);
  return (uint32_t)((1UL << (ndigit - 1)) + bitsbuf - 1);
}

/* 商部分（アルファ符号）を出力 */
static void SLARecursiveRice_PutQuotPart(
    struct SLABitStream* strm, uint32_t quot)
{
  SLA_Assert(strm != NULL);

  while (quot > 0) {
    SLABitWriter_PutBits(strm, 0, 1);
    quot--;
  }
  SLABitWriter_PutBits(strm, 1, 1);
}

/* 商部分（アルファ符号）を取得 */
static uint32_t SLARecursiveRice_GetQuotPart(struct SLABitStream* strm)
{
  uint32_t quot;
  
  SLA_Assert(strm != NULL);

  SLABitReader_GetZeroRunLength(strm, &quot);

  return quot;
}

/* 剰余部分を出力 */
static void SLARecursiveRice_PutRestPart(
    struct SLABitStream* strm, uint32_t val, uint32_t m)
{
  SLA_Assert(strm != NULL);
  SLA_Assert(m != 0);
  SLA_Assert(SLAUTILITY_IS_POWERED_OF_2(m));

  /* m == 1の時はスキップ（剰余は0で確定だから） */
  if (m != 1) {
    SLABitWriter_PutBits(strm, val & (m - 1), SLAUTILITY_LOG2CEIL(m));
  }
}

/* 剰余部分を取得 */
static uint32_t SLARecursiveRice_GetRestPart(struct SLABitStream* strm, uint32_t m)
{
  uint64_t rest;

  SLA_Assert(strm != NULL);
  SLA_Assert(m != 0);
  SLA_Assert(SLAUTILITY_IS_POWERED_OF_2(m));

  /* 1の剰余は0 */
  if (m == 1) {
    return 0;
  }

  /* ライス符号の剰余部分取得 */
  SLABitReader_GetBits(strm, &rest, SLAUTILITY_LOG2CEIL(m));
  
  return (uint32_t)rest;
}

/* 再帰的ライス符号の出力 */
static void SLARecursiveRice_PutCode(
    struct SLABitStream* strm, SLARecursiveRiceParameter* rice_parameters, uint32_t num_params, uint32_t val)
{
  uint32_t i, reduced_val, param;

  SLA_Assert(strm != NULL);
  SLA_Assert(rice_parameters != NULL);
  SLA_Assert(num_params != 0);
  SLA_Assert(SLACODER_PARAMETER_GET(rice_parameters, 0) != 0);

  reduced_val = val;
  for (i = 0; i < (num_params - 1); i++) {
    param = SLARICE_CALCULATE_RICE_PARAMETER(rice_parameters, i);
    /* 現在のパラメータ値よりも小さければ、符号化を行う */
    if (reduced_val < param) {
        /* 商部分としてはパラメータ段数 */
        SLARecursiveRice_PutQuotPart(strm, i);
        /* 剰余部分 */
        SLARecursiveRice_PutRestPart(strm, reduced_val, param);
        /* パラメータ更新 */
        SLARICE_PARAMETER_UPDATE(rice_parameters, i, reduced_val);
        break;
    }
    /* パラメータ更新 */
    SLARICE_PARAMETER_UPDATE(rice_parameters, i, reduced_val);
    /* 現在のパラメータ値で減じる */
    reduced_val -= param;
  }

  /* 末尾のパラメータに達した */
  if (i == (num_params - 1)) {
    uint32_t tail_param = SLARICE_CALCULATE_RICE_PARAMETER(rice_parameters, i);
    uint32_t tail_quot  = i + reduced_val / tail_param;
    SLA_Assert(SLAUTILITY_IS_POWERED_OF_2(tail_param));
    /* 商が大きい場合はガンマ符号を使用する */
    if (tail_quot < SLACODER_QUOTPART_THRESHOULD) {
      SLARecursiveRice_PutQuotPart(strm, tail_quot);
    } else {
      SLARecursiveRice_PutQuotPart(strm, SLACODER_QUOTPART_THRESHOULD);
      SLAGamma_PutCode(strm, tail_quot - SLACODER_QUOTPART_THRESHOULD);
    }
    SLARecursiveRice_PutRestPart(strm, reduced_val, tail_param);
    /* パラメータ更新 */
    SLARICE_PARAMETER_UPDATE(rice_parameters, i, reduced_val);
  }

}

/* 再帰的ライス符号の取得 */
static uint32_t SLARecursiveRice_GetCode(
    struct SLABitStream* strm, SLARecursiveRiceParameter* rice_parameters, uint32_t num_params)
{
  uint32_t  i, quot, val;
  uint32_t  param, tmp_val;

  SLA_Assert(strm != NULL);
  SLA_Assert(rice_parameters != NULL);
  SLA_Assert(num_params != 0);
  SLA_Assert(SLACODER_PARAMETER_GET(rice_parameters, 0) != 0);

  /* 商部分を取得 */
  quot = SLARecursiveRice_GetQuotPart(strm);

  /* 得られたパラメータ段数までのパラメータを加算 */
  val = 0;
  for (i = 0; (i < quot) && (i < (num_params - 1)); i++) {
    val += SLARICE_CALCULATE_RICE_PARAMETER(rice_parameters, i);
  }

  if (quot < (num_params - 1)) {
    /* 指定したパラメータ段数で剰余部を取得 */
    val += SLARecursiveRice_GetRestPart(strm, 
        SLARICE_CALCULATE_RICE_PARAMETER(rice_parameters, i));
  } else {
    /* 末尾のパラメータで取得 */
    uint32_t tail_param = SLARICE_CALCULATE_RICE_PARAMETER(rice_parameters, i);
    uint32_t tail_rest;
    if (quot == SLACODER_QUOTPART_THRESHOULD) {
      quot += SLAGamma_GetCode(strm);
    }
    tail_rest  = tail_param * (quot - (num_params - 1)) + SLARecursiveRice_GetRestPart(strm, tail_param);
    val       += tail_rest;
  }

  /* パラメータ更新 */
  /* 補足）符号語が分かってからでないと更新できない */
  tmp_val = val;
  for (i = 0; (i <= quot) && (i < num_params); i++) {
    param = SLARICE_CALCULATE_RICE_PARAMETER(rice_parameters, i);
    SLARICE_PARAMETER_UPDATE(rice_parameters, i, tmp_val);
    tmp_val -= param;
  }

  return val;
}

/* 符号化ハンドルの作成 */
struct SLACoder* SLACoder_Create(uint32_t max_num_channels, uint32_t max_num_parameters)
{
  uint32_t ch;
  struct SLACoder* coder;
  
  coder = (struct SLACoder *)malloc(sizeof(struct SLACoder));
  coder->max_num_channels   = max_num_channels;
  coder->max_num_parameters = max_num_parameters;

  coder->rice_parameter       = (SLARecursiveRiceParameter **)malloc(sizeof(SLARecursiveRiceParameter *) * max_num_channels);
  coder->init_rice_parameter  = (SLARecursiveRiceParameter **)malloc(sizeof(SLARecursiveRiceParameter *) * max_num_channels);

  for (ch = 0; ch < max_num_channels; ch++) {
    coder->rice_parameter[ch] 
      = (SLARecursiveRiceParameter *)malloc(sizeof(SLARecursiveRiceParameter) * max_num_parameters);
    coder->init_rice_parameter[ch] 
      = (SLARecursiveRiceParameter *)malloc(sizeof(SLARecursiveRiceParameter) * max_num_parameters);
  }

  return coder;
}

/* 符号化ハンドルの破棄 */
void SLACoder_Destroy(struct SLACoder* coder)
{
  uint32_t ch;

  if (coder != NULL) {
    for (ch = 0; ch < coder->max_num_channels; ch++) {
      NULLCHECK_AND_FREE(coder->rice_parameter[ch]);
      NULLCHECK_AND_FREE(coder->init_rice_parameter[ch]);
    }
    NULLCHECK_AND_FREE(coder->rice_parameter);
    NULLCHECK_AND_FREE(coder->init_rice_parameter);
    free(coder);
  }

}

/* 初期パラメータの計算 */
void SLACoder_CalculateInitialRecursiveRiceParameter(
    struct SLACoder* coder, uint32_t num_parameters,
    const int32_t** data, uint32_t num_channels, uint32_t num_samples)
{
  uint32_t ch, smpl, i, init_param;
  uint64_t sum;

  SLA_Assert((coder != NULL) && (data != NULL));
  SLA_Assert(num_parameters <= coder->max_num_parameters);

  for (ch = 0; ch < num_channels; ch++) {
    /* パラメータ初期値（平均値）の計算 */
    sum = 0;
    for (smpl = 0; smpl < num_samples; smpl++) {
      sum += SLAUTILITY_SINT32_TO_UINT32(data[ch][smpl]);
    }
    init_param = (uint32_t)SLAUTILITY_MAX(sum / num_samples, 1);

    /* 初期パラメータのセット */
    for (i = 0; i < num_parameters; i++) {
      SLACODER_PARAMETER_SET(coder->init_rice_parameter[ch], i, init_param);
      SLACODER_PARAMETER_SET(coder->rice_parameter[ch], i, init_param);
    }
  }
}

/* 再帰的ライス符号のパラメータを符号化 */
void SLACoder_PutInitialRecursiveRiceParameter(
    struct SLACoder* coder, struct SLABitStream* strm,
    uint32_t num_parameters, uint32_t bitwidth, uint32_t channel_index)
{
  uint64_t first_order_param;

  SLAUTILITY_UNUSED_ARGUMENT(num_parameters);
  SLA_Assert((strm != NULL) && (coder != NULL));
  SLA_Assert(num_parameters <= coder->max_num_parameters);
  SLA_Assert(channel_index < coder->max_num_channels);

  /* 1次パラメータを取得 */
  first_order_param = SLACODER_PARAMETER_GET(coder->init_rice_parameter[channel_index], 0);
  /* 書き出し */
  SLA_Assert(first_order_param < (1UL << bitwidth));
  SLABitWriter_PutBits(strm, first_order_param, bitwidth);
}

/* 再帰的ライス符号のパラメータを取得 */
void SLACoder_GetInitialRecursiveRiceParameter(
    struct SLACoder* coder, struct SLABitStream* strm,
    uint32_t num_parameters, uint32_t bitwidth, uint32_t channel_index)
{
  uint32_t i;
  uint64_t first_order_param;

  SLA_Assert((strm != NULL) && (coder != NULL));
  SLA_Assert(num_parameters <= coder->max_num_parameters);
  SLA_Assert(channel_index < coder->max_num_channels);

  /* 初期パラメータの取得 */
  SLABitReader_GetBits(strm, &first_order_param, bitwidth);
  SLA_Assert(first_order_param < (1UL << bitwidth));
  /* 初期パラメータの取得 */
  for (i = 0; i < num_parameters; i++) {
    SLACODER_PARAMETER_SET(coder->init_rice_parameter[channel_index], i, (uint32_t)first_order_param);
    SLACODER_PARAMETER_SET(coder->rice_parameter[channel_index], i, (uint32_t)first_order_param);
  }
}

/* 符号付き整数配列の符号化 */
void SLACoder_PutDataArray(
    struct SLACoder* coder, struct SLABitStream* strm,
    uint32_t num_parameters,
    const int32_t** data, uint32_t num_channels, uint32_t num_samples)
{
  uint32_t smpl, ch;
  uint64_t param_ch_avg;

  SLA_Assert((strm != NULL) && (data != NULL) && (coder != NULL));
  SLA_Assert((num_parameters != 0) && (num_parameters <= coder->max_num_parameters));
  SLA_Assert(num_samples != 0);
  SLA_Assert(num_channels != 0);

  /* 全チャンネルでのパラメータ平均を算出 */
  param_ch_avg = 0;
  for (ch = 0; ch < num_channels; ch++) {
    param_ch_avg += SLACODER_PARAMETER_GET(coder->init_rice_parameter[ch], 0);
  }
  param_ch_avg /= num_channels;

  /* チャンネルインターリーブしつつ符号化 */
  if (param_ch_avg > SLACODER_LOW_THRESHOULD_PARAMETER) {
    /* パラメータを適応的に変更しつつ符号化 */
    for (smpl = 0; smpl < num_samples; smpl++) {
      for (ch = 0; ch < num_channels; ch++) {
        SLARecursiveRice_PutCode(strm,
            coder->rice_parameter[ch], num_parameters, SLAUTILITY_SINT32_TO_UINT32(data[ch][smpl]));
      }
    }
  } else {
    /* パラメータが小さい場合はパラメータ固定で符号化 */
    for (smpl = 0; smpl < num_samples; smpl++) {
      for (ch = 0; ch < num_channels; ch++) {
        SLAGolomb_PutCode(strm,
            SLACODER_PARAMETER_GET(coder->init_rice_parameter[ch], 0), SLAUTILITY_SINT32_TO_UINT32(data[ch][smpl]));
      }
    }
  }
}

/* 符号付き整数配列の復号 */
void SLACoder_GetDataArray(
    struct SLACoder* coder, struct SLABitStream* strm,
    uint32_t num_parameters,
    int32_t** data, uint32_t num_channels, uint32_t num_samples)
{
  uint32_t ch, smpl, abs;
  uint64_t param_ch_avg;

  SLA_Assert((strm != NULL) && (data != NULL) && (coder != NULL));
  SLA_Assert((num_parameters != 0) && (num_samples != 0));

  /* 全チャンネルでのパラメータ平均を算出 */
  param_ch_avg = 0;
  for (ch = 0; ch < num_channels; ch++) {
    param_ch_avg += SLACODER_PARAMETER_GET(coder->init_rice_parameter[ch], 0);
  }
  param_ch_avg /= num_channels;

  /* チャンネルインターリーブで復号 */
  if (param_ch_avg > SLACODER_LOW_THRESHOULD_PARAMETER) {
    /* パラメータを適応的に変更しつつ符号化 */
    for (smpl = 0; smpl < num_samples; smpl++) {
      for (ch = 0; ch < num_channels; ch++) {
        abs = SLARecursiveRice_GetCode(strm, coder->rice_parameter[ch], num_parameters);
        data[ch][smpl] = SLAUTILITY_UINT32_TO_SINT32(abs);
      }
    }
  } else {
    /* パラメータが小さい場合はパラメータ固定で符号化 */
    for (smpl = 0; smpl < num_samples; smpl++) {
      for (ch = 0; ch < num_channels; ch++) {
        abs = SLAGolomb_GetCode(strm, SLACODER_PARAMETER_GET(coder->init_rice_parameter[ch], 0));
        data[ch][smpl] = SLAUTILITY_UINT32_TO_SINT32(abs);
      }
    }
  }
}
