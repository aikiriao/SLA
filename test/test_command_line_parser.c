#include <stdlib.h>
#include <string.h>
#include "test.h"

/* テスト対象のモジュール */
#include "../command_line_parser.c"

int testCommandLineParser_Initialize(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);
  return 0;
}

int testCommandLineParser_Finalize(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);
  return 0;
}

/* ショートオプションの取得テスト */
static void testCommandLineParser_GetShortOptionTest(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

  /* 簡単な成功例 */
  {
#define INPUT_FILE_NAME "inputfile"
    static const struct CommandLineParserSpecification specs[] = {
      { 'i', NULL,  COMMAND_LINE_PARSER_TRUE,  "input file", NULL, COMMAND_LINE_PARSER_FALSE },
      { 'p', NULL, COMMAND_LINE_PARSER_FALSE, "output file", NULL, COMMAND_LINE_PARSER_FALSE },
      { 0, }
    };
    struct CommandLineParserSpecification get_specs[sizeof(specs) / sizeof(specs[0])];
    char* test_argv1[] = { "progname", "-i", INPUT_FILE_NAME, "-p" };
    char* test_argv2[] = { "progname", "-p", "-i", INPUT_FILE_NAME };
    char* test_argv3[] = { "progname", "-pi", INPUT_FILE_NAME };

    /* パースしてみる */
    get_specs[0] = specs[0]; get_specs[1] = specs[1];
    Test_AssertEqual(
        CommandLineParser_ParseArguments(
          get_specs,
          sizeof(test_argv1) / sizeof(test_argv1[0]), test_argv1,
          NULL, 0),
        COMMAND_LINE_PARSER_RESULT_OK);

    /* 正しく取得できたかチェック */
    Test_AssertEqual(get_specs[0].acquired, COMMAND_LINE_PARSER_TRUE);
    Test_AssertCondition(get_specs[0].argument_string != NULL);
    if (get_specs[0].argument_string != NULL) {
      Test_AssertEqual(strcmp(get_specs[0].argument_string, INPUT_FILE_NAME), 0);
    }
    Test_AssertEqual(get_specs[1].acquired, COMMAND_LINE_PARSER_TRUE);

    /* 引数順番を変えたものをパースしてみる */
    get_specs[0] = specs[0]; get_specs[1] = specs[1];
    Test_AssertEqual(
        CommandLineParser_ParseArguments(
          get_specs,
          sizeof(test_argv2) / sizeof(test_argv2[0]), test_argv2,
          NULL, 0),
        COMMAND_LINE_PARSER_RESULT_OK);

    /* 正しく取得できたかチェック */
    Test_AssertEqual(get_specs[0].acquired, COMMAND_LINE_PARSER_TRUE);
    Test_AssertCondition(get_specs[0].argument_string != NULL);
    if (get_specs[0].argument_string != NULL) {
      Test_AssertEqual(strcmp(get_specs[0].argument_string, INPUT_FILE_NAME), 0);
    }
    Test_AssertEqual(get_specs[1].acquired, COMMAND_LINE_PARSER_TRUE);

    /* ショートオプションの連なりを含むものをパースしてみる */
    get_specs[0] = specs[0]; get_specs[1] = specs[1];
    Test_AssertEqual(
        CommandLineParser_ParseArguments(
          get_specs,
          sizeof(test_argv3) / sizeof(test_argv3[0]), test_argv3,
          NULL, 0),
        COMMAND_LINE_PARSER_RESULT_OK);

    /* 正しく取得できたかチェック */
    Test_AssertEqual(get_specs[0].acquired, COMMAND_LINE_PARSER_TRUE);
    Test_AssertCondition(get_specs[0].argument_string != NULL);
    if (get_specs[0].argument_string != NULL) {
      Test_AssertEqual(strcmp(get_specs[0].argument_string, INPUT_FILE_NAME), 0);
    }
    Test_AssertEqual(get_specs[1].acquired, COMMAND_LINE_PARSER_TRUE);
#undef INPUT_FILE_NAME
  }

  /* 失敗系 */

  /* 引数が指定されずに末尾に達した */
  {
    struct CommandLineParserSpecification specs[] = {
      { 'i', NULL, COMMAND_LINE_PARSER_TRUE,  "input file", NULL, COMMAND_LINE_PARSER_FALSE },
      { 0, }
    };
    char* test_argv[] = { "progname", "-i" };

    Test_AssertEqual(
        CommandLineParser_ParseArguments(
          specs,
          sizeof(test_argv) / sizeof(test_argv[0]), test_argv,
          NULL, 0),
        COMMAND_LINE_PARSER_RESULT_NOT_SPECIFY_ARGUMENT_TO_OPTION);
  }

  /* 引数に他のオプションが指定されている */
  {
    struct CommandLineParserSpecification specs[] = {
      { 'i', NULL, COMMAND_LINE_PARSER_TRUE, "input file", NULL, COMMAND_LINE_PARSER_FALSE },
      { 0, }
    };
    char* test_argv1[] = { "progname", "-i", "-p" };
    char* test_argv2[] = { "progname", "-i", "--pripara" };

    Test_AssertEqual(
        CommandLineParser_ParseArguments(
          specs,
          sizeof(test_argv1) / sizeof(test_argv1[0]), test_argv1,
          NULL, 0),
        COMMAND_LINE_PARSER_RESULT_NOT_SPECIFY_ARGUMENT_TO_OPTION);

    Test_AssertEqual(
        CommandLineParser_ParseArguments(
          specs,
          sizeof(test_argv2) / sizeof(test_argv2[0]), test_argv2,
          NULL, 0),
        COMMAND_LINE_PARSER_RESULT_NOT_SPECIFY_ARGUMENT_TO_OPTION);
  }

  /* 仕様にないオプションが指定された */
  {
    struct CommandLineParserSpecification specs[] = {
      { 'i', NULL, COMMAND_LINE_PARSER_TRUE,  "input file", NULL, COMMAND_LINE_PARSER_FALSE },
      { 'p', NULL, COMMAND_LINE_PARSER_FALSE, "output file", NULL, COMMAND_LINE_PARSER_FALSE },
      { 0, }
    };
    char* test_argv[] = { "progname", "-i", "kiriya aoi", "-p", "-s" };

    Test_AssertEqual(
        CommandLineParser_ParseArguments(
          specs,
          sizeof(test_argv) / sizeof(test_argv[0]), test_argv,
          NULL, 0),
        COMMAND_LINE_PARSER_RESULT_UNKNOWN_OPTION);
  }

  /* 同じオプションが複数回指定されている ケース1 */
  {
    struct CommandLineParserSpecification specs[] = {
      { 'i', NULL, COMMAND_LINE_PARSER_TRUE,  "input file", NULL, COMMAND_LINE_PARSER_FALSE },
      { 0, }
    };
    char* test_argv[] = { "progname", "-i", "kiriya aoi", "-i", "shibuki ran" };

    Test_AssertEqual(
        CommandLineParser_ParseArguments(
          specs,
          sizeof(test_argv) / sizeof(test_argv[0]), test_argv,
          NULL, 0),
        COMMAND_LINE_PARSER_RESULT_OPTION_MULTIPLY_SPECIFIED);
  }

  /* 同じオプションが複数回指定されている ケース2 */
  {
    struct CommandLineParserSpecification specs[] = {
      { 'i', NULL,  COMMAND_LINE_PARSER_TRUE, "input file", NULL, COMMAND_LINE_PARSER_FALSE },
      { 'p', NULL, COMMAND_LINE_PARSER_FALSE, "prichan", NULL, COMMAND_LINE_PARSER_FALSE },
      { 0, }
    };
    char* test_argv[] = { "progname", "-p", "-i", "kiriya aoi", "-p" };

    Test_AssertEqual(
        CommandLineParser_ParseArguments(
          specs,
          sizeof(test_argv) / sizeof(test_argv[0]), test_argv,
          NULL, 0),
        COMMAND_LINE_PARSER_RESULT_OPTION_MULTIPLY_SPECIFIED);
  }

  /* ショートオプションの使い方が正しくない パート1 */
  {
    struct CommandLineParserSpecification specs[] = {
      { 'i', NULL, COMMAND_LINE_PARSER_TRUE,  "input file", NULL, COMMAND_LINE_PARSER_FALSE },
      { 'p', NULL, COMMAND_LINE_PARSER_FALSE, "prichan",    NULL, COMMAND_LINE_PARSER_FALSE },
      { 0, }
    };
    char* test_argv[] = { "progname", "-ip", "filename" };

    Test_AssertEqual(
        CommandLineParser_ParseArguments(
          specs,
          sizeof(test_argv) / sizeof(test_argv[0]), test_argv,
          NULL, 0),
        COMMAND_LINE_PARSER_RESULT_INVAILD_SHORT_OPTION_ARGUMENT);
  }

  /* ショートオプションの使い方が正しくない パート2 */
  {
    struct CommandLineParserSpecification specs[] = {
      { 'i', NULL, COMMAND_LINE_PARSER_TRUE, "input file", NULL, COMMAND_LINE_PARSER_FALSE },
      { 'p', NULL, COMMAND_LINE_PARSER_TRUE, "prichan",    NULL, COMMAND_LINE_PARSER_FALSE },
      { 0, }
    };
    char* test_argv[] = { "progname", "-ip", "filename" };

    Test_AssertEqual(
        CommandLineParser_ParseArguments(
          specs,
          sizeof(test_argv) / sizeof(test_argv[0]), test_argv,
          NULL, 0),
        COMMAND_LINE_PARSER_RESULT_INVAILD_SHORT_OPTION_ARGUMENT);
  }

}

