#ifndef SLA_INTERNAL_H_INCLUDED
#define SLA_INTERNAL_H_INCLUDED

#include <assert.h>

#define SLA_MAX_CHANNELS                            8                       /* 処理可能な最大チャンネル数（撤廃予定） */
/* 内部エンコードパラメータ */
#define SLA_BLOCK_SYNC_CODE                         0xFFFF                  /* ブロック先頭の同期コード                 */
#define SLALONGTERM_MAX_PERIOD                      256                     /* ロングタームの最大周期                   */
#define SLALONGTERM_PERIOD_NUM_BITS                 10                      /* ロングターム係数の記録に保存するビット数 */
#define SLALONGTERM_NUM_PITCH_CANDIDATES            SLALONGTERM_MAX_PERIOD  /* ロングターム使用時の最大ピッチ候補数     */
#define SLAPARCOR_COEF_LOW_ORDER_THRESHOULD         4                       /* 何次までのPARCOR係数に高ビットを割り当てるか */
#define SLALONGTERM_MIN_PITCH_THRESHOULD            3                       /* 最小ピッチ周期                           */
#define SLA_MIN_BLOCK_NUM_SAMPLES                   2048                    /* 最小ブロックサイズ                       */
#define SLA_SEARCH_BLOCK_NUM_SAMPLES_DELTA          1024                    /* ブロックサイズ探索時のブロックサイズ増分 */
#define SLA_PRE_EMPHASIS_COEFFICIENT_SHIFT          5                       /* プレエンファシスのシフト量               */
#define SLALMS_DELTA_WEIGHT_SHIFT                   4                       /* LMSの更新量の固定小数値のシフト量      */
#define SLACODER_NUM_RECURSIVERICE_PARAMETER        2                       /* 再帰的ライス符号のパラメータ数 */
#define SLACODER_LOW_THRESHOULD_PARAMETER           8  /* [-4,4] */         /* 固定パラメータ符号を使うか否かの閾値 */
#define SLACODER_QUOTPART_THRESHOULD                16                      /* 再帰的ライス符号の商部分の閾値 これ以上の大きさの商はガンマ符号化 */
#define SLA_STREAMING_DECODE_NUM_SAMPLES_MARGIN     1.05f                   /* ストリーミングデコード時の出力サンプルの余裕をもたせるための比率 */
#define SLA_STREAMING_DECODE_MAX_NUM_PACKETS        8                       /* ストリーミングデコードで使用する最大パケット数 */

/* パスの長さに対して与えるペナルティサイズ[byte]
 * 補足）分割を増やすと以下の要因でサイズが増える
 *  1. ブロック先頭における残差の収束
 *  2. LMSが収束しきらない */
#define SLAOPTIMALENCODEESTIMATOR_LONGPATH_PENALTY  300
#define SLA_ESTIMATE_CODELENGTH_THRESHOLD           0.95f                   /* 推定符号長比（=推定符号長/元データ長）がこの値以上ならば圧縮を諦め、生データを書き出す */

/* ヘッダのCRC16書き込み開始位置 */
#define SLA_HEADER_CRC16_CALC_START_OFFSET          (1 * 4 + 4 + 2)         /* シグネチャ + 先頭ブロックまでのオフセット + CRC16記録フィールド */
/* ブロックのCRC16書き込み開始位置 */
#define SLA_BLOCK_CRC16_CALC_START_OFFSET           (2 + 4 + 2)             /* 同期コード + 次のブロックまでのオフセット + CRC16記録フィールド */
#define SLA_MINIMUM_BLOCK_HEADER_SIZE               (2 + 4 + 2 + 2 + 1)     /* 最小のブロックヘッダサイズ: 同期コード + オフセット + CRC16 + ブロックサンプル数 + ブロックデータタイプ をバイト境界に合わせた値 */

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

/* ブロックデータタイプ */
typedef enum SLABlockDataTypeTag {
  SLA_BLOCK_DATA_TYPE_COMPRESSDATA  = 0,     /* 圧縮済みデータ */
  SLA_BLOCK_DATA_TYPE_SILENT        = 1,     /* 無音データ     */
  SLA_BLOCK_DATA_TYPE_RAWDATA       = 2,     /* 生データ       */
  SLA_BLOCK_DATA_TYPE_INVAILD       = 3      /* 無効           */
} SLABlockDataType;

#endif /* SLA_INTERNAL_H_INCLUDED */
