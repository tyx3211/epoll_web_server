# Compiler and flags
CC = gcc
# Adjust include paths to point to our own include dir and the sibling project's structure
CFLAGS = -Iinclude -I../epoll_server_core/include -I../epoll_server_core/deps/l8w8jwt/include -I../epoll_server_core/deps/l8w8jwt/lib/mbedtls/include -I../epoll_server_core/deps/yyjson -Wall -Wextra -g

# Linker flags
# We need to tell the linker where to find our custom libraries from the sibling directory.
LDFLAGS = -L../epoll_server_core/lib -lwebserver -ljwt -lpthread -ldl

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
	$(MAKE) -C ../epoll_server_core
	# Now, link our application
	$(CC) $(CFLAGS) $(OBJECTS) -o $@ $(LDFLAGS)

# Compile our application source files into object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	# CFLAGS already includes the path to our own 'include' directory
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: all clean clean_all rebuild

# Clean only user_backend's build artifacts
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

# Clean everything: user_backend + epoll_server_core
clean_all: clean
	$(MAKE) -C ../epoll_server_core clean_lib

# Full rebuild: clean everything and rebuild
rebuild: clean_all all