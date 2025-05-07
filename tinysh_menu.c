#include "tinysh_menu.h"
#include "tinysh.h"
#include <string.h>
#include <stdio.h>

/* Global state */
static tinysh_menu_state_t menu_state;
static uint8_t in_menu_mode = 0;
static uint8_t waiting_for_keypress = 0;

/* Forward declarations */
static void navigate_menu(int direction);
static void display_menu_header(void);
static void display_menu_item(uint8_t index, uint8_t is_selected);
static void display_menu_footer(void);
static void execute_menu_item(tinysh_menu_item_t *item);
static void menu_select_item(uint8_t index);
static void clear_screen(void);
static void prompt_for_arguments(char *title, char *param_desc, void (*function_arg)(int argc, char **argv));

/* Menu command */
tinysh_cmd_t menu_cmd = {
    0, "menu", "enter menu-based UI mode", 0, 
    menu_cmd_handler, 0, 0, 0
};

/**
 * Initialize menu system
 */
void tinysh_menu_init(tinysh_menu_t *root_menu) {
    /* Set up initial state */
    menu_state.current_menu = root_menu;
    menu_state.current_index = 0;
    menu_state.scroll_offset = 0;
    menu_state.menu_stack_idx = 0;
    menu_state.menu_stack[0] = root_menu;
    menu_state.index_stack[0] = 0;
    
    /* Register the menu command */
    tinysh_add_command(&menu_cmd);
}

/**
 * Menu mode command handler
 */
void menu_cmd_handler(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    tinysh_menu_enter();
}

/**
 * Enter menu mode
 */
void tinysh_menu_enter(void) {
    if (in_menu_mode) return;
    
    in_menu_mode = 1;
    
    /* Reset menu position to root */
    menu_state.current_menu = menu_state.menu_stack[0];
    menu_state.current_index = menu_state.index_stack[0];
    menu_state.scroll_offset = 0;
    
    /* Display the menu */
    clear_screen();
    tinysh_menu_display();
}

/**
 * Exit menu mode
 */
void tinysh_menu_exit(void) {
    if (!in_menu_mode) return;
    
    in_menu_mode = 0;
    tinysh_printf("\r\n");
    tinysh_reset_context();  /* Reset shell context */
    
    /* Restore shell prompt */
    tinysh_char_in('\r');
}

/**
 * Process character input in menu mode
 */
int tinysh_menu_process_char(char c) {
    if (!in_menu_mode) return 0;
    
    /* Check if we're waiting for a keypress to continue */
    if (waiting_for_keypress) {
        waiting_for_keypress = 0;
        tinysh_menu_display(); // Redisplay the menu
        return 1;
    }
    
    /* Handle escape sequences for arrow keys */
    static int in_escape_seq = 0;
    static int escape_char_count = 0;
    
    if (in_escape_seq) {
        escape_char_count++;
        
        if (escape_char_count == 1 && c == '[') {
            // Got the '[' after ESC
            return 1;
        }
        else if (escape_char_count == 2) {
            // Got the final character of the sequence
            in_escape_seq = 0;
            escape_char_count = 0;
            
            // Process arrow keys
            switch (c) {
                case 'A': navigate_menu(-1); return 1; // Up
                case 'B': navigate_menu(1); return 1;  // Down
                case 'C': tinysh_menu_execute_selection(); return 1; // Right
                case 'D': tinysh_menu_go_back(); return 1; // Left
            }
        }
        else {
            // If we got a third character that's not part of a recognized sequence,
            // treat it as a standalone ESC press followed by other characters
            if (escape_char_count > 2) {
                in_escape_seq = 0;
                escape_char_count = 0;
                
                // Handle ESC press (go back or exit)
                if (!tinysh_menu_go_back()) {
                    tinysh_menu_exit();
                }
                // Now process the current character normally
                return tinysh_menu_process_char(c);
            }
        }
    }
    
    /* Process regular navigation keys */
    switch (c) {
        case 27: // ESC - could be start of an escape sequence
            in_escape_seq = 1;
            escape_char_count = 0;
            return 1;
            
        case MENU_KEY_ENTER:
            tinysh_menu_execute_selection();
            return 1;
            
        case MENU_KEY_BACK:
            if (!tinysh_menu_go_back()) {
                tinysh_menu_exit();
            }
            return 1;
            
        default:
            // Check if it's a numeric shortcut
            if (c >= '0' && c <= '9') {
                int index = c - '0';
                if (index < menu_state.current_menu->item_count) {
                    menu_select_item(index);
                    tinysh_menu_display();
                }
                return 1;
            }
    }
    
    return 0;  // Default: not consumed
}

