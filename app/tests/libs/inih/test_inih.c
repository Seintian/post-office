#include "unity/unity.h"
#include "inih/ini.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Maximum number of key-value entries we expect to collect */
#define MAX_ENTRIES 32

/* Structure to store a parsed INI entry */
typedef struct {
    char section[64];
    char name[64];
    char value[128];
} entry_t;

/* Structure used to pass to the handler and accumulate entries */
typedef struct {
    entry_t entries[MAX_ENTRIES];
    int entry_count;
} test_data_t;

/*
 * The handler function that will be called by ini_parse().
 * It copies the section, name, and value strings into our test_data_t structure.
 */
static int ini_test_handler(void* user, const char* section, const char* name, const char* value)
{
    test_data_t* data = (test_data_t*) user;
    if (data->entry_count < MAX_ENTRIES) {
        // If section is NULL, we treat it as an empty string
        if (section) {
            // Copy section name, ensuring we do not overflow the buffer
            strncpy(data->entries[data->entry_count].section, section, sizeof(data->entries[data->entry_count].section) - 1);
        }
        else {
            return 0; // Don't accept empty sections
        }
        data->entries[data->entry_count].section[sizeof(data->entries[data->entry_count].section) - 1] = '\0';

        // If name is NULL, we treat it as an empty string
        if (name) {
            strncpy(data->entries[data->entry_count].name, name, sizeof(data->entries[data->entry_count].name) - 1);
        }
        else {
            return 0; // Don't accept empty names
        }
        data->entries[data->entry_count].name[sizeof(data->entries[data->entry_count].name) - 1] = '\0';

        // If value is NULL, we treat it as an empty string
        if (value) {
            strncpy(data->entries[data->entry_count].value, value, sizeof(data->entries[data->entry_count].value) - 1);
        }
        else {
            return 0; // Don't accept empty values
        }
        data->entries[data->entry_count].value[sizeof(data->entries[data->entry_count].value) - 1] = '\0';

        data->entry_count++;
        return 1;  // Continue parsing
    }
    return 0; // Stop parsing if too many entries
}

/*
 * Helper function to write INI content to a temporary file.
 */
static void write_temp_file(const char* filename, const char* content)
{
    FILE* fp = fopen(filename, "w");
    TEST_ASSERT_NOT_NULL(fp);
    fputs(content, fp);
    fclose(fp);
}

/*
 * Helper function to remove a temporary file.
 */
static void remove_temp_file(const char* filename)
{
    remove(filename);
}

/*
 * Test case: Parse a valid INI file.
 *
 * This INI file contains comments, two sections, and three key-value pairs.
 */
void test_parse_valid_ini(void) {
    const char* filename = "temp_valid.ini";
    const char* ini_content =
        "; This is a comment line\n"
        "[section1]\n"
        "key1=value1\n"
        "key2 = value2\n"
        "\n"
        "[section2]\n"
        "key3=value3\n";

    write_temp_file(filename, ini_content);

    test_data_t data;
    memset(&data, 0, sizeof(data));

    int result = ini_parse(filename, ini_test_handler, &data);
    TEST_ASSERT_EQUAL_INT(0, result);  // ini_parse returns 0 on success
    TEST_ASSERT_EQUAL_INT(3, data.entry_count);

    /* Verify that the parsed entries match what we expect */
    TEST_ASSERT_EQUAL_STRING("section1", data.entries[0].section);
    TEST_ASSERT_EQUAL_STRING("key1", data.entries[0].name);
    TEST_ASSERT_EQUAL_STRING("value1", data.entries[0].value);

    TEST_ASSERT_EQUAL_STRING("section1", data.entries[1].section);
    TEST_ASSERT_EQUAL_STRING("key2", data.entries[1].name);
    TEST_ASSERT_EQUAL_STRING("value2", data.entries[1].value);

    TEST_ASSERT_EQUAL_STRING("section2", data.entries[2].section);
    TEST_ASSERT_EQUAL_STRING("key3", data.entries[2].name);
    TEST_ASSERT_EQUAL_STRING("value3", data.entries[2].value);

    remove_temp_file(filename);
}

/*
 * Test case: Parse an empty INI file.
 *
 * This verifies that an empty file is handled gracefully.
 */
void test_parse_empty_file(void) {
    const char* filename = "temp_empty.ini";
    const char* ini_content = "";
    write_temp_file(filename, ini_content);

    test_data_t data;
    memset(&data, 0, sizeof(data));

    int result = ini_parse(filename, ini_test_handler, &data);
    TEST_ASSERT_EQUAL_INT(0, result);   // An empty file should be valid
    TEST_ASSERT_EQUAL_INT(0, data.entry_count);

    remove_temp_file(filename);
}

/*
 * Test case: Parse an invalid INI file.
 *
 * This file contains a malformed line (missing an '=' sign), which should trigger an error.
 * According to the INI parser's behavior, ini_parse() should return a non-zero value indicating the error line.
 */
void test_parse_invalid_ini(void) {
    const char* filename = "temp_invalid.ini";
    const char* ini_content =
        "[section]\n"
        "invalid_line_without_equal_sign\n"
        "key=value\n";
    write_temp_file(filename, ini_content);

    test_data_t data;
    memset(&data, 0, sizeof(data));

    int result = ini_parse(filename, ini_test_handler, &data);
    /* 
     * Expect a parse error.
     * The ini_parse() function from inih returns the line number where the error occurred
     * (which is > 0) if the file contains an error.
     * Also, if it encounters an error, it should stop the parsing process where it occurred.
     */
    TEST_ASSERT_TRUE(result > 0);
    TEST_ASSERT_EQUAL_INT(0, data.entry_count);

    remove_temp_file(filename);
}
