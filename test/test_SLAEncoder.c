#include "test.h"

#include "SLA_TestUtility.h"

/* テスト対象のモジュール */
#include "../SLAEncoder.c"

/* テストのセットアップ関数 */
void testSLAEncoder_Setup(void);

static int testSLAEncoder_Initialize(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);
  return 0;
}

static int testSLAEncoder_Finalize(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);
  return 0;
}

/* ヘッダエンコードテスト */
static void testSLAEncoder_EncodeHeaderTest(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

  /* 単純成功例 */
  {
    struct SLAHeaderInfo header;
    uint8_t* data;
    uint8_t* data_ptr;
    uint32_t u32buf;
    uint16_t u16buf;
    uint8_t  u8buf;

    /* ヘッダ書き込み領域の確保 */
    data = malloc(SLA_HEADER_SIZE);

    /* ヘッダ構造体に有効な情報を設定 */
    SLATestUtility_SetValidHeaderInfo(&header);

    /* 書いてみる */
    Test_AssertEqual(
        SLAEncoder_EncodeHeader(&header, data, SLA_HEADER_SIZE),
        SLA_APIRESULT_OK);

    /* 最低限の確認 */
    data_ptr = data;
    /* シグネチャ */
    SLAByteArray_GetUint8(data_ptr, &u8buf);
    Test_AssertEqual(u8buf, 'S');
    SLAByteArray_GetUint8(data_ptr, &u8buf);
    Test_AssertEqual(u8buf, 'L');
    SLAByteArray_GetUint8(data_ptr, &u8buf);
    Test_AssertEqual(u8buf, '*');
    SLAByteArray_GetUint8(data_ptr, &u8buf);
    Test_AssertEqual(u8buf, '\1');
    /* 一番最初のデータブロックまでのオフセット */
    SLAByteArray_GetUint32(data_ptr, &u32buf);
    Test_AssertCondition(u32buf <= SLA_HEADER_SIZE);
    /* ヘッダCRC16（その場で計算した値と一致するか？） */
    SLAByteArray_GetUint16(data_ptr, &u16buf);
    Test_AssertEqual(
        SLAUtility_CalculateCRC16(
            &data[SLA_HEADER_CRC16_CALC_START_OFFSET],
            SLA_HEADER_SIZE - SLA_HEADER_CRC16_CALC_START_OFFSET), 
        u16buf);
    /* フォーマットバージョン */
    SLAByteArray_GetUint32(data_ptr, &u32buf);
    Test_AssertEqual(u32buf, SLA_FORMAT_VERSION);
    /* チャンネル数 */
    SLAByteArray_GetUint8(data_ptr, &u8buf);
    Test_AssertEqual(u8buf, header.wave_format.num_channels);
    /* サンプル数 */
    SLAByteArray_GetUint32(data_ptr, &u32buf);
    Test_AssertEqual(u32buf, header.num_samples);
    /* サンプリングレート */
    SLAByteArray_GetUint32(data_ptr, &u32buf);
    Test_AssertEqual(u32buf, header.wave_format.sampling_rate);
    /* サンプルあたりbit数 */
    SLAByteArray_GetUint8(data_ptr, &u8buf);
    Test_AssertEqual(u8buf, header.wave_format.bit_per_sample);
    /* オフセット分の左シフト量 */
    SLAByteArray_GetUint8(data_ptr, &u8buf);
    Test_AssertEqual(u8buf, header.wave_format.offset_lshift);
    /* PARCOR係数次数 */
    SLAByteArray_GetUint8(data_ptr, &u8buf);
    Test_AssertEqual(u8buf, header.encode_param.parcor_order);
    /* ロングターム係数次数 */
    SLAByteArray_GetUint8(data_ptr, &u8buf);
    Test_AssertEqual(u8buf, header.encode_param.longterm_order);
    /* LMS次数 */
    SLAByteArray_GetUint8(data_ptr, &u8buf);
    Test_AssertEqual(u8buf, header.encode_param.lms_order_per_filter);
    /* チャンネル毎の処理法 */
    SLAByteArray_GetUint8(data_ptr, &u8buf);
    Test_AssertEqual(u8buf, header.encode_param.ch_process_method);
    /* SLAブロック数 */
    SLAByteArray_GetUint32(data_ptr, &u32buf);
    Test_AssertEqual(u32buf, header.num_blocks);
    /* SLAブロックあたりサンプル数 */
    SLAByteArray_GetUint16(data_ptr, &u16buf);
    Test_AssertEqual(u16buf, header.encode_param.max_num_block_samples);
    /* 最大ブロックサイズ */
    SLAByteArray_GetUint32(data_ptr, &u32buf);
    Test_AssertEqual(u32buf, header.max_block_size);
    /* 最大bps */
    SLAByteArray_GetUint32(data_ptr, &u32buf);
    Test_AssertEqual(u32buf, header.max_bit_per_second);

    /* ヘッダサイズチェック */
    Test_AssertEqual(data_ptr - data, SLA_HEADER_SIZE);

    free(data);
  }

  /* 失敗テスト */
  {
    struct SLAHeaderInfo header;
    uint8_t* data;

    /* ヘッダ書き込み領域の確保 */
    data = malloc(SLA_HEADER_SIZE);
    /* ヘッダに情報をセット */
    SLATestUtility_SetValidHeaderInfo(&header);

    /* 不正な引数を渡してやる */
    Test_AssertEqual(
        SLAEncoder_EncodeHeader(NULL, data, SLA_HEADER_SIZE),
        SLA_APIRESULT_INVALID_ARGUMENT);
    Test_AssertEqual(
        SLAEncoder_EncodeHeader(&header, NULL, SLA_HEADER_SIZE),
        SLA_APIRESULT_INVALID_ARGUMENT);

    /* バッファサイズが十分でない */
    Test_AssertEqual(
        SLAEncoder_EncodeHeader(&header, data, SLA_HEADER_SIZE-1),
        SLA_APIRESULT_INSUFFICIENT_BUFFER_SIZE);

    free(data);
  }
}

