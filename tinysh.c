#include <string.h>
#include <stdint.h> 
#include "tinysh.h"

void (*tinysh_char_out)(unsigned char);	/* Pointer to the output stream */
int (*tinysh_printf)(const char *, ...);

/* Global flag for TinyShell active state */
char tinyshell_active = 1;

#if AUTHENTICATION_ENABLED
/* Authentication state */
unsigned char tinysh_auth_level = TINYSH_AUTH_NONE;

/* Authentication command */
tinysh_cmd_t auth_cmd = {
    0, "auth", "authenticate as admin", "password", auth_cmd_handler, 0, 0, 0
};
#endif

/* Built-in commands */
tinysh_cmd_t help_cmd = {
  0,"help","display help",_NOARG_,help_fnt,0,0,0 
};

tinysh_cmd_t quit_cmd = {
    0, "quit", "exit shell", _NOARG_, quit_fnt, (void *)&tinyshell_active, 0, 0
};

#if HISTORY_DEPTH > 0
// History management using a simplified approach
// Instead of a true ring buffer which is complex to manage with variable length strings,
// we use a rolling index into a fixed array of buffers.
// Optimization: Using a single large buffer and pointers would be more RAM efficient
// but much more complex. For now, we stick to the array but ensure safety.
static char input_buffers[HISTORY_DEPTH][BUFFER_SIZE+1];
static int cur_buf_index=0;
#endif

static char trash_buffer[BUFFER_SIZE+1]={0};
static char context_buffer[BUFFER_SIZE+1]={0};
static int cur_context=0;
static int cur_index=0;
static char prompt[]=_PROMPT_;
static tinysh_cmd_t *root_cmd=&help_cmd;
static tinysh_cmd_t *cur_cmd_ctx=0;
static void *tinysh_arg=0;

int tinysh_strlen(const char *s);
void tinysh_puts(const char *s);
void start_of_line();
int complete_command_line(tinysh_cmd_t *cmd, char *_str);
int help_command_line(tinysh_cmd_t *cmd, char *_str);
void display_child_help(tinysh_cmd_t *cmd);
int exec_command_line(tinysh_cmd_t *cmd, char *_str);
void exec_command(tinysh_cmd_t *cmd, char *str);
void do_context(tinysh_cmd_t *cmd, char *str);
int parse_command(tinysh_cmd_t **_cmd, char **_str);
int strstart(const char *s1, const char *s2);

/* few useful utilities that may be missing */
int tinysh_strlen(const char *s)
{
  int i;
  if (!s) return 0; // Handle NULL pointer
  for(i=0; s[i] && i<BUFFER_SIZE; i++); // Add bound check to prevent overruns
  return i;
}

/* Safer version of puts that checks for NULL */
void tinysh_puts(const char *s)
{
  if (!s || !tinysh_char_out) return;  // Safety check
  
  while(*s)
    tinysh_char_out((unsigned char)*s++);
}

/* callback for help function
 */
void help_fnt(int argc, const char **argv)
{
  ((void)(argc));
  ((void)(argv));

#if AUTOCOMPLATION > 0  
  tinysh_puts("<TAB>        auto-completion\n\r");
#endif // AUTOCOMPLATION > 0  
  tinysh_puts("<cr>         execute\n\r");
#if HISTORY_DEPTH > 1  
  tinysh_puts("CTRL-P       recall previous input line\n\r");
  tinysh_puts("CTRL-N       recall next input line\n\r");
  tinysh_puts("CTRL-D       quit tinyshell\n\r");
#endif  
  tinysh_puts("<any>        treat as input character\n\r");
  tinysh_puts("cmd help sym $   ->string\n\r");
  tinysh_puts("             #   ->integer or float\n\r");
  tinysh_puts("             |   ->or\n\r");
  tinysh_puts("             [..]->options\n\r");
  tinysh_puts("             {..}->grouping\n\r");
  tinysh_puts("eg.\n\r");
  tinysh_puts("?            display help on given or available commands\n\r");
  tinysh_printf("%sreset ?\n\r",_PROMPT_);

}

