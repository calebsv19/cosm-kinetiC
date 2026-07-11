PHYSICS_SIM_PATH_OPENER_TEST_SRCS := \
	tests/physics_sim_path_opener_test.c \
	$(SRC_DIR)/app/platform/physics_sim_path_opener.c

.PHONY: test-physics-sim-path-opener

test-physics-sim-path-opener: $(PHYSICS_SIM_PATH_OPENER_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CSTD) $(WARN) $(DEBUG) -D_DARWIN_C_SOURCE -D_POSIX_C_SOURCE=200809L -DPHYSICS_SIM_PATH_OPENER_FORCE_LINUX=1 \
		-I$(INC_DIR) -I$(SRC_DIR) \
		-o $(BUILD_DIR)/physics_sim_path_opener_test $(PHYSICS_SIM_PATH_OPENER_TEST_SRCS)
	$(BUILD_DIR)/physics_sim_path_opener_test
