# TinyShell-NG: Modern Lightweight Embedded Command-Line Interface

TinyShell-NG is a compact, modular command-line shell designed for embedded systems with limited resources. It offers both a traditional command-line interface and a feature-rich menu-based navigation system for improved user experience.


## Key Features

- **Ultra-Lightweight**: Configurable buffer sizes with minimal RAM footprint
- **Visual UI**: ANSI color-based terminal menu interface 
- **Memory Efficient**: Optimized for memory-constrained environments
- **Hierarchical Navigation**: Nested menus with intuitive navigation
- **Authentication System**: Role-based command access control
- **Cross-Platform**: Works on embedded systems and desktop environments
- **Command System**: Parse and execute structured commands
- **Extensible**: Easy to add custom commands and menus

## Architecture

```
├── Core Shell (tinysh.c/h)             # Main shell engine
├── Platform Layer (tiny_port.c/h)      # Platform-specific I/O
├── Menu System (tinysh_menu.c/h)       # Menu UI framework
├── Menu Content (tinysh_menuconf.c)    # Custom menu definitions
├── Configuration (project-conf.h)      # System configuration
└── Tests (tinysh_test.c/h, tinysh_menu_test.c/h)  # Test framework
```

## Quick Start

### Building

```bash
# Clone repository
git clone https://github.com/yourusername/tinysh.git
cd tinysh

# Build the shell
make

# Run with different options
./tinysh_shell            # Run shell in command mode
./tinysh_shell -m         # Run shell in menu mode
./tinysh_shell -t         # Run tests
```

### Custom Build Options

```bash
# Set custom admin password
make ADMIN_PWD=mysecretpassword

# Clean and rebuild
make rebuild
```

## Command Mode Usage

In command mode, TinyShell operates like a traditional command-line shell:

```
TinyShell v0.1.0 starting on Ubuntu
Type '?' for help

tinysh> help
Available commands:
  help [cmd]           : show help for commands
  echo [args]          : echo arguments
  sysinfo              : show system information
  menu                 : enter menu-based UI mode
  test [cmd]           : run tests
  auth                 : authenticate as admin

tinysh> echo Hello World
Hello World

tinysh> sysinfo
System: Ubuntu Linux
TinyShell version: 0.1.0
Buffer size: 256 bytes
History depth: 4 entries
```

## Menu Mode Usage

Menu mode provides a user-friendly interface with arrow-key navigation:

```
=== TinyShell Main Menu ===
 [↑/↓] Select  [Enter] Execute  [q] Back
----------------------------------------------
> ... System
1     Tools
2 ... Commands
3     Set Parameter
4     Exit Menu Mode
----------------------------------------------
```

### Menu Navigation

- **Navigate**: Use arrow keys (↑/↓) or number keys
- **Select**: Press Enter to execute or enter selection
- **Back**: Press ESC or 'q' to go back or exit
- **Admin Items**: Marked with * require authentication

## Implementing Custom Commands

Add your own commands by defining handlers and command structures:

```c
// Define command handler
void my_cmd_handler(int argc, char **argv) {
    tinysh_printf("My command executed with %d arguments\r\n", argc);
    for (int i = 1; i < argc; i++) {
        tinysh_printf("  Arg %d: %s\r\n", i, argv[i]);
    }
}

// Define command structure
tinysh_cmd_t my_cmd = {
    0,                  // Next command pointer
    "mycommand",        // Command name
    "my custom command",// Help text
    "[args...]",        // Arguments help
    my_cmd_handler,     // Handler function
    0,                  // Arguments
    0,                  // Child commands
    0                   // Flags
};

// Register command
tinysh_add_command(&my_cmd);
```

### Adding Child Commands

Create hierarchical commands with parent-child relationships:

