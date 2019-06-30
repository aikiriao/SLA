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
  SLAUtility_RoundUp2Powered(SLAUTILITY_MAX(SLACODER_FIXED_FLOAT_TO_UINT32((param_array)[(order)] >> 1), 1UL))

/* 2の冪数に対するlog2計算のためのテーブル */
static const uint8_t log2_for_2powered_val_table[4][0x100] = {
  /* 1byte目 */
  {
    0, 0, 1, 0, 2, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0,
    4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
  },
  /* 2byte目 */
  {
    0, 8, 9, 0,10, 0, 0, 0,11, 0, 0, 0, 0, 0, 0, 0,
    12,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    13,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    14,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    15,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
  },
  /* 3byte目 */
  {
    0,16,17, 0,18, 0, 0, 0,19, 0, 0, 0, 0, 0, 0, 0,
    20,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    21,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    22,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    23,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
  },
  /* 4byte目 */
  {
    0,24,25, 0,26, 0, 0, 0,27, 0, 0, 0, 0, 0, 0, 0,
    28,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    29,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    30,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    31,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
  }
};

/* 2の冪乗数に対するceil(log2(val))計算 */
/* 注意: 
 * - インライン展開を期待するためstatic関数
 * - 出現頻度に偏りがあるのを想定. 分岐予測により速度が上がる */
static uint32_t SLACoder_Log2CeilFor2PoweredValue(uint32_t val)
{
  SLA_Assert(val != 0);
  SLA_Assert(SLAUTILITY_IS_POWERED_OF_2(val));

  if (val < 0x10000) {
    return  log2_for_2powered_val_table[0][0xFF & (val >>  0)]
          | log2_for_2powered_val_table[1][0xFF & (val >>  8)];
  }

  return  log2_for_2powered_val_table[2][0xFF & (val >> 16)]
        | log2_for_2powered_val_table[3][0xFF & (val >> 24)];
}


/* ゴロム符号化の出力 */
static void SLAGolomb_PutCode(struct SLABitStream* strm, uint32_t m, uint32_t val)
{
  uint32_t quot;
  uint32_t rest;
  uint32_t b, two_b;
  uint32_t i;
  SLABitStreamApiResult ret;

  SLA_Assert(strm != NULL);
  SLA_Assert(m != 0);

  /* 商部分長と剰余部分の計算 */
  quot = val / m;
  rest = val % m;

  /* 前半部分の出力(unary符号) */
  for (i = 0; i < quot; i++) {
    ret = SLABitStream_PutBit(strm, 0);
    SLA_Assert(ret == SLABITSTREAM_APIRESULT_OK);
  }
  ret = SLABitStream_PutBit(strm, 1);
  SLA_Assert(ret == SLABITSTREAM_APIRESULT_OK);

  /* 剰余部分の出力 */
  if (SLAUTILITY_IS_POWERED_OF_2(m)) {
    /* mが2の冪: ライス符号化 */
    ret = SLABitStream_PutBits(strm, SLACoder_Log2CeilFor2PoweredValue(m), rest);
    SLA_Assert(ret == SLABITSTREAM_APIRESULT_OK);
    return;
  }

  /* ゴロム符号化 */
  b = SLAUtility_Log2Ceil(m);
  two_b = (uint32_t)(1UL << b);
  if (rest < (two_b - m)) {
    ret = SLABitStream_PutBits(strm, b - 1, rest);
    SLA_Assert(ret == SLABITSTREAM_APIRESULT_OK);
  } else {
    ret = SLABitStream_PutBits(strm, b, rest + two_b - m);
    SLA_Assert(ret == SLABITSTREAM_APIRESULT_OK);
  }
}