/**
 * Command handler for the 'quit' command
 */
void quit_fnt(int argc, const char **argv) {
  (void)argc; // Unused
  (void)argv; // Unused
  
  char *val = (char *)tinysh_get_arg();
  if (val != NULL) {
    *val = 0;
  } else {
    // Direct fallback in case arg is NULL
    tinyshell_active = 0;
  }

  // Clear authentication status when quitting
#if AUTHENTICATION_ENABLED
  tinysh_auth_level = TINYSH_AUTH_NONE;
#endif

  if(tinysh_printf)
      tinysh_printf("Exiting shell...\r\n");
  
  /* Exit handled by the application, we just set the flag */
}

/**
* Check if TinyShell is active
*/
char is_tinyshell_active(void) {
  return tinyshell_active;
}

#if AUTHENTICATION_ENABLED
/* Command handler for authentication */
void auth_cmd_handler(int argc, const char **argv) {
    if (argc != 2) {
        tinysh_printf("Usage: auth <password>\r\n");
        return;
    }
    
    if (tinysh_verify_password(argv[1])) {
        tinysh_auth_level = TINYSH_AUTH_ADMIN;
        tinysh_printf("Authentication successful. Admin privileges granted.\r\n");
    } else {
        tinysh_printf("Authentication failed. Incorrect password.\r\n");
    }
}

/* Password verification */
unsigned char tinysh_verify_password(const char *password) {
    if (!password) return 0;
    return (strcmp(password, DEFAULT_ADMIN_PASSWORD) == 0);
}

/* Auth level getter/setter */
void tinysh_set_auth_level(unsigned char level) {
    tinysh_auth_level = level;
}

unsigned char tinysh_get_auth_level(void) {
    return tinysh_auth_level;
}

/* Initialize auth system */
void tinysh_auth_init(void) {
    tinysh_auth_level = TINYSH_AUTH_NONE;
    tinysh_add_command(&auth_cmd);
}
#endif

/*
 */

enum { NULLMATCH,FULLMATCH,PARTMATCH,UNMATCH,MATCH,AMBIG };

/* verify if the non-spaced part of s2 is included at the begining
 * of s1.
 * return FULLMATCH if s2 equal to s1, PARTMATCH if s1 starts with s2
 * but there are remaining chars in s1, UNMATCH if s1 does not start with
 * s2
 */
int strstart(const char *s1, const char *s2)
{
  while(*s1 && *s1==*s2) { s1++; s2++; }

  if(*s2==' ' || *s2==0)
    {
      if(*s1==0)
        return FULLMATCH; /* full match */
      else
#if PARTIAL_MATCH > 0
        return PARTMATCH; /* partial match */
#else
        return UNMATCH;   /* no match */
#endif // PARTIAL_MATCH > 0      
    }
  else
    return UNMATCH;     /* no match */
}

/*
 * check commands at given level with input string.
 * _cmd: point to first command at this level, return matched cmd
 * _str: point to current unprocessed input, return next unprocessed
 */
int parse_command(tinysh_cmd_t **_cmd, char **_str)
{
  char *str=*_str;
  tinysh_cmd_t *cmd;
  tinysh_cmd_t *matched_cmd=0;

  /* first eliminate first blanks */
  while(*str==' ') str++;
  if(!*str)
    {
      *_str=str;
      return NULLMATCH; /* end of input */
    }

  /* first pass: count matches */
  for(cmd=*_cmd;cmd;cmd=cmd->next)
    {
      int ret=strstart(cmd->name,str);

      if(ret==FULLMATCH)
        {
          /* found full match */
          while(*str && *str!=' ') str++;
          while(*str==' ') str++;
          *_str=str;
          *_cmd=cmd;
          return MATCH;
        }
      else if (ret==PARTMATCH)
        {
          if(matched_cmd)
            {
              *_cmd=matched_cmd;
              return AMBIG;
            }
          else
            {
              matched_cmd=cmd;
            }
        }
      else /* UNMATCH */
        {
        }
    }
  if(matched_cmd)
    {
      while(*str && *str!=' ') str++;
      while(*str==' ') str++;
      *_cmd=matched_cmd;
      *_str=str;
      return MATCH;
    }
  else
    return UNMATCH;
}

