# Local numerical references; no app refresh, release or retained-root cleanup.
.PHONY: channel-reference-probe
channel-reference-probe:
	python3 -B scripts/atomic_output.py -- $(CC) -std=c11 -O2 -Wall -Wextra -Werror -ffp-contract=off -Iinclude src/tools/physics_sim_channel_reference.c $(OPEN_ATMOSPHERE3D_SRCS) -lm -o $(BUILD_DIR)/flow-validation/channel-reference
.PHONY: inert-reference-probe inert-reference-sanitize
INERT_REFERENCE_SRCS := src/tools/physics_sim_inert_reference.c src/tools/physics_sim_inert_coupling.c src/tools/physics_sim_heated_face.c $(OPEN_ATMOSPHERE3D_SRCS)
inert-reference-probe:
	python3 -B scripts/atomic_output.py -- $(CC) -std=c11 -O2 -Wall -Wextra -Werror -ffp-contract=off -Iinclude $(INERT_REFERENCE_SRCS) -lm -o $(BUILD_DIR)/flow-validation/inert-reference
inert-reference-sanitize:
	python3 -B scripts/atomic_output.py -- $(CC) -std=c11 -O1 -g -Wall -Wextra -Werror -ffp-contract=off -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude $(INERT_REFERENCE_SRCS) -lm -o $(BUILD_DIR)/flow-validation/inert-sanitize
.PHONY: test-half-channel-native test-half-channel-sanitize
test-half-channel-native:
	python3 -B scripts/atomic_output.py -- $(CC) -std=c11 -O2 -Wall -Wextra -Werror -ffp-contract=off -Iinclude tests/cfd_half_channel_test.c $(OPEN_ATMOSPHERE3D_SRCS) -lm -o $(BUILD_DIR)/flow-validation/half-channel-test
	$(BUILD_DIR)/flow-validation/half-channel-test
test-half-channel-sanitize:
	python3 -B scripts/atomic_output.py -- $(CC) -std=c11 -O1 -g -Wall -Wextra -Werror -ffp-contract=off -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/cfd_half_channel_test.c $(OPEN_ATMOSPHERE3D_SRCS) -lm -o $(BUILD_DIR)/flow-validation/half-channel-sanitize
	ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 $(BUILD_DIR)/flow-validation/half-channel-sanitize
.PHONY: source-object-probe source-object-sanitize
SOURCE_OBJECT_SRCS := src/tools/physics_sim_source_object_probe.c src/tools/physics_sim_inert_coupling.c src/tools/physics_sim_heated_face.c $(OPEN_ATMOSPHERE3D_SRCS)
source-object-probe:
	python3 -B scripts/atomic_output.py -- $(CC) -std=c11 -O2 -Wall -Wextra -Werror -ffp-contract=off -Iinclude $(SOURCE_OBJECT_SRCS) -lm -o $(BUILD_DIR)/flow-validation/source-object
source-object-sanitize:
	python3 -B scripts/atomic_output.py -- $(CC) -std=c11 -O1 -g -Wall -Wextra -Werror -ffp-contract=off -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude $(SOURCE_OBJECT_SRCS) -lm -o $(BUILD_DIR)/flow-validation/source-object-sanitize

.PHONY: open-inert-sanitize
open-inert-sanitize:
	python3 -B scripts/atomic_output.py -- $(CC) -std=c11 -O1 -g -Wall -Wextra -Werror -ffp-contract=off -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude $(JSON_CFLAGS) src/tools/physics_sim_open_atmosphere_worker.c $(OPEN_ATMOSPHERE_FORCING_IO_SRCS) $(INERT_OBJECT_IO_SRCS) $(OPEN_ATMOSPHERE3D_SRCS) $(JSON_LIBS) -lm -o $(BUILD_DIR)/open-atmosphere/inert-worker-sanitize
