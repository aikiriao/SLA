#include "test.h"
#include "SLA_TestUtility.h"
#include "../SLAEncoder.h"

#include <string.h>
#include <stdlib.h>
#include <math.h>

/* テスト対象のモジュール */
#include "../SLADecoder.c"

int testSLADecoder_Initialize(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);
  return 0;
}

int testSLADecoder_Finalize(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);
  return 0;
}

/* ヘッダデコードテスト */
static void testSLADecoder_DecodeHeaderTest(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

  /* エンコードしたものをデコードして元に戻るか？ */
  {
    struct SLAHeaderInfo write_header, get_header;
    uint8_t data[SLA_HEADER_SIZE];

    SLATestUtility_SetValidHeaderInfo(&write_header);

    /* 書き出し */
    Test_AssertEqual(
        SLAEncoder_EncodeHeader(&write_header, data, sizeof(data)),
        SLA_APIRESULT_OK);

    /* 取得 */
    Test_AssertEqual(
        SLADecoder_DecodeHeader((const uint8_t *)data, sizeof(data), &get_header),
        SLA_APIRESULT_OK);

    /* 一致確認 */
    Test_AssertEqual(write_header.wave_format.num_channels,       get_header.wave_format.num_channels);
    Test_AssertEqual(write_header.wave_format.bit_per_sample,     get_header.wave_format.bit_per_sample);
    Test_AssertEqual(write_header.wave_format.sampling_rate,      get_header.wave_format.sampling_rate);
    Test_AssertEqual(write_header.encode_param.parcor_order,      get_header.encode_param.parcor_order);
    Test_AssertEqual(write_header.encode_param.longterm_order,    get_header.encode_param.longterm_order);
    Test_AssertEqual(write_header.encode_param.nlms_order,        get_header.encode_param.nlms_order);
    Test_AssertEqual(write_header.encode_param.ch_process_method, get_header.encode_param.ch_process_method);
    Test_AssertEqual(
        write_header.encode_param.max_num_block_samples, get_header.encode_param.max_num_block_samples);
    Test_AssertEqual(write_header.num_samples,                    get_header.num_samples);
    Test_AssertEqual(write_header.num_blocks,                     get_header.num_blocks);
    Test_AssertEqual(write_header.max_block_size,                 get_header.max_block_size);
  }

  /* 簡単な失敗テスト */
  {
    struct SLAHeaderInfo write_header, get_header;
    uint8_t data[SLA_HEADER_SIZE];
    
    /* 適当なヘッダを設定 */
    SLATestUtility_SetValidHeaderInfo(&write_header);
    /* 書き出し */
    Test_AssertEqual(
        SLAEncoder_EncodeHeader(&write_header, data, sizeof(data)),
        SLA_APIRESULT_OK);

    /* 引数が不正 */
    Test_AssertEqual(
        SLADecoder_DecodeHeader(NULL, sizeof(data), &get_header),
        SLA_APIRESULT_INVALID_ARGUMENT);
    Test_AssertEqual(
        SLADecoder_DecodeHeader(data, sizeof(data), NULL),
        SLA_APIRESULT_INVALID_ARGUMENT);

    /* データサイズが足らない */
    Test_AssertEqual(
        SLADecoder_DecodeHeader(data, SLA_HEADER_SIZE - 1, &get_header),
        SLA_APIRESULT_INSUFFICIENT_DATA_SIZE);
  }

  /* 異常なシグネチャ読み込み */
  {
    struct SLAHeaderInfo write_header, get_header;
    uint8_t data[SLA_HEADER_SIZE];
    uint32_t i;
    
    /* 適当なヘッダを設定 */
    SLATestUtility_SetValidHeaderInfo(&write_header);

    for (i = 0; i < 4; i++) {
      Test_AssertEqual(
          SLAEncoder_EncodeHeader(&write_header, data, sizeof(data)),
          SLA_APIRESULT_OK);
      /* シグネチャを一部破壊 */
      data[i] ^= 0xFF;
      Test_AssertEqual(
          SLADecoder_DecodeHeader(data, sizeof(data), &get_header),
          SLA_APIRESULT_INVALID_HEADER_FORMAT);
    }
  }

  /* CRCチェックテスト */
  {
    struct SLAHeaderInfo header;
    uint8_t data[SLA_HEADER_SIZE];

    /* 適当なヘッダを設定 */
    SLATestUtility_SetValidHeaderInfo(&header);

    /* 書き出し */
    Test_AssertEqual(
        SLAEncoder_EncodeHeader(&header, data, sizeof(data)),
        SLA_APIRESULT_OK);

    /* CRCが記録されたフィールドを壊す */
    data[SLA_HEADER_CRC16_CALC_START_OFFSET - 2] ^= 0xFF;

    /* 失敗を期待 */
    Test_AssertEqual(
        SLADecoder_DecodeHeader(data, sizeof(data), &header),
        SLA_APIRESULT_DETECT_DATA_CORRUPTION);

    /* 再度適当なヘッダを設定 */
    SLATestUtility_SetValidHeaderInfo(&header);

    /* 書き出し */
    Test_AssertEqual(
        SLAEncoder_EncodeHeader(&header, data, sizeof(data)),
        SLA_APIRESULT_OK);

    /* 末尾を破壊 */
    data[SLA_HEADER_SIZE - 1] ^= 0xFF;

    /* 失敗を期待 */
    Test_AssertEqual(
        SLADecoder_DecodeHeader(data, sizeof(data), &header),
        SLA_APIRESULT_DETECT_DATA_CORRUPTION);
  }

}