/* ロングオプションの取得テスト */
static void testCommandLineParser_GetLongOptionTest(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

  /* 簡単な成功例 */
  {
#define INPUT_FILE_NAME "inputfile"
    struct CommandLineParserSpecification specs[] = {
      { 'i', "input",   COMMAND_LINE_PARSER_TRUE, "input file",   NULL, COMMAND_LINE_PARSER_FALSE },
      { 'a', "aikatsu", COMMAND_LINE_PARSER_FALSE, "aikatsu dakega boku no shinri datta",  NULL, COMMAND_LINE_PARSER_FALSE },
      { 0, }
    };
    char* test_argv[] = { "progname", "--input", INPUT_FILE_NAME, "--aikatsu" };

    /* パースしてみる */
    Test_AssertEqual(
        CommandLineParser_ParseArguments(
          specs,
          sizeof(test_argv) / sizeof(test_argv[0]), test_argv,
          NULL, 0),
        COMMAND_LINE_PARSER_RESULT_OK);

    /* 正しく取得できたかチェック */
    Test_AssertEqual(specs[0].acquired, COMMAND_LINE_PARSER_TRUE);
    Test_AssertCondition(specs[0].argument_string != NULL);
    if (specs[0].argument_string != NULL) {
      Test_AssertEqual(strcmp(specs[0].argument_string, INPUT_FILE_NAME), 0);
    }
    Test_AssertEqual(specs[1].acquired, COMMAND_LINE_PARSER_TRUE);

#undef INPUT_FILE_NAME
  }

  /* 簡単な成功例 パート2 */
  {
#define INPUT_FILE_NAME "inputfile"
    struct CommandLineParserSpecification specs[] = {
      { 'i', "input",   COMMAND_LINE_PARSER_TRUE, "input file",   NULL, COMMAND_LINE_PARSER_FALSE },
      { 'a', "aikatsu", COMMAND_LINE_PARSER_FALSE, "aikatsu dakega boku no shinri datta",  NULL, COMMAND_LINE_PARSER_FALSE },
      { 0, }
    };
    char* test_argv[] = { "progname", "--input=" INPUT_FILE_NAME, "--aikatsu" };

    /* パースしてみる */
    Test_AssertEqual(
        CommandLineParser_ParseArguments(
          specs,
          sizeof(test_argv) / sizeof(test_argv[0]), test_argv,
          NULL, 0),
        COMMAND_LINE_PARSER_RESULT_OK);

    /* 正しく取得できたかチェック */
    Test_AssertEqual(specs[0].acquired, COMMAND_LINE_PARSER_TRUE);
    Test_AssertCondition(specs[0].argument_string != NULL);
    if (specs[0].argument_string != NULL) {
      Test_AssertEqual(strcmp(specs[0].argument_string, INPUT_FILE_NAME), 0);
    }
    Test_AssertEqual(specs[1].acquired, COMMAND_LINE_PARSER_TRUE);

#undef INPUT_FILE_NAME
  }

  /* 失敗系 */

  /* 引数が指定されずに末尾に達した */
  {
    struct CommandLineParserSpecification specs[] = {
      { 'i', "input", COMMAND_LINE_PARSER_TRUE, "input file", NULL, COMMAND_LINE_PARSER_FALSE },
      { 0, }
    };
    char* test_argv[] = { "progname", "--input" };

    Test_AssertEqual(
        CommandLineParser_ParseArguments(
          specs,
          sizeof(test_argv) / sizeof(test_argv[0]), test_argv,
          NULL, 0),
        COMMAND_LINE_PARSER_RESULT_NOT_SPECIFY_ARGUMENT_TO_OPTION);
  }

  /* 引数に他のオプションが指定されている */
  {
    struct CommandLineParserSpecification specs[] = {
      { 'i', "input", COMMAND_LINE_PARSER_TRUE, "input file", NULL, COMMAND_LINE_PARSER_FALSE },
      { 0, }
    };
    char* test_argv1[] = { "progname", "--input", "-a" };
    char* test_argv2[] = { "progname", "--input", "--aikatsu" };

    Test_AssertEqual(
        CommandLineParser_ParseArguments(
          specs,
          sizeof(test_argv1) / sizeof(test_argv1[0]), test_argv1,
          NULL, 0),
        COMMAND_LINE_PARSER_RESULT_NOT_SPECIFY_ARGUMENT_TO_OPTION);

    Test_AssertEqual(
        CommandLineParser_ParseArguments(
          specs,
          sizeof(test_argv2) / sizeof(test_argv2[0]), test_argv2,
          NULL, 0),
        COMMAND_LINE_PARSER_RESULT_NOT_SPECIFY_ARGUMENT_TO_OPTION);
  }

  /* 仕様にないオプションが指定された */
  {
    struct CommandLineParserSpecification specs[] = {
      { 'i', "input",   COMMAND_LINE_PARSER_TRUE, "input file", NULL, COMMAND_LINE_PARSER_FALSE },
      { 'p', "aikatsu", COMMAND_LINE_PARSER_FALSE, "aikatsu mode", NULL, COMMAND_LINE_PARSER_FALSE },
      { 0, }
    };
    char* test_argv[] = { "progname", "--input", "kiriya aoi", "--aikatsu", "--unknown" };

    Test_AssertEqual(
        CommandLineParser_ParseArguments(
          specs,
          sizeof(test_argv) / sizeof(test_argv[0]), test_argv,
          NULL, 0),
        COMMAND_LINE_PARSER_RESULT_UNKNOWN_OPTION);
  }

  /* 同じオプションが複数回指定されている ケース1 */
  {
    struct CommandLineParserSpecification specs[] = {
      { 'i', "input", COMMAND_LINE_PARSER_TRUE, "input file", NULL, COMMAND_LINE_PARSER_FALSE },
      { 0, }
    };
    char* test_argv[] = { "progname", "--input", "kiriya aoi", "--input", "shibuki ran" };

    Test_AssertEqual(
        CommandLineParser_ParseArguments(
          specs,
          sizeof(test_argv) / sizeof(test_argv[0]), test_argv,
          NULL, 0),
        COMMAND_LINE_PARSER_RESULT_OPTION_MULTIPLY_SPECIFIED);
  }

  /* 同じオプションが複数回指定されている ケース2 */
  {
    struct CommandLineParserSpecification specs[] = {
      { 'i', "input", COMMAND_LINE_PARSER_TRUE, "input file", NULL, COMMAND_LINE_PARSER_FALSE },
      { 0, }
    };
    char* test_argv[] = { "progname", "--input=kiriya aoi", "--input=shibuki ran" };

    Test_AssertEqual(
        CommandLineParser_ParseArguments(
          specs,
          sizeof(test_argv) / sizeof(test_argv[0]), test_argv,
          NULL, 0),
        COMMAND_LINE_PARSER_RESULT_OPTION_MULTIPLY_SPECIFIED);
  }

  /* 同じオプションが複数回指定されている ケース3 */
  {
    struct CommandLineParserSpecification specs[] = {
      { 'i', "input", COMMAND_LINE_PARSER_TRUE, "input file", NULL, COMMAND_LINE_PARSER_FALSE },
      { 0, }
    };
    char* test_argv[] = { "progname", "--input=kiriya aoi", "--input", "shibuki ran" };

    Test_AssertEqual(
        CommandLineParser_ParseArguments(
          specs,
          sizeof(test_argv) / sizeof(test_argv[0]), test_argv,
          NULL, 0),
        COMMAND_LINE_PARSER_RESULT_OPTION_MULTIPLY_SPECIFIED);
  }
}

