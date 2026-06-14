# =========================
#  fisiCs memory-check audit
# =========================

.PHONY: memory-check-build memory-check-run memory-check-audit

MEMORY_CHECK_FISICS_OVERLAYS := physics-units,memory-check
MEMORY_CHECK_FISICS_FLAGS := --overlay=$(MEMORY_CHECK_FISICS_OVERLAYS)
MEMORY_CHECK_REPORT_DIR := $(BUILD_DIR)/memory_check
MEMORY_CHECK_STDOUT := $(MEMORY_CHECK_REPORT_DIR)/physics_sim.stdout
MEMORY_CHECK_STDERR := $(MEMORY_CHECK_REPORT_DIR)/physics_sim.stderr
MEMORY_CHECK_TEST_BIN := $(MEMORY_CHECK_REPORT_DIR)/soft_body_memory_check_audit
MEMORY_CHECK_TEST_OBJ_DIR := $(MEMORY_CHECK_REPORT_DIR)/obj
MEMORY_CHECK_TEST_OBJS := \
	$(MEMORY_CHECK_TEST_OBJ_DIR)/soft_body_contract_test.o \
	$(MEMORY_CHECK_TEST_OBJ_DIR)/soft_body.o
MEMORY_CHECK_REPORT_POLICY ?= always
FISICS_MEMCHECK_RUNTIME ?= /Users/calebsv/Desktop/CodeWork/fisiCs/build/unsanitized/libfisics_memcheck_runtime.a
FISICS_MEMCHECK_LINK_LIBS ?=

memory-check-build:
	@$(MAKE) FISICS_FLAGS="$(MEMORY_CHECK_FISICS_FLAGS)" FISICS_MEMCHECK_LINK_LIBS="$(FISICS_MEMCHECK_RUNTIME)" -B fisics-build

$(MEMORY_CHECK_TEST_OBJ_DIR)/soft_body_contract_test.o: tests/soft_body_contract_test.c \
		include/physics/soft/soft_body.h
	mkdir -p "$(MEMORY_CHECK_TEST_OBJ_DIR)"
	FISICS_MAX_PROCS=0 $(FISICS) $(MEMORY_CHECK_FISICS_FLAGS) $(FISICS_CFLAGS) -c "$<" -o "$@"

$(MEMORY_CHECK_TEST_OBJ_DIR)/soft_body.o: $(SRC_DIR)/physics/soft/soft_body.c \
		include/physics/soft/soft_body.h
	mkdir -p "$(MEMORY_CHECK_TEST_OBJ_DIR)"
	FISICS_MAX_PROCS=0 $(FISICS) $(MEMORY_CHECK_FISICS_FLAGS) $(FISICS_CFLAGS) -c "$<" -o "$@"

$(MEMORY_CHECK_TEST_BIN): $(MEMORY_CHECK_TEST_OBJS)
	mkdir -p "$(MEMORY_CHECK_REPORT_DIR)"
	$(CLANG) $(LDFLAGS) -o "$@" $(MEMORY_CHECK_TEST_OBJS) $(FISICS_MEMCHECK_RUNTIME) -lm

memory-check-run: memory-check-build
	@$(MAKE) FISICS_FLAGS="$(MEMORY_CHECK_FISICS_FLAGS)" "$(MEMORY_CHECK_TEST_BIN)"
	mkdir -p "$(MEMORY_CHECK_REPORT_DIR)"
	set +e; FISICS_MEMCHECK_REPORT="$(MEMORY_CHECK_REPORT_POLICY)" "$(MEMORY_CHECK_TEST_BIN)" > "$(MEMORY_CHECK_STDOUT)" 2> "$(MEMORY_CHECK_STDERR)"; status=$$?; \
	echo "memory-check stdout: $(MEMORY_CHECK_STDOUT)"; \
	echo "memory-check stderr: $(MEMORY_CHECK_STDERR)"; \
	exit $$status

memory-check-audit: memory-check-build
	@$(MAKE) FISICS_FLAGS="$(MEMORY_CHECK_FISICS_FLAGS)" "$(MEMORY_CHECK_TEST_BIN)"
	mkdir -p "$(MEMORY_CHECK_REPORT_DIR)"
	set +e; FISICS_MEMCHECK_REPORT="$(MEMORY_CHECK_REPORT_POLICY)" "$(MEMORY_CHECK_TEST_BIN)" > "$(MEMORY_CHECK_STDOUT)" 2> "$(MEMORY_CHECK_STDERR)"; status=$$?; \
	echo "memory-check stdout: $(MEMORY_CHECK_STDOUT)"; \
	echo "memory-check stderr: $(MEMORY_CHECK_STDERR)"; \
	echo "memory-check summary:"; \
	grep -E "\\[fisics:memory-check\\] (summary|leak|double free|unknown pointer free)" "$(MEMORY_CHECK_STDERR)" || true; \
	exit $$status
