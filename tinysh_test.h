#ifndef TINYSH_TEST_H
#define TINYSH_TEST_H

#include "tinysh.h"

/**
 * TinyShell Test Framework
 * ----------------------
 * Test suite for verifying TinyShell functionality. Tests cover:
 * - Command parsing and execution
 * - History functionality
 * - Tokenization and string handling
 * - Number conversion utilities
 * - Authentication system
 *
 * Usage:
 * 1. From running shell: type "test run" to execute all tests
 * 2. From command line: ./tinysh_shell -t to run tests and exit
 * 3. Individual test commands:
 *    - test parser
 *    - test history
 *    - test commands
 *    - test tokenize
 *    - test conversion
 *    - test auth
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

/* Export test commands so they can be added manually if desired */
extern tinysh_cmd_t test_cmd;
extern tinysh_cmd_t test_run_cmd;
extern tinysh_cmd_t test_parser_cmd;
extern tinysh_cmd_t test_history_cmd;
extern tinysh_cmd_t test_commands_cmd;
extern tinysh_cmd_t test_tokenize_cmd;
extern tinysh_cmd_t test_conversion_cmd;

#endif /* TINYSH_TEST_H */