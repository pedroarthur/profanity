#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdlib.h>

#include "tools/parser.h"

void
parse_null_returns_null(void **state)
{
    char *inp = NULL;
    gchar **result = parse_args(inp, 1, 2);

    assert_null(result);
    g_strfreev(result);
}

void
parse_empty_returns_null(void **state)
{
    char *inp = "";
    gchar **result = parse_args(inp, 1, 2);

    assert_null(result);
    g_strfreev(result);
}

void
parse_space_returns_null(void **state)
{
    char *inp = "   ";
    gchar **result = parse_args(inp, 1, 2);

    assert_null(result);
    g_strfreev(result);
}

void
parse_cmd_no_args_returns_null(void **state)
{
    char *inp = "/cmd";
    gchar **result = parse_args(inp, 1, 2);

    assert_null(result);
    g_strfreev(result);
}

void
parse_cmd_with_space_returns_null(void **state)
{
    char *inp = "/cmd   ";
    gchar **result = parse_args(inp, 1, 2);

    assert_null(result);
    g_strfreev(result);
}

void
parse_cmd_with_too_few_returns_null(void **state)
{
    char *inp = "/cmd arg1";
    gchar **result = parse_args(inp, 2, 3);

    assert_null(result);
    g_strfreev(result);
}

void
parse_cmd_with_too_many_returns_null(void **state)
{
    char *inp = "/cmd arg1 arg2 arg3 arg4";
    gchar **result = parse_args(inp, 1, 3);

    assert_null(result);
    g_strfreev(result);
}

void
parse_cmd_one_arg(void **state)
{
    char *inp = "/cmd arg1";
    gchar **result = parse_args(inp, 1, 2);

    assert_int_equal(1, g_strv_length(result));
    assert_string_equal("arg1", result[0]);
    g_strfreev(result);
}

void
parse_cmd_two_args(void **state)
{
    char *inp = "/cmd arg1 arg2";
    gchar **result = parse_args(inp, 1, 2);

    assert_int_equal(2, g_strv_length(result));
    assert_string_equal("arg1", result[0]);
    assert_string_equal("arg2", result[1]);
    g_strfreev(result);
}

void
parse_cmd_three_args(void **state)
{
    char *inp = "/cmd arg1 arg2 arg3";
    gchar **result = parse_args(inp, 3, 3);

    assert_int_equal(3, g_strv_length(result));
    assert_string_equal("arg1", result[0]);
    assert_string_equal("arg2", result[1]);
    assert_string_equal("arg3", result[2]);
    g_strfreev(result);
}

void
parse_cmd_three_args_with_spaces(void **state)
{
    char *inp = "  /cmd    arg1  arg2     arg3 ";
    gchar **result = parse_args(inp, 3, 3);

    assert_int_equal(3, g_strv_length(result));
    assert_string_equal("arg1", result[0]);
    assert_string_equal("arg2", result[1]);
    assert_string_equal("arg3", result[2]);
    g_strfreev(result);
}

void
parse_cmd_with_freetext(void **state)
{
    char *inp = "/cmd this is some free text";
    gchar **result = parse_args_with_freetext(inp, 1, 1);

    assert_int_equal(1, g_strv_length(result));
    assert_string_equal("this is some free text", result[0]);
    g_strfreev(result);
}

void
parse_cmd_one_arg_with_freetext(void **state)
{
    char *inp = "/cmd arg1 this is some free text";
    gchar **result = parse_args_with_freetext(inp, 1, 2);

    assert_int_equal(2, g_strv_length(result));
    assert_string_equal("arg1", result[0]);
    assert_string_equal("this is some free text", result[1]);
    g_strfreev(result);
}

void
parse_cmd_two_args_with_freetext(void **state)
{
    char *inp = "/cmd arg1 arg2 this is some free text";
    gchar **result = parse_args_with_freetext(inp, 1, 3);

    assert_int_equal(3, g_strv_length(result));
    assert_string_equal("arg1", result[0]);
    assert_string_equal("arg2", result[1]);
    assert_string_equal("this is some free text", result[2]);
    g_strfreev(result);
}