/* create a context from current input line
 */
void do_context(tinysh_cmd_t *cmd, char *str)
{
  while(*str)
    context_buffer[cur_context++]=*str++;
  context_buffer[cur_context]=0;
  cur_cmd_ctx=cmd;
}

/* execute the given command by calling callback with appropriate
 * arguments
 */
void exec_command(tinysh_cmd_t *cmd, char *str)
{
  if (!cmd) return; // Safety check

#if AUTHENTICATION_ENABLED
  /* Check admin rights */
  if (tinysh_is_admin_command(cmd) && tinysh_auth_level < TINYSH_AUTH_ADMIN) {
      tinysh_printf("Error: Command requires admin privileges\r\n");
      tinysh_printf("Use 'auth <password>' to authenticate\r\n");
      return;
  }

  /* Extract real argument without admin flag */
  void *real_arg = tinysh_is_admin_command(cmd) ? 
                  tinysh_get_real_arg(cmd) : cmd->arg;
#else
  /* When authentication is disabled, just use arg as-is */
  void *real_arg = cmd->arg;
#endif

  /* Continue with normal command execution */
  const char *argv[MAX_ARGS];
  int argc=0;
  int i;

  /* Original command execution code */
  for(i=0;i<BUFFER_SIZE-1 && str[i];i++)
    trash_buffer[i]=str[i];
  trash_buffer[i]=0;
  str=trash_buffer;

  argv[argc++]=cmd->name;
  while(*str && argc<MAX_ARGS)
    {
      while(*str==' ') str++;
      if(*str==0)
        break;
      argv[argc++]=str;
      while(*str!=' ' && *str) str++;
      if(!*str) break;
      *str++=0;
    }
  /* Call command function if present */
  if(cmd->function)
    {
      tinysh_arg = real_arg;
      cmd->function(argc, argv);
    }
}

/* try to execute the current command line
 */
int exec_command_line(tinysh_cmd_t *cmd, char *_str)
{
  char *str=_str;

  while(1)
    {
      int ret;
      ret=parse_command(&cmd,&str);
      if(ret==MATCH) /* found unique match */
        {
          if(cmd)
            {
              if(!cmd->child) /* no sub-command, execute */
                {
                  exec_command(cmd,str);
                  return 0;
                }
              else
                {
                  if(*str==0) /* no more input, this is a context */
                    {
                      do_context(cmd,_str);
                      return 0;
                    }
                  else /* process next command word */
                    {
                      cmd=cmd->child;
                    }
                }
            }
          else /* cmd == 0 */
            {
              return 0;
            }
        }
      else if(ret==AMBIG)
        {
          tinysh_puts("ambiguity: ");
          tinysh_puts(str);
          tinysh_puts("\n\r");

          return 0;
        }
      else if(ret==UNMATCH) /* UNMATCH */
        {
          // Check if str is valid before printing
          if (str && tinysh_char_out) {
            tinysh_puts("no match: ");
            tinysh_puts(str);
            tinysh_puts("\n\r");
          }
          return 0;
        }
      else /* NULLMATCH */

        return 0;
    }
}

/* display help for list of commands
*/
void display_child_help(tinysh_cmd_t *cmd)
{
  tinysh_cmd_t *cm;
  int len=0;

  tinysh_puts("\n\r");
  for(cm=cmd;cm;cm=cm->next)
    if(len<tinysh_strlen(cm->name))
      len=tinysh_strlen(cm->name);
  for(cm=cmd;cm;cm=cm->next)
    if(cm->help)
      {
        int i;
        
        // Add asterisk indicator for admin commands
#if AUTHENTICATION_ENABLED
        if (tinysh_is_admin_command(cm)) {
            tinysh_puts("* ");  // Add admin indicator
        } else {
            tinysh_puts("  ");  // Add spacing for alignment
        }
#else
        tinysh_puts("  ");      // Always add spacing when auth is disabled
#endif
        
        tinysh_puts(cm->name);
        // Adjust padding to maintain alignment with the indicators
        for(i=tinysh_strlen(cm->name);i<len+2;i++)
          tinysh_char_out(' ');
        tinysh_puts(cm->help);
        tinysh_puts("\n\r");
      }
}

