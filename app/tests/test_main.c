#include "unity/unity.h"
// #include "utils/test_files_utils.h"
// #include "utils/test_strings_utils.h"
// #include "utils/test_signals_utils.h"
// #include "utils/test_timers_utils.h"
// #include "utils/test_configs_utils.h"
#include "libs/prime/test_prime.h"
#include "libs/hashtable/test_hashtable.h"
#include "libs/inih/test_inih.h"


void setUp(void) {
    // set stuff up here
}

void tearDown(void) {
    // clean stuff up here
}

int main() {
    UNITY_BEGIN();

    // utils/signals
    // RUN_TEST(test_sigutil_setup_with_null_handler);
    // RUN_TEST(test_sigutil_handle);
    // RUN_TEST(test_sigutil_handle_all);
    // RUN_TEST(test_sigutil_block_unblock_one);
    // RUN_TEST(test_sigutil_block_unblock_all);
    // RUN_TEST(test_sigutil_wait);
    // RUN_TEST(test_sigutil_wait_any);
    // RUN_TEST(test_sigutil_set_and_restore_handler);

    // utils/files
    // RUN_TEST(test_non_existing_dir);
    // RUN_TEST(test_empty_dir);
    // RUN_TEST(test_app_dir);
    // RUN_TEST(test_docs_dir);

    // utils/helpers
    // RUN_TEST(test_parse_bool_true);
    // RUN_TEST(test_parse_bool_false);
    // RUN_TEST(test_parse_bool_invalid);
    // RUN_TEST(test_join_strings_empty_array);
    // RUN_TEST(test_join_strings_single_string);
    // RUN_TEST(test_join_strings_multiple_strings);
    // RUN_TEST(test_join_strings_no_separator);
    // RUN_TEST(test_filter_strings_empty_array);
    // RUN_TEST(test_filter_strings_no_match);
    // RUN_TEST(test_filter_strings_one_match);
    // RUN_TEST(test_filter_strings_multiple_matches);

    // utils/timers
    // RUN_TEST(test_one_shot_timer);
    // RUN_TEST(test_periodic_timer);
    // RUN_TEST(test_timer_stop);
    // RUN_TEST(test_timer_destroy);
    // RUN_TEST(test_timer_invalid_arguments);

    // utils/configs
    // RUN_TEST(test_get_configuration_value_valid_config);
    // RUN_TEST(test_get_configuration_value_nonexistent_config);
    // RUN_TEST(test_read_config);

    // prime
    RUN_TEST(test_is_prime);
    RUN_TEST(test_next_prime);

    // hashtable
    RUN_TEST(test_hash_table_create);
    RUN_TEST(test_hash_table_put_and_get);
    RUN_TEST(test_hash_table_resized);
    RUN_TEST(test_hash_table_update_existing_key);
    RUN_TEST(test_hash_table_contains_key);
    RUN_TEST(test_hash_table_remove);
    RUN_TEST(test_hash_table_keyset);
    RUN_TEST(test_hash_table_values);
    RUN_TEST(test_hash_table_clear);
    RUN_TEST(test_hash_table_load_factor);
    RUN_TEST(test_hash_table_replace);
    RUN_TEST(test_hash_table_map);
    RUN_TEST(test_hash_table_equals);
    RUN_TEST(test_hash_table_copy);
    RUN_TEST(test_hash_table_merge);

    // inih
    RUN_TEST(test_parse_valid_ini);
    RUN_TEST(test_parse_empty_file);
    RUN_TEST(test_parse_invalid_ini);

    return UNITY_END();
}
