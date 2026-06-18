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

/* TinyShell Configuration.
   Every default below is wrapped in #ifndef so projects can override
   any value at compile time with -D, e.g.:
     gcc -DBUFFER_SIZE=128 -DMENU_DISPLAY_ITEMS=5 ...
   Without these guards, gcc picks up project-conf.h via the source
   file's own directory (current-directory-first lookup) BEFORE any
   -I include path is searched, which silently prevents downstream
   projects from tuning TinyShell without modifying this file. */
#ifndef BUFFER_SIZE
#define BUFFER_SIZE                 256
#endif
#ifndef HISTORY_DEPTH
#define HISTORY_DEPTH               4
#endif
#ifndef MAX_ARGS
#define MAX_ARGS                    8
#endif
#ifndef TOPCHAR
#define TOPCHAR                    '/'
#endif
#ifndef ECHO_INPUT
#define ECHO_INPUT                  1
#endif
#ifndef PARTIAL_MATCH
#define PARTIAL_MATCH               1
#endif
#ifndef AUTOCOMPLATION
#define AUTOCOMPLATION              1
#endif
#ifndef _PROMPT_
#define _PROMPT_                    "tinysh> "
#endif

/**
 * TinyShell Security Configuration
 * -------------------------------
 * DEFAULT_ADMIN_PASSWORD: Set your admin password here
 *   - This password will be compiled into the firmware
 *   - Keep this secure and change from default value
 *   - Used by "auth" command for admin privileges
 */

#ifndef AUTHENTICATION_ENABLED
#define AUTHENTICATION_ENABLED      1               // Enable authentication
#endif
#ifndef DEFAULT_ADMIN_PASSWORD
#define DEFAULT_ADMIN_PASSWORD      "embedded2024"  // Custom admin password
#endif

/* Menu System Configuration */
#ifndef MENU_ENABLED
#define MENU_ENABLED              1  // Enable by default
#endif

#if MENU_ENABLED
/* Menu-based UI configuration.
   MENU_MAX_ITEMS is the total slots in the items[] array of tinysh_menu_t
   (including the "Back" item appended after the user commands). The
   maximum number of USER commands that can be listed in the auto-generated
   Commands submenu is therefore MENU_MAX_ITEMS - 1. */
#ifndef MENU_MAX_DEPTH
#define MENU_MAX_DEPTH            5  // Maximum nesting level of menus
#endif
#ifndef MENU_MAX_ITEMS
#define MENU_MAX_ITEMS            16 // Total slots per menu (commands + Back)
#endif
#ifndef MENU_DISPLAY_ITEMS
#define MENU_DISPLAY_ITEMS        10  // Number of items to display at once
#endif
#ifndef MAX_CMD_MENU_ITEMS
#define MAX_CMD_MENU_ITEMS        (MENU_MAX_ITEMS - 1)  // max USER commands
#endif
#ifndef MAX_CMD_SUBMENUS
#define MAX_CMD_SUBMENUS          (MENU_MAX_ITEMS - 1) // max submenus
#endif
#ifndef MENU_COLOR_ENABLED
#define MENU_COLOR_ENABLED        1
#endif
#endif

#endif /* PROJECT_CONF_H */