/* try to display help for current comand line
 */
int help_command_line(tinysh_cmd_t *cmd, char *_str)
{
  char *str=_str;

  while(1)
    {
      int ret;
      ret=parse_command(&cmd,&str);
      if(ret==MATCH && *str==0) /* found unique match or empty line */
        {
          if(cmd->child) /* display sub-commands help */
            {
              display_child_help(cmd->child);
              return 0;
            }
          else  /* no sub-command, show single help */
            {
              if(*(str-1)!=' ')
                tinysh_char_out(' ');
              if(cmd->usage)
                tinysh_puts(cmd->usage);
              tinysh_puts(": ");
              if(cmd->help)
                tinysh_puts(cmd->help);
              else
                tinysh_puts("no help available");
              tinysh_puts("\n\r");
            }
          return 0;
        }
      else if(ret==MATCH && *str)
        { /* continue processing the line */
          cmd=cmd->child;
        }
      else if(ret==AMBIG)
        {
          tinysh_puts("\nambiguity: ");
          tinysh_puts(str);
          tinysh_puts("\n\r");
          return 0;
        }
      else if(ret==UNMATCH)
        {
          tinysh_puts("\nno match: ");
          tinysh_puts(str);
          tinysh_puts("\n\r");
          return 0;
        }
      else /* NULLMATCH */
        {
          if(cur_cmd_ctx)
            display_child_help(cur_cmd_ctx->child);
          else
            display_child_help(root_cmd);
          return 0;
        }
    }
}

/* try to complete current command line
 */
int complete_command_line(tinysh_cmd_t *cmd, char *_str)
{
  char *str=_str;

  while(1)
    {
      int ret;
      int common_len=BUFFER_SIZE;
      int _str_len;
      int i;
      char *__str=str;

      ret=parse_command(&cmd,&str);
      for(_str_len=0;__str[_str_len]&&__str[_str_len]!=' ';_str_len++);
      if(ret==MATCH && *str)
        {
          cmd=cmd->child;
        }
      else if(ret==AMBIG || ret==MATCH || ret==NULLMATCH)
        {
          tinysh_cmd_t *cm;
          tinysh_cmd_t *matched_cmd=0;
          int nb_match=0;

          for(cm=cmd;cm;cm=cm->next)
            {
              int r=strstart(cm->name,__str);
              if(r==FULLMATCH)
                {
                  for(i=_str_len;cmd->name[i];i++)
                    tinysh_char_in(cmd->name[i]);
                  if(*(str-1)!=' ')
                    tinysh_char_in(' ');
                  if(!cmd->child)
                    {
                      if(cmd->usage)
                        {
                          tinysh_puts(cmd->usage);
                          tinysh_puts("\n\r");
                          return 1;
                        }
                      else
                        return 0;
                    }
                  else
                    {
                      cmd=cmd->child;
                      break;
                    }
                }
              else if(r==PARTMATCH)
                {
                  nb_match++;
                  if(!matched_cmd)
                    {
                      matched_cmd=cm;
                      common_len=tinysh_strlen(cm->name);
                    }
                  else
                    {
                      for(i=_str_len;cm->name[i] && i<common_len &&
                            cm->name[i]==matched_cmd->name[i];i++);
                      if(i<common_len)
                        common_len=i;
                    }
                }
            }
          if(cm)
            continue;
          if(matched_cmd)
            {
              if(_str_len==common_len)
                {
                  tinysh_puts("\n\r");
                  for(cm=cmd;cm;cm=cm->next)
                    {
                      int r=strstart(cm->name,__str);
                      if(r==FULLMATCH || r==PARTMATCH)
                        {
                          tinysh_puts(cm->name);
                          tinysh_puts("\n\r");
                        }
                    }
                  return 1;
                }
              else
                {
                  for(i=_str_len;i<common_len;i++)
                    tinysh_char_in(matched_cmd->name[i]);
                  if(nb_match==1)
                    tinysh_char_in(' ');
                }
            }
          return 0;
        }
      else /* UNMATCH */
        {
          return 0;
        }
    }
}