/* 1ブロックエンコードテスト */
static void testSLAEncoder_EncodeBlockTest(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

  /* 失敗テスト */
  { 
    struct SLAEncoder*      encoder;
    struct SLAEncoderConfig config;
    struct SLAHeaderInfo    header;
    const int32_t**         input;
    uint8_t*                data;
    uint32_t                ch, suff_size, outsize, num_samples;

    SLAEncoder_SetDefaultConfig(&config);
    SLATestUtility_SetValidHeaderInfo(&header);
    num_samples = header.encode_param.max_num_block_samples;
    suff_size 
      = SLA_CalculateSufficientBlockSize(header.wave_format.num_channels,
        num_samples, header.wave_format.bit_per_sample);

    /* エンコーダ作成 */
    encoder = SLAEncoder_Create(&config);
    Test_AssertCondition(encoder != NULL);

    /* 波形パラメータとエンコードパラメータの設定 */
    Test_AssertEqual(
        SLAEncoder_SetWaveFormat(encoder, &header.wave_format),
        SLA_APIRESULT_OK);
    Test_AssertEqual(
        SLAEncoder_SetEncodeParameter(encoder, &header.encode_param),
        SLA_APIRESULT_OK);

    /* 入力データ領域の確保 */
    input = (const int32_t **)malloc(sizeof(int32_t *) * header.wave_format.num_channels);
    data  = (uint8_t *)malloc(suff_size);
    for (ch = 0; ch < header.wave_format.num_channels; ch++) {
      input[ch] = (int32_t *)malloc(sizeof(int32_t) * header.encode_param.max_num_block_samples);
    }

    /* 不正な引数を渡してやる */
    Test_AssertEqual(
        SLAEncoder_EncodeBlock(NULL, input, num_samples, data, suff_size, &outsize),
        SLA_APIRESULT_INVALID_ARGUMENT);
    Test_AssertEqual(
        SLAEncoder_EncodeBlock(encoder, NULL, num_samples, data, suff_size, &outsize),
        SLA_APIRESULT_INVALID_ARGUMENT);
    Test_AssertEqual(
        SLAEncoder_EncodeBlock(encoder, input, num_samples, NULL, suff_size, &outsize),
        SLA_APIRESULT_INVALID_ARGUMENT);
    Test_AssertEqual(
        SLAEncoder_EncodeBlock(encoder, input, num_samples, data, suff_size, NULL),
        SLA_APIRESULT_INVALID_ARGUMENT);

    /* 許容サンプル数を超えたサンプル数入力 */
    Test_AssertEqual(
        SLAEncoder_EncodeBlock(encoder, input,
          config.max_num_block_samples + 1, data, suff_size, &outsize),
        SLA_APIRESULT_EXCEED_HANDLE_CAPACITY);

    /* チャンネル毎の処理が不正 */
    encoder->encode_param.ch_process_method = SLA_CHPROCESSMETHOD_STEREO_MS;
    encoder->wave_format.num_channels       = 1;
    Test_AssertEqual(
        SLAEncoder_EncodeBlock(encoder, input,
          num_samples, data, suff_size, &outsize),
        SLA_APIRESULT_INVAILD_CHPROCESSMETHOD);
    /* 設定を元に戻す */
    SLATestUtility_SetValidWaveFormat(&(encoder->wave_format));
    SLATestUtility_SetValidEncodeParameter(&(encoder->encode_param));

    /* 不正な窓関数タイプを指定 */
    encoder->encode_param.window_function_type = -1;
    Test_AssertEqual(
        SLAEncoder_EncodeBlock(encoder, input,
          num_samples, data, suff_size, &outsize),
        SLA_APIRESULT_INVALID_WINDOWFUNCTION_TYPE);
    /* 設定を元に戻す */
    SLATestUtility_SetValidEncodeParameter(&(encoder->encode_param));

    /* 領域の開放 */
    for (ch = 0; ch < header.wave_format.num_channels; ch++) {
      free((void *)input[ch]);
    }
    free(input);
    free(data);
    SLAEncoder_Destroy(encoder);
  }

  /* パラメータセット前にエンコードするとエラーになるか？ */
  {
    struct SLAEncoder*      encoder;
    struct SLAEncoderConfig config;
    struct SLAHeaderInfo    header;
    const int32_t**         input;
    uint8_t*                data;
    uint32_t                ch, suff_size, outsize, num_samples;

    SLAEncoder_SetDefaultConfig(&config);
    SLATestUtility_SetValidHeaderInfo(&header);
    num_samples = header.encode_param.max_num_block_samples;
    suff_size 
      = SLA_CalculateSufficientBlockSize(header.wave_format.num_channels,
        num_samples, header.wave_format.bit_per_sample);

    /* 入力データ領域の確保 */
    input = (const int32_t **)malloc(sizeof(int32_t *) * header.wave_format.num_channels);
    data  = (uint8_t *)malloc(suff_size);
    for (ch = 0; ch < header.wave_format.num_channels; ch++) {
      input[ch] = (int32_t *)malloc(sizeof(int32_t) * header.encode_param.max_num_block_samples);
    }

    /* エンコーダ作成 */
    encoder = SLAEncoder_Create(&config);

    /* セット前だからエラー */
    Test_AssertEqual(
        SLAEncoder_EncodeBlock(encoder, input, num_samples, data, suff_size, &outsize),
        SLA_APIRESULT_PARAMETER_NOT_SET);

    /* 波形パラメータだけ設定 */
    Test_AssertEqual(
        SLAEncoder_SetWaveFormat(encoder, &header.wave_format),
        SLA_APIRESULT_OK);

    /* エンコードパラメータが足りない */
    Test_AssertEqual(
        SLAEncoder_EncodeBlock(encoder, input, num_samples, data, suff_size, &outsize),
        SLA_APIRESULT_PARAMETER_NOT_SET);

    /* ハンドル再作成 */
    SLAEncoder_Destroy(encoder);
    encoder = SLAEncoder_Create(&config);

    /* エンコードパラメータだけ設定 */
    Test_AssertEqual(
        SLAEncoder_SetEncodeParameter(encoder, &header.encode_param),
        SLA_APIRESULT_OK);

    /* 波形パラメータが足りない */
    Test_AssertEqual(
        SLAEncoder_EncodeBlock(encoder, input, num_samples, data, suff_size, &outsize),
        SLA_APIRESULT_PARAMETER_NOT_SET);

    /* 領域の開放 */
    for (ch = 0; ch < header.wave_format.num_channels; ch++) {
      free((void *)input[ch]);
    }
    free(input);
    free(data);
    SLAEncoder_Destroy(encoder);
  }

  /* 無音エンコードテスト */
  { 
    struct SLAEncoder*      encoder;
    struct SLAEncoderConfig config;
    struct SLAHeaderInfo    header;
    const int32_t**         input;
    uint8_t*                data;
    uint32_t                ch, suff_size, outsize, num_samples;
    uint8_t*                data_ptr;
    uint16_t                u16buf;
    uint32_t                u32buf;
    uint8_t                 u8buf;

    SLAEncoder_SetDefaultConfig(&config);
    SLATestUtility_SetValidHeaderInfo(&header);
    num_samples = header.encode_param.max_num_block_samples;
    suff_size 
      = SLA_CalculateSufficientBlockSize(header.wave_format.num_channels,
        num_samples, header.wave_format.bit_per_sample);

    /* エンコーダ作成 */
    encoder = SLAEncoder_Create(&config);
    Test_AssertCondition(encoder != NULL);

    /* 波形パラメータとエンコードパラメータの設定 */
    Test_AssertEqual(
        SLAEncoder_SetWaveFormat(encoder, &header.wave_format),
        SLA_APIRESULT_OK);
    Test_AssertEqual(
        SLAEncoder_SetEncodeParameter(encoder, &header.encode_param),
        SLA_APIRESULT_OK);

    /* 入力データ領域の確保 */
    input = (const int32_t **)malloc(sizeof(int32_t *) * header.wave_format.num_channels);
    data  = (uint8_t *)malloc(suff_size);
    for (ch = 0; ch < header.wave_format.num_channels; ch++) {
      input[ch] = (int32_t *)malloc(sizeof(int32_t) * num_samples);
    }

    /* 無音セット */
    for (ch = 0; ch < header.wave_format.num_channels; ch++) {
      memset((void *)input[ch], 0, sizeof(int32_t) * num_samples);
    }

    /* ブロックエンコードを実行 */
    Test_AssertEqual(
        SLAEncoder_EncodeBlock(encoder, input, num_samples, data, suff_size, &outsize),
        SLA_APIRESULT_OK);

    /* 出力サイズはブロックヘッダよりは大きいはず */
    Test_AssertCondition(outsize >= (2 + 4 + 2 + 2));

    /* ブロックの内容を検査 */
    data_ptr = data;
    /* 同期コード */
    SLAByteArray_GetUint16(data_ptr, &u16buf);
    Test_AssertEqual(u16buf, 0xFFFF);
    /* 次のブロックまでのオフセット */
    SLAByteArray_GetUint32(data_ptr, &u32buf);
    Test_AssertCondition(u32buf > 0 && u32buf < outsize);
    /* CRC16: 計算結果と合うか？ */
    SLAByteArray_GetUint16(data_ptr, &u16buf);
    Test_AssertEqual(
        u16buf,
        SLAUtility_CalculateCRC16(data_ptr, outsize - SLA_BLOCK_CRC16_CALC_START_OFFSET));
    /* ブロックサンプル数 */
    SLAByteArray_GetUint16(data_ptr, &u16buf);
    Test_AssertEqual(u16buf, num_samples);
    /* 無音フラグ いずれかのチャンネルが1だから0ではないはず */
    SLAByteArray_GetUint8(data_ptr, &u8buf);
    Test_AssertNotEqual(u8buf, 0);

    /* ここでブロックが終わっているはず */
    Test_AssertCondition(data_ptr == (data + outsize));

    /* 領域の開放 */
    free(input);
    free(data);
    SLAEncoder_Destroy(encoder);
  }

}

void testSLAEncoder_Setup(void)
{
  struct TestSuite *suite
    = Test_AddTestSuite("SLA Encoder Test Suite",
        NULL, testSLAEncoder_Initialize, testSLAEncoder_Finalize);

  Test_AddTest(suite, testSLAEncoder_EncodeHeaderTest);
  Test_AddTest(suite, testSLAEncoder_EncodeBlockTest);
}