/* ゴロム符号化の取得 */
static uint32_t SLAGolomb_GetCode(struct SLABitStream* strm, uint32_t m) 
{
  uint32_t quot;
  uint64_t rest;
  uint32_t b, two_b;
  uint8_t  bit;
  SLABitStreamApiResult ret;

  SLA_Assert(strm != NULL);
  SLA_Assert(m != 0);

  /* 前半のunary符号部分を読み取り */
  quot = 0;
  while (1) {
    ret = SLABitStream_GetBit(strm, &bit);
    SLA_Assert(ret == SLABITSTREAM_APIRESULT_OK);
    if (bit != 0) {
      break;
    }
    quot++;
  }

  /* 剰余部分の読み取り */
  if (SLAUTILITY_IS_POWERED_OF_2(m)) {
    /* mが2の冪: ライス符号化 */
    ret = SLABitStream_GetBits(strm, SLACoder_Log2CeilFor2PoweredValue(m), &rest);
    SLA_Assert(ret == SLABITSTREAM_APIRESULT_OK);
    return (uint32_t)(quot * m + rest);
  }

  /* ゴロム符号化 */
  b = SLAUtility_Log2Ceil(m);
  two_b = (uint32_t)(1UL << b);
  ret = SLABitStream_GetBits(strm, b - 1, &rest);
  SLA_Assert(ret == SLABITSTREAM_APIRESULT_OK);
  if (rest < (two_b - m)) {
    return (uint32_t)(quot * m + rest);
  } else {
    rest <<= 1;
    ret = SLABitStream_GetBit(strm, &bit);
    SLA_Assert(ret == SLABITSTREAM_APIRESULT_OK);
    rest += bit;
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
    SLABitStream_PutBit(strm, 1);
    return;
  } 

  /* 桁数を取得 */
  ndigit = SLAUtility_Log2Ceil(val + 2);
  /* 桁数-1だけ0を続ける */
  SLABitStream_PutBits(strm, ndigit - 1, 0);
  /* 桁数を使用して符号語を2進数で出力 */
  SLABitStream_PutBits(strm, ndigit, val + 1);
}

/* ガンマ符号の取得 */
static uint32_t SLAGamma_GetCode(struct SLABitStream* strm)
{
  uint32_t ndigit;
  uint8_t  bitbuf;
  uint64_t bitsbuf;

  SLA_Assert(strm != NULL);

  /* 桁数を取得 */
  /* 1が出現するまで桁数を増加 */
  ndigit = 0;
  do {
    ndigit++;
    SLABitStream_GetBit(strm, &bitbuf);
  } while (bitbuf == 0);

  /* 桁数が1のときは0 */
  if (ndigit == 1) {
    return 0;
  }

  /* 桁数から符号語を出力 */
  SLABitStream_GetBits(strm, ndigit - 1, &bitsbuf);
  return (uint32_t)((1UL << (ndigit - 1)) + bitsbuf - 1);
}

/* 商部分（アルファ符号）を出力 */
static void SLARecursiveRice_PutQuotPart(
    struct SLABitStream* strm, uint32_t quot)
{
  uint32_t i;

  SLA_Assert(strm != NULL);

  for (i = 0; i < quot; i++) {
    SLABitStream_PutBit(strm, 0);
  }
  SLABitStream_PutBit(strm, 1);
}

/* 商部分（アルファ符号）を取得 */
static uint32_t SLARecursiveRice_GetQuotPart(struct SLABitStream* strm)
{
  uint32_t  quot;
  SLABitStreamApiResult ret;
  
  SLA_Assert(strm != NULL);

  ret = SLABitStream_GetZeroRunLength(strm, &quot);
  SLA_Assert(ret == SLABITSTREAM_APIRESULT_OK);

  return quot;
}

/* 剰余部分を出力 */
static void SLARecursiveRice_PutRestPart(
    struct SLABitStream* strm, uint32_t val, uint32_t m)
{
  SLABitStreamApiResult ret;

  SLA_Assert(strm != NULL);
  SLA_Assert(m != 0);
  SLA_Assert(SLAUTILITY_IS_POWERED_OF_2(m));

  ret = SLABitStream_PutBits(strm, SLACoder_Log2CeilFor2PoweredValue(m), val & (m - 1));
  SLA_Assert(ret == SLABITSTREAM_APIRESULT_OK);
}

/* 剰余部分を取得 */
static uint32_t SLARecursiveRice_GetRestPart(struct SLABitStream* strm, uint32_t m)
{
  uint64_t rest;
  SLABitStreamApiResult ret;

  SLA_Assert(strm != NULL);
  SLA_Assert(m != 0);
  SLA_Assert(SLAUTILITY_IS_POWERED_OF_2(m));

  /* 1の剰余は0 */
  if (m == 1) {
    return 0;
  }

  /* ライス符号の剰余部分取得 */
  ret = SLABitStream_GetBits(strm, SLACoder_Log2CeilFor2PoweredValue(m), &rest);

  SLA_Assert(ret == SLABITSTREAM_APIRESULT_OK);
  
  return (uint32_t)rest;
}

