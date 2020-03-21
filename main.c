/* This program is free software. It comes without any warranty, to
 * the extent permitted by applicable law. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar. See
 * http://www.wtfpl.net/ for more details. */

#include "SLAEncoder.h"
#include "SLADecoder.h"

#include "wav.h"
#include "command_line_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* CUIインターフェースバージョン */
#define SLA_CUI_VERSION_STRING     "0.0.1(beta)"

/* 2つのうち小さい値の選択 */
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

/* エンコード */
static int do_encode(const char* in_filename, const char* out_filename, uint32_t encode_preset_no, uint8_t verpose_flag);

/* デコード */
static int do_decode(const char* in_filename, const char* out_filename, uint8_t enable_crc_check, uint8_t verpose_flag);

/* ストリーミングデコード */
static int do_streaming_decode(const char* in_filename, const char* out_filename, uint8_t enable_crc_check, uint8_t verpose_flag);

/* コマンドライン仕様 */
static struct CommandLineParserSpecification command_line_spec[] = {
  { 'e', "encode", COMMAND_LINE_PARSER_FALSE, 
    "Encode mode", 
    NULL, COMMAND_LINE_PARSER_FALSE },
  { 'd', "decode", COMMAND_LINE_PARSER_FALSE, 
    "Decode mode", 
    NULL, COMMAND_LINE_PARSER_FALSE },
  { 'm', "mode", COMMAND_LINE_PARSER_TRUE, 
    "Specify compress mode: 0(fast decode), ..., 4(high compression) default:2", 
    NULL, COMMAND_LINE_PARSER_FALSE },
  { 'p', "verpose", COMMAND_LINE_PARSER_FALSE, 
    "Verpose mode(try to display all information)", 
    NULL, COMMAND_LINE_PARSER_FALSE },
  { 's', "silence", COMMAND_LINE_PARSER_FALSE, 
    "Silence mode(suppress outputs)", 
    NULL, COMMAND_LINE_PARSER_FALSE },
  { 'c', "crc-check", COMMAND_LINE_PARSER_TRUE, 
    "Whether to check CRC16 at decoding(yes or no) default:yes", 
    NULL, COMMAND_LINE_PARSER_FALSE },
  { 'h', "help", COMMAND_LINE_PARSER_FALSE, 
    "Show command help message", 
    NULL, COMMAND_LINE_PARSER_FALSE },
  { 'v', "version", COMMAND_LINE_PARSER_FALSE, 
    "Show version information", 
    NULL, COMMAND_LINE_PARSER_FALSE },
  { 'r', "streaming", COMMAND_LINE_PARSER_FALSE, 
    "Use streaming decode(for debug; 120fps)", 
    NULL, COMMAND_LINE_PARSER_FALSE },
  { 0, }
};

/* エンコードプリセット */
static const struct SLAEncodeParameter encode_preset[] = {
  /* parcor, longterm, lms,             ch_porcess_method,                   window_func_type, max_block_size */
  {       8,        1,   4,      SLA_CHPROCESSMETHOD_NONE, SLA_WINDOWFUNCTIONTYPE_RECTANGULAR,           4096 },
  {       8,        1,   8, SLA_CHPROCESSMETHOD_STEREO_MS,         SLA_WINDOWFUNCTIONTYPE_SIN,          12288 },
  {      16,        1,   8, SLA_CHPROCESSMETHOD_STEREO_MS,         SLA_WINDOWFUNCTIONTYPE_SIN,          12288 },
  {      32,        3,   8, SLA_CHPROCESSMETHOD_STEREO_MS,         SLA_WINDOWFUNCTIONTYPE_SIN,          12288 },
  {      32,        3,   8, SLA_CHPROCESSMETHOD_STEREO_MS,         SLA_WINDOWFUNCTIONTYPE_SIN,          16384 }
};

/* エンコードプリセット数 */
static const uint32_t num_encode_preset = sizeof(encode_preset) / sizeof(encode_preset[0]);

/* デフォルトのプリセット番号 */
static const uint32_t default_preset_no = 2;