/**
 * Display current menu
 */
void tinysh_menu_display(void) {
    tinysh_menu_t *menu = menu_state.current_menu;
    int i, display_count;
    
    if (!menu || !in_menu_mode) return;
    
    clear_screen();
    
    /* Display header */
    display_menu_header();
    
    /* Calculate visible items */
    display_count = menu->item_count;
    if (display_count > MENU_DISPLAY_ITEMS) {
        display_count = MENU_DISPLAY_ITEMS;
    }
    
    /* Adjust scroll offset if needed */
    if (menu_state.current_index >= menu_state.scroll_offset + display_count) {
        menu_state.scroll_offset = menu_state.current_index - display_count + 1;
    } else if (menu_state.current_index < menu_state.scroll_offset) {
        menu_state.scroll_offset = menu_state.current_index;
    }
    
    /* Display menu items */
    for (i = 0; i < display_count; i++) {
        uint8_t item_index = i + menu_state.scroll_offset;
        if (item_index >= menu->item_count) break;
        
        display_menu_item(item_index, item_index == menu_state.current_index);
    }
    
    /* Display footer */
    display_menu_footer();
}

/**
 * Execute selected menu item
 */
void tinysh_menu_execute_selection(void) {
    tinysh_menu_t *menu = menu_state.current_menu;
    tinysh_menu_item_t *item;
    
    if (!menu || menu_state.current_index >= menu->item_count) return;
    
    item = &menu->items[menu_state.current_index];
    
    /* Check if item requires admin rights */
    if (item->type & MENU_ITEM_ADMIN) {
#if AUTHENTICATION_ENABLED
        if (tinysh_get_auth_level() < TINYSH_AUTH_ADMIN) {
            tinysh_printf("\r\nAdmin rights required for this item!\r\n");
            tinysh_printf("Press any key to continue...");
            waiting_for_keypress = 1; // Set the waiting state flag
            return;
        }
#endif
    }
    
    execute_menu_item(item);
}

/**
 * Go back to parent menu
 */
int tinysh_menu_go_back(void) {
    /* Check if we're already at root level */
    if (menu_state.menu_stack_idx == 0) {
        return 0;
    }
    
    /* Pop from menu stack */
    menu_state.menu_stack_idx--;
    menu_state.current_menu = menu_state.menu_stack[menu_state.menu_stack_idx];
    menu_state.current_index = menu_state.index_stack[menu_state.menu_stack_idx];
    menu_state.scroll_offset = 0;
    
    /* Display the new menu */
    tinysh_menu_display();
    
    return 1;
}

/**
 * Display menu header with title
 */
static void display_menu_header(void) {
    tinysh_menu_t *menu = menu_state.current_menu;
    
    // Define the display width
    int menu_seperator_len = strlen(MENU_SEPARATOR);

    // Calculate the full title text width
    int title_len = strlen(MENU_TITLE_PREFIX) + strlen(menu->title) + strlen(MENU_TITLE_SUFFIX);
    
    // Calculate left padding to center the title
    int padding = (menu_seperator_len - title_len) / 2;
    if (padding < 0) padding = 0;  // Safety check
    
    // Print the centered title with padding
    tinysh_printf("\r\n");
    for (int i = 0; i < padding; i++) {
        tinysh_printf(" ");
    }
    tinysh_printf("%s%s%s\r\n", MENU_TITLE_PREFIX, menu->title, MENU_TITLE_SUFFIX);
    
    // Center the navigation help
    const char *nav_help = "[↑/↓] Select  [Enter] Execute  [q] Back";
    int nav_padding = (menu_seperator_len - strlen(nav_help)) / 2;
    if (nav_padding < 0) nav_padding = 0;
    
    for (int i = 0; i < nav_padding; i++) {
        tinysh_printf(" ");
    }
    tinysh_printf("%s\r\n", nav_help);
    
    // Print the separator line
    tinysh_printf("%s\r\n",MENU_SEPARATOR);
}

/**
 * Display a single menu item
 */