/* ロングオプションの取得テスト */
static void testCommandLineParser_GetOtherStringTest(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

  /* 簡単な成功例 */
  {
    struct CommandLineParserSpecification specs[] = {
      { 'i', "input", COMMAND_LINE_PARSER_TRUE, "input file", NULL, COMMAND_LINE_PARSER_FALSE },
      { 0, }
    };
    char* test_argv[] = { "progname", "Ichgo Hoshimiya", "Aoi Kiriya", "Ran Shibuki" };
    const char* other_string_array[3];

    Test_AssertEqual(
        CommandLineParser_ParseArguments(
          specs,
          sizeof(test_argv) / sizeof(test_argv[0]), test_argv,
          other_string_array, sizeof(other_string_array) / sizeof(other_string_array[0])),
        COMMAND_LINE_PARSER_RESULT_OK);

    /* 順番含め取れたか確認 */
    Test_AssertEqual(strcmp(other_string_array[0], test_argv[1]), 0);
    Test_AssertEqual(strcmp(other_string_array[1], test_argv[2]), 0);
    Test_AssertEqual(strcmp(other_string_array[2], test_argv[3]), 0);
  }

  /* オプションを混ぜてみる */
  {
    struct CommandLineParserSpecification specs[] = {
      { 'i', "input", COMMAND_LINE_PARSER_TRUE, "input file", NULL, COMMAND_LINE_PARSER_FALSE },
      { 0, }
    };
    char* test_argv[] = { "progname", "Ichgo Hoshimiya", "-i", "inputfile", "Aoi Kiriya", "Ran Shibuki" };
    const char* other_string_array[3];

    Test_AssertEqual(
        CommandLineParser_ParseArguments(
          specs,
          sizeof(test_argv) / sizeof(test_argv[0]), test_argv,
          other_string_array, sizeof(other_string_array) / sizeof(other_string_array[0])),
        COMMAND_LINE_PARSER_RESULT_OK);

    /* 順番含め取れたか確認 */
    Test_AssertEqual(strcmp(other_string_array[0], test_argv[1]), 0);
    Test_AssertEqual(strcmp(other_string_array[1], test_argv[4]), 0);
    Test_AssertEqual(strcmp(other_string_array[2], test_argv[5]), 0);
    Test_AssertEqual(strcmp(specs[0].argument_string, "inputfile"), 0);
  }

  /* 失敗系 */

  /* バッファサイズが足らない */
  {
    struct CommandLineParserSpecification specs[] = {
      { 'i', "input", COMMAND_LINE_PARSER_TRUE, "input file", NULL, COMMAND_LINE_PARSER_FALSE },
      { 0, }
    };
    char* test_argv[] = { "progname", "Ichgo Hoshimiya", "Aoi Kiriya", "Ran Shibuki" };
    const char* other_string_array[2];

    Test_AssertEqual(
        CommandLineParser_ParseArguments(
          specs,
          sizeof(test_argv) / sizeof(test_argv[0]), test_argv,
          other_string_array, sizeof(other_string_array) / sizeof(other_string_array[0])),
        COMMAND_LINE_PARSER_RESULT_INSUFFICIENT_OTHER_STRING_ARRAY_SIZE);
  }
}

