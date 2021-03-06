#include <stdlib.h>
#include <string.h>
#include "test.h"

/* テスト対象のモジュール */
#include "../src/include/private/SLAByteArray.h"

/* テストのセットアップ関数 */
void testSLAByteArray_Setup(void);

static int testSLAByteArray_Initialize(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);
  return 0;
}

static int testSLAByteArray_Finalize(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);
  return 0;
}

/* 読み書きテスト */
static void testSLAByteArray_ReadWriteTest(void *obj)
{
#define TEST_SIZE (256 * 256)
  TEST_UNUSED_PARAMETER(obj);

  /* 1バイト読み/書き */
  {
    uint8_t *pos;
    uint8_t array[TEST_SIZE], answer[TEST_SIZE];
    uint32_t i;

    /* 書き出し */
    pos = array;
    for (i = 0; i < TEST_SIZE; i++) {
      answer[i] = (uint8_t)i;
      SLAByteArray_WriteUint8(pos, (uint8_t)i);
      pos += 1;
    }

    /* リファレンスと一致するか？ */
    Test_AssertEqual(memcmp(array, answer, sizeof(uint8_t) * TEST_SIZE), 0);

    /* 読み出し */
    pos = array;
    for (i = 0; i < TEST_SIZE; i++) {
      array[i] = SLAByteArray_ReadUint8(pos);
      pos += 1;
    }

    /* リファレンスと一致するか？ */
    Test_AssertEqual(memcmp(array, answer, sizeof(uint8_t) * TEST_SIZE), 0);
  }
  /* 同じことをGet/Putでやる */
  {
    uint8_t *pos;
    uint8_t array[TEST_SIZE], answer[TEST_SIZE];
    uint32_t i;

    /* 書き出し */
    pos = array;
    for (i = 0; i < TEST_SIZE; i++) {
      answer[i] = (uint8_t)i;
      SLAByteArray_PutUint8(pos, (uint8_t)i);
    }

    /* リファレンスと一致するか？ */
    Test_AssertEqual(memcmp(array, answer, sizeof(uint8_t) * TEST_SIZE), 0);

    /* 読み出し */
    pos = array;
    for (i = 0; i < TEST_SIZE; i++) {
      SLAByteArray_GetUint8(pos, &array[i]);
    }

    /* リファレンスと一致するか？ */
    Test_AssertEqual(memcmp(array, answer, sizeof(uint8_t) * TEST_SIZE), 0);
  }

  /* 2バイト読み/書き */
  {
#define TEST_SIZE_UINT16 (TEST_SIZE / sizeof(uint16_t))
    uint8_t   *pos;
    uint8_t   array[TEST_SIZE];
    uint16_t  test[TEST_SIZE_UINT16], answer[TEST_SIZE_UINT16];
    uint32_t  i;

    /* 書き出し */
    pos = array;
    for (i = 0; i < TEST_SIZE_UINT16; i++) {
      answer[i] = (uint16_t)i;
      SLAByteArray_WriteUint16(pos, (uint16_t)i);
      pos += 2;
    }

    /* 読み出し */
    pos = array;
    for (i = 0; i < TEST_SIZE_UINT16; i++) {
      test[i] = SLAByteArray_ReadUint16(pos);
      pos += 2;
    }

    /* 読み出した結果がリファレンスと一致するか？ */
    Test_AssertEqual(memcmp(test, answer, sizeof(uint8_t) * TEST_SIZE), 0);
#undef TEST_SIZE_UINT16
  }
  /* 同じことをGet/Putでやる */
  {
#define TEST_SIZE_UINT16 (TEST_SIZE / sizeof(uint16_t))
    uint8_t   *pos;
    uint8_t   array[TEST_SIZE];
    uint16_t  test[TEST_SIZE_UINT16], answer[TEST_SIZE_UINT16];
    uint32_t  i;

    /* 書き出し */
    pos = array;
    for (i = 0; i < TEST_SIZE_UINT16; i++) {
      answer[i] = (uint16_t)i;
      SLAByteArray_PutUint16(pos, (uint16_t)i);
    }

    /* 読み出し */
    pos = array;
    for (i = 0; i < TEST_SIZE_UINT16; i++) {
      SLAByteArray_GetUint16(pos, &test[i]);
    }

    /* 読み出した結果がリファレンスと一致するか？ */
    Test_AssertEqual(memcmp(test, answer, sizeof(uint8_t) * TEST_SIZE), 0);
#undef TEST_SIZE_UINT16
  }

  /* 4バイト読み/書き */
  {
#define TEST_SIZE_UINT32 (TEST_SIZE / sizeof(uint32_t))
    uint8_t   *pos;
    uint8_t   array[TEST_SIZE];
    uint32_t  test[TEST_SIZE_UINT32], answer[TEST_SIZE_UINT32];
    uint32_t  i;

    /* 書き出し */
    pos = array;
    for (i = 0; i < TEST_SIZE_UINT32; i++) {
      answer[i] = (uint32_t)i;
      SLAByteArray_WriteUint32(pos, (uint32_t)i);
      pos += 4;
    }

    /* 読み出し */
    pos = array;
    for (i = 0; i < TEST_SIZE_UINT32; i++) {
      test[i] = SLAByteArray_ReadUint32(pos);
      pos += 4;
    }

    /* 読み出した結果がリファレンスと一致するか？ */
    Test_AssertEqual(memcmp(test, answer, sizeof(uint8_t) * TEST_SIZE), 0);
#undef TEST_SIZE_UINT32
  }
  /* 同じことをGet/Putでやる */
  {
#define TEST_SIZE_UINT32 (TEST_SIZE / sizeof(uint32_t))
    uint8_t   *pos;
    uint8_t   array[TEST_SIZE];
    uint32_t  test[TEST_SIZE_UINT32], answer[TEST_SIZE_UINT32];
    uint32_t  i;

    /* 書き出し */
    pos = array;
    for (i = 0; i < TEST_SIZE_UINT32; i++) {
      answer[i] = (uint32_t)i;
      SLAByteArray_PutUint32(pos, (uint32_t)i);
    }

    /* 読み出し */
    pos = array;
    for (i = 0; i < TEST_SIZE_UINT32; i++) {
      SLAByteArray_GetUint32(pos, &test[i]);
    }

    /* 読み出した結果がリファレンスと一致するか？ */
    Test_AssertEqual(memcmp(test, answer, sizeof(uint8_t) * TEST_SIZE), 0);
#undef TEST_SIZE_UINT16
  }

#undef TEST_SIZE
}

void testSLAByteArray_Setup(void)
{
  struct TestSuite *suite
    = Test_AddTestSuite("SLA Byte Array Test Suite",
        NULL, testSLAByteArray_Initialize, testSLAByteArray_Finalize);

  Test_AddTest(suite, testSLAByteArray_ReadWriteTest);
}