static void display_menu_item(uint8_t index, uint8_t is_selected) {
    tinysh_menu_t *menu = menu_state.current_menu;
    tinysh_menu_item_t *item;
    
    if (index >= menu->item_count) return;
    
    item = &menu->items[index];
    
    /* Display selector if this is the selected item */
    if (is_selected) {
        tinysh_printf("%s ", MENU_SELECTOR);
        
        /* Display submenu indicator if needed */
        if (item->type & MENU_ITEM_SUBMENU) {
            tinysh_printf("%s ", MENU_SUBMENU_INDICATOR);
        } else {
            tinysh_printf("%s ", MENU_INDENT);
        }
    } else {
        tinysh_printf("%d ", index);
        
        /* Display submenu indicator if needed */
        if (item->type & MENU_ITEM_SUBMENU) {
            tinysh_printf("%s ", MENU_SUBMENU_INDICATOR);
        } else {
            tinysh_printf("%s ", MENU_INDENT);
        }
    }
    
    /* Display admin indicator if needed */
    if (item->type & MENU_ITEM_ADMIN) {
        tinysh_printf("%s ", MENU_ADMIN_INDICATOR);
    } else {
        tinysh_printf("  ");
    }
    
    /* Display item title */
    tinysh_printf("%s", item->title);
    
    tinysh_printf("\r\n");
}

/**
 * Display menu footer
 */
static void display_menu_footer(void) {
    tinysh_menu_t *menu = menu_state.current_menu;
    
    /* Show pagination info if needed */
    if (menu->item_count > MENU_DISPLAY_ITEMS) {
        tinysh_printf("%s\r\n",MENU_SEPARATOR);
        tinysh_printf("Showing items %d-%d of %d\r\n", 
                     menu_state.scroll_offset + 1,
                     menu_state.scroll_offset + MENU_DISPLAY_ITEMS > menu->item_count ?
                         menu->item_count : menu_state.scroll_offset + MENU_DISPLAY_ITEMS,
                     menu->item_count);
    }
    
    tinysh_printf("%s\r\n",MENU_SEPARATOR);
}

/**
 * Prompt for arguments and call a function with them
 */
static void prompt_for_arguments(char *title, char *param_desc, void (*function_arg)(int argc, char **argv)) {
    char input_buffer[BUFFER_SIZE];
    char *args[MAX_ARGS];
    int argc = 0;
    
    // Clear screen and show prompt
    clear_screen();
    tinysh_printf("Function: %s\r\n\n", title);
    
    // Show parameter description if available
    if (param_desc && *param_desc) {
        tinysh_printf("Parameters: %s\r\n\n", param_desc);
    }
    
    // Get input
    tinysh_printf("Enter arguments: ");
    
    // Temporarily exit menu mode to get input
    uint8_t prev_menu_mode = in_menu_mode;
    in_menu_mode = 0;
    
    // Reset buffer
    memset(input_buffer, 0, sizeof(input_buffer));
    int char_count = 0;
    
    // Simple input routine
    char c;
    while (1) {
        c = getchar();
        
        if (c == '\r' || c == '\n') {
            tinysh_printf("\r\n");
            break;
        }
        else if (c == 8 || c == 127) { // Backspace
            if (char_count > 0) {
                tinysh_printf("\b \b");
                input_buffer[--char_count] = 0;
            }
        }
        else if (char_count < BUFFER_SIZE-1) {
            tinysh_printf("%c", c);
            input_buffer[char_count++] = c;
            input_buffer[char_count] = 0;
        }
    }
    
    // Tokenize the input
    argc = tinysh_tokenize(input_buffer, ' ', args, MAX_ARGS);
    
    // Call the function with arguments
    if (function_arg) {
        function_arg(argc, args);
    }
    
    // Restore menu mode
    in_menu_mode = prev_menu_mode;
    
    // Wait for user acknowledgment
    tinysh_printf("\r\n\nPress any key to return to menu...");
    waiting_for_keypress = 1;
}

/**
 * Execute a menu item action
 */