static void testCommandLineParser_ParseVariousStringTest(void* obj)
{
  TEST_UNUSED_PARAMETER(obj);

  /* 失敗系 */

  /* パーサ仕様が不正 ケース1（ショートオプション重複） */
  {
    struct CommandLineParserSpecification specs[] = {
      { 'i', "input",   COMMAND_LINE_PARSER_TRUE, "input file",   NULL, COMMAND_LINE_PARSER_FALSE },
      { 'a', "aikatsu", COMMAND_LINE_PARSER_FALSE, "aikatsu dakega boku no shinri datta",  NULL, COMMAND_LINE_PARSER_FALSE },
      { 'i', NULL, COMMAND_LINE_PARSER_FALSE, "aikatsu dakega boku no shinri datta",  NULL, COMMAND_LINE_PARSER_FALSE },
      { 0, }
    };
    char* test_argv[] = { "progname", "--input", "inputfile", "--aikatsu" };

    Test_AssertEqual(
        CommandLineParser_ParseArguments(
          specs,
          sizeof(test_argv) / sizeof(test_argv[0]), test_argv,
          NULL, 0),
        COMMAND_LINE_PARSER_RESULT_INVALID_SPECIFICATION);
  }

  /* パーサ仕様が不正 ケース2（ロングオプション重複） */
  {
    struct CommandLineParserSpecification specs[] = {
      { 'i', "input",   COMMAND_LINE_PARSER_TRUE, "input file",   NULL, COMMAND_LINE_PARSER_FALSE },
      { 'a', "aikatsu", COMMAND_LINE_PARSER_FALSE, "aikatsu dakega boku no shinri datta",  NULL, COMMAND_LINE_PARSER_FALSE },
      { 'p', "input",   COMMAND_LINE_PARSER_FALSE, "aikatsu dakega boku no shinri datta",  NULL, COMMAND_LINE_PARSER_FALSE },
      { 0, }
    };
    char* test_argv[] = { "progname", "--input", "inputfile", "--aikatsu" };

    Test_AssertEqual(
        CommandLineParser_ParseArguments(
          specs,
          sizeof(test_argv) / sizeof(test_argv[0]), test_argv,
          NULL, 0),
        COMMAND_LINE_PARSER_RESULT_INVALID_SPECIFICATION);
  }
}