/* 再帰的ライス符号の出力 */
static void SLARecursiveRice_PutCode(
    struct SLABitStream* strm, SLARecursiveRiceParameter* m_params, uint32_t num_params, uint32_t val)
{
  uint32_t i, reduced_val, param;

  SLA_Assert(strm != NULL);
  SLA_Assert(m_params != NULL);
  SLA_Assert(num_params != 0);
  SLA_Assert(SLACODER_PARAMETER_GET(m_params, 0) != 0);

  reduced_val = val;
  for (i = 0; i < (num_params - 1); i++) {
    param = SLARICE_CALCULATE_RICE_PARAMETER(m_params, i);
    /* 現在のパラメータ値よりも小さければ、符号化を行う */
    if (reduced_val < param) {
        /* 商部分としてはパラメータ段数 */
        SLARecursiveRice_PutQuotPart(strm, i);
        /* 剰余部分 */
        SLARecursiveRice_PutRestPart(strm, reduced_val, param);
        /* パラメータ更新 */
        SLARICE_PARAMETER_UPDATE(m_params, i, reduced_val);
        break;
    }
    /* パラメータ更新 */
    SLARICE_PARAMETER_UPDATE(m_params, i, reduced_val);
    /* 現在のパラメータ値で減じる */
    reduced_val -= param;
  }

  /* 末尾のパラメータに達した */
  if (i == (num_params - 1)) {
    uint32_t tail_param = SLARICE_CALCULATE_RICE_PARAMETER(m_params, i);
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
    SLARICE_PARAMETER_UPDATE(m_params, i, reduced_val);
  }

}

/* 再帰的ライス符号の取得 */
static uint32_t SLARecursiveRice_GetCode(
    struct SLABitStream* strm, SLARecursiveRiceParameter* m_params, uint32_t num_params)
{
  uint32_t  i, quot, val;
  uint32_t  param, tmp_val;

  SLA_Assert(strm != NULL);
  SLA_Assert(m_params != NULL);
  SLA_Assert(num_params != 0);
  SLA_Assert(SLACODER_PARAMETER_GET(m_params, 0) != 0);

  /* 商部分を取得 */
  quot = SLARecursiveRice_GetQuotPart(strm);

  /* 得られたパラメータ段数までのパラメータを加算 */
  val = 0;
  for (i = 0; (i < quot) && (i < (num_params - 1)); i++) {
    val += SLARICE_CALCULATE_RICE_PARAMETER(m_params, i);
  }

  if (quot < (num_params - 1)) {
    /* 指定したパラメータ段数で剰余部を取得 */
    val += SLARecursiveRice_GetRestPart(strm, 
        SLARICE_CALCULATE_RICE_PARAMETER(m_params, i));
  } else {
    /* 末尾のパラメータで取得 */
    uint32_t tail_param = SLARICE_CALCULATE_RICE_PARAMETER(m_params, i);
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
    param = SLARICE_CALCULATE_RICE_PARAMETER(m_params, i);
    SLARICE_PARAMETER_UPDATE(m_params, i, tmp_val);
    tmp_val -= param;
  }

  return val;
}

/* 初期パラメータの計算 */
void SLACoder_CalculateInitialRecursiveRiceParameter(
    SLARecursiveRiceParameter* rice_parameter, uint32_t num_parameters,
    const int32_t* data, uint32_t num_data)
{
  uint32_t i;
  SLARecursiveRiceParameter init_param;
  uint64_t sum;

  SLA_Assert((rice_parameter != NULL) && (data != NULL));

  /* パラメータ初期値（平均値）の計算 */
  sum = 0;
  for (i = 0; i < num_data; i++) {
    sum += SLAUTILITY_SINT32_TO_UINT32(data[i]);
  }
  init_param = SLAUTILITY_MAX(sum / num_data, 1);

  /* 初期パラメータのセット */
  for (i = 0; i < num_parameters; i++) {
    SLACODER_PARAMETER_SET(rice_parameter, i, init_param);
  }

}

/* 再帰的ライス符号のパラメータを符号化 */
void SLACoder_PutRecursiveRiceParameter(
    struct SLABitStream* strm,
    SLARecursiveRiceParameter* rice_parameter, uint32_t num_parameters,
    uint32_t bitwidth)
{
  SLABitStreamApiResult ret;
  uint32_t first_order_param;

  SLAUTILITY_UNUSED_ARGUMENT(num_parameters);
  SLA_Assert((strm != NULL) && (rice_parameter != NULL));

  /* 1次パラメータを取得 */
  first_order_param = SLACODER_PARAMETER_GET(rice_parameter, 0);

  /* 書き出し */
  SLA_Assert(first_order_param < (1UL << bitwidth));
  ret = SLABitStream_PutBits(strm, bitwidth, first_order_param);
  SLA_Assert(ret == SLABITSTREAM_APIRESULT_OK);
}

