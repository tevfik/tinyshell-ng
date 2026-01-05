/**
 * TinyShell Platform Abstraction Layer
 * -----------------------------------
 * This module provides platform-specific implementations for TinyShell's
 * I/O operations. It handles terminal setup, character input/output,
 * and platform-specific commands.
 *
 * For porting to a new platform:
 * 1. Implement tiny_port_putchar() for character output
 * 2. Implement tiny_port_printf() for formatted output
 * 3. Implement tiny_port_init() for platform initialization
 * 4. Implement tiny_port_cleanup() for platform cleanup
 * 5. Customize tiny_port_setup() for platform-specific setup
 *
 * Example Usage for a New Platform:
 * 
 * void my_putchar(unsigned char c) {
 *     // Platform-specific character output
 *     UART_SendChar(c);
 * }
 *
 * int my_printf(const char *fmt, ...) {
 *     // Platform-specific formatted output
 *     // Implement or use existing printf-like function
 *     // ...
 * }
 *
 * // In initialization code:
 * tinysh_out(my_putchar);
 * tinysh_print_out(my_printf);
 */

#ifndef TINY_PORT_H
#define TINY_PORT_H

#include "tinysh.h"  // Include this to access tinysh_cmd_t

/**
 * Initialize the terminal for raw input mode
 * 
 * @return 0 on success, -1 on error
 */
int tiny_port_init(void);

/**
 * Reset terminal to original settings
 */
void tiny_port_cleanup(void);

/**
 * Character output function - prints a character to stdout
 * 
 * @param c Character to print
 */
void tiny_port_putchar(unsigned char c);

/**
 * Printf-like function for formatted output
 * 
 * @param fmt Format string
 * @param ... Variable arguments
 * @return Number of characters printed
 */
int tiny_port_printf(const char *fmt, ...);

/**
 * Setup TinyShell with proper output functions and initial prompt
 */
void tiny_port_setup(void);

/* Command prototypes */
void cmd_sysinfo(int argc, const char **argv);
void cmd_echo(int argc, const char **argv);

/* Command structures */
extern tinysh_cmd_t sysinfo_cmd;
extern tinysh_cmd_t echo_cmd;

#endif /* TINY_PORT_H */