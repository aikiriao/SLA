#include "test.h"

/* 各テストスイートのセットアップ関数宣言 */
void testSLABitStream_Setup(void);
void testSLAPredictor_Setup(void);
void testSLACoder_Setup(void);
void testSLAEncoder_Setup(void);
void testSLADecoder_Setup(void);
void testSLAUtility_Setup(void);
void testSLAByteArray_Setup(void);
void testSLAEncodeDecode_Setup(void);

void testWAV_Setup(void);
void testCommandLineParser_Setup(void);

/* テスト実行 */
int main(int argc, char **argv)
{
  int ret;

  Test_Initialize();

  testSLABitStream_Setup();
  testSLAPredictor_Setup();
  testSLACoder_Setup();
  testSLAEncoder_Setup();
  testSLADecoder_Setup();
  testSLAUtility_Setup();
  testSLAByteArray_Setup();
  testSLAEncodeDecode_Setup();

  testWAV_Setup();
  testCommandLineParser_Setup();

  ret = Test_RunAllTestSuite();

  Test_PrintAllFailures();

  Test_Finalize();

  return ret;
}
