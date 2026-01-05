#include "tinysh_menu.h"
#include "tinysh.h"
#include <string.h>
#include <stdio.h>

/* Global state */
static tinysh_menu_state_t menu_state;
static char in_menu_mode = 0;  /* Changed from uint8_t to char */
static char waiting_for_keypress = 0;  /* Changed from uint8_t to char */

/* Argument collection state variables */
static char collecting_arguments = 0;  /* Changed from uint8_t to char */
static void (*pending_function_arg)(int argc, const char **argv) = NULL;
static char arg_buffer[BUFFER_SIZE];
static int arg_buffer_index = 0;  /* Keep as int for potential large buffers */
static const char *arg_title = NULL;
static const char *arg_param_desc = NULL;

/* Forward declarations */
static void navigate_menu(int direction);
static void display_menu_header(void);
static void display_menu_item(unsigned char index, char is_selected);  /* Changed from uint8_t */
static void display_menu_footer(void);
static void execute_menu_item(tinysh_menu_item_t *item);
static void menu_select_item(unsigned char index);  /* Changed from uint8_t */
static void clear_screen(void);
static void prompt_for_arguments(const char *title, const char *param_desc, void (*function_arg)(int argc, const char **argv));
static void start_argument_collection(const char *title, const char *param_desc, void (*function_arg)(int argc, const char **argv));
static int handle_argument_input(char c); /* Keep return as int for compatibility */

/* Forward variable declarations to ensure visibility */
static tinysh_menu_t cmd_menu;
static tinysh_menu_t cmd_submenus[MAX_CMD_SUBMENUS];
static int submenu_count = 0;
static char submenu_titles[MAX_CMD_SUBMENUS][64];

/* Menu command */
tinysh_cmd_t menu_cmd = {
    0, "menu", "enter menu-based UI mode", 0, 
    menu_cmd_handler, 0, 0, 0
};

/**
 * Apply a color theme and return to previous position
 * Memory-efficient approach: doesn't allocate any memory
 */
static void apply_theme(const char* color_code) {
#if MENU_COLOR_ENABLED
    if (color_code && *color_code) {
        tinysh_printf("%s", color_code);
    }
#else
    (void)color_code; // Prevent unused parameter warning
#endif
}

/**
 * Reset colors to default terminal colors
 */
static void reset_theme(void) {
#if MENU_COLOR_ENABLED
    tinysh_printf("%s", COLOR_RESET);
#endif
}

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
void menu_cmd_handler(int argc, const char **argv) {
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
    
    // Check if we're collecting arguments
    if (collecting_arguments) {
        return handle_argument_input(c);
    }
    
    // Check if we're waiting for a keypress to continue
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
                    menu_select_item((unsigned char)index);
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
    unsigned char i, display_count;  /* Changed from uint8_t */
    
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
        menu_state.scroll_offset = (uint8_t)(menu_state.current_index - display_count + 1);
    } else if (menu_state.current_index < menu_state.scroll_offset) {
        menu_state.scroll_offset = menu_state.current_index;
    }
    
    /* Display menu items */
    for (i = 0; i < display_count; i++) {
        unsigned char item_index = i + menu_state.scroll_offset;  /* Changed from uint8_t */
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
    int menu_separator_len = strlen(MENU_SEPARATOR);

    // Calculate the full title text width
    int title_len = (int)(strlen(MENU_TITLE_PREFIX) + strlen(menu->title) + strlen(MENU_TITLE_SUFFIX));
    
    // Calculate left padding to center the title
    int padding = (menu_separator_len - title_len) / 2;
    if (padding < 0) padding = 0;
    
    // Print the centered title with padding
    tinysh_printf("\r\n");
    for (int i = 0; i < padding; i++) {
        tinysh_printf(" ");
    }
    
    // Apply title theme
    apply_theme(THEME_TITLE);
    tinysh_printf("%s%s%s", MENU_TITLE_PREFIX, menu->title, MENU_TITLE_SUFFIX);
    reset_theme();
    tinysh_printf("\r\n");

    // Center the navigation help
    const char *nav_help = "[↑/↓] Select  [Enter/→] Execute  [q/←] Back";
    int nav_padding = (menu_separator_len - (int)strlen(nav_help)) / 2;
    if (nav_padding < 0) nav_padding = 0;
    
    for (int i = 0; i < nav_padding; i++) {
        tinysh_printf(" ");
    }
    
    // Apply header theme to navigation help
    apply_theme(THEME_HEADER);
    tinysh_printf("%s", nav_help);
    reset_theme();
    tinysh_printf("\r\n%s\r\n", MENU_SEPARATOR);
}