/* エンコード */
static int do_encode(const char* in_filename, const char* out_filename, uint32_t encode_preset_no, uint8_t verpose_flag)
{
  FILE*                             out_fp;
  struct WAVFile*                   in_wav;
  struct stat                       fstat;
  struct SLAEncoder*                encoder;
  struct SLAEncoderConfig           config;
  struct SLAEncodeParameter         enc_param;
  struct SLAWaveFormat              wave_format;
  uint8_t*                          buffer;
  uint32_t                          buffer_size, encoded_data_size;
  const struct SLAEncodeParameter*  ppreset;
  SLAApiResult                      ret;

  /* エンコーダハンドルの作成 */
  config.max_num_channels         = 8;
  config.max_num_block_samples    = 16384;
  config.max_parcor_order         = 48;
  config.max_longterm_order       = 5;
  config.max_lms_order_per_filter = 40;
  config.verpose_flag             = verpose_flag;
  if ((encoder = SLAEncoder_Create(&config)) == NULL) {
    fprintf(stderr, "Failed to create encoder handle. \n");
    return 1;
  }

  /* WAVファイルオープン */
  if ((in_wav = WAV_CreateFromFile(in_filename)) == NULL) {
    fprintf(stderr, "Failed to open %s \n", in_filename);
    return 1;
  }

  /* 波形パラメータの設定 */
  wave_format.num_channels    = in_wav->format.num_channels;
  wave_format.bit_per_sample  = in_wav->format.bits_per_sample;
  wave_format.sampling_rate   = in_wav->format.sampling_rate;
  if ((ret = SLAEncoder_SetWaveFormat(encoder, &wave_format)) != SLA_APIRESULT_OK) {
    fprintf(stderr, "Failed to set wave parameter: %d \n", ret);
    return 1;
  }

  /* エンコードパラメータの設定 */
  ppreset = &encode_preset[encode_preset_no];
  enc_param.parcor_order            = ppreset->parcor_order;
  enc_param.longterm_order          = ppreset->longterm_order;
  enc_param.lms_order_per_filter    = ppreset->lms_order_per_filter;
  if ((in_wav->format.num_channels == 2) 
      && (ppreset->ch_process_method == SLA_CHPROCESSMETHOD_STEREO_MS)) {
    /* 音源がステレオのときだけMSは有効 */
    enc_param.ch_process_method = SLA_CHPROCESSMETHOD_STEREO_MS;
  } else {
    enc_param.ch_process_method = SLA_CHPROCESSMETHOD_NONE;
  }
  enc_param.window_function_type  = ppreset->window_function_type;
  enc_param.max_num_block_samples = ppreset->max_num_block_samples;
  if ((ret = SLAEncoder_SetEncodeParameter(encoder, &enc_param)) != SLA_APIRESULT_OK) {
    fprintf(stderr, "Failed to set encode parameter: %d \n", ret);
    return 1;
  }

  /* 入力ファイルのサイズを拾っておく */
  stat(in_filename, &fstat);
  /* 入力wavの2倍よりは大きくならないだろうという想定 */
  buffer_size = (uint32_t)(2 * fstat.st_size);

  /* エンコードデータ領域を作成 */
  buffer = (uint8_t *)malloc(buffer_size);

  /* 一括エンコード */
  if ((ret = SLAEncoder_EncodeWhole(encoder, 
          (const int32_t* const *)in_wav->data, in_wav->format.num_samples,
          buffer, buffer_size, &encoded_data_size)) != SLA_APIRESULT_OK) {
    fprintf(stderr, "Encoding error! %d \n", ret);
    return 1;
  }

  out_fp = fopen(out_filename, "wb");
  fwrite(buffer, sizeof(uint8_t), encoded_data_size, out_fp);

  if (verpose_flag != 0) {
    printf("Encode succuess! size:%d -> %d \n", 
        (uint32_t)fstat.st_size, encoded_data_size);
  }

  fclose(out_fp);
  free(buffer);
  WAV_Destroy(in_wav);
  SLAEncoder_Destroy(encoder);

  return 0;
}

