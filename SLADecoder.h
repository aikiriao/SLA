#ifndef SLA_DECODER_H_INCLUDED
#define SLA_DECODER_H_INCLUDED

#include "SLA.h"
#include <stdint.h>

/* デコーダバージョン */
#define SLA_DECODER_VERSION_STRING   "0.0.1(beta)"

/* デコーダハンドル */
struct SLADecoder;

/* デコーダコンフィグ */
struct SLADecoderConfig {
	uint32_t  max_num_channels;			      /* エンコード可能な最大チャンネル数 */
	uint32_t  max_num_block_samples;		  /* ブロックあたり最大サンプル数 */
	uint32_t  max_parcor_order;				    /* 最大PARCOR係数次数 */
	uint32_t  max_longterm_order;		      /* 最大ロングターム次数 */
	uint32_t  max_lms_order_par_filter;   /* 最大LMS次数 */
  uint8_t   enable_crc_check;           /* CRCチェックを有効にするか */
  uint8_t   verpose_flag;               /* 詳細な情報を表示するか */
};

#ifdef __cplusplus
extern "C" {
#endif

/* デコーダハンドルの作成 */
struct SLADecoder* SLADecoder_Create(const struct SLADecoderConfig* condig);

/* デコーダハンドルの破棄 */
void SLADecoder_Destroy(struct SLADecoder* decoder);

/* ヘッダデコード */
SLAApiResult SLADecoder_DecodeHeader(
    const uint8_t* data, uint32_t data_size, struct SLAHeaderInfo* header_info);

/* 波形パラメータをデコーダにセット */
SLAApiResult SLADecoder_SetWaveFormat(struct SLADecoder* decoder,
    const struct SLAWaveFormat* wave_format);

/* エンコードパラメータをデコーダにセット */
SLAApiResult SLADecoder_SetEncodeParameter(struct SLADecoder* decoder,
    const struct SLAEncodeParameter* encode_param);

/* 1ブロックデコード */
SLAApiResult SLADecoder_DecodeBlock(struct SLADecoder* decoder,
    const uint8_t* data, uint32_t data_size,
    int32_t** buffer, uint32_t buffer_num_samples,
    uint32_t* output_block_size, uint32_t* output_num_samples);

/* ヘッダを含めて全ブロックデコード（波形パラメータ・エンコードパラメータも自動でセット） */
SLAApiResult SLADecoder_DecodeWhole(struct SLADecoder* decoder,
    const uint8_t* data, uint32_t data_size,
    int32_t** buffer, uint32_t buffer_num_samples, uint32_t* output_num_samples);

#ifdef __cplusplus
}
#endif

#endif /* SLA_DECODER_H_INCLUDED */