/**
 * Display a single menu item
 */
static void display_menu_item(unsigned char index, char is_selected) {
    tinysh_menu_t *menu = menu_state.current_menu;
    tinysh_menu_item_t *item;
    
    if (index >= menu->item_count) return;
    
    item = &menu->items[index];
    
    // Apply selection theme if this is the selected item
    if (is_selected) {
        apply_theme(THEME_SELECTED);
    } else {
        apply_theme(THEME_NORMAL);
    }

    /* Display selector or index */
    if (is_selected) {
        tinysh_printf("%s ", MENU_SELECTOR);
    } else {
        tinysh_printf("%d ", index);
    }

    /* Display submenu indicator if needed */
    if (item->type & MENU_ITEM_SUBMENU) {
        // Temporarily switch to submenu theme
        reset_theme();
        apply_theme(THEME_SUBMENU);
        tinysh_printf("%s", MENU_SUBMENU_INDICATOR);
        reset_theme();
        
        // Return to original theme
        if (is_selected) {
            apply_theme(THEME_SELECTED);
        } else {
            apply_theme(THEME_NORMAL);
        }
        tinysh_printf(" ");
    } else {
        tinysh_printf("%s ", MENU_INDENT);
    }
    
    /* Display admin indicator if needed */
    if (item->type & MENU_ITEM_ADMIN) {
        // Temporarily switch to admin theme
        reset_theme();
        apply_theme(THEME_ADMIN);
        tinysh_printf("%s", MENU_ADMIN_INDICATOR);
        reset_theme();

        // Return to original theme
        if (is_selected) {
            apply_theme(THEME_SELECTED);
        } else {
            apply_theme(THEME_NORMAL);
        }
        tinysh_printf(" ");
    } else {
        tinysh_printf("  ");
    }
    
    /* Display item title */
    tinysh_printf("%s", item->title);
    
    // Reset colors at end of line
    reset_theme();
    tinysh_printf("\r\n");
}

/**
 * Display menu footer
 */
static void display_menu_footer(void) {
    tinysh_menu_t *menu = menu_state.current_menu;
    
    tinysh_printf("%s\r\n", MENU_SEPARATOR);

    /* Show pagination info if needed */
    if (menu->item_count > MENU_DISPLAY_ITEMS) {
        apply_theme(THEME_FOOTER);
        tinysh_printf("Showing items %d-%d of %d",
                     menu_state.scroll_offset + 1,
                     menu_state.scroll_offset + MENU_DISPLAY_ITEMS > menu->item_count ?
                         menu->item_count : menu_state.scroll_offset + MENU_DISPLAY_ITEMS,
                     menu->item_count);
        reset_theme();
        tinysh_printf("\r\n%s\r\n", MENU_SEPARATOR);
    }
}

/**
 * Start argument collection mode
 */
static void start_argument_collection(const char *title, const char *param_desc, void (*function_arg)(int argc, const char **argv)) {
    collecting_arguments = 1;
    pending_function_arg = function_arg;
    arg_buffer_index = 0;
    memset(arg_buffer, 0, BUFFER_SIZE);
    arg_title = title;
    arg_param_desc = param_desc;
    
    // Display the prompt with colors
    clear_screen();
    apply_theme(THEME_TITLE);
    tinysh_printf("Function: %s", title);
    reset_theme();
    tinysh_printf("\r\n\n");

    if (param_desc && *param_desc) {
        apply_theme(THEME_HEADER);
        tinysh_printf("Parameters: %s", param_desc);
        reset_theme();
        tinysh_printf("\r\n\n");
    }

    apply_theme(THEME_PROMPT);
    tinysh_printf("Enter arguments: ");
    reset_theme();
}

/**
 * Handle input while collecting arguments
 */
