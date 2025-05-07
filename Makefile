CC = gcc
CFLAGS = -Wall -Wextra -g -ggdb3
LDFLAGS = 
OBJDIR = obj

# Allow overriding password from command line
ifdef ADMIN_PWD
  CFLAGS += -DDEFAULT_ADMIN_PASSWORD=\"$(ADMIN_PWD)\"
endif

# Source files
SRCS = main.c tinysh.c tiny_port.c tinysh_test.c tinysh_menu.c tinysh_menuconf.c tinysh_menu_test.c
OBJS = $(patsubst %.c,$(OBJDIR)/%.o,$(SRCS))

# Target executable
TARGET = tinysh_shell

# Make sure the obj directory exists
$(shell mkdir -p $(OBJDIR))

# Default target
all: $(TARGET)

# Link object files to create executable
$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

# Compile source files into object files
$(OBJDIR)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up build artifacts
clean:
	rm -f $(TARGET) $(OBJS)
	rm -rf $(OBJDIR)

# Run the shell for testing
run: $(TARGET)
	./$(TARGET)

# Shorthand to run the automated tests
test: $(TARGET)
	./$(TARGET) -test

# Rebuild everything
rebuild: clean all

.PHONY: all clean run test rebuild