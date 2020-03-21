#include "test.h"

#include <sys/stat.h>
#include <string.h>

/* テスト対象のモジュール */
#include "../SLACoder.c"

/* テストのセットアップ関数 */
void testSLACoder_Setup(void);

static int testSLACoder_Initialize(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);
  return 0;
}

static int testSLACoder_Finalize(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);
  return 0;
}

/* 再帰的ライス符号テスト */
static void testSLACoder_SLARecursiveRiceTest(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

  /* 簡単に出力テスト */
  {
    uint32_t code;
    const char* test_file_name
      = "coding_util_recur_rice_test.bin";
    struct SLABitStream* strm;
    SLARecursiveRiceParameter param_array[2] = {0, 0};

    /* 0を4回出力 */
    SLACODER_PARAMETER_SET(param_array, 0, 1);
    SLACODER_PARAMETER_SET(param_array, 1, 1);
    strm = SLABitStream_Open(test_file_name, "w", NULL, 0);
    SLARecursiveRice_PutCode(strm, param_array, 2, 0);
    SLARecursiveRice_PutCode(strm, param_array, 2, 0);
    SLARecursiveRice_PutCode(strm, param_array, 2, 0);
    SLARecursiveRice_PutCode(strm, param_array, 2, 0);
    SLABitStream_Close(strm);

    /* 取得 */
    SLACODER_PARAMETER_SET(param_array, 0, 1);
    SLACODER_PARAMETER_SET(param_array, 1, 1);
    strm = SLABitStream_Open(test_file_name, "r", NULL, 0);
    code = SLARecursiveRice_GetCode(strm, param_array, 2);
    Test_AssertEqual(code, 0);
    code = SLARecursiveRice_GetCode(strm, param_array, 2);
    Test_AssertEqual(code, 0);
    code = SLARecursiveRice_GetCode(strm, param_array, 2);
    Test_AssertEqual(code, 0);
    code = SLARecursiveRice_GetCode(strm, param_array, 2);
    Test_AssertEqual(code, 0);
    SLABitStream_Close(strm);

    /* 1を4回出力 */
    SLACODER_PARAMETER_SET(param_array, 0, 1);
    SLACODER_PARAMETER_SET(param_array, 1, 1);
    strm = SLABitStream_Open(test_file_name, "w", NULL, 0);
    SLARecursiveRice_PutCode(strm, param_array, 2, 1);
    SLARecursiveRice_PutCode(strm, param_array, 2, 1);
    SLARecursiveRice_PutCode(strm, param_array, 2, 1);
    SLARecursiveRice_PutCode(strm, param_array, 2, 1);
    SLABitStream_Close(strm);

    /* 取得 */
    SLACODER_PARAMETER_SET(param_array, 0, 1);
    SLACODER_PARAMETER_SET(param_array, 1, 1);
    strm = SLABitStream_Open(test_file_name, "r", NULL, 0);
    code = SLARecursiveRice_GetCode(strm, param_array, 2);
    Test_AssertEqual(code, 1);
    code = SLARecursiveRice_GetCode(strm, param_array, 2);
    Test_AssertEqual(code, 1);
    code = SLARecursiveRice_GetCode(strm, param_array, 2);
    Test_AssertEqual(code, 1);
    code = SLARecursiveRice_GetCode(strm, param_array, 2);
    Test_AssertEqual(code, 1);
    SLABitStream_Close(strm);


    /* パラメータを変えて0を4回出力 */
    SLACODER_PARAMETER_SET(param_array, 0, 2);
    SLACODER_PARAMETER_SET(param_array, 1, 2);
    strm = SLABitStream_Open(test_file_name, "w", NULL, 0);
    SLARecursiveRice_PutCode(strm, param_array, 2, 0);
    SLARecursiveRice_PutCode(strm, param_array, 2, 0);
    SLARecursiveRice_PutCode(strm, param_array, 2, 0);
    SLARecursiveRice_PutCode(strm, param_array, 2, 0);
    SLABitStream_Close(strm);

    /* 取得 */
    SLACODER_PARAMETER_SET(param_array, 0, 2);
    SLACODER_PARAMETER_SET(param_array, 1, 2);
    strm = SLABitStream_Open(test_file_name, "r", NULL, 0);
    code = SLARecursiveRice_GetCode(strm, param_array, 2);
    Test_AssertEqual(code, 0);
    code = SLARecursiveRice_GetCode(strm, param_array, 2);
    Test_AssertEqual(code, 0);
    code = SLARecursiveRice_GetCode(strm, param_array, 2);
    Test_AssertEqual(code, 0);
    code = SLARecursiveRice_GetCode(strm, param_array, 2);
    Test_AssertEqual(code, 0);
    SLABitStream_Close(strm);

    /* パラメータを変えて3を4回出力 */
    SLACODER_PARAMETER_SET(param_array, 0, 2);
    SLACODER_PARAMETER_SET(param_array, 1, 2);
    strm = SLABitStream_Open(test_file_name, "w", NULL, 0);
    SLARecursiveRice_PutCode(strm, param_array, 2, 3);
    SLARecursiveRice_PutCode(strm, param_array, 2, 3);
    SLARecursiveRice_PutCode(strm, param_array, 2, 3);
    SLARecursiveRice_PutCode(strm, param_array, 2, 3);
    SLABitStream_Close(strm);

    /* 取得 */
    SLACODER_PARAMETER_SET(param_array, 0, 2);
    SLACODER_PARAMETER_SET(param_array, 1, 2);
    strm = SLABitStream_Open(test_file_name, "r", NULL, 0);
    code = SLARecursiveRice_GetCode(strm, param_array, 2);
    Test_AssertEqual(code, 3);
    code = SLARecursiveRice_GetCode(strm, param_array, 2);
    Test_AssertEqual(code, 3);
    code = SLARecursiveRice_GetCode(strm, param_array, 2);
    Test_AssertEqual(code, 3);
    code = SLARecursiveRice_GetCode(strm, param_array, 2);
    Test_AssertEqual(code, 3);
    SLABitStream_Close(strm);
  }

  /* 長めの信号を出力してみる */
  {
#define TEST_OUTPUT_LENGTH (128)
    uint32_t i, code, is_ok;
    const char* test_file_name
      = "coding_util_recur_rice_test_case2.bin";
    struct SLABitStream* strm;
    SLARecursiveRiceParameter param_array[3] = {0, 0, 0};
    uint32_t test_output_pattern[TEST_OUTPUT_LENGTH];

    /* 出力の生成 */
    for (i = 0; i < TEST_OUTPUT_LENGTH; i++) {
      test_output_pattern[i] = i;
    }

    /* 出力 */
    SLACODER_PARAMETER_SET(param_array, 0, 1);
    SLACODER_PARAMETER_SET(param_array, 1, 1);
    SLACODER_PARAMETER_SET(param_array, 2, 1);
    strm = SLABitStream_Open(test_file_name, "w", NULL, 0);
    for (i = 0; i < TEST_OUTPUT_LENGTH; i++) {
      SLARecursiveRice_PutCode(strm,
          param_array, 3, test_output_pattern[i]);
    }
    SLABitStream_Close(strm);

    /* 取得 */
    SLACODER_PARAMETER_SET(param_array, 0, 1);
    SLACODER_PARAMETER_SET(param_array, 1, 1);
    SLACODER_PARAMETER_SET(param_array, 2, 1);
    strm = SLABitStream_Open(test_file_name, "r", NULL, 0);
    is_ok = 1;
    for (i = 0; i < TEST_OUTPUT_LENGTH; i++) {
      code = SLARecursiveRice_GetCode(strm, param_array, 3);
      if (code != test_output_pattern[i]) {
        printf("actual:%d != test:%d \n", code, test_output_pattern[i]);
        is_ok = 0;
        break;
      }
    }
    Test_AssertEqual(is_ok, 1);
    SLABitStream_Close(strm);
#undef TEST_OUTPUT_LENGTH
  }

  /* 長めの信号を出力してみる（乱数） */
  {
#define TEST_OUTPUT_LENGTH (128)
    uint32_t i, code, is_ok;
    const char* test_file_name
      = "coding_util_recur_rice_test_case3.bin";
    struct SLABitStream* strm;
    SLARecursiveRiceParameter param_array[3] = {0, 0, 0};
    uint32_t test_output_pattern[TEST_OUTPUT_LENGTH];

    /* 出力の生成 */
    srand(0);
    for (i = 0; i < TEST_OUTPUT_LENGTH; i++) {
      test_output_pattern[i] = rand() % 0xFF;
    }

    /* 出力 */
    SLACODER_PARAMETER_SET(param_array, 0, 1);
    SLACODER_PARAMETER_SET(param_array, 1, 1);
    SLACODER_PARAMETER_SET(param_array, 2, 1);
    strm = SLABitStream_Open(test_file_name, "w", NULL, 0);
    for (i = 0; i < TEST_OUTPUT_LENGTH; i++) {
      SLARecursiveRice_PutCode(strm,
          param_array, 3, test_output_pattern[i]);
    }
    SLABitStream_Close(strm);

    /* 取得 */
    SLACODER_PARAMETER_SET(param_array, 0, 1);
    SLACODER_PARAMETER_SET(param_array, 1, 1);
    SLACODER_PARAMETER_SET(param_array, 2, 1);
    strm = SLABitStream_Open(test_file_name, "r", NULL, 0);
    is_ok = 1;
    for (i = 0; i < TEST_OUTPUT_LENGTH; i++) {
      code = SLARecursiveRice_GetCode(strm, param_array, 3);
      if (code != test_output_pattern[i]) {
        printf("actual:%d != test:%d \n", code, test_output_pattern[i]);
        is_ok = 0;
        break;
      }
    }
    Test_AssertEqual(is_ok, 1);
    SLABitStream_Close(strm);
#undef TEST_OUTPUT_LENGTH
  }

  /* 実データを符号化してみる */
  {
    uint32_t i;
    struct stat fstat;
    const char* test_infile_name  = "PriChanIcon.png";
    const char* test_outfile_name = "coding_util_recur_rice_test_case4.bin";
    uint8_t*    fileimg;
    uint8_t*    decimg;
    FILE*       fp;
    struct SLABitStream* strm;
    SLARecursiveRiceParameter param_array[8];
    const uint32_t num_params = sizeof(param_array) / sizeof(param_array[0]);

    /* 入力データ読み出し */
    stat(test_infile_name, &fstat);
    fileimg = malloc(fstat.st_size);
    decimg  = malloc(fstat.st_size);
    fp = fopen(test_infile_name, "rb");
    fread(fileimg, sizeof(uint8_t), fstat.st_size, fp);
    fclose(fp);

    /* 書き込み */
    strm = SLABitStream_Open(test_outfile_name, "wb", NULL, 0);
    for (i = 0; i < num_params; i++) {
      SLACODER_PARAMETER_SET(param_array, i, 1);
    }
    for (i = 0; i < fstat.st_size; i++) {
      SLARecursiveRice_PutCode(strm, param_array, num_params, fileimg[i]);
    }
    SLABitStream_Close(strm);

    /* 読み込み */
    strm = SLABitStream_Open(test_outfile_name, "rb", NULL, 0);
    for (i = 0; i < num_params; i++) {
      SLACODER_PARAMETER_SET(param_array, i, 1);
    }
    for (i = 0; i < fstat.st_size; i++) {
      decimg[i] = (uint8_t)SLARecursiveRice_GetCode(strm, param_array, num_params);
    }
    SLABitStream_Close(strm);

    /* 一致確認 */
    Test_AssertEqual(memcmp(fileimg, decimg, sizeof(uint8_t) * fstat.st_size), 0);

    free(decimg);
    free(fileimg);
  }

}

static void testSLACoder_CalculateLog2CeilTest(void* obj)
{
  TEST_UNUSED_PARAMETER(obj);

  /* 2の冪乗数に対するテスト */
  {
    uint32_t get, val, lshift, ref;

    for (lshift = 0; lshift < 32; lshift++) {
      val = (1UL << lshift);
      get = SLACoder_Log2CeilFor2PoweredValue(val);
      ref = SLAUtility_Log2Ceil(val);
      Test_AssertEqual(ref, lshift);
      Test_AssertEqual(get, lshift);
    }
  }
}

void testSLACoder_Setup(void)
{
  struct TestSuite *suite
    = Test_AddTestSuite("SLA Coder Test Suite",
        NULL, testSLACoder_Initialize, testSLACoder_Finalize);

  Test_AddTest(suite, testSLACoder_SLARecursiveRiceTest);
  Test_AddTest(suite, testSLACoder_CalculateLog2CeilTest);
}
