#ifndef SLA_DECODER_H_INCLUDED
#define SLA_DECODER_H_INCLUDED

#include "SLA.h"
#include <stdint.h>

/* デコーダハンドル */
struct SLADecoder;

/* デコーダコンフィグ */
struct SLADecoderConfig {
	uint32_t  max_num_channels;			    /* エンコード可能な最大チャンネル数 */
	uint32_t  max_num_block_samples;		/* ブロックあたり最大サンプル数 */
	uint32_t  max_parcor_order;				  /* 最大PARCOR係数次数 */
	uint32_t  max_longterm_order;		    /* 最大ロングターム次数 */
	uint32_t  max_nlms_order;				    /* 最大NLMS次数 */
  uint8_t   enable_crc_check;         /* CRCチェックを有効にするか */
};

/* デコーダハンドルの作成 */
struct SLADecoder* SLADecoder_Create(const struct SLADecoderConfig* condig);

/* デコーダハンドルの破棄 */
void SLADecoder_Destroy(struct SLADecoder* encoder);

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

/* ヘッダを含めて全ブロックデコード */
SLAApiResult SLADecoder_DecodeWhole(struct SLADecoder* decoder,
    const uint8_t* data, uint32_t data_size,
    int32_t** buffer, uint32_t buffer_num_samples, uint32_t* output_num_samples);

#endif /* SLA_DECODER_H_INCLUDED */