static int handle_argument_input(char c) {
    if (!collecting_arguments) return 0;
    
    if (c == '\r' || c == '\n') {
        // End of input - process arguments
        tinysh_printf("\r\n");
        
        // Tokenize the user-entered arguments
        char *user_args[MAX_ARGS-1];  // Leave room for command name
        int user_argc = tinysh_tokenize(arg_buffer, ' ', user_args, MAX_ARGS-1);

        // Create complete argument array with command name as first argument
        const char *args[MAX_ARGS];
        args[0] = arg_title;  // Command name as argv[0]

        // Copy user arguments to positions 1 and beyond
        for (int i = 0; i < user_argc && i < MAX_ARGS-1; i++) {
            args[i+1] = user_args[i];
        }
        
        // Total argument count is user arguments plus command name
        int argc = user_argc + 1;

        if (pending_function_arg) {
            pending_function_arg(argc, args);
        }
        
        // Reset collection state
        collecting_arguments = 0;
        pending_function_arg = NULL;
        
        // Wait for user acknowledgment before returning to menu
        tinysh_printf("\r\n\nPress any key to return to menu...");
        waiting_for_keypress = 1;
        return 1;
    }
    else if (c == 8 || c == 127) { // Backspace
        if (arg_buffer_index > 0) {
            tinysh_printf("\b \b");
            arg_buffer[--arg_buffer_index] = 0;
        }
        return 1;
    }
    else if (arg_buffer_index < BUFFER_SIZE-1 && c >= 32 && c <= 126) { // Printable chars
        tinysh_printf("%c", c);
        arg_buffer[arg_buffer_index++] = c;
        arg_buffer[arg_buffer_index] = 0;
        return 1;
    }
    
    return 1; // Consumed character
}

/**
 * Prompt for arguments and call a function with them
 */
static void prompt_for_arguments(const char *title, const char *param_desc, void (*function_arg)(int argc, const char **argv)) {
    // Just start the argument collection process
    // The actual character processing will happen in tinysh_menu_process_char
    start_argument_collection(title, param_desc, function_arg);
}

/**
 * Execute a menu item action
 */
