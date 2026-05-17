# =========================
#  Shim validation rules
# =========================
SHIM_PARSE_SMOKE_SRC := tests/shim_include_smoke.c
SHIM_PARSE_LOG_DIR := $(BUILD_DIR)/shim
SHIM_PARSE_BASELINE_LOG := $(SHIM_PARSE_LOG_DIR)/parse_headers_baseline.log
SHIM_PARSE_SHADOW_LOG := $(SHIM_PARSE_LOG_DIR)/parse_headers_shadow.log
SHIM_PARSE_FLAGS := $(CSTD) $(WARN) $(DEBUG) -I$(INC_DIR) -I$(SRC_DIR) -I$(SRC_DIR)/tools
SHIM_COMPILE_SUBSET_SRCS := \
	tests/shim_include_smoke.c \
	src/physics/math/math2d.c \
	src/app/app_config.c

shim-parse-smoke:
	@mkdir -p $(SHIM_PARSE_LOG_DIR)
	@echo "Running physics_sim shim parse smoke (shadow include overlay)..."
	@$(CC) $(SHIM_PARSE_FLAGS) -I$(SYS_SHIMS_OVERLAY_DIR) -I$(SYS_SHIMS_INCLUDE_DIR) \
		-fsyntax-only $(SHIM_PARSE_SMOKE_SRC) -H -o /dev/null 2> $(SHIM_PARSE_SHADOW_LOG)
	@grep -E "overlay/include/(stdbool.h|stdint.h|stddef.h|stdarg.h|stdio.h)" $(SHIM_PARSE_SHADOW_LOG) >/dev/null || \
		( echo "shim parse smoke failed: expected overlay header usage not found"; exit 1 )
	@echo "physics_sim shim parse smoke passed."

shim-parse-parity:
	@mkdir -p $(SHIM_PARSE_LOG_DIR)
	@echo "Running physics_sim baseline parse smoke..."
	@$(CC) $(SHIM_PARSE_FLAGS) -fsyntax-only $(SHIM_PARSE_SMOKE_SRC) -H -o /dev/null 2> $(SHIM_PARSE_BASELINE_LOG)
	@echo "Running physics_sim shadow parse smoke..."
	@$(CC) $(SHIM_PARSE_FLAGS) -I$(SYS_SHIMS_OVERLAY_DIR) -I$(SYS_SHIMS_INCLUDE_DIR) \
		-fsyntax-only $(SHIM_PARSE_SMOKE_SRC) -H -o /dev/null 2> $(SHIM_PARSE_SHADOW_LOG)
	@grep -E "overlay/include/(stdbool.h|stdint.h|stddef.h|stdarg.h|stdio.h)" $(SHIM_PARSE_SHADOW_LOG) >/dev/null || \
		( echo "shim parse parity failed: overlay headers not detected"; exit 1 )
	@echo "physics_sim shim parse parity checks passed."

shim-compile-subset:
	@mkdir -p $(SHIM_PARSE_LOG_DIR)
	@echo "Running physics_sim shim compile subset (baseline)..."
	@for src in $(SHIM_COMPILE_SUBSET_SRCS); do \
		obj="$(SHIM_PARSE_LOG_DIR)/$$(basename "$$src" .c)_baseline.o"; \
		$(CC) $(CFLAGS) -c "$$src" -o "$$obj"; \
	done
	@echo "Running physics_sim shim compile subset (shadow)..."
	@for src in $(SHIM_COMPILE_SUBSET_SRCS); do \
		obj="$(SHIM_PARSE_LOG_DIR)/$$(basename "$$src" .c)_shadow.o"; \
		$(CC) $(CFLAGS) -I$(SYS_SHIMS_OVERLAY_DIR) -I$(SYS_SHIMS_INCLUDE_DIR) -DSYS_SHIM_MODE_SHADOW=1 -c "$$src" -o "$$obj"; \
	done
	@echo "physics_sim shim compile subset passed."

shim-gate: shim-parse-parity shim-compile-subset
	@echo "physics_sim shim gate passed."