/* 再帰的ライス符号のパラメータを取得 */
void SLACoder_GetRecursiveRiceParameter(
    struct SLABitStream* strm,
    SLARecursiveRiceParameter* rice_parameter, uint32_t num_parameters,
    uint32_t bitwidth)
{
  SLABitStreamApiResult ret;
  uint32_t i;
  uint64_t first_order_param;

  SLA_Assert((strm != NULL) && (rice_parameter != NULL));

  /* 初期パラメータの取得 */
  ret = SLABitStream_GetBits(strm, bitwidth, &first_order_param);
  SLA_Assert(ret == SLABITSTREAM_APIRESULT_OK);
  SLA_Assert(first_order_param < (1UL << bitwidth));

  /* 初期パラメータの取得 */
  for (i = 0; i < num_parameters; i++) {
    SLACODER_PARAMETER_SET(rice_parameter, i, (uint32_t)first_order_param);
  }
}

/* 符号付き整数配列の符号化 */
void SLACoder_PutDataArray(
    struct SLABitStream* strm, 
    SLARecursiveRiceParameter** rice_parameter, uint32_t num_parameters,
    const int32_t** data, uint32_t num_channels, uint32_t num_samples)
{
  uint32_t smpl, ch;
  uint32_t first_order_param[SLA_MAX_CHANNELS];

  SLA_Assert((strm != NULL) && (data != NULL) && (rice_parameter != NULL));
  SLA_Assert(num_samples != 0);
  SLA_Assert(num_channels != 0);
  SLA_Assert(num_parameters != 0);

  /* 1次パラメータの取得 */
  for (ch = 0; ch < num_channels; ch++) {
    first_order_param[ch] = SLACODER_PARAMETER_GET(rice_parameter[ch], 0);
  }

  /* チャンネルインターリーブしつつ符号化 */
  for (smpl = 0; smpl < num_samples; smpl++) {
    for (ch = 0; ch < num_channels; ch++) {
      /* パラメータが小さい場合はパラメータ固定で符号化 */
      if (first_order_param[ch] <= SLACODER_LOW_THRESHOULD_PARAMETER) {
        SLAGolomb_PutCode(strm, first_order_param[ch], SLAUTILITY_SINT32_TO_UINT32(data[ch][smpl]));
      } else {
        /* パラメータを適応的に変更しつつ符号化 */
        SLARecursiveRice_PutCode(strm,
            rice_parameter[ch], num_parameters, SLAUTILITY_SINT32_TO_UINT32(data[ch][smpl]));
      }
    }
  }

}

/* 符号付き整数配列の復号 */
void SLACoder_GetDataArray(
    struct SLABitStream* strm, 
    SLARecursiveRiceParameter** rice_parameter, uint32_t num_parameters,
    int32_t** data, uint32_t num_channels, uint32_t num_samples)
{
  uint32_t  ch, smpl, abs;
  uint32_t  first_order_param[SLA_MAX_CHANNELS];

  SLA_Assert((strm != NULL) && (data != NULL) && (rice_parameter != NULL));
  SLA_Assert((num_parameters != 0) && (num_samples != 0));

  /* 1次パラメータの取得 */
  for (ch = 0; ch < num_channels; ch++) {
    first_order_param[ch] = SLACODER_PARAMETER_GET(rice_parameter[ch], 0);
  }

  /* チャンネルインターリーブしつつ復号 */
  for (smpl = 0; smpl < num_samples; smpl++) {
    for (ch = 0; ch < num_channels; ch++) {
      if (first_order_param[ch] <= SLACODER_LOW_THRESHOULD_PARAMETER) {
        /* パラメータが小さい場合はパラメータ固定で復号 */
        abs = SLAGolomb_GetCode(strm, first_order_param[ch]);
        data[ch][smpl] = SLAUTILITY_UINT32_TO_SINT32(abs);
      } else {
        /* パラメータを適応的に変更しつつ復号 */
        abs = SLARecursiveRice_GetCode(strm, rice_parameter[ch], num_parameters);
        data[ch][smpl] = SLAUTILITY_UINT32_TO_SINT32(abs);
      }
    }
  }

}
