#include "SLAEncoder.h"
#include "SLADecoder.h"

#include "wav.h"
#include "command_line_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* CUIインターフェースバージョン */
#define SLA_CUI_VERSION     1

/* コマンドライン仕様 */
static struct CommandLineParserSpecification command_line_spec[] = {
  { 'e', "encode", COMMAND_LINE_PARSER_FALSE, COMMAND_LINE_PARSER_FALSE, 
    "Encode mode", 
    NULL, COMMAND_LINE_PARSER_FALSE },
  { 'd', "decode", COMMAND_LINE_PARSER_FALSE, COMMAND_LINE_PARSER_FALSE, 
    "Decode mode", 
    NULL, COMMAND_LINE_PARSER_FALSE },
  { 'i', "input", COMMAND_LINE_PARSER_TRUE, COMMAND_LINE_PARSER_FALSE, 
    "Specify input file", 
    NULL, COMMAND_LINE_PARSER_FALSE },
  { 'o', "output", COMMAND_LINE_PARSER_TRUE, COMMAND_LINE_PARSER_FALSE, 
    "Specify output file", 
    NULL, COMMAND_LINE_PARSER_FALSE },
  { 'm', "mode", COMMAND_LINE_PARSER_TRUE, COMMAND_LINE_PARSER_FALSE, 
    "Specify compress mode: 0(fast decode), ..., 4(high compression)", 
    "2", COMMAND_LINE_PARSER_FALSE },
  { 'p', "verpose", COMMAND_LINE_PARSER_FALSE, COMMAND_LINE_PARSER_FALSE, 
    "Verpose mode(try to display all information)", 
    NULL, COMMAND_LINE_PARSER_FALSE },
  { 's', "silence", COMMAND_LINE_PARSER_FALSE, COMMAND_LINE_PARSER_FALSE, 
    "Silence mode(suppress outputs)", 
    NULL, COMMAND_LINE_PARSER_FALSE },
  { 'c', "crc-check", COMMAND_LINE_PARSER_TRUE, COMMAND_LINE_PARSER_FALSE, 
    "Whether to check CRC16 at decoding(yes or no)", 
    "yes", COMMAND_LINE_PARSER_FALSE },
  { 'h', "help", COMMAND_LINE_PARSER_FALSE, COMMAND_LINE_PARSER_FALSE, 
    "Show command help message", 
    NULL, COMMAND_LINE_PARSER_FALSE },
  { 'v', "version", COMMAND_LINE_PARSER_FALSE, COMMAND_LINE_PARSER_FALSE, 
    "Show version information", 
    NULL, COMMAND_LINE_PARSER_FALSE },
};

/* コマンドライン仕様数 */
const uint32_t num_specification = sizeof(command_line_spec) / sizeof(command_line_spec[0]);

/* エンコードプリセット */
static const struct SLAEncodeParameter encode_preset[] = {
  /* parcor, longterm, lms, lms_cascade,                ch_porcess_method,                    window_func_type, max_block_size */
  {       5,        1,   5,           1,         SLA_CHPROCESSMETHOD_NONE,  SLA_WINDOWFUNCTIONTYPE_RECTANGULAR,           4096 },
  {       5,        1,   5,           1,    SLA_CHPROCESSMETHOD_STEREO_MS,          SLA_WINDOWFUNCTIONTYPE_SIN,          10240 },
  {      10,        1,  10,           1,    SLA_CHPROCESSMETHOD_STEREO_MS,          SLA_WINDOWFUNCTIONTYPE_SIN,          12288 },
  {      20,        1,  20,           1,    SLA_CHPROCESSMETHOD_STEREO_MS,          SLA_WINDOWFUNCTIONTYPE_SIN,          12288 },
  {      30,        3,  30,           1,    SLA_CHPROCESSMETHOD_STEREO_MS,          SLA_WINDOWFUNCTIONTYPE_SIN,          16384 },
};

/* エンコードプリセット数 */
const uint32_t num_encode_preset = sizeof(encode_preset) / sizeof(encode_preset[0]);