static void execute_menu_item(tinysh_menu_item_t *item) {
    if (!item) return;
    
    /* Handle different item types */
    if (item->type & MENU_ITEM_SUBMENU) {
        /* Navigate to submenu */
        if (!item->submenu) return;
        
        /* Push current menu to stack */
        if (menu_state.menu_stack_idx < MENU_MAX_DEPTH - 1) {
            menu_state.menu_stack_idx++;
            menu_state.menu_stack[menu_state.menu_stack_idx] = item->submenu;
            menu_state.index_stack[menu_state.menu_stack_idx] = 0;
            
            menu_state.current_menu = item->submenu;
            menu_state.current_index = 0;
            menu_state.scroll_offset = 0;
            
            tinysh_menu_display();
        }
    }
    else if (item->type & MENU_ITEM_FUNCTION_ARG) {
        /* Call function handler with arguments */
        if (item->function_arg) {
            /* 
             * For command menu items, we need to handle differently
             * to ensure the command name is included in the arguments
             */
            if (item->function_arg == tinysh_menu_execute_command) {
                /* This is for command execution from the menu */
                /* Prompt for arguments and call function with command name */
                prompt_for_arguments(item->title, item->params, item->function_arg);
            } else {
                /* Regular function with arguments */
                prompt_for_arguments(item->title, item->params, item->function_arg);
            }
        }
    }
    else if (item->type & MENU_ITEM_FUNCTION) {
        /* Call function handler */
        if (item->function) {
            /* Temporarily exit menu display */
            clear_screen();
            tinysh_printf("Executing %s...\r\n\n", item->title);
            
            /* Call the function */
            item->function();
            
            /* Wait for user acknowledgment */
            tinysh_printf("\r\n\nPress any key to return to menu...");
            waiting_for_keypress = 1; // Set waiting state flag
        }
    }
    else if (item->type & MENU_ITEM_COMMAND) {
        /* Execute shell command */
        if (item->command) {
            /* Temporarily exit menu display */
            clear_screen();
            tinysh_printf("Executing command: %s\r\n\n", item->command);
            
            /* Exit menu mode to allow command execution */
            in_menu_mode = 0;
            
            /* Feed command to shell character by character */
            char *cmd = item->command;
            while (*cmd) {
                tinysh_char_in(*cmd++);
            }
            tinysh_char_in('\r');
            
            /* Return to menu mode */
            in_menu_mode = 1;
            
            /* Wait for user acknowledgment */
            tinysh_printf("\r\n\nPress any key to return to menu...");
            waiting_for_keypress = 1; // Set waiting state flag
        }
    }
    else if (item->type & MENU_ITEM_BACK) {
        /* Go back to parent menu */
        tinysh_menu_go_back();
    }
    else if (item->type & MENU_ITEM_EXIT) {
        /* Exit menu mode */
        tinysh_menu_exit();
    }
}

/**
 * Navigate menu up or down
 */
static void navigate_menu(int direction) {
    tinysh_menu_t *menu = menu_state.current_menu;
    int new_index;
    
    if (!menu || menu->item_count == 0) return;
    
    /* Calculate new index with wrap-around */
    new_index = menu_state.current_index + direction;
    if (new_index < 0) {
        new_index = menu->item_count - 1;
    } else if (new_index >= menu->item_count) {
        new_index = 0;
    }
    
    menu_state.current_index = new_index;
    
    /* Redisplay menu */
    tinysh_menu_display();
}

/**
 * Directly select a menu item by index
 */
static void menu_select_item(uint8_t index) {
    tinysh_menu_t *menu = menu_state.current_menu;
    
    if (!menu || index >= menu->item_count) return;
    
    menu_state.current_index = index;
}

/**
 * Clear the screen
 */
static void clear_screen(void) {
    /* ANSI escape sequence to clear screen and position cursor at top left */
    tinysh_printf("\033[2J\033[H");
}

/**
 * Hook function for main.c to integrate menu processing
 * Returns 1 if character was consumed by menu system
 */
int tinysh_menu_hook(char c) {
    if (in_menu_mode) {
        return tinysh_menu_process_char(c);
    }
    return 0;
}

/* Command menu implementation with hierarchical support */
#define MAX_CMD_MENU_ITEMS 30
#define MAX_CMD_SUBMENUS 10  // Maximum number of command submenus

/* Storage for command menu and submenus */
static tinysh_menu_t cmd_menu = {
    "Shell Commands",
    { {0} },  // Will be filled dynamically
    0,        // Item count will be set during initialization
    0         // Parent index
};

/* Storage for command submenus (like "test" commands) */
static tinysh_menu_t cmd_submenus[MAX_CMD_SUBMENUS];