void
parse_cmd_min_zero(void **state)
{
    char *inp = "/cmd";
    gchar **result = parse_args(inp, 0, 2);

    assert_int_equal(0, g_strv_length(result));
    assert_null(result[0]);
    g_strfreev(result);
}

void
parse_cmd_min_zero_with_freetext(void **state)
{
    char *inp = "/cmd";
    gchar **result = parse_args_with_freetext(inp, 0, 2);

    assert_int_equal(0, g_strv_length(result));
    assert_null(result[0]);
    g_strfreev(result);
}

void
parse_cmd_with_quoted(void **state)
{
    char *inp = "/cmd \"arg1\" arg2";
    gchar **result = parse_args(inp, 2, 2);

    assert_int_equal(2, g_strv_length(result));
    assert_string_equal("arg1", result[0]);
    assert_string_equal("arg2", result[1]);
    g_strfreev(result);
}

void
parse_cmd_with_quoted_and_space(void **state)
{
    char *inp = "/cmd \"the arg1\" arg2";
    gchar **result = parse_args(inp, 2, 2);

    assert_int_equal(2, g_strv_length(result));
    assert_string_equal("the arg1", result[0]);
    assert_string_equal("arg2", result[1]);
    g_strfreev(result);
}

void
parse_cmd_with_quoted_and_many_spaces(void **state)
{
    char *inp = "/cmd \"the arg1 is here\" arg2";
    gchar **result = parse_args(inp, 2, 2);

    assert_int_equal(2, g_strv_length(result));
    assert_string_equal("the arg1 is here", result[0]);
    assert_string_equal("arg2", result[1]);
    g_strfreev(result);
}

void
parse_cmd_with_many_quoted_and_many_spaces(void **state)
{
    char *inp = "/cmd \"the arg1 is here\" \"and arg2 is right here\"";
    gchar **result = parse_args(inp, 2, 2);

    assert_int_equal(2, g_strv_length(result));
    assert_string_equal("the arg1 is here", result[0]);
    assert_string_equal("and arg2 is right here", result[1]);
    g_strfreev(result);
}

void
parse_cmd_freetext_with_quoted(void **state)
{
    char *inp = "/cmd \"arg1\" arg2 hello there whats up";
    gchar **result = parse_args_with_freetext(inp, 3, 3);

    assert_int_equal(3, g_strv_length(result));
    assert_string_equal("arg1", result[0]);
    assert_string_equal("arg2", result[1]);
    assert_string_equal("hello there whats up", result[2]);
    g_strfreev(result);
}

void
parse_cmd_freetext_with_quoted_and_space(void **state)
{
    char *inp = "/cmd \"the arg1\" arg2 another bit of freetext";
    gchar **result = parse_args_with_freetext(inp, 3, 3);

    assert_int_equal(3, g_strv_length(result));
    assert_string_equal("the arg1", result[0]);
    assert_string_equal("arg2", result[1]);
    assert_string_equal("another bit of freetext", result[2]);
    g_strfreev(result);
}

void
parse_cmd_freetext_with_quoted_and_many_spaces(void **state)
{
    char *inp = "/cmd \"the arg1 is here\" arg2 some more freetext";
    gchar **result = parse_args_with_freetext(inp, 3, 3);

    assert_int_equal(3, g_strv_length(result));
    assert_string_equal("the arg1 is here", result[0]);
    assert_string_equal("arg2", result[1]);
    assert_string_equal("some more freetext", result[2]);
    g_strfreev(result);
}

void
parse_cmd_freetext_with_many_quoted_and_many_spaces(void **state)
{
    char *inp = "/cmd \"the arg1 is here\" \"and arg2 is right here\" and heres the free text";
    gchar **result = parse_args_with_freetext(inp, 3, 3);

    assert_int_equal(3, g_strv_length(result));
    assert_string_equal("the arg1 is here", result[0]);
    assert_string_equal("and arg2 is right here", result[1]);
    assert_string_equal("and heres the free text", result[2]);
    g_strfreev(result);
}

