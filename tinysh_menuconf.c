/**
 * TinyShell Menu Configuration
 * -------------------------
 * This file defines the structure and content of the menu system.
 * It contains the menu hierarchy, command mappings, and action handlers.
 *
 * Menu Structure:
 * - Main Menu
 *   ├── System Menu
 *   │   ├── System Information
 *   │   ├── Reboot System (Admin)
 *   │   └── Back
 *   ├── Tools Menu
 *   │   ├── Run Echo Test
 *   │   ├── Toggle LED
 *   │   └── Back
 *   ├── Commands Menu
 *   ├── Set Parameter (with arguments)
 *   └── Exit Menu Mode
 *
 * To customize:
 * 1. Define handler functions for menu actions
 * 2. Define menu structures with items, types, and handler mappings
 * 3. Initialize with tinysh_menuconf_init()
 */

#include "tinysh_menu.h"
#include "tinysh.h"

/* Forward declarations of menu functions */
static void show_system_info(void);
static void toggle_led(void);

/* Define the menu structures */

/* System submenu */
tinysh_menu_t system_menu = {
    "System Menu",
    {
        {"System Information", MENU_ITEM_FUNCTION, .function = show_system_info},
        {"Back to Main Menu", MENU_ITEM_BACK, {.submenu = NULL}}
    },
    2,  /* item_count */
    0   /* parent_index */
};

/* Tools submenu */
tinysh_menu_t tools_menu = {
    "Tools Menu",
    {
        {"Run Echo Test", MENU_ITEM_COMMAND, .command = "echo Hello from menu!"},
        {"Toggle LED", MENU_ITEM_FUNCTION, .function = toggle_led},
        {"Back to Main Menu", MENU_ITEM_BACK, {.submenu = NULL}}
    },
    3,  /* item_count */
    0   /* parent_index */
};

/* Main menu */
tinysh_menu_t main_menu = {
    "TinyShell Main Menu",
    {
        {"System", MENU_ITEM_SUBMENU, .submenu = &system_menu},
        {"Tools", MENU_ITEM_SUBMENU, .submenu = &tools_menu},
        {"Commands", MENU_ITEM_SUBMENU, .submenu = NULL}, /* set in init */
        {"Exit Menu Mode", MENU_ITEM_EXIT, {.submenu = NULL}}
    },
    4,  /* item_count */
    0   /* parent_index */
};

/* Menu action functions */
static void show_system_info(void) {
    tinysh_printf("System Information:\r\n");
    tinysh_printf("  TinyShell Version: %s\r\n", TINYSHELL_VERSION);
    tinysh_printf("  Buffer Size: %d bytes\r\n", BUFFER_SIZE);
    tinysh_printf("  History Depth: %d entries\r\n", HISTORY_DEPTH);
    tinysh_printf("  Authentication: %s\r\n", AUTHENTICATION_ENABLED ? "Enabled" : "Disabled");
    tinysh_printf("  Menu Extention: %s\r\n", MENU_ENABLED ? "Enabled" : "Disabled");
}

static void toggle_led(void) {
    static int led_state = 0;
    led_state = !led_state;
    tinysh_printf("LED is now %s\r\n", led_state ? "ON" : "OFF");
}

void tinysh_menuconf_init(void);
/* Function to initialize the menu system */
void tinysh_menuconf_init(void) {
    // Generate command menu and link it to main menu
    tinysh_menu_t *cmd_menu = tinysh_generate_cmd_menu();
    main_menu.items[2].submenu = cmd_menu;

    // Initialize the menu system with the main menu
    tinysh_menu_init(&main_menu);
}
