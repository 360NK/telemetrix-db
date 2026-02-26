# Compilers
CC = clang
CXX = clang++

# Flags
CFLAGS = -Wall -Wextra -O3 -std=c11
CXXFLAGS = -Wall -Wextra -O3 -std=c++17

# Directories
BIN_DIR = bin
BUILD_DIR = build

# The final executable
TARGET = $(BIN_DIR)/telemetrix-db

# Source files
C_SOURCES = src/ingestion/buffer.c
CXX_SOURCES = src/main.cpp

# Convert src/%.c to build/%.o
C_OBJECTS = $(patsubst src/%.c, $(BUILD_DIR)/%.o, $(C_SOURCES))
CXX_OBJECTS = $(patsubst src/%.cpp, $(BUILD_DIR)/%.o, $(CXX_SOURCES))

# Default rule
all: directories $(TARGET)

# Ensure directories exist before compiling
directories:
	@mkdir -p $(BIN_DIR)
	@mkdir -p $(BUILD_DIR)/ingestion
	@mkdir -p $(BUILD_DIR)/storage
	@mkdir -p $(BUILD_DIR)/network

# Link the final binary
$(TARGET): $(C_OBJECTS) $(CXX_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(C_OBJECTS) $(CXX_OBJECTS) -lpthread

# Compile C files
$(BUILD_DIR)/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Compile C++ files
$(BUILD_DIR)/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)