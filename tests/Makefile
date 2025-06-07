CC := cc
CFLAGS := -std=gnu11 -Wall -Wextra -Werror -g -O0 -fPIC -pthread
TEST_DIR := .
BUILD_DIR := ../build/debug/test
STATIC_LIB := ../build/debug/static/libeavg.a

TEST_SRC := $(wildcard $(TEST_DIR)/*.c)
TEST_OBJ := $(patsubst $(TEST_DIR)/%.c,$(BUILD_DIR)/%.o,$(TEST_SRC))
TEST_BIN := $(BUILD_DIR)/eavg_tests

.PHONY: all
all: $(TEST_BIN)

$(TEST_BIN): $(TEST_OBJ) $(STATIC_LIB)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(TEST_OBJ) -L../build/debug/static -leavg -o $@

$(BUILD_DIR)/%.o: $(TEST_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I../include -c $< -o $@

.PHONY: test clean

test: $(TEST_BIN)
	./$(TEST_BIN)

clean:
	rm -rf build

