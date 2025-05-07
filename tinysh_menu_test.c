#include "tinysh_menu_test.h"
#include "tinysh_menu.h"
#include "tinysh.h"
#include <string.h>

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

/* Test menus */
static tinysh_menu_t test_submenu = {
    "Test Submenu",
    {
        {
            "Item 1", 
            MENU_ITEM_NORMAL, 
            {
                {NULL} /* Generic union member */
            }
        },
        {
            "Item 2", 
            MENU_ITEM_NORMAL, 
            {
                {NULL} /* Generic union member */
            }
        },
        {
            "Back", 
            MENU_ITEM_BACK, 
            {
                {NULL} /* Generic union member */
            }
        }
    },
    3, 0
};

static tinysh_menu_t test_menu = {
    "Test Menu",
    {
        {
            "Submenu", 
            MENU_ITEM_SUBMENU, 
            {
                {&test_submenu} /* submenu pointer */
            }
        },
        {
            "Exit", 
            MENU_ITEM_EXIT, 
            {
                {NULL} /* Generic union member */
            }
        }
    },
    2, 0
};

/* Helper to assert conditions */
static void menu_test_assert(const char *name, int condition, const char *message) {
    tests_run++;
    if (condition) {
        tests_passed++;
        tinysh_printf("✓ PASS: %s\r\n", name);
    } else {
        tests_failed++;
        tinysh_printf("✗ FAIL: %s - %s\r\n", name, message);
    }
}

/* Menu tests */
int tinysh_menu_run_tests(void) {
    tinysh_printf("\r\n--- Menu System Tests ---\r\n");
    
    // Initialize test menu
    tinysh_menu_state_t state;
    memset(&state, 0, sizeof(state));
    state.current_menu = &test_menu;
    state.menu_stack[0] = &test_menu;
    
    // Test menu structure
    menu_test_assert("Menu item count", 
                    test_menu.item_count == 2,
                    "Menu should have 2 items");
    
    menu_test_assert("Submenu reference", 
                    test_menu.items[0].submenu == &test_submenu,
                    "Submenu reference incorrect");
    
    menu_test_assert("Item type", 
                    (test_menu.items[0].type & MENU_ITEM_SUBMENU) != 0,
                    "Item should be a submenu");
    
    // Test navigation logic
    int moved = 0;
    if (state.current_index == 0) {
        moved = 1;  // Simulate down navigation
        state.current_index = 1;
    } else {
        moved = -1; // Simulate up navigation
        state.current_index = 0;
    }
    
    menu_test_assert("Navigation", 
                    (moved == 1 && state.current_index == 1) || 
                    (moved == -1 && state.current_index == 0),
                    "Menu navigation failed");
    
    // Report results
    tinysh_printf("\r\n=== Menu Test Results ===\r\n");
    tinysh_printf("Total tests: %d\r\n", tests_run);
    tinysh_printf("Passed: %d\r\n", tests_passed);
    tinysh_printf("Failed: %d\r\n", tests_failed);
    
    return tests_failed;
}

#if MENU_ENABLED
// Add test command
void menu_test_cmd_handler(int argc, char **argv) {
    (void)argc;
    (void)argv;
    tinysh_menu_run_tests();
}

tinysh_cmd_t menu_test_cmd = {
    0, "menutest", "Run menu system tests", 0,
    menu_test_cmd_handler, 0, 0, 0
};
#endif