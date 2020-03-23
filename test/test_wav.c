#include <stdlib.h>
#include <string.h>
#include "test.h"

/* テスト対象のモジュール */
#include "../src/wav.c"

/* テストのセットアップ関数 */
void testWAV_Setup(void);

static int testWAV_Initialize(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);
  return 0;
}

static int testWAV_Finalize(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);
  return 0;
}

/* WAVファイルフォーマット取得テスト */
static void testWAV_GetWAVFormatTest(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

  /* 失敗テスト */
  {
    struct WAVFileFormat format;

    Test_AssertEqual(
        WAV_GetWAVFormatFromFile(NULL, &format),
        WAV_APIRESULT_NG);
    Test_AssertEqual(
        WAV_GetWAVFormatFromFile("a.wav", NULL),
        WAV_APIRESULT_NG);
    Test_AssertEqual(
        WAV_GetWAVFormatFromFile(NULL, NULL),
        WAV_APIRESULT_NG);

    /* 存在しないファイルを開こうとして失敗するか？ */
    Test_AssertEqual(
        WAV_GetWAVFormatFromFile("dummy.a.wav.wav", &format),
        WAV_APIRESULT_NG);
  }

  /* 実wavファイルからの取得テスト */
  {
    struct WAVFileFormat format;
    Test_AssertEqual(
        WAV_GetWAVFormatFromFile("a.wav", &format),
        WAV_APIRESULT_OK);
  }

}

/* WAVファイルデータ取得テスト */
static void testWAV_CreateDestroyTest(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

  /* 失敗テスト */
  {
    struct WAVFileFormat format;
    format.data_format      = 255; /* 不正なフォーマット */
    format.bits_per_sample  = 16;
    format.num_channels     = 8;
    format.sampling_rate    = 48000;
    format.num_samples      = 48000 * 5;
    Test_AssertCondition(WAV_Create(NULL) == NULL);
    Test_AssertCondition(WAV_Create(&format) == NULL);
    Test_AssertCondition(WAV_CreateFromFile(NULL) == NULL);
    Test_AssertCondition(WAV_CreateFromFile("dummy.a.wav.wav") == NULL);
  }

  /* ハンドル作成 / 破棄テスト */
  {
    uint32_t  ch, is_ok;
    struct WAVFile*       wavfile;
    struct WAVFileFormat  format;
    format.data_format     = WAV_DATA_FORMAT_PCM;
    format.bits_per_sample = 16;
    format.num_channels    = 8;
    format.sampling_rate   = 48000;
    format.num_samples     = 48000 * 5;

    wavfile = WAV_Create(&format);
    Test_AssertCondition(wavfile != NULL);
    Test_AssertCondition(wavfile->data != NULL);
    Test_AssertEqual(
        memcmp(&wavfile->format, &format, sizeof(struct WAVFileFormat)), 0);
    is_ok = 1;
    for (ch = 0; ch < wavfile->format.num_channels; ch++) {
      if (wavfile->data[ch] == NULL) {
        is_ok = 0;
        break;
      }
    }
    Test_AssertEqual(is_ok, 1);

    WAV_Destroy(wavfile);
    Test_AssertCondition(wavfile->data == NULL);
  }


  /* 実wavファイルからの取得テスト */
  {
    struct WAVFile* wavfile;

    wavfile = WAV_CreateFromFile("a.wav");
    Test_AssertCondition(wavfile != NULL);
    WAV_Destroy(wavfile);
  }
}

