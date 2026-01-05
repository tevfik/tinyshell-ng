#ifndef TINYSH_TEST_H
#define TINYSH_TEST_H

#include "tinysh.h"

/**
 * TinyShell Test Framework
 * ----------------------
 * Test suite for verifying TinyShell functionality.
 */

/**
 * Initialize the TinyShell test framework
 * Sets up all test commands and prepares testing environment
 */
void tinysh_test_init(void);

/**
 * Run all TinyShell tests automatically
 * Can be called from code or via "test run" command
 * 
 * @return Number of failed tests (0 = all passed)
 */
int tinysh_run_tests(void);

/**
 * Initialize output capture
 * Redirects tinysh_char_out to an internal buffer for verification
 */
void test_capture_start(void);

/**
 * Stop output capture and restore normal output
 */
void test_capture_stop(void);

/**
 * Get the captured output buffer
 */
const char* test_capture_get(void);

/**
 * Verify if captured output contains the expected string
 * 
 * @param expected String to search for
 * @return 1 if found, 0 if not
 */
int test_capture_contains(const char *expected);

/**
 * Clear the capture buffer
 */
void test_capture_clear(void);

/* Export test commands so they can be added manually if desired */
extern tinysh_cmd_t test_cmd;

#endif /* TINYSH_TEST_H */
