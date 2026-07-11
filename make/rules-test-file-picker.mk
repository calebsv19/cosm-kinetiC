PHYSICS_SIM_FILE_PICKER_TEST_SRCS := \
	tests/physics_sim_file_picker_test.c \
	$(SRC_DIR)/app/platform/physics_sim_file_picker.c

.PHONY: test-physics-sim-file-picker

test-physics-sim-file-picker: $(PHYSICS_SIM_FILE_PICKER_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CSTD) $(WARN) $(DEBUG) -DPHYSICS_SIM_FILE_PICKER_FORCE_LINUX=1 \
		-I$(INC_DIR) -I$(SRC_DIR) \
		-o $(BUILD_DIR)/physics_sim_file_picker_test $(PHYSICS_SIM_FILE_PICKER_TEST_SRCS)
	$(BUILD_DIR)/physics_sim_file_picker_test