/* start a new line
 */
void start_of_line()
{
  /* display start of new line */
  tinysh_puts(prompt);
  if(cur_context)
    {
      tinysh_puts(context_buffer);
      tinysh_puts("> ");
    }
  cur_index=0;
}

/* character input
 */
void tinysh_char_in(char c)
{
#if HISTORY_DEPTH > 0
  char *line=input_buffers[cur_buf_index];
#else
  static char line[BUFFER_SIZE+1];
#endif

  // Safety check - ensure output functions are initialized
  if (!tinysh_char_out) {
    return;  // Can't process without output function
  }

  if(c=='\n' || c=='\r') /* validate command */
    {
      tinysh_cmd_t *cmd;

      /* first, echo the newline */
      if(ECHO_INPUT){
          tinysh_puts("\n\r");
      }
      while(*line && *line==' ') line++;
      if(*line) /* not empty line */
        {
          cmd=cur_cmd_ctx?cur_cmd_ctx->child:root_cmd;
          exec_command_line(cmd,line);
#if HISTORY_DEPTH > 0
          cur_buf_index=(cur_buf_index+1)%HISTORY_DEPTH;
          input_buffers[cur_buf_index][0]=0;
#else
          line[0]=0;
#endif
          cur_index=0;
        }
      if(ECHO_INPUT)
        start_of_line();   
      }
  else if(c==TOPCHAR) /* return to top level */
    {
      if(ECHO_INPUT)
        tinysh_char_out((unsigned char)c);

      cur_context = 0;
      cur_cmd_ctx = 0;
      context_buffer[0] = 0;  // Also clear the buffer for consistency
    }
  else if(c==8 || c==127) /* backspace */
    {
      if(cur_index>0)
        {
          tinysh_puts("\b \b");
          cur_index--;
          line[cur_index]=0;
        }
    }
#if HISTORY_DEPTH > 1
  else if(c==16) /* CTRL-P: back in history */
    {
      int prevline=(cur_buf_index+HISTORY_DEPTH-1)%HISTORY_DEPTH;

      if(input_buffers[prevline][0])
        {
          line=input_buffers[prevline];
          /* fill the rest of the line with spaces */
          while(cur_index-->tinysh_strlen(line))
            tinysh_puts("\b \b");
          tinysh_char_out('\r');
          start_of_line();
          tinysh_puts(line);
          cur_index=tinysh_strlen(line);
          cur_buf_index=prevline;
        }
    }
  else if(c==14) /* CTRL-N: next in history */
    {
      int nextline=(cur_buf_index+1)%HISTORY_DEPTH;

      if(input_buffers[nextline][0])
        {
          line=input_buffers[nextline];
          /* fill the rest of the line with spaces */
          while(cur_index-->tinysh_strlen(line))
            tinysh_puts("\b \b");
          tinysh_char_out('\r');
          start_of_line();
          tinysh_puts(line);
          cur_index=tinysh_strlen(line);
          cur_buf_index=nextline;
        }
    }
#endif
  else if(c=='?') /* display help */
    {
      tinysh_cmd_t *cmd;
      cmd=cur_cmd_ctx?cur_cmd_ctx->child:root_cmd;
      help_command_line(cmd,line);
      start_of_line();
      tinysh_puts(line);
      cur_index=tinysh_strlen(line);
    }
#if AUTOCOMPLATION > 0
  else if(c==9 || c=='!') /* TAB: autocompletion */
    {
      tinysh_cmd_t *cmd;
      cmd=cur_cmd_ctx?cur_cmd_ctx->child:root_cmd;
      if(complete_command_line(cmd,line))
        {
          start_of_line();
          tinysh_puts(line);
        }
      cur_index=tinysh_strlen(line);
    }
#endif //AUTOCOMPLATION > 0
  else if(c==4) /* CTRL-D: exit */
    {
      if(ECHO_INPUT)
      {
        tinysh_puts("\r\nQuit shell...\r\n");
      } 
      tinyshell_active = 0;
      // Signal the main loop to exit
      return;  // Let main.c detect this and clean up
    }
  else /* any input character */
    {
      if(cur_index<BUFFER_SIZE)
        {
          if(ECHO_INPUT)
            tinysh_char_out((unsigned char)c);
          line[cur_index++]=c;
          line[cur_index]=0;
        }
    }
}

