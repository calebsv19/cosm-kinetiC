# Independent reference probes, linked to existing native operators only.
.PHONY: accuracy-reference-probes
accuracy-reference-probes:
	@mkdir -p $(BUILD_DIR)/accuracy-reference
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -ffp-contract=off -Iinclude src/tools/physics_sim_scalar_accuracy_probe.c $(OPEN_ATMOSPHERE3D_SRCS) -lm -o $(BUILD_DIR)/accuracy-reference/scalar
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -ffp-contract=off -Iinclude src/tools/physics_sim_object_accuracy_probe.c src/tools/physics_sim_heated_face.c $(OPEN_ATMOSPHERE3D_SRCS) -lm -o $(BUILD_DIR)/accuracy-reference/object
	$(CC) -std=c11 -O1 -g -Wall -Wextra -Werror -ffp-contract=off -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude src/tools/physics_sim_scalar_accuracy_probe.c $(OPEN_ATMOSPHERE3D_SRCS) -lm -o $(BUILD_DIR)/accuracy-reference/scalar-sanitize
	$(CC) -std=c11 -O1 -g -Wall -Wextra -Werror -ffp-contract=off -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude src/tools/physics_sim_object_accuracy_probe.c src/tools/physics_sim_heated_face.c $(OPEN_ATMOSPHERE3D_SRCS) -lm -o $(BUILD_DIR)/accuracy-reference/object-sanitize

.PHONY: accuracy-compact-probes
accuracy-compact-probes:
	@mkdir -p $(BUILD_DIR)/accuracy-reference
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -ffp-contract=off -Iinclude src/tools/physics_sim_prescribed_transport_probe.c $(OPEN_ATMOSPHERE3D_SRCS) -lm -o $(BUILD_DIR)/accuracy-reference/compact
	$(CC) -std=c11 -O1 -g -Wall -Wextra -Werror -ffp-contract=off -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude src/tools/physics_sim_prescribed_transport_probe.c $(OPEN_ATMOSPHERE3D_SRCS) -lm -o $(BUILD_DIR)/accuracy-reference/compact-sanitize

# Existing native MC/SSPRK2 bounds, source, boundary and rollback controls.
.PHONY: test-scalar-candidate-native test-scalar-candidate-sanitize
test-scalar-candidate-native:
	@mkdir -p $(BUILD_DIR)/scalar-candidate
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -ffp-contract=off -Iinclude tests/cfd_scalar_mc_test.c $(OPEN_ATMOSPHERE3D_SRCS) -lm -o $(BUILD_DIR)/scalar-candidate/native-test
	$(BUILD_DIR)/scalar-candidate/native-test
test-scalar-candidate-sanitize:
	@mkdir -p $(BUILD_DIR)/scalar-candidate
	$(CC) -std=c11 -O1 -g -Wall -Wextra -Werror -ffp-contract=off -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/cfd_scalar_mc_test.c $(OPEN_ATMOSPHERE3D_SRCS) -lm -o $(BUILD_DIR)/scalar-candidate/native-sanitize
	ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 $(BUILD_DIR)/scalar-candidate/native-sanitize