/* デコード結果の一致確認テスト */
static int32_t testSLADecoder_DoEncodeDecodeTest(
    const int32_t** input,
    const struct SLAHeaderInfo* header)
{
  int32_t                 is_ok;
  uint32_t                ch, smpl, suff_size, num_channels, num_samples;
  uint32_t                output_size, output_samples;
  int32_t**               decode_output;
  uint8_t*                data;
  struct SLADecoder*      decoder;
  struct SLAEncoder*      encoder;
  struct SLADecoderConfig decoder_config;
  struct SLAEncoderConfig encoder_config;

  assert((input != NULL) && (header != NULL));

  num_channels = header->wave_format.num_channels;
  num_samples  = header->encode_param.max_num_block_samples;
  suff_size    = SLA_CalculateSufficientBlockSize(num_channels, num_samples, header->wave_format.bit_per_sample);

  /* 出力データの割り当て */
  decode_output = (int32_t **)malloc(sizeof(int32_t *) * num_channels);
  for (ch = 0; ch < num_channels; ch++) {
    decode_output[ch] = (int32_t *)malloc(sizeof(int32_t) * num_samples);
  }
  data = (uint8_t *)malloc(suff_size);

  /* エンコーダ・デコーダハンドル作成 */
  SLAEncoder_SetDefaultConfig(&encoder_config);
  SLADecoder_SetDefaultConfig(&decoder_config);
  encoder = SLAEncoder_Create(&encoder_config);
  decoder = SLADecoder_Create(&decoder_config);

  /* パラメータ設定 */
  SLAEncoder_SetWaveFormat(encoder, &header->wave_format);
  SLAEncoder_SetEncodeParameter(encoder, &header->encode_param);
  SLADecoder_SetWaveFormat(decoder, &header->wave_format);
  SLADecoder_SetEncodeParameter(decoder, &header->encode_param);

  /* 入力をエンコード */
  if (SLAEncoder_EncodeBlock(encoder,
        input, num_samples, 
        data, suff_size, &output_size) != SLA_APIRESULT_OK) {
    is_ok = -1;
    goto EXIT;
  }

  /* 生成したデータをデコード */
  if (SLADecoder_DecodeBlock(decoder,
        data, output_size,
        decode_output, num_samples, 
        &output_size, &output_samples) != SLA_APIRESULT_OK) {
    is_ok = -2;
    goto EXIT;
  }

  /* サンプル数確認 */
  if (num_samples != output_samples) {
    is_ok = -3;
    goto EXIT;
  }

  /* 一致確認 */
  for (ch = 0; ch < num_channels; ch++) {
    for (smpl = 0; smpl < num_samples; smpl++) {
      if (input[ch][smpl] != decode_output[ch][smpl]) {
      printf("[%d][%d] %d vs %d \n", ch, smpl, input[ch][smpl], decode_output[ch][smpl]);
        is_ok = -4;
        goto EXIT;
      }
    }
  }

  /* ここまで来れば成功 */
  is_ok = 0;

EXIT:
  /* ハンドル破棄 */
  SLAEncoder_Destroy(encoder);
  SLADecoder_Destroy(decoder);

  /* 出力データの領域開放 */
  for (ch = 0; ch < num_channels; ch++) {
    free(decode_output[ch]);
  }
  free(decode_output);
  free(data);

  return is_ok;
}

