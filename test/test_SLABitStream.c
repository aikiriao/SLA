#include <stdlib.h>
#include <string.h>
#include "test.h"

/* テスト対象のモジュール */
#include "../SLABitStream.c"

int testSLABitStream_Initialize(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);
  return 0;
}

int testSLABitStream_Finalize(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);
  return 0;
}

/* インスタンス作成破棄テスト */
static void testSLABitStream_CreateDestroyTest(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

  /* ワークサイズ計算テスト */
  {
    Test_AssertCondition((int32_t)sizeof(struct SLABitStream) <= SLABitStream_CalculateWorkSize());
  }

  /* インスタンス作成・破棄 */
  {
    struct SLABitStream* strm;
    void*             work;
    const char test_filename[] = "test.bin";

    /* 書きモードでインスタンス作成 */
    strm = SLABitStream_Open(test_filename, "w", NULL, 0);
    Test_AssertCondition(strm != NULL);
    Test_AssertCondition(strm->stm.fp != NULL);
    Test_AssertCondition(!(strm->flags & SLABITSTREAM_FLAGS_MEMORYALLOC_BYWORK));
    Test_AssertCondition(strm->work_ptr != NULL);
    Test_AssertEqual(strm->bit_buffer, 0);
    Test_AssertCondition(!(strm->flags & SLABITSTREAM_FLAGS_FILEOPENMODE_READ));
    Test_AssertEqual(strm->bit_count, 8);
    SLABitStream_Close(strm);
    Test_AssertCondition(strm->work_ptr == NULL);

    /* 読みモードでインスタンス作成 */
    strm = SLABitStream_Open(test_filename, "r", NULL, 0);
    Test_AssertCondition(strm != NULL);
    Test_AssertCondition(strm->stm.fp != NULL);
    Test_AssertCondition(!(strm->flags & SLABITSTREAM_FLAGS_MEMORYALLOC_BYWORK));
    Test_AssertCondition(strm->work_ptr != NULL);
    Test_AssertEqual(strm->bit_buffer, 0);
    Test_AssertCondition(strm->flags & SLABITSTREAM_FLAGS_FILEOPENMODE_READ);
    Test_AssertEqual(strm->bit_count, 0);
    SLABitStream_Close(strm);
    Test_AssertCondition(strm->work_ptr == NULL);

    /* ワークメモリを渡してインスタンス作成・破棄 */
    work = malloc(SLABitStream_CalculateWorkSize());

    strm = SLABitStream_Open(test_filename, "w", work, SLABitStream_CalculateWorkSize());
    Test_AssertCondition(strm != NULL);
    Test_AssertCondition(strm->stm.fp != NULL);
    Test_AssertCondition(strm->flags & SLABITSTREAM_FLAGS_MEMORYALLOC_BYWORK);
    Test_AssertCondition(strm->work_ptr != NULL);
    Test_AssertEqual(strm->bit_buffer, 0);
    Test_AssertCondition(!(strm->flags & SLABITSTREAM_FLAGS_FILEOPENMODE_READ));
    Test_AssertEqual(strm->bit_count, 8);
    SLABitStream_Close(strm);

    strm = SLABitStream_Open(test_filename, "r", work, SLABitStream_CalculateWorkSize());
    Test_AssertCondition(strm != NULL);
    Test_AssertCondition(strm->stm.fp != NULL);
    Test_AssertCondition(strm->flags & SLABITSTREAM_FLAGS_MEMORYALLOC_BYWORK);
    Test_AssertCondition(strm->work_ptr != NULL);
    Test_AssertEqual(strm->bit_buffer, 0);
    Test_AssertCondition(strm->flags & SLABITSTREAM_FLAGS_FILEOPENMODE_READ);
    Test_AssertEqual(strm->bit_count, 0);
    SLABitStream_Close(strm);

    free(work);
  }

  /* 作成失敗テスト */
  {
    struct SLABitStream* strm;
    void*             work;
    const char test_filename[] = "test.bin";

    /* テスト用にファイル作成 */
    fclose(fopen(test_filename, "w"));

    /* モードが不正/NULL */
    strm = SLABitStream_Open("test.bin", "a", NULL, 0);
    Test_AssertCondition(strm == NULL);
    strm = SLABitStream_Open("test.bin", "+", NULL, 0);
    Test_AssertCondition(strm == NULL);
    strm = SLABitStream_Open("test.bin", NULL, NULL, 0);
    Test_AssertCondition(strm == NULL);

    /* ワークサイズが不正 */
    strm = SLABitStream_Open("test.bin", "w", NULL, -1);
    Test_AssertCondition(strm == NULL);
    {
      work = malloc(SLABitStream_CalculateWorkSize());
      strm = SLABitStream_Open("test.bin", "w", work, -1);
      Test_AssertCondition(strm == NULL);
      strm = SLABitStream_Open("test.bin", "w", work, SLABitStream_CalculateWorkSize()-1);
      Test_AssertCondition(strm == NULL);
      free(work);
    }
  }

  /* インスタンス作成・破棄（メモリ） */
  {
    struct SLABitStream* strm;
    uint8_t test_memory[] = {'A', 'I', 'K', 'A', 'T', 'S', 'U'};
    const uint32_t test_memory_size = sizeof(test_memory) / sizeof(test_memory[0]);
    void* work;

    /* 書きモードでインスタンス作成 */
    strm = SLABitStream_OpenMemory(test_memory, test_memory_size, "w", NULL, 0);
    Test_AssertCondition(strm != NULL);
    Test_AssertCondition(strm->stm.mem.memory_image == test_memory);
    Test_AssertEqual(strm->stm.mem.memory_size, test_memory_size);
    Test_AssertEqual(strm->stm.mem.memory_p, 0);
    Test_AssertCondition(!(strm->flags & SLABITSTREAM_FLAGS_MEMORYALLOC_BYWORK));
    Test_AssertCondition(strm->work_ptr != NULL);
    Test_AssertEqual(strm->bit_buffer, 0);
    Test_AssertCondition(!(strm->flags & SLABITSTREAM_FLAGS_FILEOPENMODE_READ));
    Test_AssertEqual(strm->bit_count, 8);
    SLABitStream_Close(strm);
    Test_AssertCondition(strm->work_ptr == NULL);

    /* 読みモードでインスタンス作成 */
    strm = SLABitStream_OpenMemory(test_memory, test_memory_size, "r", NULL, 0);
    Test_AssertCondition(strm != NULL);
    Test_AssertCondition(strm->stm.mem.memory_image == test_memory);
    Test_AssertEqual(strm->stm.mem.memory_size, test_memory_size);
    Test_AssertEqual(strm->stm.mem.memory_p, 0);
    Test_AssertCondition(!(strm->flags & SLABITSTREAM_FLAGS_MEMORYALLOC_BYWORK));
    Test_AssertCondition(strm->work_ptr != NULL);
    Test_AssertEqual(strm->bit_buffer, 0);
    Test_AssertCondition(strm->flags & SLABITSTREAM_FLAGS_FILEOPENMODE_READ);
    Test_AssertEqual(strm->bit_count, 0);
    SLABitStream_Close(strm);
    Test_AssertCondition(strm->work_ptr == NULL);

    /* ワークメモリを渡して作成・破棄 */
    work = malloc(SLABitStream_CalculateWorkSize());

    strm = SLABitStream_OpenMemory(test_memory, test_memory_size, "w", work, SLABitStream_CalculateWorkSize());
    Test_AssertCondition(strm != NULL);
    Test_AssertCondition(strm->stm.mem.memory_image == test_memory);
    Test_AssertEqual(strm->stm.mem.memory_size, test_memory_size);
    Test_AssertEqual(strm->stm.mem.memory_p, 0);
    Test_AssertCondition(strm->flags & SLABITSTREAM_FLAGS_MEMORYALLOC_BYWORK);
    Test_AssertCondition(strm->work_ptr != NULL);
    Test_AssertEqual(strm->bit_buffer, 0);
    Test_AssertCondition(!(strm->flags & SLABITSTREAM_FLAGS_FILEOPENMODE_READ));
    Test_AssertEqual(strm->bit_count, 8);
    SLABitStream_Close(strm);

    strm = SLABitStream_OpenMemory(test_memory, test_memory_size, "r", work, SLABitStream_CalculateWorkSize());
    Test_AssertCondition(strm != NULL);
    Test_AssertCondition(strm->stm.mem.memory_image == test_memory);
    Test_AssertEqual(strm->stm.mem.memory_size, test_memory_size);
    Test_AssertEqual(strm->stm.mem.memory_p, 0);
    Test_AssertCondition(strm->flags & SLABITSTREAM_FLAGS_MEMORYALLOC_BYWORK);
    Test_AssertCondition(strm->work_ptr != NULL);
    Test_AssertEqual(strm->bit_buffer, 0);
    Test_AssertCondition(strm->flags & SLABITSTREAM_FLAGS_FILEOPENMODE_READ);
    Test_AssertEqual(strm->bit_count, 0);
    SLABitStream_Close(strm);

    free(work);
  }

  /* 作成失敗テスト（メモリ） */
  {
    struct SLABitStream* strm;
    void*             work;
    uint8_t test_memory[] = {'A', 'I', 'K', 'A', 'T', 'S', 'U'};
    const uint32_t test_memory_size = sizeof(test_memory) / sizeof(test_memory[0]);

    /* メモリがNULL */
    strm = SLABitStream_OpenMemory(NULL, 0, "w", NULL, 0);
    Test_AssertCondition(strm == NULL);

    /* モードが不正/NULL */
    strm = SLABitStream_OpenMemory(test_memory, test_memory_size, "a", NULL, 0);
    Test_AssertCondition(strm == NULL);
    strm = SLABitStream_OpenMemory(test_memory, test_memory_size, "+", NULL, 0);
    Test_AssertCondition(strm == NULL);
    strm = SLABitStream_OpenMemory(test_memory, test_memory_size, NULL, NULL, 0);
    Test_AssertCondition(strm == NULL);

    /* ワークサイズが不正 */
    strm = SLABitStream_Open("test.bin", "w", NULL, -1);
    Test_AssertCondition(strm == NULL);
    {
      work = malloc(SLABitStream_CalculateWorkSize());
      strm = SLABitStream_OpenMemory(test_memory, test_memory_size, "w", work, -1);
      Test_AssertCondition(strm == NULL);
      strm = SLABitStream_OpenMemory(test_memory, test_memory_size, "w", work, SLABitStream_CalculateWorkSize()-1);
      Test_AssertCondition(strm == NULL);
      free(work);
    }
  }

}