static void execute_menu_item(tinysh_menu_item_t *item) {
    if (!item) return;

    // Handle Back item
    if (item->type & MENU_ITEM_BACK) {
        tinysh_menu_go_back();
        return;
    }

    // Handle Exit item
    if (item->type & MENU_ITEM_EXIT) {
        tinysh_menu_exit();
        return;
    }
    
    // Handle direct command references
    if (item->type & MENU_ITEM_CMD_REF) {
        tinysh_cmd_t *cmd = item->cmd;
        
        if (!cmd) return;

        if (item->type & MENU_ITEM_SUBMENU) {
            // Handle submenu case - create submenu on demand
            if (submenu_count < MAX_CMD_SUBMENUS) {
                tinysh_menu_t *submenu = &cmd_submenus[submenu_count];
                memset(submenu, 0, sizeof(tinysh_menu_t));

                // Set submenu title
                snprintf(submenu_titles[submenu_count],
                        sizeof(submenu_titles[0]),
                        "%s Commands", cmd->name);
                submenu->title = submenu_titles[submenu_count];

                // Create menu items for child commands
                tinysh_cmd_t *child = cmd->child;
                int child_count = 0;

                while (child && child_count < MENU_MAX_ITEMS - 1) {
                    if (child->name) {
                        submenu->items[child_count].title = child->name;
                        submenu->items[child_count].type = MENU_ITEM_CMD_REF;
                        submenu->items[child_count].cmd = child;

                        #if AUTHENTICATION_ENABLED
                        if (tinysh_is_admin_command(child)) {
                            submenu->items[child_count].type |= MENU_ITEM_ADMIN;
                        }
                        #endif

                        child_count++;
                    }
                    child = child->next;
                }

                // Add back button
                submenu->items[child_count].title = "Back";
                submenu->items[child_count].type = MENU_ITEM_BACK;
                submenu->item_count = (unsigned char)(child_count + 1);
                submenu->parent_index = 0;

                // Navigate to submenu
                menu_state.index_stack[menu_state.menu_stack_idx] = menu_state.current_index;
                menu_state.menu_stack_idx++;
                menu_state.menu_stack[menu_state.menu_stack_idx] = submenu;
                menu_state.current_menu = submenu;
                menu_state.current_index = 0;
                menu_state.scroll_offset = 0;

                submenu_count++;
                tinysh_menu_display();
            }
        } else {
            // Direct command execution
            if (cmd->function) {
                // Check if command expects arguments (has usage info)
                if (cmd->usage && strcmp(cmd->usage, _NOARG_) != 0) {
                    // Command requires arguments - prompt for them
                    prompt_for_arguments(cmd->name, cmd->usage,
                                       (void (*)(int, const char**))cmd->function);
                } else {
                    // No arguments required - execute directly
                    // Save menu mode state
                    char was_in_menu = in_menu_mode;

                    // Temporarily exit menu mode for command execution
                    in_menu_mode = 0;

                    // Execute the command directly
                    const char *argv[1] = {cmd->name};
                    cmd->function(1, argv);

                    // Restore menu mode
                    in_menu_mode = was_in_menu;

                    // Wait for user acknowledgment
                    tinysh_printf("\r\nPress any key to return to menu...");
                    waiting_for_keypress = 1;
                }
            }
        }
        return;
    }

    // Handle submenu navigation
    if (item->type & MENU_ITEM_SUBMENU) {
        // Push menu to stack and navigate
        if (item->submenu && menu_state.menu_stack_idx < MENU_MAX_DEPTH - 1) {
            menu_state.index_stack[menu_state.menu_stack_idx] = menu_state.current_index;
            menu_state.menu_stack_idx++;
            menu_state.menu_stack[menu_state.menu_stack_idx] = item->submenu;
            menu_state.current_menu = item->submenu;
            menu_state.current_index = 0;
            menu_state.scroll_offset = 0;

            tinysh_menu_display();
        }
        return;
    }

    // Handle function calls
    if (item->type & MENU_ITEM_FUNCTION) {
        if (item->function) {
            item->function();

            // Wait for keypress to return to menu
            tinysh_printf("\r\nPress any key to return to menu...");
            waiting_for_keypress = 1;
        }
        return;
    }

    // Handle function with arguments
    if (item->type & MENU_ITEM_FUNCTION_ARG) {
        if (item->function_arg) {
            prompt_for_arguments(item->title, item->params, item->function_arg);
        }
        return;
    }

    // Handle shell command execution
    if (item->type & MENU_ITEM_COMMAND) {
        if (item->command) {
            // Temporarily exit menu mode
            in_menu_mode = 0;

            // Execute the command via the shell
            tinysh_char_in('\r'); // Clear current line

            // Type the command
            const char *cmd = item->command;
            while (*cmd) {
                tinysh_char_in(*cmd++);
            }

            // Execute it
            tinysh_char_in('\r');

            // Return to menu mode
            in_menu_mode = 1;

            // Wait for keypress
            tinysh_printf("\r\nPress any key to return to menu...");
            waiting_for_keypress = 1;
        }
        return;
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
    
    menu_state.current_index = (unsigned char)new_index;  /* Explicit cast */
    
    /* Redisplay menu */
    tinysh_menu_display();
}

/**
 * Directly select a menu item by index
 */
static void menu_select_item(unsigned char index) {
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

/**
 * Generate menu of all registered commands
 * Uses direct references to tinyshell command structures
 */
tinysh_menu_t* tinysh_generate_cmd_menu(void) {
    tinysh_cmd_t *root = tinysh_get_root_cmd();

    // Reset menu structure (reuse the same structure)
    memset(&cmd_menu, 0, sizeof(cmd_menu));
    cmd_menu.title = "Shell Commands";
    cmd_menu.parent_index = 0;

    int count = 0;
    submenu_count = 0;  // Reset the global submenu_count
    
    // First pass: count top-level commands and initialize
    tinysh_cmd_t *cmd = root;
    while (cmd && count < MAX_CMD_MENU_ITEMS) {
        // Skip special commands and NULL names
        if (!cmd->name ||
            strcmp(cmd->name, "menu") == 0 ||
            strcmp(cmd->name, "quit") == 0 ||
            strcmp(cmd->name, "menutest") == 0) {
            cmd = cmd->next;
            continue;
        }

        // Set up menu item with direct reference
        unsigned char item_type = MENU_ITEM_CMD_REF;

        // Check if command has children - create submenu reference
        if (cmd->child) {
            item_type |= MENU_ITEM_SUBMENU;
        }

        #if AUTHENTICATION_ENABLED
        if (tinysh_is_admin_command(cmd)) {
            item_type |= MENU_ITEM_ADMIN;
        }
        #endif
        
        // Add to menu using direct reference - no string duplication
        cmd_menu.items[count].title = cmd->name;
        cmd_menu.items[count].type = item_type;
        cmd_menu.items[count].cmd = cmd;

        count++;
        cmd = cmd->next;
    }
    
    // Add back button
    if (count < MAX_CMD_MENU_ITEMS) {
        cmd_menu.items[count].title = "Back to Main Menu";
        cmd_menu.items[count].type = MENU_ITEM_BACK;
        cmd_menu.item_count = (unsigned char)(count + 1);
    } else {
        cmd_menu.item_count = MAX_CMD_MENU_ITEMS;
    }
    
    return &cmd_menu;
}

/* Storage for command menu and submenus */
static tinysh_menu_t cmd_menu = {
    "Shell Commands",
    { {0} },  // Will be filled dynamically
    0,        // Item count will be set during initialization
    0         // Parent index
};