/* Array to store command references for menu items */
static tinysh_cmd_t *cmd_references[MAX_CMD_MENU_ITEMS];

/* Create a static array for submenu titles to avoid memory leaks from strdup() */
static char submenu_titles[MAX_CMD_SUBMENUS][64];

/**
 * Execute a shell command from the menu system
 */
void tinysh_menu_execute_command(int argc, char **argv) {
    // Get command name from the calling menu item's title or first arg
    char *cmd_name = NULL;
    
    // Check if we're executing a child command (submenu item)
    if (argc > 0 && strstr(argv[0], " ")) {
        // This is a child command with format "parent child"
        cmd_name = argv[0];
    } else {
        // Standard command from menu title
        tinysh_menu_t *menu = menu_state.current_menu;
        if (menu) {
            cmd_name = menu->items[menu_state.current_index].title;
        }
    }
    
    if (!cmd_name) {
        tinysh_printf("Error: Could not determine command name\r\n");
        return;
    }
    
    // Special case for quit command - handle directly to avoid segfault
    if (strcmp(cmd_name, "quit") == 0) {
        tinysh_printf("Exiting TinyShell...\r\n");
        tinyshell_active = 0;
        tinysh_menu_exit();
        return;
    }
    
    // Two different command execution paths depending on if this is a child command
    tinysh_cmd_t *cmd = NULL;
    char parent_name[32] = {0};
    char child_name[32] = {0};
    
    if (strstr(cmd_name, " ")) {
        // Parse "parent child" format
        char *space = strstr(cmd_name, " ");
        strncpy(parent_name, cmd_name, space - cmd_name);
        strcpy(child_name, space + 1);
        
        // Find parent command
        tinysh_cmd_t *parent_cmd = NULL;
        for (int i = 0; i < MAX_CMD_MENU_ITEMS && cmd_references[i]; i++) {
            if (strcmp(cmd_references[i]->name, parent_name) == 0) {
                parent_cmd = cmd_references[i];
                break;
            }
        }
        
        // Find child command
        if (parent_cmd && parent_cmd->child) {
            cmd = parent_cmd->child;
            while (cmd) {
                if (strcmp(cmd->name, child_name) == 0) {
                    break;
                }
                cmd = cmd->next;
            }
        }
    } else {
        // Standard command lookup
        for (int i = 0; i < MAX_CMD_MENU_ITEMS && cmd_references[i]; i++) {
            if (strcmp(cmd_references[i]->name, cmd_name) == 0) {
                cmd = cmd_references[i];
                break;
            }
        }
    }
    
    if (!cmd) {
        tinysh_printf("Error: Command '%s' not found\r\n",cmd_name);
        return;
    }
    
#if AUTHENTICATION_ENABLED
    // Check admin rights
    if (tinysh_is_admin_command(cmd) && tinysh_get_auth_level() < TINYSH_AUTH_ADMIN) {
        tinysh_printf("Error: Command requires admin privileges\r\n");
        tinysh_printf("Use 'auth <password>' to authenticate\r\n");
        return;
    }
#endif
    
    // Get argument and call function
    void *arg = cmd->arg;
#if AUTHENTICATION_ENABLED
    if (tinysh_is_admin_command(cmd)) {
        arg = tinysh_get_real_arg(cmd);
    }
#endif
    
    if (cmd->function) {
        // Create new argv array with proper command name as first element
        char *new_argv[MAX_ARGS + 1];
        
        if (child_name[0]) {
            // This is a child command call, use child name as argv[0]
            new_argv[0] = child_name;
        } else {
            // Standard command, use the command name
            new_argv[0] = cmd_name;
        }
        
        // Copy user arguments to positions 1 onwards
        int new_argc = 1;  // Start with 1 for the command name
        for (int i = 0; i < argc && new_argc < MAX_ARGS + 1; i++) {
            new_argv[new_argc++] = argv[i];
        }
        
        // Save original arg and execute command
        void *original_arg = cmd->arg;
        cmd->arg = arg;
        
        cmd->function(new_argc, new_argv);
        
        // Restore original arg
        cmd->arg = original_arg;
    }
}

/**
 * Generate menu of all registered commands
 */
