#ifndef _TEST_STRING_UTILS_
#define _TEST_STRING_UTILS_

void test_parse_bool_true(void);
void test_parse_bool_false(void);
void test_parse_bool_invalid(void);

void test_join_strings_empty_array(void);
void test_join_strings_single_string(void);
void test_join_strings_multiple_strings(void);
void test_join_strings_no_separator(void);

void test_filter_strings_empty_array(void);
void test_filter_strings_no_match(void);
void test_filter_strings_one_match(void);
void test_filter_strings_multiple_matches(void);

#endif // _TEST_STRING_UTILS_ 