/* add a new command */
void tinysh_add_command(tinysh_cmd_t *cmd)
{
  tinysh_cmd_t *cm;

  if(cmd->parent)
    {
      cm=cmd->parent->child;
      if(!cm)
        {
          cmd->parent->child=cmd;
        }
      else
        {
          while(cm->next) {
            if(cm == cmd) return; /* Prevent duplicate addition */
            cm=cm->next;
          }
          if(cm == cmd) return;
          cm->next=cmd;
        }
    }
  else if(!root_cmd)
    {
      root_cmd=cmd;
    }
  else
    {
      cm=root_cmd;
      while(cm->next) {
        if(cm == cmd) return; /* Prevent duplicate addition */
        cm=cm->next;
      }
      if(cm == cmd) return;
      cm->next=cmd;
    }
}

/* modify shell prompt
 */
void tinysh_set_prompt(const char *str)
{
  unsigned int i;
  for(i=0;str[i] && i<PROMPT_SIZE;i++)
    prompt[i]=str[i];
  prompt[i]=0;
  /* force prompt display by generating empty command */
  tinysh_char_in('\r');
}

/* return current command argument
 */
void *tinysh_get_arg()
{
  return tinysh_arg;
}

/* string to decimal/hexadecimal conversion
 */
#include <limits.h>

unsigned long tinysh_atoxi(char *s)
{
  int ishex=0;
  unsigned long res=0;

  if(*s==0) return 0;

  if(*s=='0' && *(s+1)=='x')
    {
      ishex=1;
      s+=2;
    }

  while(*s)
    {
      if(ishex) {
          if (res > (ULONG_MAX / 16)) return ULONG_MAX; // Overflow check
          res*=16;
      }
      else {
          if (res > (ULONG_MAX / 10)) return ULONG_MAX; // Overflow check
          res*=10;
      }

      unsigned long digit = 0;
      if(*s>='0' && *s<='9')
          digit = (unsigned long)(*s-'0');
      else if(ishex && *s>='a' && *s<='f')
          digit = (unsigned long)(*s+10-'a');
      else if(ishex && *s>='A' && *s<='F')
          digit = (unsigned long)(*s+10-'A');
      else
        break;

      if (ULONG_MAX - res < digit) return ULONG_MAX; // Overflow check
      res += digit;

      s++;
    }

  return res;
}

static char* tinysh__strtok_r(char* s, const char* delim, char** last)
{
    char *spanp, *tok;
    int c, sc;

    if(s == NULL && (s = *last) == NULL)
    {
        {
            return (NULL);
        }
    }

/*
 * Skip (span) leading delimiters (s += strspn(s, delim), sort of).
 */
cont:
    c = *s++;
    for(spanp = (char*)delim; (sc = *spanp++) != 0;)
    {
        if(c == sc)
        {
            {
                goto cont;
            }
        }
    }

    if(c == 0)
    { /* no non-delimiter characters */
        *last = NULL;
        return (NULL);
    }
    tok = s - 1;

    /*
     * Scan token (scan for delimiters: s += strcspn(s, delim), sort of).
     * Note that delim must have one NUL; we stop if we see that, too.
     */
    for(;;)
    {
        c = *s++;
        spanp = (char*)delim;
        do
        {
            if((sc = *spanp++) == c)
            {
                if(c == 0)
                {
                    {
                        s = NULL;
                    }
                }
                else
                {
                    {
                        s[-1] = '\0';
                    }
                }
                *last = s;
                return (tok);
            }
        } while(sc != 0);
    }
    return (NULL);
    /* NOTREACHED */
}

