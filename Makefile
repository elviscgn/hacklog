CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -D_XOPEN_SOURCE=700
LDFLAGS = -lncurses -lm

SRC_DIR = src
TEST_DIR = tests
BUILD_DIR = build

# Source files (all .c in src/)
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))

# Test source files
TEST_SRCS = $(wildcard $(TEST_DIR)/*.c)
TEST_OBJS = $(patsubst $(TEST_DIR)/%.c,$(BUILD_DIR)/test_%.o,$(TEST_SRCS))

# Library objects (everything except main.o — used for tests)
LIB_OBJS = $(filter-out $(BUILD_DIR)/main.o,$(OBJS))

BINARY = hacklog
TEST_BINARY = test_runner

.PHONY: all test clean install

all: $(BUILD_DIR) $(BINARY)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BINARY): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

# Test build — link test objects with library objects (no main.o)
$(TEST_BINARY): $(TEST_OBJS) $(LIB_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/test_%.o: $(TEST_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

test: $(BUILD_DIR) $(TEST_BINARY)
	./$(TEST_BINARY)

clean:
	rm -rf $(BUILD_DIR) $(BINARY) $(TEST_BINARY)

install: $(BINARY)
	@if [ -d "$$HOME/.local/bin" ] && echo "$$PATH" | grep -q "$$HOME/.local/bin"; then \
		cp $(BINARY) "$$HOME/.local/bin/$(BINARY)"; \
		echo "Installed to $$HOME/.local/bin/$(BINARY)"; \
	else \
		echo "Installing to /usr/local/bin (may require sudo)"; \
		cp $(BINARY) /usr/local/bin/$(BINARY) 2>/dev/null || \
		(echo "Permission denied. Try: sudo make install" && exit 1); \
	fi
