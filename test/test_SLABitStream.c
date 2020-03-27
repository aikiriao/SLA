#include <stdlib.h>
#include <string.h>
#include "test.h"

/* テスト対象のモジュール */
#include "../src/SLABitStream.c"

/* テストのセットアップ関数 */
void testSLABitStream_Setup(void);

static int testSLABitStream_Initialize(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);
  return 0;
}

static int testSLABitStream_Finalize(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);
  return 0;
}

/* インスタンス作成破棄テスト */
static void testSLABitStream_CreateDestroyTest(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

  /* インスタンス作成・破棄（メモリ） */
  {
    struct SLABitStream strm;
    uint8_t test_memory[] = {'A', 'I', 'K', 'A', 'T', 'S', 'U'};
    const uint32_t test_memory_size = sizeof(test_memory) / sizeof(test_memory[0]);

    /* 書きモードでインスタンス作成 */
    SLABitWriter_Open(&strm, test_memory, test_memory_size);
    Test_AssertCondition(strm.memory_image == test_memory);
    Test_AssertEqual(strm.memory_size, test_memory_size);
    Test_AssertCondition(strm.memory_p == test_memory);
    Test_AssertEqual(strm.bit_buffer, 0);
    Test_AssertEqual(strm.bit_count, 8);
    Test_AssertCondition(!(strm.flags & SLABITSTREAM_FLAGS_MODE_READ));
    SLABitStream_Close(&strm);

    /* 読みモードでインスタンス作成 */
    SLABitReader_Open(&strm, test_memory, test_memory_size);
    Test_AssertCondition(strm.memory_image == test_memory);
    Test_AssertEqual(strm.memory_size, test_memory_size);
    Test_AssertCondition(strm.memory_p == test_memory);
    Test_AssertEqual(strm.bit_buffer, 0);
    Test_AssertEqual(strm.bit_count, 0);
    Test_AssertCondition(strm.flags & SLABITSTREAM_FLAGS_MODE_READ);
    SLABitStream_Close(&strm);
  }
}

static void testSLABitStream_PutGetTest(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

  /* PutBit関数テスト */
  {
    struct SLABitStream strm;
    uint8_t bit_pattern[] = { 1, 1, 1, 1, 0, 0, 0, 0 };
    uint8_t memory_image[256];
    uint32_t bit_pattern_length = sizeof(bit_pattern) / sizeof(bit_pattern[0]);
    uint32_t i, is_ok;

    /* 書き込んでみる */
    SLABitWriter_Open(&strm, memory_image, sizeof(memory_image));
    for (i = 0; i < bit_pattern_length; i++) { 
      SLABitWriter_PutBits(&strm, bit_pattern[i], 1);
    }
    SLABitStream_Close(&strm);

    /* 正しく書き込めているか？ */
    SLABitReader_Open(&strm, memory_image, sizeof(memory_image));
    is_ok = 1;
    for (i = 0; i < bit_pattern_length; i++) { 
      uint64_t buf;
      SLABitReader_GetBits(&strm, &buf, 1);
      if ((uint8_t)buf != bit_pattern[i]) {
        is_ok = 0;
        break;
      }
    }
    Test_AssertEqual(is_ok, 1);
    SLABitStream_Close(&strm);
  }

  /* PutBit関数テスト2 8bitパターンチェック */
  {
    struct SLABitStream strm;
    uint8_t memory_image[256];
    uint32_t i, is_ok, nbits;

    for (nbits = 1; nbits <= 8; nbits++) {
      /* 書き込んでみる */
      SLABitWriter_Open(&strm, memory_image, sizeof(memory_image));
      for (i = 0; i < (1 << nbits); i++) { 
        SLABitWriter_PutBits(&strm, i, nbits);
      }
      SLABitStream_Close(&strm);

      /* 正しく書き込めているか？ */
      SLABitReader_Open(&strm, memory_image, sizeof(memory_image));
      is_ok = 1;
      for (i = 0; i < (1 << nbits); i++) { 
        uint64_t buf;
        SLABitReader_GetBits(&strm, &buf, nbits);
        if (buf != i) {
          is_ok = 0;
          break;
        }
      }
      Test_AssertEqual(is_ok, 1);
      SLABitStream_Close(&strm);
    }

  }

  /* Flushテスト */
  {
    struct SLABitStream strm;
    uint8_t memory_image[256];
    uint64_t bits;

    SLABitWriter_Open(&strm, memory_image, sizeof(memory_image));
    SLABitWriter_PutBits(&strm, 1, 1);
    SLABitWriter_PutBits(&strm, 1, 1);
    /* 2bitしか書いていないがフラッシュ */
    SLABitStream_Flush(&strm);
    Test_AssertEqual(strm.bit_buffer, 0);
    Test_AssertEqual(strm.bit_count,  8);
    SLABitStream_Close(&strm);

    /* 1バイトで先頭2bitだけが立っているはず */
    SLABitReader_Open(&strm, memory_image, sizeof(memory_image));
    SLABitReader_GetBits(&strm, &bits, 8);
    Test_AssertEqual(bits, 0xC0);
    SLABitStream_Flush(&strm);
    Test_AssertEqual(strm.bit_count,  0);
    Test_AssertEqual(strm.bit_buffer, 0xC0);
    SLABitStream_Close(&strm);
  }

}

