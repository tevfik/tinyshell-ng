/**
 * TinyShell Example Application
 * --------------------------
 * This example demonstrates using TinyShell with terminal I/O,
 * command registration, authentication, and menu system integration.
 *
 * Features Demonstrated:
 * - Terminal handling and signal trapping
 * - Command line argument processing
 * - TinyShell initialization and configuration
 * - Command registration and handling
 * - Optional authentication system
 * - Menu system integration and navigation
 *
 * Usage:
 *   ./tinysh_shell            Run the shell normally
 *   ./tinysh_shell -h         Display help message
 *   ./tinysh_shell -m         Start directly in menu mode
 *   ./tinysh_shell -t         Run test framework
 */

#include "project-conf.h"
#include "tinysh.h"
#include "tiny_port.h"
#include "tinysh_test.h"

#if MENU_ENABLED
#include "tinysh_menu.h"
#include "tinysh_menu_test.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

// Signal handler to clean up before exit
void sigint_handler(int sig) {
    (void)sig; // Unused
    
    // Reset shell context in case we're stuck somewhere
    tinysh_reset_context();
    
    tiny_port_cleanup();
    printf("\nExiting tinysh_shell\n");
    exit(0);
}

/**
 * Example admin command handler for system reboot
 */
void reboot_cmd_handler(int argc, const char **argv) {
    (void)argc; // Unused
    (void)argv; // Unused
    
    // Get the admin command argument (for demonstration)
    void *arg = tinysh_get_arg();
    if (arg) {
        uintptr_t value = (uintptr_t)tinysh_get_real_arg((tinysh_cmd_t*)0); // Get real value without admin flag
        tinysh_printf("Admin command executed with arg: 0x%lx\r\n", value);
    }
    
    tinysh_printf("System reboot initiated (simulated)...\r\n");
    
    // In a real embedded system, this would trigger a hardware reboot
    tinysh_printf("On a real system, this would restart the hardware\r\n");
}

/* Main function */
int main(int argc, char *argv[]) {
    int c;
    bool start_in_menu_mode = false;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("TinyShell Usage:\n");
            printf("  -h, --help    : Show this help message\n");
            printf("  -m, --menu    : Start directly in menu mode\n");
            printf("  -t, --test    : Run test framework\n");
            return 0;
        }
        else if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--menu") == 0) {
            start_in_menu_mode = true;
        }
        else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--test") == 0) {
            // Will run tests and exit
            printf("Running TinyShell tests...\n");
            // Initialize minimal environment
            tiny_port_init();
            tiny_port_setup();
            tinysh_test_init();
            
            tinysh_run_tests();
            
            #if MENU_ENABLED
            tinysh_menu_run_tests();
            #endif
            
            tiny_port_cleanup();
            return 0;
        }
    }
    
    // Setup signal handler for Ctrl+C
    signal(SIGINT, sigint_handler);
    
    // Initialize terminal
    if (tiny_port_init() < 0) {
        fprintf(stderr, "Failed to initialize terminal\n");
        return 1;
    }
    
    // Setup TinyShell
    tiny_port_setup();
    
    // Add example commands
    extern tinysh_cmd_t sysinfo_cmd;
    extern tinysh_cmd_t echo_cmd;
    tinysh_add_command(&sysinfo_cmd);
    tinysh_add_command(&echo_cmd);
    extern tinysh_cmd_t quit_cmd;
    tinysh_add_command(&quit_cmd);
    
    // After TinyShell setup
#if AUTHENTICATION_ENABLED
    // Initialize authentication system
    tinysh_auth_init();

    // Example admin command
    tinysh_cmd_t reboot_cmd = TINYSH_ADMIN_CMD(
        0, "reboot", "reboot system (admin only)", _NOARG_, 
        reboot_cmd_handler, (void*)0x12345678, 0, 0
    );
    tinysh_add_command(&reboot_cmd);
#else
    // Regular command version when auth is disabled
    tinysh_cmd_t reboot_cmd = {
        0, "reboot", "reboot system", _NOARG_, 
        reboot_cmd_handler, (void*)0x12345678, 0, 0
    };
    tinysh_add_command(&reboot_cmd);
#endif

    // Add in the initialization section after other commands are registered
#if MENU_ENABLED
    // Add menu test command
    extern tinysh_cmd_t menu_test_cmd;
    tinysh_add_command(&menu_test_cmd);
#endif
    
    // Initialize test framework
    tinysh_test_init();

#if MENU_ENABLED
    // Initialize menu system
    extern void tinysh_menuconf_init(void);
    tinysh_menuconf_init();
    
    // Start in menu mode if requested
    if (start_in_menu_mode) {
        tinysh_menu_enter();
    }
#endif
    
    /* Main loop section */
    // Main loop - read characters from stdin and pass to TinyShell
    while ((c = getchar()) != EOF && is_tinyshell_active()) {
        // Debug to check raw incoming characters
        // printf("Got char: %d\n", c);
        
    #if MENU_ENABLED
        // First try to handle it with menu system if in menu mode
        if (!tinysh_menu_hook(c)) {
            // If not in menu mode, pass to regular shell
            tinysh_char_in(c);
        }
    #else
        // No menu system, just pass to shell
        tinysh_char_in(c);
    #endif
    }
    
    // Cleanup terminal settings
    tiny_port_cleanup();
    
    return 0;
}