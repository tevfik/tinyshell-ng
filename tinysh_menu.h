/**
 * TinyShell Menu System
 * -------------------
 * Provides a hierarchical menu interface for TinyShell. The menu system
 * allows navigation through nested menus using arrow keys, executing
 * commands or functions, and handling admin-restricted operations.
 *
 * Features:
 * - Hierarchical nested menus with parent-child relationships
 * - Support for direct function calls, shell commands, or submenu navigation
 * - Support for admin-restricted menu items
 * - Function argument collection through interactive prompts
 * - Navigation using arrow keys, numeric shortcuts, or ESC/Enter
 *
 * Example Usage:
 * 
 * // Define menu action functions
 * void show_info(void) {
 *     tinysh_printf("System information display\r\n");
 * }
 * 
 * // Define menu structures
 * tinysh_menu_t system_menu = {
 *     "System Menu",
 *     {
 *         {"Information", MENU_ITEM_FUNCTION, .function = show_info},
 *         {"Back", MENU_ITEM_BACK, {0}}
 *     },
 *     2, 0
 * };
 * 
 * tinysh_menu_t main_menu = {
 *     "Main Menu",
 *     {
 *         {"System", MENU_ITEM_SUBMENU, .submenu = &system_menu},
 *         {"Exit", MENU_ITEM_EXIT, {0}}
 *     },
 *     2, 0
 * };
 * 
 * // Initialize and activate menu
 * tinysh_menu_init(&main_menu);
 * tinysh_menu_enter();
 */

#ifndef TINYSH_MENU_H
#define TINYSH_MENU_H

#include "tinysh.h"

/* Menu-based UI Configuration */
#ifndef MENU_MAX_DEPTH
#define MENU_MAX_DEPTH        5      // Maximum nesting level of menus
#endif

#ifndef MENU_MAX_ITEMS
#define MENU_MAX_ITEMS        10     // Maximum items per menu level
#endif

#ifndef MENU_DISPLAY_ITEMS
#define MENU_DISPLAY_ITEMS    5      // Number of items to display at once
#endif

/* Menu item type flags */
#define MENU_ITEM_NORMAL      0x00   // Regular menu item
#define MENU_ITEM_SUBMENU     0x01   // Has submenu
#define MENU_ITEM_COMMAND     0x02   // Executes a shell command
#define MENU_ITEM_FUNCTION    0x04   // Calls a C function
#define MENU_ITEM_ADMIN       0x08   // Requires admin rights
#define MENU_ITEM_BACK        0x10   // Special "back" item
#define MENU_ITEM_EXIT        0x20   // Exit menu mode
#define MENU_ITEM_FUNCTION_ARG 0x40  // Function with arguments
#define MENU_ITEM_CMD_REF     0x80   // Direct reference to tinysh_cmd_t

/* Menu navigation keys */
#define MENU_KEY_UP           'A'    // Up arrow (ANSI escape sequence)
#define MENU_KEY_DOWN         'B'    // Down arrow (ANSI escape sequence)
#define MENU_KEY_ENTER        '\r'   // Enter key
#define MENU_KEY_ESC          27     // Escape key
#define MENU_KEY_BACK         'q'    // Alternative back key

/* Menu display formatting */
#define MENU_INDENT           "  "
#define MENU_SELECTOR         ">"
#define MENU_ADMIN_INDICATOR  "*"
#define MENU_SUBMENU_INDICATOR "..."
#define MENU_TITLE_PREFIX     "=== "
#define MENU_SEPARATOR        "----------------------------------------------"
#define MENU_TITLE_SUFFIX     " ==="

/**
 * Menu item structure
 */
typedef struct {
    char *title;                     // Display title
    unsigned char type;              // Item type flags
    
    union {
        // For submenu items (MENU_ITEM_SUBMENU)
        struct {
            struct tinysh_menu_t *submenu;
        };
        
        // For command items (MENU_ITEM_COMMAND)
        struct {
            char *command;           // Command to execute
        };
        
        // For function items (MENU_ITEM_FUNCTION)
        struct {
            void (*function)(void);  // Function to call
            void *arg;               // Optional argument
        };
        
        // For function items with arguments (MENU_ITEM_FUNCTION_ARG)
        struct {
            void (*function_arg)(int argc, char **argv);  // Function with arguments
            char *params;           // Parameter string description "param1 param2..."
        };

        // For direct command reference items (MENU_ITEM_CMD_REF)
        struct {
            tinysh_cmd_t *cmd;         // Direct reference to command
        };
    };
} tinysh_menu_item_t;

/**
 * Menu structure
 */
typedef struct tinysh_menu_t {
    char *title;                     // Menu title
    tinysh_menu_item_t items[MENU_MAX_ITEMS];  // Menu items
    unsigned char item_count;        // Number of valid items
    unsigned char parent_index;      // Index in parent menu (-1 for root)
} tinysh_menu_t;

/* Menu state */
typedef struct {
    tinysh_menu_t *current_menu;     // Current active menu
    unsigned char current_index;     // Currently selected item
    unsigned char scroll_offset;     // Scroll position for long menus
    unsigned char menu_stack_idx;    // Current position in menu stack
    tinysh_menu_t *menu_stack[MENU_MAX_DEPTH];  // Menu navigation stack
    unsigned char index_stack[MENU_MAX_DEPTH];  // Selected index stack
} tinysh_menu_state_t;

/* Public API */

/**
 * Initialize the menu system
 * 
 * @param root_menu Pointer to the root menu
 */
void tinysh_menu_init(tinysh_menu_t *root_menu);

/**
 * Enter menu mode
 * This takes over input processing from regular shell mode
 */
void tinysh_menu_enter(void);

/**
 * Exit menu mode
 * Returns to regular shell command mode
 */
void tinysh_menu_exit(void);

/**
 * Process input character in menu mode
 * 
 * @param c Input character
 * @return 1 if character was consumed, 0 otherwise
 */
int tinysh_menu_process_char(char c);

/**
 * Display the current menu
 * Renders the current menu to the shell output
 */
void tinysh_menu_display(void);

/**
 * Execute the currently selected menu item
 */
void tinysh_menu_execute_selection(void);

/**
 * Navigate to parent menu
 * 
 * @return 1 if successful, 0 if already at root
 */
int tinysh_menu_go_back(void);

/**
 * Command handler for entering menu mode
 */
void menu_cmd_handler(int argc, char **argv);

/**
 * Hook function for main.c to integrate menu processing
 * Returns 1 if character was consumed by menu system
 */
int tinysh_menu_hook(char c);

/* Export menu command */
extern tinysh_cmd_t menu_cmd;

/**
 * Generate a menu of all registered shell commands
 * This creates a submenu containing all available commands
 * 
 * @return Pointer to the generated command menu
 */
tinysh_menu_t* tinysh_generate_cmd_menu(void);

/**
 * Execute a shell command from the menu
 * This function is used internally by the command menu system
 * 
 * @param argc Argument count including command name
 * @param argv Argument vector, argv[0] is command name
 */
void tinysh_menu_execute_command(int argc, char **argv);

#endif /* TINYSH_MENU_H */