/* seek, tellなどのストリーム操作系APIテスト */
static void testSLABitStream_StreamOperationTest(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

  /* Seek/Tellテスト */
  {
    struct SLABitStream   strm;
    int32_t               tell_result;
    uint8_t               test_memory[8];

    /* テスト用に適当にデータ作成 */
    SLABitWriter_Open(&strm, test_memory, sizeof(test_memory));
    SLABitWriter_PutBits(&strm, 0xDEADBEAF, 32);
    SLABitWriter_PutBits(&strm, 0xABADCAFE, 32);
    SLABitStream_Tell(&strm, &tell_result);
    Test_AssertEqual(tell_result, 8);
    SLABitStream_Close(&strm);

    /* ビットリーダを使ったseek & tellテスト */
    SLABitReader_Open(&strm, test_memory, sizeof(test_memory));
    SLABitStream_Seek(&strm, 0, SLABITSTREAM_SEEK_SET);
    SLABitStream_Tell(&strm, &tell_result);
    Test_AssertEqual(tell_result, 0);
    SLABitStream_Seek(&strm, 1, SLABITSTREAM_SEEK_CUR);
    SLABitStream_Tell(&strm, &tell_result);
    Test_AssertEqual(tell_result, 1);
    SLABitStream_Seek(&strm, 2, SLABITSTREAM_SEEK_CUR);
    SLABitStream_Tell(&strm, &tell_result);
    Test_AssertEqual(tell_result, 3);
    SLABitStream_Seek(&strm, 0, SLABITSTREAM_SEEK_END);
    SLABitStream_Tell(&strm, &tell_result);
    Test_AssertEqual(tell_result, 7);
    SLABitStream_Close(&strm);

    /* ビットライタを使ったseek & tellテスト */
    SLABitWriter_Open(&strm, test_memory, sizeof(test_memory));
    SLABitStream_Seek(&strm, 0, SLABITSTREAM_SEEK_SET);
    SLABitStream_Tell(&strm, &tell_result);
    Test_AssertEqual(tell_result, 0);
    SLABitStream_Seek(&strm, 1, SLABITSTREAM_SEEK_CUR);
    SLABitStream_Tell(&strm, &tell_result);
    Test_AssertEqual(tell_result, 1);
    SLABitStream_Seek(&strm, 2, SLABITSTREAM_SEEK_CUR);
    SLABitStream_Tell(&strm, &tell_result);
    Test_AssertEqual(tell_result, 3);
    SLABitStream_Seek(&strm, 0, SLABITSTREAM_SEEK_END);
    SLABitStream_Tell(&strm, &tell_result);
    Test_AssertEqual(tell_result, 7);
    SLABitStream_Close(&strm);
  }
}

/* ランレングス取得テスト */
static void testSLABitStream_GetZeroRunLengthTest(void* obj)
{
  TEST_UNUSED_PARAMETER(obj);

  {
    struct SLABitStream strm;
    uint8_t data[5];
    uint32_t test_length, run;

    for (test_length = 1; test_length <= 32; test_length++) {
      /* ラン長だけ0を書き込み、1で止める */
      SLABitWriter_Open(&strm, data, sizeof(data));
      SLABitWriter_PutBits(&strm, 0, test_length);
      SLABitWriter_PutBits(&strm, 1, 1);
      SLABitStream_Close(&strm);

      SLABitReader_Open(&strm, data, sizeof(data));
      SLABitReader_GetZeroRunLength(&strm, &run);
      Test_AssertEqual(run, test_length);
    }
  }

}

void testSLABitStream_Setup(void)
{
  struct TestSuite *suite
    = Test_AddTestSuite("SLA Bit Stream Test Suite",
        NULL, testSLABitStream_Initialize, testSLABitStream_Finalize);

  Test_AddTest(suite, testSLABitStream_CreateDestroyTest);
  Test_AddTest(suite, testSLABitStream_PutGetTest);
  Test_AddTest(suite, testSLABitStream_StreamOperationTest);
  Test_AddTest(suite, testSLABitStream_GetZeroRunLengthTest);
}
