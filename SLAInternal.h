#ifndef SLA_INTERNAL_H_INCLUDED
#define SLA_INTERNAL_H_INCLUDED

#include <assert.h>

/* 内部エンコードパラメータ */
#define SLALONGTERM_MAX_PERIOD                      1024                    /* ロングタームの最大周期                   */
#define SLALONGTERM_PERIOD_NUM_BITS                 10                      /* ロングターム係数の記録に保存するビット数 */
#define SLALONGTERM_NUM_PITCH_CANDIDATES            SLALONGTERM_MAX_PERIOD  /* ロングターム使用時の最大ピッチ候補数     */
#define SLAPARCOR_COEF_LOW_ORDER_THRESHOULD         4                       /* 何次までのPARCOR係数に高ビットを割り当てるか */
#define SLALONGTERM_MIN_PITCH_THRESHOULD            3                       /* 最小ピッチ周期                           */
#define SLA_MIN_BLOCK_NUM_SAMPLES                   2048                    /* 最小ブロックサイズ                       */
#define SLA_SEARCH_BLOCK_NUM_SAMPLES_DELTA          1024                    /* ブロックサイズ探索時のブロックサイズ増分 */
#define SLA_PRE_EMPHASIS_COEFFICIENT_SHIFT          5                       /* プレエンファシスのシフト量               */
#define SLALMS_DELTA_WEIGHT_SHIFT                   10                      /* LMSの更新量の固定小数値の右シフト量      */
/* パスの長さに対して与えるペナルティサイズ[byte]
 * 補足）分割を増やすと以下の要因でサイズが増える
 *  1. ブロック先頭における残差の収束
 *  2. LMSが収束しきらない */
#define SLAOPTIMALENCODEESTIMATOR_LONGPATH_PENALTY  300

/* ヘッダのCRC16書き込み開始位置 */
#define SLA_HEADER_CRC16_CALC_START_OFFSET  (1 * 4 + 4 + 2)         /* シグネチャ + 先頭ブロックまでのオフセット + CRC16記録フィールド */
/* ブロックのCRC16書き込み開始位置 */
#define SLA_BLOCK_CRC16_CALC_START_OFFSET   (2 + 4 + 2)             /* 同期コード + 次のブロックまでのオフセット + CRC16記録フィールド */

/* PARCORの次数から係数のビット幅を取得 */
#define SLA_GET_PARCOR_QUANTIZE_BIT_WIDTH(order)  (((order) < SLAPARCOR_COEF_LOW_ORDER_THRESHOULD) ? 16 : 8)

/* NULLチェックと領域解放 */
#define NULLCHECK_AND_FREE(ptr) { \
  if ((ptr) != NULL) {            \
    free(ptr);                    \
    (ptr) = NULL;                 \
  }                               \
}

/* アサート */
#ifdef NDEBUG
/* 未使用変数警告を明示的に回避 */
#define SLA_Assert(condition) ((void)(condition))
#else
#define SLA_Assert(condition) assert(condition)
#endif

#endif /* SLA_INTERNAL_H_INCLUDED */
