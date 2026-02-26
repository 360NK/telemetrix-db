# Compilers
CC = clang
CXX = clang++

# Flags
CFLAGS = -Wall -Wextra -O3 -std=c11
CXXFLAGS = -Wall -Wextra -O3 -std=c++17

# Directories
BIN_DIR = bin
BUILD_DIR = build

# Targets
MAIN_TARGET = $(BIN_DIR)/telemetrix-db
BENCH_TARGET = $(BIN_DIR)/benchmark_buffer

# Source files
C_SOURCES = src/ingestion/buffer.c
CXX_MAIN_SOURCES = src/main.cpp
CXX_BENCH_SOURCES = tests/benchmark_buffer.cpp

# Object mapping
C_OBJECTS = $(patsubst src/%.c, $(BUILD_DIR)/%.o, $(C_SOURCES))
CXX_MAIN_OBJECTS = $(patsubst src/%.cpp, $(BUILD_DIR)/%.o, $(CXX_MAIN_SOURCES))
CXX_BENCH_OBJECTS = $(patsubst tests/%.cpp, $(BUILD_DIR)/tests/%.o, $(CXX_BENCH_SOURCES))

# Default rule builds both!
all: directories $(MAIN_TARGET) $(BENCH_TARGET)

# Directory creation
directories:
	@mkdir -p $(BIN_DIR)
	@mkdir -p $(BUILD_DIR)/ingestion
	@mkdir -p $(BUILD_DIR)/storage
	@mkdir -p $(BUILD_DIR)/network
	@mkdir -p $(BUILD_DIR)/tests

# 1. Link the Main App
$(MAIN_TARGET): $(C_OBJECTS) $(CXX_MAIN_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $(MAIN_TARGET) $(C_OBJECTS) $(CXX_MAIN_OBJECTS) -lpthread

# 2. Link the Benchmark App
$(BENCH_TARGET): $(C_OBJECTS) $(CXX_BENCH_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $(BENCH_TARGET) $(C_OBJECTS) $(CXX_BENCH_OBJECTS) -lpthread

# Compile C files
$(BUILD_DIR)/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Compile main C++ files
$(BUILD_DIR)/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile test C++ files
$(BUILD_DIR)/tests/%.o: tests/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)