/* デコード */
static int do_decode(const char* in_filename, const char* out_filename, uint8_t enable_crc_check, uint8_t verpose_flag)
{
  FILE*                     in_fp;
  struct WAVFile*           out_wav;
  struct WAVFileFormat      wav_format;
  struct stat               fstat;
  struct SLADecoder*        decoder;
  struct SLADecoderConfig   config;
  struct SLAHeaderInfo      header;
  uint8_t*                  buffer;
  uint32_t                  buffer_size, decode_num_samples;
  SLAApiResult              ret;

  /* デコーダハンドルの作成 */
  config.max_num_channels         = 8;
  config.max_num_block_samples    = 16384;
  config.max_parcor_order         = 48;
  config.max_longterm_order       = 5;
  config.max_lms_order_per_filter = 40;
  config.enable_crc_check         = enable_crc_check;
  config.verpose_flag             = verpose_flag;
  if ((decoder = SLADecoder_Create(&config)) == NULL) {
    fprintf(stderr, "Failed to create decoder handle. \n");
    return 1;
  }

  /* 入力ファイルオープン */
  in_fp = fopen(in_filename, "rb");
  /* 入力ファイルのサイズ取得 / バッファ領域割り当て */
  stat(in_filename, &fstat);
  buffer_size = (uint32_t)fstat.st_size;
  buffer = (uint8_t *)malloc(buffer_size);
  /* バッファ領域にデータをロード */
  fread(buffer, sizeof(uint8_t), buffer_size, in_fp);
  fclose(in_fp);

  /* ヘッダデコード */
  if ((ret = SLADecoder_DecodeHeader(buffer, buffer_size, &header))
      != SLA_APIRESULT_OK) {
    fprintf(stderr, "Failed to get header information: %d \n", ret);
    return 1;
  }

  /* ヘッダから得られた情報を表示 */
  if (verpose_flag != 0) {
    printf("Num Channels:                %d \n", header.wave_format.num_channels);
    printf("Bit Per Sample:              %d \n", header.wave_format.bit_per_sample);
    printf("Sampling Rate:               %d \n", header.wave_format.sampling_rate);
    printf("Offset Left Shift:           %d \n", header.wave_format.offset_lshift);
    printf("PARCOR Order:                %d \n", header.encode_param.parcor_order);
    printf("Longterm Order:              %d \n", header.encode_param.longterm_order);
    printf("LMS Order Par Filter:        %d \n", header.encode_param.lms_order_per_filter);
    printf("Channel Process Method:      %d \n", header.encode_param.ch_process_method);
    printf("Max Number of Block Samples: %d \n", header.encode_param.max_num_block_samples);
    printf("Number of Samples:           %d \n", header.num_samples);
    printf("Number of Blocks:            %d \n", header.num_blocks);
    printf("Max Block Size:              %d \n", header.max_block_size);
    printf("Max Bit Per Second(bps):     %d \n", header.max_bit_per_second);
  }

  /* 出力wavハンドルの生成 */
  wav_format.data_format     = WAV_DATA_FORMAT_PCM;
  wav_format.num_channels    = header.wave_format.num_channels;
  wav_format.sampling_rate   = header.wave_format.sampling_rate;
  wav_format.bits_per_sample = header.wave_format.bit_per_sample;
  wav_format.num_samples     = header.num_samples;
  if ((out_wav = WAV_Create(&wav_format)) == NULL) {
    fprintf(stderr, "Failed to create wav handle. \n");
    return 1;
  }

  /* ヘッダから読み取ったパラメータをデコーダにセット */
  if ((ret = SLADecoder_SetWaveFormat(decoder, 
          &header.wave_format)) != SLA_APIRESULT_OK) {
    fprintf(stderr, "Failed to set wave parameter: %d \n", ret);
    return 1;
  }
  if ((ret = SLADecoder_SetEncodeParameter(decoder, 
          &header.encode_param)) != SLA_APIRESULT_OK) {
    fprintf(stderr, "Failed to set encode parameter: %d \n", ret);
    return 1;
  }

  /* 一括デコード */
  if ((ret = SLADecoder_DecodeWhole(decoder, 
          buffer, buffer_size,
          (int32_t **)out_wav->data, out_wav->format.num_samples,
          &decode_num_samples)) != SLA_APIRESULT_OK) {
    fprintf(stderr, "Decoding error! %d \n", ret);
    return 1;
  }

  /* WAVファイル書き出し */
  if (WAV_WriteToFile(out_filename, out_wav) != WAV_APIRESULT_OK) {
    fprintf(stderr, "Failed to write wav file. \n");
    return 1;
  }

  free(buffer);
  WAV_Destroy(out_wav);
  SLADecoder_Destroy(decoder);

  return 0;
}