static void testSLABitStream_PutGetTest(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

  /* 失敗テスト */
  {
    struct SLABitStream* strm;
    const char test_filename[] = "test_put.bin";

    strm = SLABitStream_Open(test_filename, "w", NULL, 0);
    Test_AssertNotEqual(SLABitStream_PutBit(NULL, 1), SLABITSTREAM_APIRESULT_OK);
    Test_AssertNotEqual(SLABitStream_PutBit(NULL, 0), SLABITSTREAM_APIRESULT_OK);
    Test_AssertNotEqual(SLABitStream_PutBits(NULL,  1, 1), SLABITSTREAM_APIRESULT_OK);
    Test_AssertNotEqual(SLABitStream_PutBits(NULL,  1, 0), SLABITSTREAM_APIRESULT_OK);
    Test_AssertNotEqual(SLABitStream_PutBits(strm, 65, 0), SLABITSTREAM_APIRESULT_OK);

    SLABitStream_Close(strm);
  }

  /* PutBit関数テスト（ファイル） */
  {
    struct SLABitStream* strm;
    uint8_t bit;
    const char test_filename[] = "test_putbit.bin";
    uint8_t bit_pattern[] = { 1, 1, 1, 1, 0, 0, 0, 0 };
    uint32_t bit_pattern_length = sizeof(bit_pattern) / sizeof(bit_pattern[0]);
    uint32_t i, is_ok;

    /* 書き込んでみる */
    strm = SLABitStream_Open(test_filename, "w", NULL, 0);
    is_ok = 1;
    for (i = 0; i < bit_pattern_length; i++) { 
      if (SLABitStream_PutBit(strm, bit_pattern[i]) != SLABITSTREAM_APIRESULT_OK) {
        is_ok = 0;
        break;
      }
    }
    Test_AssertEqual(is_ok, 1);
    SLABitStream_Close(strm);

    /* 正しく書き込めているか？ */
    strm = SLABitStream_Open(test_filename, "r", NULL, 0);
    is_ok = 1;
    for (i = 0; i < bit_pattern_length; i++) { 
      if (SLABitStream_GetBit(strm, &bit) != SLABITSTREAM_APIRESULT_OK) {
        is_ok = 0;
        break;
      }
      if (bit != bit_pattern[i]) {
        is_ok = 0;
        break;
      }
    }
    Test_AssertEqual(is_ok, 1);
    SLABitStream_Close(strm);
  }

  /* PutBits関数テスト（ファイル） */
  {
    struct SLABitStream* strm;
    uint64_t bits;
    const char test_filename[] = "test_putbits.bin";
    uint16_t bit_pattern[] = { 0xDEAD, 0xBEAF, 0xABAD, 0xCAFE };
    uint32_t bit_pattern_length = sizeof(bit_pattern) / sizeof(bit_pattern[0]);
    uint32_t i, is_ok;

    /* 書き込んでみる */
    strm = SLABitStream_Open(test_filename, "w", NULL, 0);
    is_ok = 1;
    for (i = 0; i < bit_pattern_length; i++) { 
      if (SLABitStream_PutBits(strm, 16, bit_pattern[i]) != SLABITSTREAM_APIRESULT_OK) {
        is_ok = 0;
        break;
      }
    }
    Test_AssertEqual(is_ok, 1);
    SLABitStream_Close(strm);

    /* 正しく書き込めているか？ */
    strm = SLABitStream_Open(test_filename, "r", NULL, 0);
    is_ok = 1;
    for (i = 0; i < bit_pattern_length; i++) { 
      if (SLABitStream_GetBits(strm, 16, &bits) != SLABITSTREAM_APIRESULT_OK) {
        is_ok = 0;
        break;
      }
      if ((uint16_t)bits != bit_pattern[i]) {
        is_ok = 0;
        break;
      }
    }
    Test_AssertEqual(is_ok, 1);
    SLABitStream_Close(strm);
  }

  /* PutBit関数テスト（ワークメモリ渡し、ファイル） */
  {
    struct SLABitStream* strm;
    uint8_t bit;
    const char test_filename[] = "test_putbit.bin";
    uint8_t bit_pattern[] = { 1, 1, 1, 1, 0, 0, 0, 0 };
    uint32_t bit_pattern_length = sizeof(bit_pattern) / sizeof(bit_pattern[0]);
    uint32_t i, is_ok;
    void* work;

    /* 書き込んでみる */
    work = malloc(SLABitStream_CalculateWorkSize());
    strm = SLABitStream_Open(test_filename, "w", work, SLABitStream_CalculateWorkSize());
    is_ok = 1;
    for (i = 0; i < bit_pattern_length; i++) { 
      if (SLABitStream_PutBit(strm, bit_pattern[i]) != SLABITSTREAM_APIRESULT_OK) {
        is_ok = 0;
        break;
      }
    }
    Test_AssertEqual(is_ok, 1);
    SLABitStream_Close(strm);
    free(work);

    /* 正しく書き込めているか？ */
    work = malloc(SLABitStream_CalculateWorkSize());
    strm = SLABitStream_Open(test_filename, "r", work, SLABitStream_CalculateWorkSize());
    is_ok = 1;
    for (i = 0; i < bit_pattern_length; i++) { 
      if (SLABitStream_GetBit(strm, &bit) != SLABITSTREAM_APIRESULT_OK) {
        is_ok = 0;
        break;
      }
      if (bit != bit_pattern[i]) {
        is_ok = 0;
        break;
      }
    }
    Test_AssertEqual(is_ok, 1);
    SLABitStream_Close(strm);
    free(work);
  }

  /* PutBits関数テスト（ワークメモリ渡し、ファイル） */
  {
    struct SLABitStream* strm;
    uint64_t bits;
    const char test_filename[] = "test_putbits.bin";
    uint16_t bit_pattern[] = { 0xDEAD, 0xBEAF, 0xABAD, 0xCAFE };
    uint32_t bit_pattern_length = sizeof(bit_pattern) / sizeof(bit_pattern[0]);
    uint32_t i, is_ok;
    void* work;

    /* 書き込んでみる */
    work = malloc(SLABitStream_CalculateWorkSize());
    strm = SLABitStream_Open(test_filename, "w", work, SLABitStream_CalculateWorkSize());
    is_ok = 1;
    for (i = 0; i < bit_pattern_length; i++) { 
      if (SLABitStream_PutBits(strm, 16, bit_pattern[i]) != SLABITSTREAM_APIRESULT_OK) {
        is_ok = 0;
        break;
      }
    }
    Test_AssertEqual(is_ok, 1);
    SLABitStream_Close(strm);
    free(work);

    /* 正しく書き込めているか？ */
    work = malloc(SLABitStream_CalculateWorkSize());
    strm = SLABitStream_Open(test_filename, "r", work, SLABitStream_CalculateWorkSize());
    is_ok = 1;
    for (i = 0; i < bit_pattern_length; i++) { 
      if (SLABitStream_GetBits(strm, 16, &bits) != SLABITSTREAM_APIRESULT_OK) {
        is_ok = 0;
        break;
      }
      if ((uint16_t)bits != bit_pattern[i]) {
        is_ok = 0;
        break;
      }
    }
    Test_AssertEqual(is_ok, 1);
    SLABitStream_Close(strm);
    free(work);
  }

  /* PutBit関数テスト（メモリ） */
  {
    struct SLABitStream* strm;
    uint8_t bit;
    uint8_t bit_pattern[] = { 1, 1, 1, 1, 0, 0, 0, 0 };
    uint8_t memory_image[256];
    uint32_t bit_pattern_length = sizeof(bit_pattern) / sizeof(bit_pattern[0]);
    uint32_t i, is_ok;

    /* 書き込んでみる */
    strm = SLABitStream_OpenMemory(memory_image, sizeof(memory_image), "w", NULL, 0);
    is_ok = 1;
    for (i = 0; i < bit_pattern_length; i++) { 
      if (SLABitStream_PutBit(strm, bit_pattern[i]) != SLABITSTREAM_APIRESULT_OK) {
        is_ok = 0;
        break;
      }
    }
    Test_AssertEqual(is_ok, 1);
    SLABitStream_Close(strm);

    /* 正しく書き込めているか？ */
    strm = SLABitStream_OpenMemory(memory_image, sizeof(memory_image), "r", NULL, 0);
    is_ok = 1;
    for (i = 0; i < bit_pattern_length; i++) { 
      if (SLABitStream_GetBit(strm, &bit) != SLABITSTREAM_APIRESULT_OK) {
        is_ok = 0;
        break;
      }
      if (bit != bit_pattern[i]) {
        is_ok = 0;
        break;
      }
    }
    Test_AssertEqual(is_ok, 1);
    SLABitStream_Close(strm);
  }

  /* PutBits関数テスト（ファイル） */
  {
    struct SLABitStream* strm;
    uint64_t bits;
    uint16_t bit_pattern[] = { 0xDEAD, 0xBEAF, 0xABAD, 0xCAFE };
    uint32_t bit_pattern_length = sizeof(bit_pattern) / sizeof(bit_pattern[0]);
    uint8_t memory_image[256];
    uint32_t i, is_ok;

    /* 書き込んでみる */
    strm = SLABitStream_OpenMemory(memory_image, sizeof(memory_image), "w", NULL, 0);
    is_ok = 1;
    for (i = 0; i < bit_pattern_length; i++) { 
      if (SLABitStream_PutBits(strm, 16, bit_pattern[i]) != SLABITSTREAM_APIRESULT_OK) {
        is_ok = 0;
        break;
      }
    }
    Test_AssertEqual(is_ok, 1);
    SLABitStream_Close(strm);

    /* 正しく書き込めているか？ */
    strm = SLABitStream_OpenMemory(memory_image, sizeof(memory_image), "r", NULL, 0);
    is_ok = 1;
    for (i = 0; i < bit_pattern_length; i++) { 
      if (SLABitStream_GetBits(strm, 16, &bits) != SLABITSTREAM_APIRESULT_OK) {
        is_ok = 0;
        break;
      }
      if ((uint16_t)bits != bit_pattern[i]) {
        is_ok = 0;
        break;
      }
    }
    Test_AssertEqual(is_ok, 1);
    SLABitStream_Close(strm);
  }

  /* PutBit関数テスト（ワークメモリ渡し、ファイル） */
  {
    struct SLABitStream* strm;
    uint8_t bit;
    uint8_t bit_pattern[] = { 1, 1, 1, 1, 0, 0, 0, 0 };
    uint32_t bit_pattern_length = sizeof(bit_pattern) / sizeof(bit_pattern[0]);
    uint8_t memory_image[256];
    uint32_t i, is_ok;
    void* work;

    /* 書き込んでみる */
    work = malloc(SLABitStream_CalculateWorkSize());
    strm = SLABitStream_OpenMemory(memory_image,
        sizeof(memory_image), "w", work, SLABitStream_CalculateWorkSize());
    is_ok = 1;
    for (i = 0; i < bit_pattern_length; i++) { 
      if (SLABitStream_PutBit(strm, bit_pattern[i]) != SLABITSTREAM_APIRESULT_OK) {
        is_ok = 0;
        break;
      }
    }
    Test_AssertEqual(is_ok, 1);
    SLABitStream_Close(strm);
    free(work);

    /* 正しく書き込めているか？ */
    work = malloc(SLABitStream_CalculateWorkSize());
    strm = SLABitStream_OpenMemory(memory_image,
        sizeof(memory_image), "r", work, SLABitStream_CalculateWorkSize());
    is_ok = 1;
    for (i = 0; i < bit_pattern_length; i++) { 
      if (SLABitStream_GetBit(strm, &bit) != SLABITSTREAM_APIRESULT_OK) {
        is_ok = 0;
        break;
      }
      if (bit != bit_pattern[i]) {
        is_ok = 0;
        break;
      }
    }
    Test_AssertEqual(is_ok, 1);
    SLABitStream_Close(strm);
    free(work);
  }

  /* PutBits関数テスト（ワークメモリ渡し、ファイル） */
  {
    struct SLABitStream* strm;
    uint64_t bits;
    uint8_t memory_image[256];
    uint16_t bit_pattern[] = { 0xDEAD, 0xBEAF, 0xABAD, 0xCAFE };
    uint32_t bit_pattern_length = sizeof(bit_pattern) / sizeof(bit_pattern[0]);
    uint32_t i, is_ok;
    void* work;

    /* 書き込んでみる */
    work = malloc(SLABitStream_CalculateWorkSize());
    strm = SLABitStream_OpenMemory(memory_image,
        sizeof(memory_image), "w", work, SLABitStream_CalculateWorkSize());
    is_ok = 1;
    for (i = 0; i < bit_pattern_length; i++) { 
      if (SLABitStream_PutBits(strm, 16, bit_pattern[i]) != SLABITSTREAM_APIRESULT_OK) {
        is_ok = 0;
        break;
      }
    }
    Test_AssertEqual(is_ok, 1);
    SLABitStream_Close(strm);
    free(work);

    /* 正しく書き込めているか？ */
    work = malloc(SLABitStream_CalculateWorkSize());
    strm = SLABitStream_OpenMemory(memory_image,
        sizeof(memory_image), "r", work, SLABitStream_CalculateWorkSize());
    is_ok = 1;
    for (i = 0; i < bit_pattern_length; i++) { 
      if (SLABitStream_GetBits(strm, 16, &bits) != SLABITSTREAM_APIRESULT_OK) {
        is_ok = 0;
        break;
      }
      if ((uint16_t)bits != bit_pattern[i]) {
        is_ok = 0;
        break;
      }
    }
    Test_AssertEqual(is_ok, 1);
    SLABitStream_Close(strm);
    free(work);
  }

  /* Flushテスト（ファイル） */
  {
    struct SLABitStream* strm;
    const char test_filename[] = "test_flush.bin";
    uint64_t bits;

    strm = SLABitStream_Open(test_filename, "w", NULL, 0);
    Test_AssertEqual(SLABitStream_PutBit(strm, 1), SLABITSTREAM_APIRESULT_OK);
    Test_AssertEqual(SLABitStream_PutBit(strm, 1), SLABITSTREAM_APIRESULT_OK);
    /* 2bitしか書いていないがフラッシュ */
    Test_AssertEqual(SLABitStream_Flush(strm), SLABITSTREAM_APIRESULT_OK);
    Test_AssertEqual(strm->bit_buffer, 0);
    Test_AssertEqual(strm->bit_count,  8);
    SLABitStream_Close(strm);

    /* 1バイトで先頭2bitだけが立っているはず */
    strm = SLABitStream_Open(test_filename, "r", NULL, 0);
    Test_AssertEqual(SLABitStream_GetBits(strm, 8, &bits), SLABITSTREAM_APIRESULT_OK);
    Test_AssertEqual(bits, 0xC0);
    Test_AssertEqual(SLABitStream_Flush(strm), SLABITSTREAM_APIRESULT_OK);
    Test_AssertEqual(strm->bit_count,  0);
    Test_AssertEqual(strm->bit_buffer, 0xC0);
    SLABitStream_Close(strm);
  }

  /* Flushテスト（メモリ） */
  {
    struct SLABitStream* strm;
    uint8_t memory_image[256];
    uint64_t bits;

    strm = SLABitStream_OpenMemory(memory_image, sizeof(memory_image), "w", NULL, 0);
    Test_AssertEqual(SLABitStream_PutBit(strm, 1), SLABITSTREAM_APIRESULT_OK);
    Test_AssertEqual(SLABitStream_PutBit(strm, 1), SLABITSTREAM_APIRESULT_OK);
    /* 1bitしか書いていないがフラッシュ */
    Test_AssertEqual(SLABitStream_Flush(strm), SLABITSTREAM_APIRESULT_OK);
    Test_AssertEqual(strm->bit_buffer, 0);
    Test_AssertEqual(strm->bit_count,  8);
    SLABitStream_Close(strm);

    /* 1バイトで先頭2bitだけが立っているはず */
    strm = SLABitStream_OpenMemory(memory_image, sizeof(memory_image), "r", NULL, 0);
    Test_AssertEqual(SLABitStream_GetBits(strm, 8, &bits), SLABITSTREAM_APIRESULT_OK);
    Test_AssertEqual(bits, 0xC0);
    Test_AssertEqual(SLABitStream_Flush(strm), SLABITSTREAM_APIRESULT_OK);
    Test_AssertEqual(strm->bit_count,  0);
    Test_AssertEqual(strm->bit_buffer, 0xC0);
    SLABitStream_Close(strm);
  }

}

