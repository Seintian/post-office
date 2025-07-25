#ifndef _TEST_SIGNALS_UTILS_H
#define _TEST_SIGNALS_UTILS_H

void test_sigutil_setup_with_null_handler(void);

void test_sigutil_handle(void);
void test_sigutil_handle_all(void);

void test_sigutil_block_unblock_one(void);
void test_sigutil_block_unblock_all(void);

void test_sigutil_wait(void);
void test_sigutil_wait_any(void);

void test_sigutil_set_and_restore_handler(void);

#endif // _TEST_SIGNALS_UTILS_H