/* ストリーミングデコード */
static int do_streaming_decode(const char* in_filename, const char* out_filename, uint8_t enable_crc_check, uint8_t verpose_flag)
{
  FILE*                               in_fp;
  struct WAVFile*                     out_wav;
  struct WAVFileFormat                wav_format;
  struct stat                         fstat;
  struct SLAStreamingDecoder*         decoder;
  struct SLADecoderConfig             core_config;
  struct SLAStreamingDecoderConfig    streaming_config;
  struct SLAHeaderInfo                header;
  uint8_t*                            buffer;
  uint32_t                            buffer_size, sample_progress, data_progress;
  SLAApiResult                        ret;

  /* デコーダハンドルの作成 */
  core_config.max_num_channels         = 8;
  core_config.max_num_block_samples    = 16384;
  core_config.max_parcor_order         = 48;
  core_config.max_longterm_order       = 5;
  core_config.max_lms_order_per_filter = 40;
  core_config.enable_crc_check         = enable_crc_check;
  core_config.verpose_flag             = verpose_flag;

  streaming_config.max_bit_per_sample = 24;
  streaming_config.decode_interval_hz = 120.0f;
  streaming_config.core_config = core_config;

  if ((decoder = SLAStreamingDecoder_Create(&streaming_config)) == NULL) {
    fprintf(stderr, "Failed to create decoder handle. \n");
    return 1;
  }

  /* 入力ファイルオープン */
  in_fp = fopen(in_filename, "rb");
  /* 入力ファイルのサイズ取得 / バッファ領域割り当て */
  stat(in_filename, &fstat);
  buffer_size = (uint32_t)fstat.st_size;
  buffer = (uint8_t *)malloc(buffer_size);
  /* バッファ領域にデータをロード */
  fread(buffer, sizeof(uint8_t), buffer_size, in_fp);
  fclose(in_fp);

  /* ヘッダデコード */
  if ((ret = SLADecoder_DecodeHeader(buffer, buffer_size, &header))
      != SLA_APIRESULT_OK) {
    fprintf(stderr, "Failed to get header information: %d \n", ret);
    return 1;
  }

  /* ヘッダから得られた情報を表示 */
  if (verpose_flag != 0) {
    printf("Num Channels:                %d \n", header.wave_format.num_channels);
    printf("Bit Per Sample:              %d \n", header.wave_format.bit_per_sample);
    printf("Sampling Rate:               %d \n", header.wave_format.sampling_rate);
    printf("Offset Left Shift:           %d \n", header.wave_format.offset_lshift);
    printf("PARCOR Order:                %d \n", header.encode_param.parcor_order);
    printf("Longterm Order:              %d \n", header.encode_param.longterm_order);
    printf("LMS Order Per Filter:        %d \n", header.encode_param.lms_order_per_filter);
    printf("Channel Process Method:      %d \n", header.encode_param.ch_process_method);
    printf("Max Number of Block Samples: %d \n", header.encode_param.max_num_block_samples);
    printf("Number of Samples:           %d \n", header.num_samples);
    printf("Number of Blocks:            %d \n", header.num_blocks);
    printf("Max Block Size:              %d \n", header.max_block_size);
    printf("Max Bit Per Second(bps):     %d \n", header.max_bit_per_second);
  }

  /* 出力wavハンドルの生成 */
  wav_format.data_format     = WAV_DATA_FORMAT_PCM;
  wav_format.num_channels    = header.wave_format.num_channels;
  wav_format.sampling_rate   = header.wave_format.sampling_rate;
  wav_format.bits_per_sample = header.wave_format.bit_per_sample;
  wav_format.num_samples     = header.num_samples;
  if ((out_wav = WAV_Create(&wav_format)) == NULL) {
    fprintf(stderr, "Failed to create wav handle. \n");
    return 1;
  }

  /* ヘッダから読み取ったパラメータをデコーダにセット */
  if ((ret = SLAStreamingDecoder_SetWaveFormat(decoder, 
          &header.wave_format)) != SLA_APIRESULT_OK) {
    fprintf(stderr, "Failed to set wave parameter: %d \n", ret);
    return 1;
  }
  if ((ret = SLAStreamingDecoder_SetEncodeParameter(decoder, 
          &header.encode_param)) != SLA_APIRESULT_OK) {
    fprintf(stderr, "Failed to set encode parameter: %d \n", ret);
    return 1;
  }

  /* ストリーミングデコード */
  sample_progress = 0;
  data_progress = SLA_HEADER_SIZE;
  while (sample_progress < header.num_samples) {
    uint32_t ch;
    uint32_t put_data_size, estimate_min_data_size, tmp_output_samples;
    int32_t  *output_ptr[8];
    const uint8_t* dummy_out_ptr;
    uint32_t dummy_out_size;

    /* 供給データサイズの確定 */
    if (sample_progress == 0) {
      /* 最初は最大ブロックサイズで見積もる */
      estimate_min_data_size = header.max_block_size;
    } else {
      SLAStreamingDecoder_EstimateMinimumNessesaryDataSize(decoder, &estimate_min_data_size);
    }
    put_data_size = MIN(estimate_min_data_size, buffer_size - data_progress);

    /* データ供給 */
    SLAStreamingDecoder_AppendDataFragment(decoder, &buffer[data_progress], put_data_size);

    /* ストリーミングデコード */
    for (ch = 0; ch < header.wave_format.num_channels; ch++) {
      output_ptr[ch] = &out_wav->data[ch][sample_progress];
    }
    if ((ret = SLAStreamingDecoder_Decode(decoder,
            output_ptr, header.num_samples - sample_progress, &tmp_output_samples)) != SLA_APIRESULT_OK) {
      fprintf(stderr, "Streaming Decode failed! ret:%d \n", ret);
      return 1;
    }

    /* 消費済みデータ回収 */
    SLAStreamingDecoder_CollectDataFragment(decoder, &dummy_out_ptr, &dummy_out_size);

    /* 出力を進める */
    data_progress     += put_data_size;
    sample_progress   += tmp_output_samples;

    if (verpose_flag != 0) {
      printf("progress: %4.1f %% \r", (double)sample_progress / header.num_samples * 100.0f);
      fflush(stdout);
    }
  }

  /* WAVファイル書き出し */
  if (WAV_WriteToFile(out_filename, out_wav) != WAV_APIRESULT_OK) {
    fprintf(stderr, "Failed to write wav file. \n");
    return 1;
  }

  free(buffer);
  WAV_Destroy(out_wav);
  SLAStreamingDecoder_Destroy(decoder);

  return 0;
}

