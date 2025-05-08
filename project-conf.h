#ifndef PROJECT_CONF_H
#define PROJECT_CONF_H

/**
 * TinyShell Configuration
 * ---------------------
 * Central configuration file for TinyShell settings. Modify these
 * parameters to customize TinyShell behavior for your application.
 *
 * Major Configuration Groups:
 * - Buffer and memory settings
 * - Command handling settings
 * - Authentication settings
 * - Menu system settings
 *
 * Usage:
 * Include this file in your build to configure TinyShell behavior.
 * You can override these settings with compiler flags, e.g.:
 * gcc -DBUFFER_SIZE=128 -DAUTHENTICATION_ENABLED=0 ...
 */

/* TinyShell Configuration */
#define BUFFER_SIZE                 256
#define HISTORY_DEPTH               4
#define MAX_ARGS                    8
#define TOPCHAR                    '/'
#define ECHO_INPUT                  1
#define PARTIAL_MATCH               1
#define AUTOCOMPLATION              1
#define _PROMPT_                    "tinysh> "

/**
 * TinyShell Security Configuration
 * -------------------------------
 * DEFAULT_ADMIN_PASSWORD: Set your admin password here
 *   - This password will be compiled into the firmware
 *   - Keep this secure and change from default value
 *   - Used by "auth" command for admin privileges
 */

#define AUTHENTICATION_ENABLED      1               // Enable authentication
#define DEFAULT_ADMIN_PASSWORD      "embedded2024"  // Custom admin password

/* Menu System Configuration */
#ifndef MENU_ENABLED
#define MENU_ENABLED              1  // Enable by default
#endif

#if MENU_ENABLED
/* Menu-based UI configuration */
#define MENU_MAX_DEPTH            5  // Maximum nesting level of menus
#define MENU_MAX_ITEMS            10 // Maximum items per menu level
#define MENU_DISPLAY_ITEMS        10  // Number of items to display at once
#define MAX_CMD_MENU_ITEMS        100
#define MAX_CMD_SUBMENUS          30
#define MENU_COLOR_ENABLED        1
#endif

#endif /* PROJECT_CONF_H */