/**
 * TinyShell - Lightweight Embedded Command-Line Shell
 * -------------------------------------------------- 
 * TinyShell provides a command-line interface suited for embedded systems
 * with limited resources. It supports command hierarchies, auto-completion,
 * command history, and optional authentication.
 *
 * Features:
 * - Minimal memory footprint with configurable buffer sizes
 * - Command hierarchy with parent-child relationships
 * - Command history with configurable depth
 * - Optional authentication for privileged commands
 * - Support for auto-completion using TAB key
 *
 * Example Usage:
 * 
 * // Define a command handler
 * void cmd_hello(int argc, char **argv) {
 *     tinysh_printf("Hello, %s!\r\n", argc > 1 ? argv[1] : "World");
 * }
 * 
 * // Define a command
 * tinysh_cmd_t hello_cmd = {
 *     0, "hello", "greet the user", "[name]", cmd_hello, 0, 0, 0
 * };
 * 
 * // Initialize TinyShell
 * tinysh_out(putchar_func);
 * tinysh_print_out(printf_func);
 * 
 * // Add commands
 * tinysh_add_command(&hello_cmd);
 *
 * // Process characters
 * while (1) {
 *     char c = getchar();
 *     tinysh_char_in(c);
 * }
 */


#ifndef TINYSH_H_
#define TINYSH_H_

#include <stdlib.h>
#include <stdint.h> 
#include "project-conf.h"

#define TINYSHELL_VERSION       "0.1.0"

#ifndef AUTOCOMPLATION
#define AUTOCOMPLATION          1
#endif

#ifndef BUFFER_SIZE
#define BUFFER_SIZE             256
#endif

#ifndef HISTORY_DEPTH
#define HISTORY_DEPTH           2
#endif

#ifndef MAX_ARGS
#define MAX_ARGS                8
#endif

#ifndef TOPCHAR
#define TOPCHAR                 '/'
#endif

#ifndef _PROMPT_
#define _PROMPT_                  "tinysh> "
#endif

#ifndef PROMPT_SIZE
#define PROMPT_SIZE               strlen(_PROMPT_)
#endif

#ifndef ECHO_INPUT
#define ECHO_INPUT                1
#endif

#ifndef PARTIAL_MATCH
#define PARTIAL_MATCH             0
#endif

#ifndef AUTHENTICATION_ENABLED
  #define AUTHENTICATION_ENABLED  0
#endif

#define _NOARG_		                "[no-arg]"

/* Command structure definition - MOVED UP to fix dependency issues */
typedef void (*tinysh_fnt_t)(int argc, char **argv);
typedef struct tinysh_cmd_t {
  struct tinysh_cmd_t *parent; /* 0 if top level command */
  char *name;                  /* command input name, not 0 */
  char *help;                  /* help string, can be 0 */
  char *usage;                 /* usage string, can be 0 */
  tinysh_fnt_t function;       /* function to launch on cmd, can be 0 */
  void *arg;                   /* current argument when function called */
  struct tinysh_cmd_t *next;   /* must be set to 0 at init */
  struct tinysh_cmd_t *child;  /* must be set to 0 at init */
} tinysh_cmd_t;


#if AUTHENTICATION_ENABLED
/* Authentication defines and structures */
#ifndef DEFAULT_ADMIN_PASSWORD
#define DEFAULT_ADMIN_PASSWORD    "admin123"  // This will be overridden by project-conf.h
#endif

/* Authentication levels */
#define TINYSH_AUTH_NONE          0  // Not authenticated
#define TINYSH_AUTH_ADMIN         1  // Admin authenticated

/* Authentication session state */
extern unsigned char tinysh_auth_level;

/* Authentication command */
extern tinysh_cmd_t auth_cmd;

/* Authentication functions */
void tinysh_auth_init(void);
unsigned char tinysh_verify_password(const char *password);
void tinysh_set_auth_level(unsigned char level);
unsigned char tinysh_get_auth_level(void);
void auth_cmd_handler(int argc, char **argv);

/* Admin command flag - store in the highest bit of a byte in cmd struct */
#define TINYSH_CMD_ADMIN          0x80U

/* Helper macro to create admin command */
#define TINYSH_ADMIN_CMD(parent, name, help, usage, function, arg) \
    { parent, name, help, usage, function, \
      (void*)(((uintptr_t)(arg)) | (TINYSH_CMD_ADMIN << 24)), 0, 0 }

/* Non-invasive way to check if a command requires admin rights */
static inline unsigned char tinysh_is_admin_command(tinysh_cmd_t *cmd) {
    return (((uintptr_t)(cmd->arg)) >> 24) & TINYSH_CMD_ADMIN;
}

/* Helper to get the actual arg without admin flag */
static inline void* tinysh_get_real_arg(tinysh_cmd_t *cmd) {
    return (void*)((uintptr_t)(cmd->arg) & 0xFFFFFF);
}
#else
/* Stub versions when authentication is disabled */
#define TINYSH_ADMIN_CMD(parent, name, help, usage, function, arg) \
    { parent, name, help, usage, function, arg, 0, 0 }

static inline unsigned char tinysh_is_admin_command(tinysh_cmd_t *cmd) {
    (void)cmd;  // Prevent unused parameter warning
    return 0;  // Always return false when authentication is disabled
}

static inline void* tinysh_get_real_arg(tinysh_cmd_t *cmd) {
    return cmd->arg;  // Just return the arg directly
}

/* Define auth constants even when disabled for compatibility */
#define TINYSH_AUTH_NONE          0
#define TINYSH_AUTH_ADMIN         1
#endif

#define tinysh_out(func)            tinysh_char_out = (void(*)(unsigned char))(func)
#define tinysh_print_out(func)      tinysh_printf = (int(*)(const char * fmt, ...))(func)

extern void (*tinysh_char_out)(unsigned char);
extern int (*tinysh_printf)(const char *, ...);

/* Flag to indicate if TinyShell is active */
extern char tinyshell_active;

/* Built-in commands */
extern tinysh_cmd_t help_cmd;
extern tinysh_cmd_t quit_cmd;

/* Built-in command handlers */
void help_fnt(int argc, char **argv);
void quit_fnt(int argc, char **argv);

/* Functions provided by the tinysh module */
void tinysh_char_in(char c);
void tinysh_add_command(tinysh_cmd_t *cmd);
void tinysh_set_prompt(char *str);
void *tinysh_get_arg(void);

/* Reset shell context to top level */
void tinysh_reset_context(void);

unsigned long tinysh_atoxi(char *s);
void tinysh_bin8_print(unsigned char v);
void tinysh_bin16_print(unsigned short v);
void tinysh_bin32_print(int v);
char *tinysh_strtok(char *s, const char *delim);
int tinysh_tokenize(char *str, char token, char **vector, int max_arg);
char* tinysh_float2str(float f, int precision);
int tinysh_strlen(char *s);

char is_tinyshell_active(void);

#endif // TINYSH_H_
