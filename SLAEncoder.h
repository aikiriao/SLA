#ifndef SLA_ENCODER_H_INCLUDED
#define SLA_ENCODER_H_INCLUDED

#include "SLA.h"
#include <stdint.h>

/* エンコーダバージョン */
#define SLA_ENCODER_VERSION_STRING   "0.0.1(beta)"

/* エンコーダハンドル */
struct SLAEncoder;

/* エンコーダコンフィグ */
struct SLAEncoderConfig {
	uint32_t  max_num_channels;			      /* エンコード可能な最大チャンネル数 */
	uint32_t  max_num_block_samples;		  /* ブロックあたり最大サンプル数 */
	uint32_t  max_parcor_order;			      /* 最大PARCOR係数次数 */
	uint32_t  max_longterm_order;		      /* 最大ロングターム次数 */
	uint32_t  max_lms_order_par_filter;   /* 最大NLMS次数 */
  uint8_t   verpose_flag;               /* 詳細な情報を表示するか */
};

#ifdef __cplusplus
extern "C" {
#endif

/* エンコーダハンドルの作成 */
struct SLAEncoder* SLAEncoder_Create(const struct SLAEncoderConfig* config);

/* エンコーダハンドルの破棄 */
void SLAEncoder_Destroy(struct SLAEncoder* encoder);

/* 波形パラメータをエンコーダにセット */
SLAApiResult SLAEncoder_SetWaveFormat(struct SLAEncoder* encoder,
    const struct SLAWaveFormat* wave_format);

/* エンコードパラメータをエンコーダにセット */
SLAApiResult SLAEncoder_SetEncodeParameter(struct SLAEncoder* encoder,
    const struct SLAEncodeParameter* encode_param);

/* ヘッダ書き出し */
SLAApiResult SLAEncoder_EncodeHeader(
    const struct SLAHeaderInfo* header, uint8_t* data, uint32_t data_size);

/* 1ブロックエンコード */
SLAApiResult SLAEncoder_EncodeBlock(struct SLAEncoder* encoder,
    const int32_t* const* input, uint32_t num_samples,
    uint8_t* data, uint32_t data_size, uint32_t* output_size);

/* ヘッダを含めて全ブロックエンコード */
SLAApiResult SLAEncoder_EncodeWhole(struct SLAEncoder* encoder,
    const int32_t* const* input, uint32_t num_samples,
    uint8_t* data, uint32_t data_size, uint32_t* output_size);

#ifdef __cplusplus
}
#endif

#endif /* SLA_ENCODER_H_INCLUDED */
