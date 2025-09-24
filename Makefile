# Compiler and flags
CC = gcc
CFLAGS = -std=c11 -O2 -Wall -Wextra -D_GNU_SOURCE
LDFLAGS = -lpthread -lsqlite3
DEBUG_FLAGS = -g -DDEBUG -O0

# Directories
SRC_DIR = src
UTILS_DIR = utils
SCANNERS_DIR = scanners
TOOLS_DIR = tools
BUILD_DIR = build

# Source files
MAIN_SRC = $(SRC_DIR)/main.c
UTILS_SRC = $(UTILS_DIR)/resolver.c $(UTILS_DIR)/default_ports.c $(UTILS_DIR)/banner_grabber.c
BANNER_TOOL_SRC = $(TOOLS_DIR)/banner_tool.c
BANNER_VIEWER_SRC = $(TOOLS_DIR)/banner_viewer.c
SCANNER_SRCS = $(SCANNERS_DIR)/syn/scanner.c \
               $(SCANNERS_DIR)/ack/scanner.c \
               $(SCANNERS_DIR)/util/packet_creation.c \
               $(SCANNERS_DIR)/xmas/xmas.c

# Object files
UTILS_OBJ = $(BUILD_DIR)/resolver.o $(BUILD_DIR)/default_ports.o $(BUILD_DIR)/banner_grabber.o
SCANNER_OBJS = $(BUILD_DIR)/syn_scanner.o \
               $(BUILD_DIR)/ack_scanner.o \
               $(BUILD_DIR)/packet_creation.o \
               $(BUILD_DIR)/xmas_scanner.o

# Target executable
TARGET = spear
DEBUG_TARGET = spear_debug
BANNER_TOOL = spear-banner
BANNER_VIEWER = spear-banner-viewer

# Default target
all: $(TARGET) tools

# Main build target (with scanner modules)
$(TARGET): $(MAIN_SRC) $(UTILS_DIR)/resolver.c $(UTILS_DIR)/default_ports.c $(UTILS_DIR)/banner_grabber.c $(SCANNERS_DIR)/syn/scanner.c $(SCANNERS_DIR)/ack/scanner.c $(SCANNERS_DIR)/util/packet_creation.c $(SCANNERS_DIR)/xmas/xmas.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Banner grabber tools
tools: $(BANNER_TOOL) $(BANNER_VIEWER)

$(BANNER_TOOL): $(BANNER_TOOL_SRC) $(UTILS_DIR)/banner_grabber.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BANNER_VIEWER): $(BANNER_VIEWER_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Quick build shortcuts
build: all
b: all
fast: perf
f: perf

# Debug build
debug: $(DEBUG_TARGET)

$(DEBUG_TARGET): $(MAIN_SRC) $(UTILS_DIR)/resolver.c $(UTILS_DIR)/default_ports.c $(UTILS_DIR)/banner_grabber.c $(SCANNERS_DIR)/syn/scanner.c $(SCANNERS_DIR)/ack/scanner.c $(SCANNERS_DIR)/util/packet_creation.c $(SCANNERS_DIR)/xmas/xmas.c
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) -o $@ $^ $(LDFLAGS)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Build with all scanner modules (when ready)
full: $(BUILD_DIR) $(TARGET)_full

$(TARGET)_full: $(MAIN_SRC) $(UTILS_OBJ) $(SCANNER_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Utility objects
$(BUILD_DIR)/resolver.o: $(UTILS_DIR)/resolver.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/default_ports.o: $(UTILS_DIR)/default_ports.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/banner_grabber.o: $(UTILS_DIR)/banner_grabber.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Scanner module objects
$(BUILD_DIR)/syn_scanner.o: $(SCANNERS_DIR)/syn/scanner.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/ack_scanner.o: $(SCANNERS_DIR)/ack/scanner.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/packet_creation.o: $(SCANNERS_DIR)/util/packet_creation.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/xmas_scanner.o: $(SCANNERS_DIR)/xmas/xmas.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Performance test builds
perf: clean
perf: CFLAGS += -O3 -march=native -DNDEBUG
perf: $(TARGET)

# Testing targets
test: $(TARGET)
	@echo "Running basic functionality tests..."
	@echo "Testing single port:"
	./$(TARGET) google.com 80
	@echo "\nTesting port range:"
	./$(TARGET) google.com 80-85 5
	@echo "\nTesting domain resolution:"
	./$(TARGET) github.com 22

benchmark: $(TARGET)
	@echo "Running performance benchmarks..."
	@echo "Small range (mutex approach):"
	time ./$(TARGET) scanme.nmap.org 80-90 5
	@echo "\nLarge range (lock-free approach):"
	time ./$(TARGET) scanme.nmap.org 80-200 10

# Installation
install: $(TARGET) tools
	sudo cp $(TARGET) /usr/local/bin/
	sudo cp $(BANNER_TOOL) /usr/local/bin/
	sudo cp $(BANNER_VIEWER) /usr/local/bin/

# Clean up
clean:
	rm -f $(TARGET) $(DEBUG_TARGET) $(TARGET)_full $(BANNER_TOOL) $(BANNER_VIEWER)
	rm -rf $(BUILD_DIR)
	rm -f *.o

# Clean and rebuild
rebuild: clean all

# Help target
help:
	@echo "Available targets:"
	@echo "  all        - Build basic spear scanner and banner tools (default)"
	@echo "  tools      - Build banner grabber tools"
	@echo "  debug      - Build with debug symbols"
	@echo "  full       - Build with all scanner modules"
	@echo "  perf       - Build optimized for performance"
	@echo "  test       - Run basic functionality tests"
	@echo "  benchmark  - Run performance benchmarks"
	@echo "  install    - Install to /usr/local/bin"
	@echo "  clean      - Remove build artifacts"
	@echo "  rebuild    - Clean and rebuild"
	@echo "  help       - Show this help message"

# Force rebuild targets
force: clean all

# Quick shortcuts
run: $(TARGET)
	./$(TARGET)

# Phony targets
.PHONY: all tools debug full perf test benchmark install clean rebuild help build b fast f force run
