#ifndef SLA_DECODER_H_INCLUDED
#define SLA_DECODER_H_INCLUDED

#include "SLA.h"
#include <stdint.h>

/* デコーダバージョン */
#define SLA_DECODER_VERSION_STRING   "0.0.1(beta)"

/* デコーダハンドル */
struct SLADecoder;

/* ストリーミングデコーダハンドル */
struct SLAStreamingDecoder;

/* デコーダコンフィグ */
struct SLADecoderConfig {
	uint32_t  max_num_channels;			      /* エンコード可能な最大チャンネル数 */
	uint32_t  max_num_block_samples;		  /* ブロックあたり最大サンプル数 */
	uint32_t  max_parcor_order;				    /* 最大PARCOR係数次数 */
	uint32_t  max_longterm_order;		      /* 最大ロングターム次数 */
	uint32_t  max_lms_order_per_filter;   /* 最大LMS次数 */
  uint8_t   enable_crc_check;           /* CRCチェックを有効にするか */
  uint8_t   verpose_flag;               /* 詳細な情報を表示するか */
};

/* ストリーミングデコーダコンフィグ */
struct SLAStreamingDecoderConfig {
  struct SLADecoderConfig core_config;          /* デコーダコンフィグ         */
  float                   decode_interval_hz;   /* デコード処理間隔[Hz]       */
  uint32_t                max_bit_per_sample;   /* 最大サンプルあたりビット数 */
};

#ifdef __cplusplus
extern "C" {
#endif

/* ヘッダデコード */
SLAApiResult SLADecoder_DecodeHeader(
    const uint8_t* data, uint32_t data_size, struct SLAHeaderInfo* header_info);

/* デコーダハンドルの作成 */
struct SLADecoder* SLADecoder_Create(const struct SLADecoderConfig* condig);

/* デコーダハンドルの破棄 */
void SLADecoder_Destroy(struct SLADecoder* decoder);

/* 波形パラメータをデコーダにセット */
SLAApiResult SLADecoder_SetWaveFormat(struct SLADecoder* decoder,
    const struct SLAWaveFormat* wave_format);

/* エンコードパラメータをデコーダにセット */
SLAApiResult SLADecoder_SetEncodeParameter(struct SLADecoder* decoder,
    const struct SLAEncodeParameter* encode_param);

/* ヘッダを含めて全ブロックデコード（波形パラメータ・エンコードパラメータも自動でセット） */
SLAApiResult SLADecoder_DecodeWhole(struct SLADecoder* decoder,
    const uint8_t* data, uint32_t data_size,
    int32_t** buffer, uint32_t buffer_num_samples, uint32_t* output_num_samples);

/* ストリーミングデコーダの作成 */
struct SLAStreamingDecoder* SLAStreamingDecoder_Create(const struct SLAStreamingDecoderConfig* config);

/* ストリーミングデコーダの破棄 */
void SLAStreamingDecoder_Destroy(struct SLAStreamingDecoder* decoder);

/* 波形パラメータをデコーダにセット */
SLAApiResult SLAStreamingDecoder_SetWaveFormat(struct SLAStreamingDecoder* decoder,
    const struct SLAWaveFormat* wave_format);

/* エンコードパラメータをデコーダにセット */
SLAApiResult SLAStreamingDecoder_SetEncodeParameter(struct SLAStreamingDecoder* decoder,
    const struct SLAEncodeParameter* encode_param);

/* 最低限供給すべきデータサイズの推定値を取得 */
SLAApiResult SLAStreamingDecoder_EstimateMinimumNessesaryDataSize(struct SLAStreamingDecoder* decoder,
    uint32_t* estimate_data_size);

/* デコード可能なサンプル数の推定値を取得 */
SLAApiResult SLAStreamingDecoder_EstimateDecodableNumSamples(struct SLAStreamingDecoder* decoder,
    uint32_t* estimate_num_samples);

/* デコード関数呼び出しあたりの出力サンプル数を取得 */
SLAApiResult SLAStreamingDecoder_GetOutputNumSamplesParDecode(struct SLAStreamingDecoder* decoder,
    uint32_t* output_num_samples);

/* デコーダにデータ片を供給 */
SLAApiResult SLAStreamingDecoder_AppendDataFragment(struct SLAStreamingDecoder* decoder,
    const uint8_t* data, uint32_t data_size);

/* デコーダに残っているデータ片の回収 */
SLAApiResult SLAStreamingDecoder_CollectDataFragment(struct SLAStreamingDecoder* decoder,
    const uint8_t** data_ptr, uint32_t* data_size);

/* 残りデータサイズを取得 */
SLAApiResult SLAStreamingDecoder_GetRemainDataSize(struct SLAStreamingDecoder* decoder,
    uint32_t* remain_data_size);

/* ストリーミングデコード */
SLAApiResult SLAStreamingDecoder_Decode(struct SLAStreamingDecoder* decoder,
    int32_t** buffer, uint32_t buffer_num_samples, uint32_t* num_output_samples);

#ifdef __cplusplus
}
#endif

#endif /* SLA_DECODER_H_INCLUDED */