void
parse_cmd_with_quoted_freetext(void **state)
{
    char *inp = "/cmd arg1 here is \"some\" quoted freetext";
    gchar **result = parse_args_with_freetext(inp, 1, 2);

    assert_int_equal(2, g_strv_length(result));
    assert_string_equal("arg1", result[0]);
    assert_string_equal("here is \"some\" quoted freetext", result[1]);
    g_strfreev(result);
}

void
parse_cmd_with_third_arg_quoted_0_min_3_max(void **state)
{
    char *inp = "/group add friends \"The User\"";
    gchar **result = parse_args_with_freetext(inp, 0, 3);

    assert_int_equal(3, g_strv_length(result));
    assert_string_equal("add", result[0]);
    assert_string_equal("friends", result[1]);
    assert_string_equal("The User", result[2]);
}

void
parse_cmd_with_second_arg_quoted_0_min_3_max(void **state)
{
    char *inp = "/group add \"The Group\" friend";
    gchar **result = parse_args_with_freetext(inp, 0, 3);

    assert_int_equal(3, g_strv_length(result));
    assert_string_equal("add", result[0]);
    assert_string_equal("The Group", result[1]);
    assert_string_equal("friend", result[2]);
}

void
parse_cmd_with_second_and_third_arg_quoted_0_min_3_max(void **state)
{
    char *inp = "/group add \"The Group\" \"The User\"";
    gchar **result = parse_args_with_freetext(inp, 0, 3);

    assert_int_equal(3, g_strv_length(result));
    assert_string_equal("add", result[0]);
    assert_string_equal("The Group", result[1]);
    assert_string_equal("The User", result[2]);
}

void
count_one_token(void **state)
{
    char *inp = "one";
    int result = count_tokens(inp);

    assert_int_equal(1, result);
}

void
count_one_token_quoted_no_whitespace(void **state)
{
    char *inp = "\"one\"";
    int result = count_tokens(inp);

    assert_int_equal(1, result);
}

void
count_one_token_quoted_with_whitespace(void **state)
{
    char *inp = "\"one two\"";
    int result = count_tokens(inp);

    assert_int_equal(1, result);
}

void
count_two_tokens(void **state)
{
    char *inp = "one two";
    int result = count_tokens(inp);

    assert_int_equal(2, result);
}

void
count_two_tokens_first_quoted(void **state)
{
    char *inp = "\"one and\" two";
    int result = count_tokens(inp);

    assert_int_equal(2, result);
}

void
count_two_tokens_second_quoted(void **state)
{
    char *inp = "one \"two and\"";
    int result = count_tokens(inp);

    assert_int_equal(2, result);
}

void
count_two_tokens_both_quoted(void **state)
{
    char *inp = "\"one and then\" \"two and\"";
    int result = count_tokens(inp);

    assert_int_equal(2, result);
}

void
get_first_of_one(void **state)
{
    char *inp = "one";
    char *result = get_start(inp, 2);

    assert_string_equal("one", result);
}

void
get_first_of_two(void **state)
{
    char *inp = "one two";
    char *result = get_start(inp, 2);

    assert_string_equal("one ", result);
}

void
get_first_two_of_three(void **state)
{
    char *inp = "one two three";
    char *result = get_start(inp, 3);

    assert_string_equal("one two ", result);
}

void
get_first_two_of_three_first_quoted(void **state)
{
    char *inp = "\"one\" two three";
    char *result = get_start(inp, 3);

    assert_string_equal("\"one\" two ", result);
}

void
get_first_two_of_three_second_quoted(void **state)
{
    char *inp = "one \"two\" three";
    char *result = get_start(inp, 3);

    assert_string_equal("one \"two\" ", result);
}

void
get_first_two_of_three_first_and_second_quoted(void **state)
{
    char *inp = "\"one\" \"two\" three";
    char *result = get_start(inp, 3);

    assert_string_equal("\"one\" \"two\" ", result);
}