/* 使用法の表示 */
static void print_usage(char** argv)
{
  printf("Usage: %s [options] INPUT_FILE_NAME OUTPUT_FILE_NAME \n", argv[0]);
}

/* バージョン情報の表示 */
static void print_version_info(void)
{
  printf("SLA - Solitary Lossless Audio Compressor Version %s \n", SLA_VERSION_STRING);
}

/* メインエントリ */
int main(int argc, char** argv)
{
  const char* filename_ptr[2] = { NULL, NULL };
  const char* input_file;
  const char* output_file;
  uint8_t     verpose_flag = 1;

  /* 引数が足らない */
  if (argc == 1) {
    print_usage(argv);
    return 1;
  }

  /* コマンドライン解析 */
  if (CommandLineParser_ParseArguments(command_line_spec,
        argc, argv, filename_ptr, sizeof(filename_ptr) / sizeof(filename_ptr[0]))
      != COMMAND_LINE_PARSER_RESULT_OK) {
    return 1;
  }

  /* ヘルプやバージョン情報の表示判定 */
  if (CommandLineParser_GetOptionAcquired(command_line_spec, "help") == COMMAND_LINE_PARSER_TRUE) {
    print_usage(argv);
    printf("options: \n");
    CommandLineParser_PrintDescription(command_line_spec);
    return 0;
  } else if (CommandLineParser_GetOptionAcquired(command_line_spec, "version") == COMMAND_LINE_PARSER_TRUE) {
    print_version_info();
    return 0;
  }

  /* 入力ファイル名の取得 */
  if ((input_file = filename_ptr[0]) == NULL) {
    fprintf(stderr, "%s: input file must be specified. \n", argv[0]);
    return 1;
  }
  
  /* 出力ファイル名の取得 */
  if ((output_file = filename_ptr[1]) == NULL) {
    fprintf(stderr, "%s: output file must be specified. \n", argv[0]);
    return 1;
  }

  /* エンコードとデコードは同時に指定できない */
  if ((CommandLineParser_GetOptionAcquired(command_line_spec, "decode") == COMMAND_LINE_PARSER_TRUE)
      && (CommandLineParser_GetOptionAcquired(command_line_spec, "encode") == COMMAND_LINE_PARSER_TRUE)) {
      fprintf(stderr, "%s: encode and decode mode cannot specify simultaneously. \n", argv[0]);
      return 1;
  }

  /* 情報表示オプション */
  if (CommandLineParser_GetOptionAcquired(command_line_spec, "verpose") == COMMAND_LINE_PARSER_TRUE) {
    verpose_flag = 1;
  } else if (CommandLineParser_GetOptionAcquired(command_line_spec, "silence") == COMMAND_LINE_PARSER_TRUE) {
    verpose_flag = 0;
  }

  if (CommandLineParser_GetOptionAcquired(command_line_spec, "decode") == COMMAND_LINE_PARSER_TRUE) {
    /* デコード */
    uint8_t enable_crc_check = 1;
    /* CRC有効フラグを取得 */
    if (CommandLineParser_GetOptionAcquired(command_line_spec, "crc-check") == COMMAND_LINE_PARSER_TRUE) {
      const char* crc_check_arg
        = CommandLineParser_GetArgumentString(command_line_spec, "crc-check");
      enable_crc_check = (strcmp(crc_check_arg, "yes") == 0) ? 1 : 0;
    }
    /* 一括デコード実行 */
    if (CommandLineParser_GetOptionAcquired(command_line_spec, "streaming") == COMMAND_LINE_PARSER_TRUE) {
      if (do_streaming_decode(input_file, output_file, enable_crc_check, verpose_flag) != 0) {
        fprintf(stderr, "%s: failed to streaming decode %s. \n", argv[0], input_file);
        return 1;
      }
    } else {
      if (do_decode(input_file, output_file, enable_crc_check, verpose_flag) != 0) {
        fprintf(stderr, "%s: failed to decode %s. \n", argv[0], input_file);
        return 1;
      }
    }
  } else if (CommandLineParser_GetOptionAcquired(command_line_spec, "encode") == COMMAND_LINE_PARSER_TRUE) {
    /* エンコード */
    uint32_t encode_preset_no = default_preset_no;
    /* エンコードプリセット番号取得 */
    if (CommandLineParser_GetOptionAcquired(command_line_spec, "mode") == COMMAND_LINE_PARSER_TRUE) {
      encode_preset_no = (uint32_t)strtol(CommandLineParser_GetArgumentString(command_line_spec, "mode"), NULL, 10);
      if (encode_preset_no >= num_encode_preset) {
        fprintf(stderr, "%s: encode preset number is out of range. \n", argv[0]);
        return 1;
      }
    }
    /* 一括エンコード実行 */
    if (do_encode(input_file, output_file, encode_preset_no, verpose_flag) != 0) {
      return 1;
    }
  } else {
    fprintf(stderr, "%s: decode(-d) or encode(-e) option must be specified. \n", argv[0]);
    return 1;
  }

  return 0;
}
