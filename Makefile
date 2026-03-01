CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -g -Iinclude
LDFLAGS = -lm

SRC_DIR  = src
TST_DIR  = tests
OBJ_DIR  = obj
BIN_DIR  = bin

# All core source files (everything except main.c)
CORE_SRCS = $(SRC_DIR)/btree_core.c \
            $(SRC_DIR)/btree_insert.c \
            $(SRC_DIR)/btree_search.c \
            $(SRC_DIR)/btree_delete.c \
            $(SRC_DIR)/btree_bench.c

CORE_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(CORE_SRCS))

MAIN_OBJ  = $(OBJ_DIR)/main.o
TEST_OBJ  = $(OBJ_DIR)/test_suite.o

TARGET    = $(BIN_DIR)/file_manager
TEST_BIN  = $(BIN_DIR)/run_tests

$(shell mkdir -p $(OBJ_DIR) $(BIN_DIR) output)

# ── Default ─────────────────────────────────────────────────────
all: $(TARGET)

# ── Main executable ──────────────────────────────────────────────
$(TARGET): $(CORE_OBJS) $(MAIN_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# ── Test binary ──────────────────────────────────────────────────
$(TEST_BIN): $(CORE_OBJS) $(TEST_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# ── Object files ─────────────────────────────────────────────────
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJ_DIR)/test_suite.o: $(TST_DIR)/test_suite.c
	$(CC) $(CFLAGS) -c -o $@ $<

# ── Phony targets ────────────────────────────────────────────────
run: $(TARGET)
	./$(TARGET)

test: $(TEST_BIN)
	./$(TEST_BIN)

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
	rm -f output/*.idx output/*.csv

rebuild: clean all

.PHONY: all run test clean rebuild
