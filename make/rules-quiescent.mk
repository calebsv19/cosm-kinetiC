# Explicit policy and discrete kinetic accounting proof; contained BUILD_DIR.
.PHONY: test-quiescent-native test-quiescent-sanitize
test-quiescent-native:
	python3 -B scripts/atomic_output.py -- $(CC) -std=c11 -O2 -Wall -Wextra -Werror -ffp-contract=off -Iinclude tests/cfd_quiescent_energy_test.c $(OPEN_ATMOSPHERE3D_SRCS) -lm -o $(BUILD_DIR)/open-atmosphere/quiescent-test
	$(BUILD_DIR)/open-atmosphere/quiescent-test
test-quiescent-sanitize:
	python3 -B scripts/atomic_output.py -- $(CC) -std=c11 -O1 -g -Wall -Wextra -Werror -ffp-contract=off -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/cfd_quiescent_energy_test.c $(OPEN_ATMOSPHERE3D_SRCS) -lm -o $(BUILD_DIR)/open-atmosphere/quiescent-sanitize
	$(BUILD_DIR)/open-atmosphere/quiescent-sanitize