char* tinysh_strtok(char* s, const char* delim)
{
    static char* last;

    return (tinysh__strtok_r(s, delim, &last));
}

void tinysh_bin8_print(unsigned char v)
{
    unsigned char mask=1<<((sizeof(unsigned char)<<3)-1);
    while(mask) {
        tinysh_printf("%d", (v&mask ? 1 : 0));
        mask >>= 1;
    }
}


void tinysh_bin16_print(unsigned short v)
{
    unsigned short mask=1<<((sizeof(unsigned short)<<3)-1);
    while(mask) {
        tinysh_printf("%d", (v&mask ? 1 : 0));
        mask >>= 1;
    }
}

void tinysh_bin32_print(int v)
{
    int mask=1<<((sizeof(int)<<3)-1);
    while(mask) {
        tinysh_printf("%d", (v&mask ? 1 : 0));
        mask >>= 1;
    }
}

int tinysh_tokenize(char *str, char token, char **vector, int max_arg){
    if (!vector || !str) {
        return 0;  // Handle null pointers
    }

    int argc = 0;

    while (*str && argc < max_arg) {
        // Skip over leading tokens
        while (*str == token) str++;

        if (*str == '\0')
            break;  // End of string

        vector[argc++] = str;

        // Find the end of the current token
        while (*str && *str != token) str++;

        if (!*str) break;  // End of string
        
        *str++ = '\0';  // Null-terminate this token
    }
    
    return argc;
}

void tinysh_float2str(float f, char *str, int len, int precision)
{
    int i = 0;
    int a, b;
    
    if (!str || len <= 0) return;
    
    if (precision < 0) precision = 0;
    if (precision > 10) precision = 10;  // Reasonable limit
    
    // Handle negative numbers
    if(f < 0.0) {
        if(i < len - 1) str[i++] = '-';
        f = -f;
    }
    
    // Extract integer part
    a = (int)f;
    f -= (float)a;
    
    // Convert integer part to string
    if (a == 0) {
        if(i < len - 1) str[i++] = '0';
    } else {
        char temp[12];
        int temp_i = 0;
        
        // Build the integer part in reverse
        while (a > 0 && temp_i < 11) {
            temp[temp_i++] = (char)('0' + (a % 10));
            a /= 10;
        }
        
        // Copy to output string in correct order
        while (temp_i > 0 && i < len - 1) {
            str[i++] = temp[--temp_i];
        }
    }
    
    // Add decimal point and fractional part
    if (precision > 0 && i < len - 1) {
        str[i++] = '.';
        
        // Extract digits of the fractional part
        for (b = 0; b < precision && i < len - 1; b++) {
            f *= 10.0f;
            int digit = (int)f;
            str[i++] = (char)('0' + digit);
            f -= (float)digit;
        }
    }
    
    str[i] = '\0';
}

/**
 * Reset TinyShell context to top level
 * This can be used to recover from broken command contexts
 */
void tinysh_reset_context(void) {
    cur_context = 0;
    cur_cmd_ctx = 0;
    context_buffer[0] = 0;
}

/**
 * Get the root command
 */
tinysh_cmd_t *tinysh_get_root_cmd(void) {
    return root_cmd;
}

#if !AUTHENTICATION_ENABLED
// Stub functions when authentication is disabled
unsigned char tinysh_verify_password(const char *password) {
    (void)password;
    return 0;
}

void tinysh_set_auth_level(unsigned char level) {
    (void)level;
}

unsigned char tinysh_get_auth_level(void) {
    return 0;
}
#endif
