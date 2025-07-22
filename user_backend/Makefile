# Compiler and flags
CC = gcc
# Adjust include paths to point to our own include dir and the sibling project's structure
CFLAGS = -Iinclude -I../Web_Server_for_Learning/include -I../Web_Server_for_Learning/deps/l8w8jwt/include -I../Web_Server_for_Learning/deps/l8w8jwt/lib/mbedtls/include -Wall -Wextra -g

# Linker flags
# We need to tell the linker where to find our custom libraries from the sibling directory.
LDFLAGS = -L../Web_Server_for_Learning/lib -lwebserver -ljwt -lpthread -ldl

# Directories relative to the user_backend folder
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

# Source files for the application
SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SOURCES))

# Target executable name
TARGET_APP = $(BIN_DIR)/server_app

# Default target: build the app
all: $(TARGET_APP)

# Link the application
# Depends on all our object files and the libraries from the sibling project
$(TARGET_APP): $(OBJECTS)
	@mkdir -p $(BIN_DIR)
	# First, ensure the dependent libraries are built by calling the sibling makefile
	$(MAKE) -C ../Web_Server_for_Learning
	# Now, link our application
	$(CC) $(CFLAGS) $(OBJECTS) -o $@ $(LDFLAGS)

# Compile our application source files into object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	# CFLAGS already includes the path to our own 'include' directory
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: all clean

# Clean the application's build artifacts
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR) 