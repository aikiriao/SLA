#include "test.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

/* テスト対象のモジュール */
#include "../SLAUtility.c"

int testSLAUtility_Initialize(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);
  return 0;
}

int testSLAUtility_Finalize(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);
  return 0;
}

/* CRC16の計算テスト */
static void testSLAUtility_CalculateCRC16Test(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

  /* リファレンス値と一致するか？ */
  {
    uint32_t i;
    uint16_t ret;

    /* テストケース */
    struct CRC16TestCaseFor32BitData {
      uint8_t  data[4];
      uint16_t answer;
    };

    static const struct CRC16TestCaseFor32BitData crc16ibm_test_case[] = {
      { { 0x00, 0x00, 0x00, 0x01 }, 0xC0C1 },
      { { 0x10, 0x00, 0x00, 0x00 }, 0xC004 },
      { { 0x00, 0xFF, 0xFF, 0x00 }, 0xC071 },
      { { 0xDE, 0xAD, 0xBE, 0xAF }, 0x159A },
      { { 0xAB, 0xAD, 0xCA, 0xFE }, 0xE566 },
      { { 0x12, 0x34, 0x56, 0x78 }, 0x347B },
    };
    const uint32_t crc16ibm_num_test_cases = sizeof(crc16ibm_test_case) / sizeof(crc16ibm_test_case[0]);

    for (i = 0; i < crc16ibm_num_test_cases; i++) {
      ret = SLAUtility_CalculateCRC16(crc16ibm_test_case[i].data, sizeof(crc16ibm_test_case[0].data));
      Test_AssertEqual(ret, crc16ibm_test_case[i].answer);
    }
  }

  /* 実データでテスト */
  {
    struct stat fstat;
    uint32_t  i, data_size;
    uint16_t  ret;
    uint8_t*  data;
    FILE*     fp;

    /* テストケース */
    struct CRC16TestCaseForFile {
      const char* filename;
      uint16_t answer;
    };

    static const struct CRC16TestCaseForFile crc16ibm_test_case[] = {
      { "a.wav",            0xA611 },
      { "PriChanIcon.png",  0xEA63 },
    };
    const uint32_t crc16ibm_num_test_cases
      = sizeof(crc16ibm_test_case) / sizeof(crc16ibm_test_case[0]);

    for (i = 0; i < crc16ibm_num_test_cases; i++) {
      stat(crc16ibm_test_case[i].filename, &fstat);
      data_size = fstat.st_size;
      data = malloc(fstat.st_size * sizeof(uint8_t));

      fp = fopen(crc16ibm_test_case[i].filename, "rb");
      fread(data, sizeof(uint8_t), data_size, fp);
      ret = SLAUtility_CalculateCRC16(data, data_size);
      Test_AssertEqual(ret, crc16ibm_test_case[i].answer);

      free(data);
      fclose(fp);
    }

  }
}

void testSLAUtility_Setup(void)
{
  struct TestSuite *suite
    = Test_AddTestSuite("SLA Utility Test Suite",
        NULL, testSLAUtility_Initialize, testSLAUtility_Finalize);

  Test_AddTest(suite, testSLAUtility_CalculateCRC16Test);
}