/* seek, ftellなどのストリーム操作系APIテスト */
static void testSLABitStream_FileOperationTest(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

  /* Seek/Tellテスト */
  {
    struct SLABitStream* strm;
    SLABitStreamApiResult result;
    int32_t            tell_result;
    const char test_filename[] = "test_fileseek.bin";

    /* テスト用に適当にファイル作成 */
    strm = SLABitStream_Open(test_filename, "w", NULL, 0);
    SLABitStream_PutBits(strm, 32, 0xDEADBEAF);
    SLABitStream_PutBits(strm, 32, 0xABADCAFE);
    Test_AssertEqual(SLABitStream_Tell(strm, &tell_result), SLABITSTREAM_APIRESULT_OK);
    Test_AssertEqual(tell_result, 8);
    SLABitStream_Close(strm);

    strm = SLABitStream_Open(test_filename, "r", NULL, 0);

    result = SLABitStream_Seek(strm, 0, SLABITSTREAM_SEEK_SET);
    Test_AssertEqual(result, SLABITSTREAM_APIRESULT_OK);
    result = SLABitStream_Tell(strm, &tell_result);
    Test_AssertEqual(result, SLABITSTREAM_APIRESULT_OK);
    Test_AssertEqual(tell_result, 0);

    result = SLABitStream_Seek(strm, 1, SLABITSTREAM_SEEK_CUR);
    Test_AssertEqual(result, SLABITSTREAM_APIRESULT_OK);
    result = SLABitStream_Tell(strm, &tell_result);
    Test_AssertEqual(result, SLABITSTREAM_APIRESULT_OK);
    Test_AssertEqual(tell_result, 1);

    result = SLABitStream_Seek(strm, 2, SLABITSTREAM_SEEK_CUR);
    Test_AssertEqual(result, SLABITSTREAM_APIRESULT_OK);
    result = SLABitStream_Tell(strm, &tell_result);
    Test_AssertEqual(result, SLABITSTREAM_APIRESULT_OK);
    Test_AssertEqual(tell_result, 3);

    /* 本当はプラットフォーム依存で不定値になるのでやりたくない */
    result = SLABitStream_Seek(strm, 0, SLABITSTREAM_SEEK_END);
    Test_AssertEqual(result, SLABITSTREAM_APIRESULT_OK);
    result = SLABitStream_Tell(strm, &tell_result);
    Test_AssertEqual(result, SLABITSTREAM_APIRESULT_OK);
    Test_AssertEqual(tell_result, 8);

    SLABitStream_Close(strm);
  }
}

/* ランレングス取得テスト */
static void testSLABitStream_GetZeroRunLengthTest(void* obj)
{
  TEST_UNUSED_PARAMETER(obj);

  {
    struct SLABitStream* strm;
    uint8_t data[4];
    uint32_t test_length, run;
    SLABitStreamApiResult ret;

    for (test_length = 0; test_length < 32; test_length++) {
      /* ラン長だけ0を書き込み、1で止める */
      strm = SLABitStream_OpenMemory(data, sizeof(data), "w", NULL, 0);
      for (run = 0; run < test_length; run++) {
        SLABitStream_PutBit(strm, 0);
      }
      SLABitStream_PutBit(strm, 1);
      SLABitStream_Close(strm);

      strm = SLABitStream_OpenMemory(data, sizeof(data), "r", NULL, 0);
      ret = SLABitStream_GetZeroRunLength(strm, &run);
      Test_AssertEqual(ret, SLABITSTREAM_APIRESULT_OK);
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
  Test_AddTest(suite, testSLABitStream_FileOperationTest);
  Test_AddTest(suite, testSLABitStream_GetZeroRunLengthTest);
}
