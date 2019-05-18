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

/* 連立一次方程式ソルバーのテスト */
static void testSLAUtility_SolveLinearEquationsTest(void *obj)
{
  /* テストケース */
  struct LESolverTestCase {
    uint32_t      dim;                      /* 行列とベクトルの次元 */
    const double* coef_matrix;              /* 係数行列 */
    const double* const_vector;             /* 右辺の定数ベクトル */
    const double* reference_answer_vector;  /* リファレンス解ベクトル */
  };

  TEST_UNUSED_PARAMETER(obj);

  Test_SetFloat32Epsilon(1.0e-8);

  /* 簡単な計算例: 係数行列として単位行列を使用 */
  {
#define MAX_DIM 5
    struct SLALESolver* lesolver;
    int32_t ret;
    static const double identity_matrix[MAX_DIM][MAX_DIM] = {
      { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f }, 
      { 0.0f, 1.0f, 0.0f, 0.0f, 0.0f }, 
      { 0.0f, 0.0f, 1.0f, 0.0f, 0.0f }, 
      { 0.0f, 0.0f, 0.0f, 1.0f, 0.0f }, 
      { 0.0f, 0.0f, 0.0f, 0.0f, 1.0f }
    };
    const double *identity_matrix_p[MAX_DIM] = {
      &identity_matrix[0][0],
      &identity_matrix[1][0],
      &identity_matrix[2][0],
      &identity_matrix[3][0],
      &identity_matrix[4][0],
    };
    double answer_vector[MAX_DIM];

    lesolver = SLALESolver_Create(MAX_DIM);

    /* 1.0 x = 1.0 を解いてみる (解: x = 1.0f) */
    answer_vector[0] = 1.0f;
    ret = SLALESolver_Solve(
        lesolver, identity_matrix_p, answer_vector, 1, 0);
    Test_AssertEqual(ret, 0);
    Test_AssertFloat32EpsilonEqual(answer_vector[0], 1.0f);

    /* 1.0 x = 10.0 を解いてみる (解: x = 10.0f) */
    answer_vector[0] = 10.0f;
    ret = SLALESolver_Solve(
        lesolver, identity_matrix_p, answer_vector, 1, 0);
    Test_AssertEqual(ret, 0);
    Test_AssertFloat32EpsilonEqual(answer_vector[0], 10.0f);

    /* 1.0 x1 = 1.0, 1.0 x2 = 1.0 を解いてみる (解: x1 = 1.0, x2 = 1.0) */
    answer_vector[0] = 1.0f;
    answer_vector[1] = 1.0f;
    ret = SLALESolver_Solve(
        lesolver, identity_matrix_p, answer_vector, 2, 0);
    Test_AssertEqual(ret, 0);
    Test_AssertFloat32EpsilonEqual(answer_vector[0], 1.0f);
    Test_AssertFloat32EpsilonEqual(answer_vector[1], 1.0f);

    /* 1.0 x1 = 10.0, 1.0 x2 = 10.0 を解いてみる (解: x1 = 10.0, x2 = 10.0) */
    answer_vector[0] = 10.0f;
    answer_vector[1] = 10.0f;
    ret = SLALESolver_Solve(
        lesolver, identity_matrix_p, answer_vector, 2, 0);
    Test_AssertEqual(ret, 0);
    Test_AssertFloat32EpsilonEqual(answer_vector[0], 10.0f);
    Test_AssertFloat32EpsilonEqual(answer_vector[1], 10.0f);

    /* 1.0 x1 = 1.0, 1.0 x2 = 10.0 を解いてみる (解: x1 = 1.0, x2 = 10.0) */
    answer_vector[0] = 1.0f;
    answer_vector[1] = 10.0f;
    ret = SLALESolver_Solve(
        lesolver, identity_matrix_p, answer_vector, 2, 0);
    Test_AssertEqual(ret, 0);
    Test_AssertFloat32EpsilonEqual(answer_vector[0], 1.0f);
    Test_AssertFloat32EpsilonEqual(answer_vector[1], 10.0f);

    /* 1.0 x1 = 10.0, 1.0 x2 = 1.0 を解いてみる (解: x1 = 10.0, x2 = 1.0) */
    answer_vector[0] = 10.0f;
    answer_vector[1] = 1.0f;
    ret = SLALESolver_Solve(
        lesolver, identity_matrix_p, answer_vector, 2, 0);
    Test_AssertEqual(ret, 0);
    Test_AssertFloat32EpsilonEqual(answer_vector[0], 10.0f);
    Test_AssertFloat32EpsilonEqual(answer_vector[1], 1.0f);

    /* 5次のケース */
    answer_vector[0] = 10.0f; answer_vector[1] = 10.0f;
    answer_vector[2] = 10.0f; answer_vector[3] = 10.0f;
    answer_vector[4] = 10.0f;
    ret = SLALESolver_Solve(
        lesolver, identity_matrix_p, answer_vector, 5, 0);
    Test_AssertEqual(ret, 0);
    Test_AssertFloat32EpsilonEqual(answer_vector[0], 10.0f);
    Test_AssertFloat32EpsilonEqual(answer_vector[1], 10.0f);
    Test_AssertFloat32EpsilonEqual(answer_vector[2], 10.0f);
    Test_AssertFloat32EpsilonEqual(answer_vector[3], 10.0f);
    Test_AssertFloat32EpsilonEqual(answer_vector[4], 10.0f);

    SLALESolver_Destroy(lesolver);
#undef MAX_DIM
  }

  /* 特異行列に対する確認 */
  {
#define MAX_DIM 5
    struct SLALESolver* lesolver;
    int32_t ret;
    uint32_t dim, test_no;
    static const double singular_matrix1[MAX_DIM * MAX_DIM] = {
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f,  
      0.0f, 1.0f, 0.0f, 0.0f, 0.0f,  
      0.0f, 0.0f, 1.0f, 0.0f, 0.0f,  
      0.0f, 0.0f, 0.0f, 1.0f, 0.0f,  
      0.0f, 0.0f, 0.0f, 0.0f, 1.0f
    };
    static const double singular_matrix2[MAX_DIM * MAX_DIM] = {
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f,  
      0.0f, 1.0f, 0.0f, 0.0f, 0.0f,  
      0.0f, 0.0f, 1.0f, 0.0f, 0.0f,  
      0.0f, 0.0f, 0.0f, 1.0f, 0.0f,  
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f
    };
    static const double singular_matrix3[MAX_DIM * MAX_DIM] = {
      1.0f, 0.0f, 0.0f, 0.0f, 0.0f,  
      0.0f, 1.0f, 0.0f, 0.0f, 0.0f,  
      0.0f, 0.0f, 1.0f, 0.0f, 0.0f,  
      0.0f, 0.0f, 0.0f, 1.0f, 0.0f,  
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f
    };
    static const double singular_matrix4[MAX_DIM * MAX_DIM] = {
      1.0f, 0.0f, 0.0f, 0.0f, 0.0f,  
      0.0f, 1.0f, 0.0f, 0.0f, 0.0f,  
      0.0f, 0.0f, 1.0f, 0.0f, 0.0f,  
      0.0f, 0.0f, 0.0f, 1.0f, 0.0f,  
      1.0f, 0.0f, 0.0f, 0.0f, 0.0f
    };
    static const double singular_matrix5[MAX_DIM * MAX_DIM] = {
      1.0f, 0.0f, 0.0f, 0.0f, 0.0f,  
      1.0f, 0.0f, 0.0f, 0.0f, 0.0f,  
      1.0f, 0.0f, 0.0f, 0.0f, 0.0f,  
      1.0f, 0.0f, 0.0f, 0.0f, 0.0f,  
      1.0f, 0.0f, 0.0f, 0.0f, 0.0f
    };
    static const double singular_matrix6[MAX_DIM * MAX_DIM] = {
      1.0f, 1.0f, 1.0f, 1.0f, 1.0f,  
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f,  
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f,  
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f,  
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f
    };
    static const double singular_matrix7[4 * 4] = {
       2.0f,  1.0f,  3.0f,  4.0f,
       2.0f, -3.0f, -1.0f, -4.0f,
       1.0f, -2.0f, -1.0f, -3.0f,
      -1.0f,  2.0f,  1.0f,  3.0f
    };
    static const double const_vector[MAX_DIM] = {
      1.0f, 1.0f, 1.0f, 1.0f, 1.0f
    };

    /* テストケースリスト */
    static const struct LESolverTestCase singular_test_case[] = {
      { MAX_DIM, singular_matrix1, const_vector, NULL },
      { MAX_DIM, singular_matrix2, const_vector, NULL },
      { MAX_DIM, singular_matrix3, const_vector, NULL },
      { MAX_DIM, singular_matrix4, const_vector, NULL },
      { MAX_DIM, singular_matrix5, const_vector, NULL },
      { MAX_DIM, singular_matrix6, const_vector, NULL },
      {       4, singular_matrix7, const_vector, NULL }
    };
    const uint32_t num_test_case 
      = sizeof(singular_test_case) / sizeof(singular_test_case[0]);

    lesolver = SLALESolver_Create(MAX_DIM);

    /* 全テストケースに対して実行 */
    for (test_no = 0; test_no < num_test_case; test_no++) {
      const struct LESolverTestCase* test_p = &singular_test_case[test_no];
      const double *singular_matrix_p[MAX_DIM];
      double answer_vector[MAX_DIM];

      assert(test_p->dim <= MAX_DIM);

      /* テンポラリに情報をセット */
      for (dim = 0; dim < test_p->dim; dim++) {
        singular_matrix_p[dim] = &test_p->coef_matrix[dim * test_p->dim];
        answer_vector[dim]     = test_p->const_vector[dim];
      }

      /* 解いてみる...が失敗するはず */
      ret = SLALESolver_Solve(
          lesolver, singular_matrix_p, answer_vector, test_p->dim, 0);
      Test_AssertEqual(ret, -1);
    }

    SLALESolver_Destroy(lesolver);
#undef MAX_DIM
  }

  /* 解けるケースに対する確認 */
  {
#define MAX_DIM 5
    struct SLALESolver* lesolver;
    int32_t ret;
    uint32_t dim, test_no;

    /* テストケース1 */
    static const double coef_matrix1[MAX_DIM * MAX_DIM] = {
      1.0f, 0.0f, 0.0f, 0.0f, 0.0f,  
      0.0f, 1.0f, 0.0f, 0.0f, 0.0f,  
      0.0f, 0.0f, 1.0f, 0.0f, 0.0f,  
      0.0f, 0.0f, 0.0f, 1.0f, 0.0f,  
      0.0f, 0.0f, 0.0f, 0.0f, 1.0f
    };
    static const double const_vector1[MAX_DIM] = {
      1.0f, 1.0f, 1.0f, 1.0f, 1.0f
    };
    static const double ref_answer_vector1[MAX_DIM] = {
      1.0f, 1.0f, 1.0f, 1.0f, 1.0f
    };
    /* テストケース2 */
    static const double coef_matrix2[2 * 2] = {
       2.0f, -1.0f, 
      -1.0f,  2.0f
    };
    static const double const_vector2[2] = {
       2.0f,  5.0f
    };
    static const double ref_answer_vector2[2] = {
       3.0f,  4.0f
    };
    /* テストケース3 */
    static const double coef_matrix3[2 * 2] = {
      2.0f,  3.0f, 
      4.0f,  5.0f
    };
    static const double const_vector3[2] = {
      3.0f,  7.0f
    };
    static const double ref_answer_vector3[2] = {
      3.0f, -1.0f
    };
    /* テストケース4 */
    static const double coef_matrix4[3 * 3] = {
      1.0f,  1.0f,  1.0f,
      3.0f,  2.0f, -2.0f,
      2.0f, -1.0f,  3.0f,
    };
    static const double const_vector4[3] = {
      6.0f,  1.0f,  9.0f
    };
    static const double ref_answer_vector4[3] = {
      1.0f,  2.0f,  3.0f
    };
    /* テストケース5 */
    static const double coef_matrix5[4 * 4] = {
      2.0f,  1.0f,  3.0f,  4.0f,
      3.0f,  2.0f,  5.0f,  2.0f,
      3.0f,  4.0f,  1.0f, -1.0f,
     -1.0f, -3.0f,  1.0f,  3.0f
    };
    static const double const_vector5[4] = {
      2.0f, 12.0f,  4.0f, -1.0f
    };
    static const double ref_answer_vector5[4] = {
      1.0f, -1.0f,  3.0f, -2.0f
    };
    /* テストケース6 */
    static const double coef_matrix6[4 * 4] = {
      2.0f,  1.0f, -3.0f, -2.0f,
      2.0f, -1.0f, -1.0f,  3.0f,
      1.0f, -1.0f, -2.0f,  2.0f,
     -1.0f,  1.0f,  3.0f, -2.0f
    };
    static const double const_vector6[4] = {
     -4.0f,  1.0f, -3.0f,  5.0f
    };
    static const double ref_answer_vector6[4] = {
      1.0f,  2.0f,  2.0f,  1.0f
    };

    /* テストケースリスト */
    static const struct LESolverTestCase test_case[] = {
      { MAX_DIM, coef_matrix1, const_vector1, ref_answer_vector1 },
      {       2, coef_matrix2, const_vector2, ref_answer_vector2 },
      {       2, coef_matrix3, const_vector3, ref_answer_vector3 },
      {       3, coef_matrix4, const_vector4, ref_answer_vector4 },
      {       4, coef_matrix5, const_vector5, ref_answer_vector5 },
      {       4, coef_matrix6, const_vector6, ref_answer_vector6 }
    };
    const uint32_t num_test_case 
      = sizeof(test_case) / sizeof(test_case[0]);

    lesolver = SLALESolver_Create(MAX_DIM);

    /* 全テストケースに対して実行 */
    for (test_no = 0; test_no < num_test_case; test_no++) {
      const struct LESolverTestCase* test_p = &test_case[test_no];
      const double* coef_matrix_p[MAX_DIM];
      double        answer_vector[MAX_DIM];
      uint32_t      is_ok;

      assert(test_p->dim <= MAX_DIM);

      /* テンポラリに情報をセット */
      for (dim = 0; dim < test_p->dim; dim++) {
        coef_matrix_p[dim] = &test_p->coef_matrix[dim * test_p->dim];
        answer_vector[dim] = test_p->const_vector[dim];
      }

      /* 解く */
      ret = SLALESolver_Solve(
          lesolver, coef_matrix_p, answer_vector, test_p->dim, 1);
      Test_AssertEqual(ret, 0);

      /* リファレンス一致を確認 */
      is_ok = 1;
      for (dim = 0; dim < test_p->dim; dim++) {
        if (fabs(answer_vector[dim] - test_p->reference_answer_vector[dim]) > 1.0e-8) {
          is_ok = 0;
          break;
        }
      }
      Test_AssertEqual(is_ok, 1);
    }
  }

}

void testSLAUtility_Setup(void)
{
  struct TestSuite *suite
    = Test_AddTestSuite("SLA Utility Test Suite",
        NULL, testSLAUtility_Initialize, testSLAUtility_Finalize);

  Test_AddTest(suite, testSLAUtility_CalculateCRC16Test);
  Test_AddTest(suite, testSLAUtility_SolveLinearEquationsTest);
}