/* エンコード */
int do_encode(const char* in_filename, const char* out_filename, uint32_t encode_preset_no, uint8_t verpose_flag)
{
  FILE*                     out_fp;
  struct WAVFile*           in_wav;
  struct stat               fstat;
  struct SLAEncoder*        encoder;
  struct SLAEncoderConfig   config;
  struct SLAEncodeParameter enc_param;
  struct SLAWaveFormat      wave_format;
  uint8_t*                  buffer;
  uint32_t                  buffer_size, encoded_data_size;
  SLAApiResult              ret;

  /* エンコーダハンドルの作成 */
  config.max_num_channels         = 2;
  config.max_num_block_samples    = 16384;
  config.max_parcor_order         = 48;
  config.max_longterm_order       = 5;
  config.max_lms_order_par_filter = 40;
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
  enc_param.parcor_order            = encode_preset[encode_preset_no].parcor_order;
  enc_param.longterm_order          = encode_preset[encode_preset_no].longterm_order;
  enc_param.lms_order_par_filter    = encode_preset[encode_preset_no].lms_order_par_filter;
  enc_param.num_lms_filter_cascade  = encode_preset[encode_preset_no].num_lms_filter_cascade;
  if ((in_wav->format.num_channels == 2) 
      && (encode_preset[encode_preset_no].window_function_type == SLA_CHPROCESSMETHOD_STEREO_MS)) {
    /* 音源がステレオのときだけMSは有効 */
    enc_param.ch_process_method = SLA_CHPROCESSMETHOD_STEREO_MS;
  } else {
    enc_param.ch_process_method = SLA_CHPROCESSMETHOD_NONE;
  }
  enc_param.window_function_type  = encode_preset[encode_preset_no].window_function_type;
  enc_param.max_num_block_samples = encode_preset[encode_preset_no].max_num_block_samples;
  if ((ret = SLAEncoder_SetEncodeParameter(encoder, &enc_param)) != SLA_APIRESULT_OK) {
    fprintf(stderr, "Failed to set encode parameter: %d \n", ret);
    return 1;
  }

  /* 入力ファイルのサイズを拾っておく */
  stat(in_filename, &fstat);
  buffer_size = (uint32_t)fstat.st_size;

  /* エンコードデータ領域を作成
   * 入力wavよりは大きくならないだろうという想定 */
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

  printf("Encode succuess! size:%d -> %d \n", buffer_size, encoded_data_size);

  fclose(out_fp);
  free(buffer);
  WAV_Destroy(in_wav);
  SLAEncoder_Destroy(encoder);

  return 0;
}

/* デコード */
int do_decode(const char* in_filename, const char* out_filename, uint8_t enable_crc_check, uint8_t verpose_flag)
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
  config.max_num_channels         = 2;
  config.max_num_block_samples    = 16384;
  config.max_parcor_order         = 48;
  config.max_longterm_order       = 5;
  config.max_lms_order_par_filter = 40;
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
    printf("PARCOR Order:                %d \n", header.encode_param.parcor_order);
    printf("Longterm Order:              %d \n", header.encode_param.longterm_order);
    printf("LMS Order Par Filter:        %d \n", header.encode_param.lms_order_par_filter);
    printf("LMS Num Filter Cascades:     %d \n", header.encode_param.num_lms_filter_cascade);
    printf("Channel Process Method:      %d \n", header.encode_param.ch_process_method);
    printf("Max Number of Block Samples: %d \n", header.encode_param.max_num_block_samples);
    printf("Number of Samples:           %d \n", header.num_samples);
    printf("Number of Blocks:            %d \n", header.num_blocks);
    printf("Max Block Size:              %d \n", header.max_block_size);
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

/* 使用法の表示 */
static void print_usage(char** argv)
{
  printf("Usage: %s -[ed] [options] INPUT_FILE_NAME -o OUTPUT_FILE_NAME \n", argv[0]);
}

/* バージョン情報の表示 */
static void print_version_info(void)
{
  printf("SLA - Solitary Lossless Audio Compressor (a.k.a. SHINING LINE*) \n"
      "Encoder Version: %d \n" "Decoder Version: %d \n" "CUI     Version: %d \n",
      SLA_ENCODER_VERSION, SLA_DECODER_VERSION, SLA_CUI_VERSION);
}