/* インデックス取得テスト */
static void testCommandLineParser_GetSpecificationIndexTest(void* obj)
{
  TEST_UNUSED_PARAMETER(obj);

  /* 簡単な成功例 */
  {
    uint32_t get_index, spec_no;
    const struct CommandLineParserSpecification specs[] = {
      { 'i', "input",   COMMAND_LINE_PARSER_TRUE,  "input file",   NULL, COMMAND_LINE_PARSER_FALSE },
      { 'a', "aikatsu", COMMAND_LINE_PARSER_FALSE, "aikatsu dakega boku no shinri datta",  NULL, COMMAND_LINE_PARSER_FALSE },
      { 'p', "prichan",   COMMAND_LINE_PARSER_FALSE, "aikatsu dakega boku no shinri datta",  NULL, COMMAND_LINE_PARSER_FALSE },
      { 0, }
    };
    uint32_t num_specs = CommandLineParser_GetNumSpecifications(specs);

    for (spec_no = 0; spec_no < num_specs; spec_no++) {
      char short_option_str[2];
      /* ショートオプションに対してテスト */
      short_option_str[0] = specs[spec_no].short_option;
      short_option_str[1] = '\0';
      Test_AssertEqual(
          CommandLineParser_GetSpecificationIndex(
          specs, short_option_str, &get_index),
          COMMAND_LINE_PARSER_RESULT_OK);
      Test_AssertEqual(get_index, spec_no);
      /* ロングオプションに対してテスト */
      Test_AssertEqual(
          CommandLineParser_GetSpecificationIndex(
          specs, specs[spec_no].long_option, &get_index),
          COMMAND_LINE_PARSER_RESULT_OK);
      Test_AssertEqual(get_index, spec_no);
    }
  }

  /* 失敗系 */

  /* 引数が不正 */
  {
    uint32_t get_index;
    const struct CommandLineParserSpecification specs[] = {
      { 'i', "input",   COMMAND_LINE_PARSER_TRUE,  "input file",   NULL, COMMAND_LINE_PARSER_FALSE },
      { 'a', "aikatsu", COMMAND_LINE_PARSER_FALSE, "aikatsu dakega boku no shinri datta",  NULL, COMMAND_LINE_PARSER_FALSE },
      { 'p', "prichan",   COMMAND_LINE_PARSER_FALSE, "aikatsu dakega boku no shinri datta",  NULL, COMMAND_LINE_PARSER_FALSE },
      { 0, }
    };

    Test_AssertEqual(
        CommandLineParser_GetSpecificationIndex(
          NULL, "aikatsu", &get_index),
        COMMAND_LINE_PARSER_RESULT_INVALID_ARGUMENT);
    Test_AssertEqual(
        CommandLineParser_GetSpecificationIndex(
          specs, NULL, &get_index),
        COMMAND_LINE_PARSER_RESULT_INVALID_ARGUMENT);
    Test_AssertEqual(
        CommandLineParser_GetSpecificationIndex(
          specs, "aikatsu", NULL),
        COMMAND_LINE_PARSER_RESULT_INVALID_ARGUMENT);
  }

  /* 存在しないオプションのインデックスを問い合わせる */
  {
    uint32_t get_index;
    const struct CommandLineParserSpecification specs[] = {
      { 'i', "input",   COMMAND_LINE_PARSER_TRUE,  "input file",   NULL, COMMAND_LINE_PARSER_FALSE },
      { 'a', "aikatsu", COMMAND_LINE_PARSER_FALSE, "aikatsu dakega boku no shinri datta",  NULL, COMMAND_LINE_PARSER_FALSE },
      { 'p', "prichan",   COMMAND_LINE_PARSER_FALSE, "aikatsu dakega boku no shinri datta",  NULL, COMMAND_LINE_PARSER_FALSE },
      { 0, }
    };

    Test_AssertEqual(
        CommandLineParser_GetSpecificationIndex(
          specs, "aikats", &get_index),
        COMMAND_LINE_PARSER_RESULT_UNKNOWN_OPTION);
    Test_AssertEqual(
        CommandLineParser_GetSpecificationIndex(
          specs, "moegi emo", &get_index),
        COMMAND_LINE_PARSER_RESULT_UNKNOWN_OPTION);
    Test_AssertEqual(
        CommandLineParser_GetSpecificationIndex(
          specs, "mirai momoyama", &get_index),
        COMMAND_LINE_PARSER_RESULT_UNKNOWN_OPTION);
    Test_AssertEqual(
        CommandLineParser_GetSpecificationIndex(
          specs, "s", &get_index),
        COMMAND_LINE_PARSER_RESULT_UNKNOWN_OPTION);
    Test_AssertEqual(
        CommandLineParser_GetSpecificationIndex(
          specs, "b", &get_index),
        COMMAND_LINE_PARSER_RESULT_UNKNOWN_OPTION);
  }
}

void testCommandLineParser_Setup(void)
{
  struct TestSuite *suite
    = Test_AddTestSuite("CommandLineParser Test Suite",
        NULL, testCommandLineParser_Initialize, testCommandLineParser_Finalize);

  Test_AddTest(suite, testCommandLineParser_GetShortOptionTest);
  Test_AddTest(suite, testCommandLineParser_GetLongOptionTest);
  Test_AddTest(suite, testCommandLineParser_GetOtherStringTest);
  Test_AddTest(suite, testCommandLineParser_GetSpecificationIndexTest);
  Test_AddTest(suite, testCommandLineParser_ParseVariousStringTest);
}