```c
// Define child command handler
void child_cmd_handler(int argc, char **argv) {
    tinysh_printf("Child command executed\r\n");
}

// Define parent and child commands
tinysh_cmd_t child_cmd = {
    0, "child", "child command", 0, child_cmd_handler, 0, 0, 0
};

tinysh_cmd_t parent_cmd = {
    0, "parent", "parent command", 0, parent_cmd_handler, 0, &child_cmd, 0
};

// Register parent command (child is referenced by parent)
tinysh_add_command(&parent_cmd);

// Usage: "parent child" will execute the child command
```

## Adding Menu Items

Create custom menus by defining structures in tinysh_menuconf.c:

```c
// Define menu action function
static void my_menu_function(void) {
    tinysh_printf("Menu action executed\r\n");
}

// Define submenu
tinysh_menu_t my_submenu = {
    "My Custom Menu",
    {
        {"Do Something", MENU_ITEM_FUNCTION, .function = my_menu_function},
        {"Run Command", MENU_ITEM_COMMAND, .command = "echo Hello from menu!"},
        {"With Arguments", MENU_ITEM_FUNCTION_ARG, {
            .function_arg = my_function_with_args,
            .params = "name value"
        }},
        {"Back", MENU_ITEM_BACK, {0}}
    },
    4,  /* item_count */
    0   /* parent_index */
};

// Add submenu to main menu
// Find your main_menu definition and add:
{"My Menu", MENU_ITEM_SUBMENU, .submenu = &my_submenu},
```

## Platform Porting

To port TinyShell to a new embedded platform:

1. Implement character I/O functions:
   ```c
   void platform_putchar(unsigned char c) {
       // Platform-specific character output
       UART_SendChar(c);  // For example
   }
   
   int platform_printf(const char *fmt, ...) {
       // Platform-specific formatted output
       // Use your platform's printf functionality
   }
   ```

2. Register the I/O functions:
   ```c
   tinysh_out(platform_putchar);
   tinysh_print_out(platform_printf);
   ```

3. Customize terminal settings in tiny_port.c for your platform

## Configuration

Customize TinyShell behavior in project-conf.h:

```c
// Buffer and history settings
#define BUFFER_SIZE             256    // Input buffer size
#define HISTORY_DEPTH           4      // Command history entries

// Authentication settings
#define AUTHENTICATION_ENABLED  1      // Enable authentication
#define DEFAULT_ADMIN_PASSWORD "admin" // Default admin password

// Menu system settings
#define MENU_ENABLED            1      // Enable menu system
#define MENU_MAX_DEPTH          5      // Maximum menu nesting level
#define MENU_MAX_ITEMS          10     // Maximum items per menu
#define MENU_DISPLAY_ITEMS      5      // Items to show at once
```

## Menu Display Customization

You can customize the appearance of menus by changing the defines in tinysh_menu.h:

```c
#define MENU_TITLE_PREFIX       "=== "
#define MENU_TITLE_SUFFIX       " ==="
#define MENU_SEPARATOR          "----------------------------------------------"
#define MENU_SELECTOR           ">"    // Indicator for selected item
#define MENU_SUBMENU_INDICATOR  "..." // Indicator for submenu items
#define MENU_ADMIN_INDICATOR    "*"    // Indicator for admin-only items
#define MENU_INDENT             "   "  // Indentation for items
```

## Testing

TinyShell includes a built-in test framework:

```bash
# Run all tests
./tinysh_shell -t

# Or from within the shell
tinysh> test run
```

Individual test commands:
- `test parser` - Command parsing tests
- `test history` - Command history tests
- `test commands` - Command execution tests
- `test tokenize` - String tokenization tests
- `test conversion` - Number conversion tests
- `test auth` - Authentication tests
- `menutest` - Menu system tests

## Security Considerations

- The authentication system is simple and designed for convenience, not high security
- Admin passwords are stored in plaintext by default
- Consider enhancing security for production deployments

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add some amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Todo / Future Improvements

- Add command history navigation in menu mode
- Improve memory management for constrained environments
- Enhance tab completion functionality
- Create platform-specific ports for common microcontrollers
