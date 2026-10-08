CC ?= cc
PYTHON ?= python3
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2

INC = -Iinclude
OBJ_DIR ?= build

LIB = $(OBJ_DIR)/libcore_sim.a
LIB_OBJ = $(OBJ_DIR)/core_sim.o $(OBJ_DIR)/core_sim_time.o $(OBJ_DIR)/core_sim_schedule.o $(OBJ_DIR)/core_sim_exchange.o $(OBJ_DIR)/core_sim_progress.o
TEST_BIN = $(OBJ_DIR)/core_sim_test
TIME_TEST_BIN = $(OBJ_DIR)/core_sim_time_test
SCHEDULE_TEST_BIN = $(OBJ_DIR)/core_sim_schedule_test
EXCHANGE_TEST_BIN = $(OBJ_DIR)/core_sim_exchange_test
PROGRESS_TEST_BIN = $(OBJ_DIR)/core_sim_progress_test
REFERENCE_BIN = $(OBJ_DIR)/core_sim_reference
REFERENCE_SRC = examples/reference_main.c examples/reference_model.c examples/reference_persistence.c

.PHONY: all clean test reference test-reference

all: $(LIB)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/core_sim.o: src/core_sim.c include/core_sim.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(OBJ_DIR)/core_sim_time.o: src/core_sim_time.c include/core_sim_time.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(OBJ_DIR)/core_sim_schedule.o: src/core_sim_schedule.c include/core_sim_schedule.h include/core_sim_time.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(OBJ_DIR)/core_sim_exchange.o: src/core_sim_exchange.c include/core_sim_exchange.h include/core_sim_schedule.h include/core_sim_time.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(OBJ_DIR)/core_sim_progress.o: src/core_sim_progress.c include/core_sim_progress.h include/core_sim_exchange.h include/core_sim_schedule.h include/core_sim_time.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(LIB): $(LIB_OBJ)
	ar rcs $@ $^

$(TEST_BIN): tests/core_sim_test.c $(LIB) include/core_sim.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) $< $(LIB) -o $@

$(TIME_TEST_BIN): tests/core_sim_time_test.c $(LIB) include/core_sim_time.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) $< $(LIB) -o $@

$(SCHEDULE_TEST_BIN): tests/core_sim_schedule_test.c $(LIB) include/core_sim_schedule.h include/core_sim_time.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) $< $(LIB) -o $@

$(EXCHANGE_TEST_BIN): tests/core_sim_exchange_test.c $(LIB) include/core_sim_exchange.h include/core_sim_schedule.h include/core_sim_time.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) $< $(LIB) -o $@

$(PROGRESS_TEST_BIN): tests/core_sim_progress_test.c $(LIB) include/core_sim_progress.h include/core_sim_exchange.h include/core_sim_schedule.h include/core_sim_time.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) $< $(LIB) -o $@

test: $(TEST_BIN) $(TIME_TEST_BIN) $(SCHEDULE_TEST_BIN) $(EXCHANGE_TEST_BIN) $(PROGRESS_TEST_BIN)
	"$(TEST_BIN)"
	"$(TIME_TEST_BIN)"
	"$(SCHEDULE_TEST_BIN)"
	"$(EXCHANGE_TEST_BIN)"
	"$(PROGRESS_TEST_BIN)"

$(REFERENCE_BIN): $(REFERENCE_SRC) examples/reference_host.h $(LIB) include/core_sim_progress.h include/core_sim_exchange.h include/core_sim_schedule.h include/core_sim_time.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -Iexamples $(REFERENCE_SRC) $(LIB) -o $@

reference: $(REFERENCE_BIN)

test-reference: $(REFERENCE_BIN)
	$(PYTHON) tests/core_sim_reference_test.py "$(abspath $(REFERENCE_BIN))"

clean:
	rm -rf $(OBJ_DIR)