tinysh_menu_t* tinysh_generate_cmd_menu(void) {
    // Start with help_cmd which is the first in the linked list
    extern tinysh_cmd_t help_cmd; 
    tinysh_cmd_t *cmd = &help_cmd;
    int count = 0;
    int submenu_count = 0;
    
    memset(cmd_references, 0, sizeof(cmd_references));
    
    // First pass: find top-level commands and create menu items
    while (cmd && count < MAX_CMD_MENU_ITEMS) {
        // Skip special commands and ensure command name is valid
        if (cmd->name && strcmp(cmd->name, "menu") != 0 && 
            strcmp(cmd->name, "quit") != 0 &&
            strcmp(cmd->name, "menutest") != 0) {
            
            // Store command reference
            cmd_references[count] = cmd;
            
            // Check if this command has children
            if (cmd->child) {
                // This is a parent command - create a submenu
                if (submenu_count < MAX_CMD_SUBMENUS) {
                    // Set up submenu item
                    cmd_menu.items[count].title = cmd->name;
                    cmd_menu.items[count].type = MENU_ITEM_SUBMENU;
                    
                    // Create submenu for this command's children
                    tinysh_menu_t *submenu = &cmd_submenus[submenu_count];
                    memset(submenu, 0, sizeof(tinysh_menu_t)); // Initialize submenu
                    
                    char submenu_title[32] = {0};
                    snprintf(submenu_title, sizeof(submenu_title), "%s Commands", cmd->name);
                    
                    // Use static array for submenu titles
                    strncpy(submenu_titles[submenu_count], submenu_title, 63);
                    submenu_titles[submenu_count][63] = '\0';
                    submenu->title = submenu_titles[submenu_count];
                    
                    // Link submenu to menu item
                    cmd_menu.items[count].submenu = submenu;
                    
                    // Add child commands to submenu
                    tinysh_cmd_t *child = cmd->child;
                    int child_count = 0;
                    
                    while (child && child_count < MENU_MAX_ITEMS - 1) {
                        // Skip NULL names
                        if (!child->name) {
                            child = child->next;
                            continue;
                        }
                        
                        // Create combined name for proper execution
                        char full_name[64] = {0};
                        
                        // Use snprintf to safely handle potential buffer overflow
                        if (snprintf(full_name, sizeof(full_name), "%s %s", cmd->name, child->name) >= 0) {
                            // Use static storage rather than strdup to avoid leaks
                            static char cmd_names[MENU_MAX_ITEMS][64];
                            strncpy(cmd_names[child_count], full_name, 63);
                            cmd_names[child_count][63] = '\0';  // Ensure null termination
                            
                            // Set the menu item title to our static storage
                            submenu->items[child_count].title = cmd_names[child_count];
                            submenu->items[child_count].type = MENU_ITEM_FUNCTION_ARG;
                            submenu->items[child_count].function_arg = tinysh_menu_execute_command;
                            submenu->items[child_count].params = child->usage ? child->usage : "";
                        }
                        
                        child = child->next;
                        child_count++;
                    }
                    
                    // Add back item
                    submenu->items[child_count].title = "Back to Commands";
                    submenu->items[child_count].type = MENU_ITEM_BACK;
                    submenu->items[child_count].submenu = NULL;
                    
                    // Set submenu item count
                    submenu->item_count = child_count + 1;
                    submenu->parent_index = 0;
                    
                    submenu_count++;
                } else {
                    // Fallback to standard menu item if we're out of submenu slots
                    cmd_menu.items[count].title = cmd->name;
                    cmd_menu.items[count].type = MENU_ITEM_FUNCTION_ARG;
                    cmd_menu.items[count].function_arg = tinysh_menu_execute_command;
                    cmd_menu.items[count].params = cmd->usage ? cmd->usage : "";
                }
            } else {
                // Standard command without children
                cmd_menu.items[count].title = cmd->name;
                cmd_menu.items[count].type = MENU_ITEM_FUNCTION_ARG;
                cmd_menu.items[count].function_arg = tinysh_menu_execute_command;
                cmd_menu.items[count].params = cmd->usage ? cmd->usage : "";
            }
            
            count++;
        }
        
        cmd = cmd->next;
    }
    
    // Add back menu item
    if (count < MAX_CMD_MENU_ITEMS) {
        cmd_menu.items[count].title = "Back to Main Menu";
        cmd_menu.items[count].type = MENU_ITEM_BACK;
        cmd_menu.items[count].submenu = NULL;
        
        // Set total count
        cmd_menu.item_count = count + 1;
    }
    
    return &cmd_menu;
}