/* 1ブロックデコードテスト */
static void testSLADecoder_DecodeBlockTest(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

  /* 失敗テスト */
  {
    struct SLADecoder*      decoder;
    struct SLADecoderConfig config;
    struct SLAHeaderInfo    header;
    uint8_t*                data;
    int32_t**               output;
    uint32_t                ch, suff_size, outsize, out_num_samples, num_samples;

    SLADecoder_SetDefaultConfig(&config);
    SLATestUtility_SetValidHeaderInfo(&header);
    num_samples = header.encode_param.max_num_block_samples;
    suff_size
      = SLA_CalculateSufficientBlockSize(header.wave_format.num_channels,
        num_samples, header.wave_format.bit_per_sample);

    /* デコーダ作成 */
    decoder = SLADecoder_Create(&config);
    Test_AssertCondition(decoder != NULL);

    /* 波形パラメータとエンコードパラメータの設定 */
    Test_AssertEqual(
        SLADecoder_SetWaveFormat(decoder, &header.wave_format),
        SLA_APIRESULT_OK);
    Test_AssertEqual(
        SLADecoder_SetEncodeParameter(decoder, &header.encode_param),
        SLA_APIRESULT_OK);

    /* データ領域の確保 */
    output = (int32_t **)malloc(sizeof(int32_t *) * header.wave_format.num_channels);
    data  = (uint8_t *)malloc(suff_size);
    for (ch = 0; ch < header.wave_format.num_channels; ch++) {
      output[ch] = (int32_t *)malloc(sizeof(int32_t) * header.encode_param.max_num_block_samples);
    }

    /* 不正な引数を渡してやる */
    Test_AssertEqual(
        SLADecoder_DecodeBlock(NULL, data, suff_size, output, num_samples, &outsize, &out_num_samples),
        SLA_APIRESULT_INVALID_ARGUMENT);
    Test_AssertEqual(
        SLADecoder_DecodeBlock(decoder, NULL, suff_size, output, num_samples, &outsize, &out_num_samples),
        SLA_APIRESULT_INVALID_ARGUMENT);
    Test_AssertEqual(
        SLADecoder_DecodeBlock(decoder, data, suff_size, NULL, num_samples, &outsize, &out_num_samples),
        SLA_APIRESULT_INVALID_ARGUMENT);
    Test_AssertEqual(
        SLADecoder_DecodeBlock(decoder, data, suff_size, output, num_samples, NULL, &out_num_samples),
        SLA_APIRESULT_INVALID_ARGUMENT);
    Test_AssertEqual(
        SLADecoder_DecodeBlock(decoder, data, suff_size, output, num_samples, &outsize, NULL),
        SLA_APIRESULT_INVALID_ARGUMENT);

    /* チャンネル毎の処理が不正 */
    decoder->encode_param.ch_process_method = SLA_CHPROCESSMETHOD_STEREO_MS;
    decoder->wave_format.num_channels       = 1;
    Test_AssertEqual(
        SLADecoder_DecodeBlock(decoder, data, suff_size, output, num_samples, &outsize, &out_num_samples),
        SLA_APIRESULT_INVAILD_CHPROCESSMETHOD);
    /* 設定を元に戻す */
    SLATestUtility_SetValidWaveFormat(&(decoder->wave_format));
    SLATestUtility_SetValidEncodeParameter(&(decoder->encode_param));

    /* 領域の開放 */
    for (ch = 0; ch < header.wave_format.num_channels; ch++) {
      free(output[ch]);
    }
    free(output);
    free(data);
    SLADecoder_Destroy(decoder);
  }

  /* エンコード済みのデータのデコードテスト */
  {
    uint32_t        ch, smpl;
    uint32_t        num_samples, num_channels;
    int32_t**       input;
    struct SLAHeaderInfo  header;

    SLATestUtility_SetValidHeaderInfo(&header);
    num_samples   = header.num_samples;
    num_channels  = header.wave_format.num_channels;

    /* データ割り当て */
    input = (int32_t **)malloc(sizeof(int32_t *) * num_channels);
    for (ch = 0; ch < num_channels; ch++) {
      input[ch] = (int32_t *)malloc(sizeof(int32_t) * num_samples);
    }

    /* 無音デコード */
    for (ch = 0; ch < num_channels; ch++) {
      memset((void *)input[ch], 0, sizeof(int32_t) * num_samples);
    }
    Test_AssertEqual(
        testSLADecoder_DoEncodeDecodeTest((const int32_t **)input, &header), 0);

    /* 疑似乱数デコード */
    for (ch = 0; ch < num_channels; ch++) {
      for (smpl = 0; smpl < num_samples; smpl++) {
        input[ch][smpl] = (int32_t)(2.0f * ((double)rand() / RAND_MAX - 0.5f) * pow(2, header.wave_format.bit_per_sample - 1));
        input[ch][smpl] <<= (32 - header.wave_format.bit_per_sample);
      }
    }
    Test_AssertEqual(
        testSLADecoder_DoEncodeDecodeTest((const int32_t **)input, &header), 0);

    /* データ開放 */
    for (ch = 0; ch < num_channels; ch++) {
      free((void *)input[ch]);
    }
    free(input);
  }

  /* エンコード済みのデータのデコード失敗テスト */
  {
    struct SLADecoder*      decoder;
    struct SLAEncoder*      encoder;
    struct SLADecoderConfig decoder_config;
    struct SLAEncoderConfig encoder_config;
    struct SLAHeaderInfo    header;
    int32_t**               input;
    int32_t**               output;
    uint8_t*                data;
    uint32_t                ch, smpl, suff_size, outsize, num_samples, num_channels;
    uint32_t                output_block_size, output_num_samples;

    SLAEncoder_SetDefaultConfig(&encoder_config);
    SLADecoder_SetDefaultConfig(&decoder_config);
    SLATestUtility_SetValidHeaderInfo(&header);
    num_samples = header.encode_param.max_num_block_samples;
    num_channels = header.wave_format.num_channels;
    suff_size 
      = SLA_CalculateSufficientBlockSize(header.wave_format.num_channels,
        num_samples, header.wave_format.bit_per_sample);

    /* エンコーダ・デコーダ作成 */
    encoder = SLAEncoder_Create(&encoder_config);
    decoder = SLADecoder_Create(&decoder_config);
    Test_AssertCondition(encoder != NULL);
    Test_AssertCondition(decoder != NULL);

    /* 波形パラメータとエンコードパラメータの設定 */
    Test_AssertEqual(
        SLAEncoder_SetWaveFormat(encoder, &header.wave_format),
        SLA_APIRESULT_OK);
    Test_AssertEqual(
        SLAEncoder_SetEncodeParameter(encoder, &header.encode_param),
        SLA_APIRESULT_OK);
    Test_AssertEqual(
        SLADecoder_SetWaveFormat(decoder, &header.wave_format),
        SLA_APIRESULT_OK);
    Test_AssertEqual(
        SLADecoder_SetEncodeParameter(decoder, &header.encode_param),
        SLA_APIRESULT_OK);

    /* 入力データ領域の確保 */
    input   = (int32_t **)malloc(sizeof(int32_t *) * num_channels);
    output  = (int32_t **)malloc(sizeof(int32_t *) * num_channels);
    data  = (uint8_t *)malloc(suff_size);
    for (ch = 0; ch < num_channels; ch++) {
      input[ch]   = (int32_t *)malloc(sizeof(int32_t) * num_samples);
      output[ch]  = (int32_t *)malloc(sizeof(int32_t *) * num_samples);
    }

    /* テスト用に定数データセット */
    for (ch = 0; ch < num_channels; ch++) {
      for (smpl = 0; smpl < num_samples; smpl++) {
        input[ch][smpl] = 1L << (32 - header.wave_format.bit_per_sample);
      }
    }

    /* データサイズが足らない */
    Test_AssertEqual(
        SLAEncoder_EncodeBlock(encoder,
          (const int32_t **)input, num_samples, data, suff_size, &outsize),
        SLA_APIRESULT_OK);
    Test_AssertEqual(
        SLADecoder_DecodeBlock(decoder,
          data, suff_size, output, outsize - 1, &output_block_size, &output_num_samples),
        SLA_APIRESULT_INSUFFICIENT_DATA_SIZE);

    /* 同期コード（データ先頭16bit）が不正 */
    Test_AssertEqual(
        SLAEncoder_EncodeBlock(encoder,
          (const int32_t **)input, num_samples, data, suff_size, &outsize),
        SLA_APIRESULT_OK);
    data[0] ^= 0xFF;
    Test_AssertEqual(
        SLADecoder_DecodeBlock(decoder,
          data, suff_size, output, outsize, &output_block_size, &output_num_samples),
        SLA_APIRESULT_FAILED_TO_FIND_SYNC_CODE);
    Test_AssertEqual(
        SLAEncoder_EncodeBlock(encoder,
          (const int32_t **)input, num_samples, data, suff_size, &outsize),
        SLA_APIRESULT_OK);
    data[1] ^= 0xFF;
    Test_AssertEqual(
        SLADecoder_DecodeBlock(decoder,
          data, suff_size, output, outsize, &output_block_size, &output_num_samples),
        SLA_APIRESULT_FAILED_TO_FIND_SYNC_CODE);

    /* CRC16の破損検出 */
    decoder->enable_crc_check = 1;
    Test_AssertEqual(
        SLAEncoder_EncodeBlock(encoder,
          (const int32_t **)input, num_samples, data, suff_size, &outsize),
        SLA_APIRESULT_OK);
    data[outsize - 1] ^= 0xFF;
    Test_AssertEqual(
        SLADecoder_DecodeBlock(decoder,
          data, suff_size, output, outsize, &output_block_size, &output_num_samples),
        SLA_APIRESULT_DETECT_DATA_CORRUPTION);
    decoder->enable_crc_check = decoder_config.enable_crc_check;

    /* 領域の開放 */
    for (ch = 0; ch < num_channels; ch++) {
      free(input[ch]); free(output[ch]);
    }
    free(input);
    free(output);
    free(data);
    SLAEncoder_Destroy(encoder);
    SLADecoder_Destroy(decoder);
  }
}

void testSLADecoder_Setup(void)
{
  struct TestSuite *suite
    = Test_AddTestSuite("SLA Decoder Test Suite",
        NULL, testSLADecoder_Initialize, testSLADecoder_Finalize);

  Test_AddTest(suite, testSLADecoder_DecodeHeaderTest);
  Test_AddTest(suite, testSLADecoder_DecodeBlockTest);
}
