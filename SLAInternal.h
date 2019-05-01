#ifndef SLA_INTERNAL_H_INCLUDED
#define SLA_INTERNAL_H_INCLUDED

/* 内部エンコードパラメータ */
#define SLALONGTERM_MAX_PERIOD              1024
#define SLALONGTERM_PERIOD_NUM_BITS         10
#define SLALONGTERM_NUM_PITCH_CANDIDATES    SLALONGTERM_MAX_PERIOD
#define SLAPARCOR_COEF_LOW_ORDER_THRESHOULD 4
#define SLALONGTERM_MIN_PITCH_THRESHOULD    3
#define SLA_MIN_BLOCK_NUM_SAMPLES           2048
#define SLA_SEARCH_BLOCK_NUM_SAMPLES_DELTA  512
#define SLA_PRE_EMPHASIS_COEFFICIENT_SHIFT  5
#define SLALMS_DELTA_WEIGHT_SHIFT           9                       /* 32bit符号付き固定小数点で2**-9 */

/* ヘッダのCRC16書き込み開始位置 */
#define SLA_HEADER_CRC16_CALC_START_OFFSET  (1 * 4 + 4 + 2)         /* シグネチャ + 先頭ブロックまでのオフセット + CRC16記録フィールド */
/* ブロックのCRC16書き込み開始位置 */
#define SLA_BLOCK_CRC16_CALC_START_OFFSET   (2 + 4 + 2)             /* 同期コード + 次のブロックまでのオフセット + CRC16記録フィールド */

/* NULLチェックと領域解放 */
#define NULLCHECK_AND_FREE(ptr) { \
  if ((ptr) != NULL) {            \
    free(ptr);                    \
    (ptr) = NULL;                 \
  }                               \
}

#endif /* SLA_INTERNAL_H_INCLUDED */
