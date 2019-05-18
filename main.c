#include "SLAEncoder.h"
#include "SLADecoder.h"

#include "wav.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* エンコード */
int encode(const char* in_filename, const char* out_filename)
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
  config.max_lms_order_par_filter = 20;
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
  enc_param.parcor_order            = 10;
  enc_param.longterm_order          = 1;
  enc_param.lms_order_par_filter    = 10;
  enc_param.num_lms_filter_cascade  = 1;
  if (in_wav->format.num_channels == 2) {
    enc_param.ch_process_method = SLA_CHPROCESSMETHOD_STEREO_MS;
  } else {
    enc_param.ch_process_method = SLA_CHPROCESSMETHOD_NONE;
  }
  enc_param.window_function_type  = SLA_WINDOWFUNCTIONTYPE_SIN;
  enc_param.max_num_block_samples = 1024 * 12;
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
int decode(const char* in_filename, const char* out_filename)
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
  config.max_lms_order_par_filter = 32;
  config.enable_crc_check         = 1;
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

void print_usage(char** argv)
{
  fprintf(stderr, "Usage: %s -[dc] input output \n", argv[0]);
}

int main(int argc, char** argv)
{
  if (argc != 4) {
    print_usage(argv);
    return 1;
  }

  if (strcmp(argv[1], "-d") == 0) {
    if (decode(argv[2], argv[3]) != 0) {
      return 1;
    }
  } else if (strcmp(argv[1], "-c") == 0) {
    if (encode(argv[2], argv[3]) != 0) {
      return 1;
    }
  } else {
    print_usage(argv);
    return 1;
  }

  return 0;
}
