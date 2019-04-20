#include "test.h"

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

void testSLADecoder_Setup(void)
{
  struct TestSuite *suite
    = Test_AddTestSuite("SLA Decoder Test Suite",
        NULL, testSLADecoder_Initialize, testSLADecoder_Finalize);
}
