#ifndef SLA_TESTUTILITY_H_INCLUDED
#define SLA_TESTUTILITY_H_INCLUDED

/* エンコーダデフォルトのコンフィグをセット */
/* （将来的にSLAEncoder.hに公開予定） */
#define SLAEncoder_SetDefaultConfig(p_config) { \
  (p_config)->max_num_channels          = 6;    \
  (p_config)->max_num_block_samples     = 8192; \
  (p_config)->max_parcor_order          = 64;   \
  (p_config)->max_longterm_order        = 5;    \
  (p_config)->max_lms_order_par_filter  = 64;   \
}

/* デコーダデフォルトのコンフィグをセット */
/* （将来的にSLADecoder.hに公開予定） */
#define SLADecoder_SetDefaultConfig(p_config) { \
  (p_config)->max_num_channels          = 6;    \
  (p_config)->max_num_block_samples     = 8192; \
  (p_config)->max_parcor_order          = 64;   \
  (p_config)->max_longterm_order        = 5;    \
  (p_config)->max_lms_order_par_filter  = 64;   \
  (p_config)->enable_crc_check          = 0;    \
}

/* ひとまず有効な波形情報を設定 */
#define SLATestUtility_SetValidWaveFormat(p_format) {   \
    (p_format)->num_channels           = 2;         \
    (p_format)->bit_per_sample         = 8;         \
    (p_format)->sampling_rate          = 8000;      \
}

/* ひとまず有効なエンコードパラメータを設定 */
#define SLATestUtility_SetValidEncodeParameter(p_param) { \
    (p_param)->parcor_order             = 10;        \
    (p_param)->longterm_order           = 1;         \
    (p_param)->lms_order_par_filter     = 10;        \
    (p_param)->num_lms_filter_cascade   = 2;         \
    (p_param)->ch_process_method                     \
      = SLA_CHPROCESSMETHOD_NONE;                    \
    (p_param)->window_function_type                  \
      = SLA_WINDOWFUNCTIONTYPE_RECTANGULAR;          \
    (p_param)->max_num_block_samples = 4096;         \
}

/* ひとまず有効なヘッダ情報を設定 */
#define SLATestUtility_SetValidHeaderInfo(p_header) {                   \
    SLATestUtility_SetValidWaveFormat(&((p_header)->wave_format));      \
    SLATestUtility_SetValidEncodeParameter(&((p_header)->encode_param)) \
    (p_header)->num_samples    = 8000 * 10;                         \
    (p_header)->max_block_size = 4096;                              \
    (p_header)->num_blocks     = 1024;                              \
}

#endif /* SLA_TESTUTILITY_H_INCLUDED */
