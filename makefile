CC=gcc
CFLAGS=-Wall -Wextra -g

# Platform-specific configurations
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    # macOS
    CFLAGS += -I/usr/local/include -I/opt/homebrew/include
    LDFLAGS = -L/usr/local/lib -L/opt/homebrew/lib
    LIBS = -lSDL2 -lSDL2_ttf -lm
else
    # Linux and others
    LIBS = -lSDL2 -lSDL2_ttf -lm
endif

# Target executable
EXEC=solar_system

# Source files - just main.c since we've integrated the quadtree code
SRC=main.c

# Object files
OBJ=$(SRC:.c=.o)

# Default target
all: $(EXEC)

# Link the executable
$(EXEC): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

# Compile source files to object files
%.o: %.c
	$(CC) -c $< $(CFLAGS)

# Clean up
clean:
	rm -f $(OBJ) $(EXEC)

# Make sure clean doesn't fail if files don't exist
.PHONY: all clean