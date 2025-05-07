#include "tiny_port.h"
#include "tinysh.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdint.h>

static struct termios orig_termios; /* Original terminal settings */

/* Forward declare command handlers */
void cmd_sysinfo(int argc, char **argv);
void cmd_echo(int argc, char **argv);

/**
 * Output a character to stdout
 */
void tiny_port_putchar(unsigned char c) {
    putchar(c);
    fflush(stdout);
}

/**
 * Printf-like function for formatted output
 */
int tiny_port_printf(const char *fmt, ...) {
    va_list args;
    int result;
    
    va_start(args, fmt);
    result = vprintf(fmt, args);
    va_end(args);
    
    fflush(stdout);
    return result;
}

/**
 * Set terminal to raw mode
 */
int tiny_port_init(void) {
    struct termios raw;
    
    // Save original terminal settings
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
        perror("tcgetattr");
        return -1;
    }
    
    // Modify terminal settings for raw input
    raw = orig_termios;
    raw.c_iflag &= ~(ICRNL | IXON);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    
    // Set minimum number of characters for read to return
    // (important for escape sequences)
    raw.c_cc[VMIN] = 1;  // Return as soon as 1 character is available
    raw.c_cc[VTIME] = 0; // No timeout
    
    // Apply new settings
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        perror("tcsetattr");
        return -1;
    }
    
    return 0;
}

/**
 * Reset terminal to original settings
 */
void tiny_port_cleanup(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

/**
 * Main application initialization for TinyShell
 */
void tiny_port_setup(void) {
    // Register output functions
    tinysh_out(tiny_port_putchar);
    tinysh_print_out(tiny_port_printf);
    
    // Set initial prompt (optional)
    tinysh_set_prompt("tinysh> ");
    
    // Display welcome message
    tiny_port_printf("\r\nTinyShell v%s starting on Ubuntu\r\n", TINYSHELL_VERSION);
    tiny_port_printf("Type '?' for help\r\n");

    // Set TinyShell active flag
    tinyshell_active = 1;
}

// Define commands
tinysh_cmd_t sysinfo_cmd = {
    0, "sysinfo", "show system information", 0, cmd_sysinfo, 0, 0, 0
};

tinysh_cmd_t echo_cmd = {
    0, "echo", "echo arguments", "[args...]", cmd_echo, 0, 0, 0
};

/**
 * Example command - print system info
 */
void cmd_sysinfo(int argc, char **argv) {
    (void)argc; // Unused
    (void)argv; // Unused
    
    tiny_port_printf("System: Ubuntu Linux\r\n");
    tiny_port_printf("TinyShell version: %s\r\n", TINYSHELL_VERSION);
    tiny_port_printf("Buffer size: %d bytes\r\n", BUFFER_SIZE);
    tiny_port_printf("History depth: %d entries\r\n", HISTORY_DEPTH);
}

/**
 * Example command - echo arguments
 */
void cmd_echo(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        tiny_port_printf("%s ", argv[i]);
    }
    tiny_port_printf("\r\n");
}

