#ifndef SLA_INTERNAL_H_INCLUDED
#define SLA_INTERNAL_H_INCLUDED

/* 内部エンコードパラメータ */
#define SLALONGTERM_FFT_SIZE                8192
#define SLALONGTERM_MAX_PERIOD              1024
#define SLALONGTERM_PERIOD_NUM_BITS         10
#define SLALONGTERM_NUM_PITCH_CANDIDATES    SLALONGTERM_MAX_PERIOD
#define SLAPARCOR_COEF_LOW_ORDER_THRESHOULD 4
#define SLALONGTERM_MIN_PITCH_THRESHOULD    3

/* ヘッダのCRC16書き込み開始位置 */
#define SLA_HEADER_CRC16_CALC_START_OFFSET  (1 * 4 + 4 + 2)         /* シグネチャ + 先頭ブロックまでのオフセット + CRC16記録フィールド */
/* ブロックのCRC16書き込み開始位置 */
#define SLA_BLOCK_CRC16_CALC_START_OFFSET   (2 + 4 + 2)             /* 同期コード + 次のブロックまでのオフセット + CRC16記録フィールド */

/* サンプル数のブロック数を計算 */
#define SLA_CALCULATE_NUM_SLABLOCK(num_samples, num_block_samples) \
  (((num_samples) + ((num_block_samples) - 1)) / (num_block_samples))

/* NULLチェックと領域解放 */
#define NULLCHECK_AND_FREE(ptr) { \
  if ((ptr) != NULL) {            \
    free(ptr);                    \
    (ptr) = NULL;                 \
  }                               \
}

#endif /* SLA_INTERNAL_H_INCLUDED */