/* メインエントリ */
int main(int argc, char** argv)
{
  const char* filename_ptr[1] = { NULL };
  const char* input_file;
  const char* output_file;
  uint8_t     verpose_flag = 1;

  /* 引数が足らない */
  if (argc == 1) {
    print_usage(argv);
    return 1;
  }

  /* コマンドライン解析 */
  if (CommandLineParser_ParseArguments(argc, argv,
        command_line_spec, num_specification,
        filename_ptr, sizeof(filename_ptr) / sizeof(filename_ptr[0]))
      != COMMAND_LINE_PARSER_RESULT_OK) {
    return 1;
  }

  /* ヘルプやバージョン情報の表示判定 */
  if (CommandLineParser_GetOptionAcquired(
        command_line_spec, num_specification, "help") == COMMAND_LINE_PARSER_TRUE) {
    print_usage(argv);
    CommandLineParser_PrintDescription(command_line_spec, num_specification);
    return 0;
  } else if (CommandLineParser_GetOptionAcquired(
        command_line_spec, num_specification, "version") == COMMAND_LINE_PARSER_TRUE) {
    print_version_info();
    return 0;
  }

  /* 入力ファイル名の取得 */
  if ((input_file = filename_ptr[0]) == NULL) {
    if ((input_file = CommandLineParser_GetArgumentString(
            command_line_spec, num_specification, "input")) == NULL) {
      fprintf(stderr, "%s: input file must be specified. \n", argv[0]);
      return 1;
    }
  }
  
  /* 出力ファイル名の取得 */
  if ((output_file = CommandLineParser_GetArgumentString(
          command_line_spec, num_specification, "output")) == NULL) {
    fprintf(stderr, "%s: output file must be specified. \n", argv[0]);
    return 1;
  }

  /* エンコードとデコードは同時に指定できない */
  if ((CommandLineParser_GetOptionAcquired(
          command_line_spec, num_specification, "decode") == COMMAND_LINE_PARSER_TRUE)
      && (CommandLineParser_GetOptionAcquired(
          command_line_spec, num_specification, "encode") == COMMAND_LINE_PARSER_TRUE)) {
      fprintf(stderr, "%s: encode and decode mode cannot specify simultaneously. \n", argv[0]);
      return 1;
  }

  /* 情報表示オプション */
  if (CommandLineParser_GetOptionAcquired(
        command_line_spec, num_specification, "verpose") == COMMAND_LINE_PARSER_TRUE) {
    verpose_flag = 1;
  } else if (CommandLineParser_GetOptionAcquired(
        command_line_spec, num_specification, "silence") == COMMAND_LINE_PARSER_TRUE) {
    verpose_flag = 0;
  }

  if (CommandLineParser_GetOptionAcquired(
        command_line_spec, num_specification, "decode") == COMMAND_LINE_PARSER_TRUE) {
    /* デコード */
    const char* crc_check_arg
      = CommandLineParser_GetArgumentString(command_line_spec, num_specification, "crc-check");
    uint8_t enable_crc_check = (strcmp(crc_check_arg, "yes") == 0) ? 1 : 0;
    /* 一括デコード実行 */
    if (do_decode(input_file, output_file, enable_crc_check, verpose_flag) != 0) {
      fprintf(stderr, "%s: failed to decode %s. \n", argv[0], input_file);
      return 1;
    }
  } else if (CommandLineParser_GetOptionAcquired(
        command_line_spec, num_specification, "encode") == COMMAND_LINE_PARSER_TRUE) {
    /* エンコード */
    /* エンコードプリセット番号取得 */
    uint32_t encode_preset_no
      = (uint32_t)strtol(CommandLineParser_GetArgumentString(command_line_spec, num_specification, "mode"), NULL, 10);
    if (encode_preset_no >= num_encode_preset) {
      fprintf(stderr, "%s: encode preset no out of range. \n", argv[0]);
      return 1;
    }
    /* 一括エンコード実行 */
    if (do_encode(input_file, output_file, encode_preset_no, verpose_flag) != 0) {
      return 1;
    }
  } else {
    fprintf(stderr, "%s: decode or encode option must be specified. \n", argv[0]);
    return 1;
  }

  return 0;
}