/* WAVファイルデータ書き込みテスト */
static void testWAV_WriteTest(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

  /* 失敗テスト */
  {
    const char            test_filename[] = "test.wav";
    struct WAVWriter      writer;
    struct WAVFileFormat  format;
    FILE                  *fp;

    format.data_format     = 0xFF;  /* 不正  */
    format.num_samples     = 0;     /* dummy */
    format.num_channels    = 0;     /* dummy */
    format.bits_per_sample = 0;     /* dummy */
    format.sampling_rate   = 0;     /* dummy */

    fp = fopen(test_filename, "wb");
    WAVWriter_Initialize(&writer, fp);

    Test_AssertNotEqual(
        WAVWriter_PutWAVHeader(&writer, NULL),
        WAV_ERROR_OK);
    Test_AssertNotEqual(
        WAVWriter_PutWAVHeader(NULL, &format),
        WAV_ERROR_OK);
    Test_AssertNotEqual(
        WAVWriter_PutWAVHeader(NULL, NULL),
        WAV_ERROR_OK);
    Test_AssertNotEqual(
        WAVWriter_PutWAVHeader(&writer, &format),
        WAV_ERROR_OK);

    WAVWriter_Finalize(&writer);
    fclose(fp);
  }

  /* PCMデータ書き出しテスト */
  {
    const char            test_filename[] = "test.wav";
    struct WAVWriter      writer;
    struct WAVFileFormat  format;
    FILE                  *fp;
    struct WAVFile*       wavfile;
    uint32_t              ch, sample;

    /* 適宜フォーマットを用意 */
    format.data_format     = WAV_DATA_FORMAT_PCM;
    format.num_samples     = 16;
    format.num_channels    = 1;
    format.sampling_rate   = 48000;
    format.bits_per_sample = 8;

    /* ハンドル作成 */
    wavfile = WAV_Create(&format);
    Test_AssertCondition(wavfile != NULL);

    /* データを書いてみる */
    for (ch = 0; ch < format.num_channels; ch++) {
      for (sample = 0; sample < format.num_samples; sample++) {
        WAVFile_PCM(wavfile, sample, ch) = sample - (int32_t)format.num_samples / 2;
      }
    }

    fp = fopen(test_filename, "wb");
    WAVWriter_Initialize(&writer, fp);

    /* 不正なビット深度に書き換えて書き出し */
    /* -> 失敗を期待 */
    wavfile->format.bits_per_sample = 3;
    Test_AssertNotEqual(
        WAVWriter_PutWAVPcmData(&writer, wavfile),
        WAV_ERROR_OK);

    /* 書き出し */
    wavfile->format.bits_per_sample = 8;
    Test_AssertEqual(
        WAVWriter_PutWAVPcmData(&writer, wavfile),
        WAV_ERROR_OK);

    WAVWriter_Finalize(&writer);
    fclose(fp);
  }

  /* 実ファイルを読み出してそのまま書き出してみる */
  {
    uint32_t ch, is_ok, i_test;
    const char* test_sourcefile_list[] = {
      "a.wav",
    };
    const char test_filename[] = "tmp.wav";
    struct WAVFile *src_wavfile, *test_wavfile;

    for (i_test = 0;
         i_test < sizeof(test_sourcefile_list) / sizeof(test_sourcefile_list[0]);
         i_test++) {
      /* 元になるファイルを読み込み */
      src_wavfile = WAV_CreateFromFile(test_sourcefile_list[i_test]);

      /* 読み込んだデータをそのままファイルへ書き出し */
      WAV_WriteToFile(test_filename, src_wavfile);

      /* 一度書き出したファイルを読み込んでみる */
      test_wavfile = WAV_CreateFromFile(test_filename);

      /* 最初に読み込んだファイルと一致するか？ */
      /* フォーマットの一致確認 */
      Test_AssertEqual(
          memcmp(&src_wavfile->format, &test_wavfile->format,
            sizeof(struct WAVFileFormat)), 0);

      /* PCMの一致確認 */
      is_ok = 1;
      for (ch = 0; ch < src_wavfile->format.num_channels; ch++) {
        if (memcmp(src_wavfile->data[ch], test_wavfile->data[ch],
              sizeof(WAVPcmData) * src_wavfile->format.num_samples) != 0) {
          is_ok = 0;
          break;
        }
      }
      Test_AssertEqual(is_ok, 1);

      /* ハンドル破棄 */
      WAV_Destroy(src_wavfile);
      WAV_Destroy(test_wavfile);
    }
  }

}

void testWAV_Setup(void)
{
  struct TestSuite *suite
    = Test_AddTestSuite("WAV Test Suite",
        NULL, testWAV_Initialize, testWAV_Finalize);

  Test_AddTest(suite, testWAV_GetWAVFormatTest);
  Test_AddTest(suite, testWAV_CreateDestroyTest);
  Test_AddTest(suite, testWAV_WriteTest);
}
