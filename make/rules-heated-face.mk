# Bounded development tools; no desktop/worker linkage or retained output cleanup.
HEATED_FACE_SRCS := src/tools/physics_sim_heated_face.c $(OPEN_ATMOSPHERE3D_SRCS)
.PHONY: box-forcing-probe
box-forcing-probe:
	python3 -B scripts/atomic_output.py -- $(CC) -std=c11 -O2 -Wall -Wextra -Werror -ffp-contract=off -DPHYSICS_SIM_OPEN_FULL_GRID_QUALIFIER=1 -DPHYSICS_SIM_OPEN_EXPLICIT_SCALAR_DT=.00125 -Iinclude src/tools/physics_sim_box_forcing_probe.c $(HEATED_FACE_SRCS) -lm -o $(BUILD_DIR)/heated-face/box-forcing-probe
.PHONY: test-heated-face-support-native test-heated-face-support-sanitize
test-heated-face-support-native:
	python3 -B scripts/atomic_output.py -- $(CC) -std=c11 -O2 -Wall -Wextra -Werror -ffp-contract=off -Iinclude tests/cfd_heated_face_support_test.c $(HEATED_FACE_SRCS) -lm -o $(BUILD_DIR)/heated-face/support-test
	$(BUILD_DIR)/heated-face/support-test
test-heated-face-support-sanitize:
	python3 -B scripts/atomic_output.py -- $(CC) -std=c11 -O1 -g -Wall -Wextra -Werror -ffp-contract=off -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/cfd_heated_face_support_test.c $(HEATED_FACE_SRCS) -lm -o $(BUILD_DIR)/heated-face/support-sanitize
	$(BUILD_DIR)/heated-face/support-sanitize
.PHONY: heated-face-probe test-heated-face-native test-heated-face-sanitize test-open-xy-parity
heated-face-probe:
	python3 -B scripts/atomic_output.py -- $(CC) -std=c11 -O2 -Wall -Wextra -Werror -ffp-contract=off -Iinclude src/tools/physics_sim_heated_face_probe.c $(HEATED_FACE_SRCS) -lm -o $(BUILD_DIR)/heated-face/probe
test-heated-face-native:
	python3 -B scripts/atomic_output.py -- $(CC) -std=c11 -O2 -Wall -Wextra -Werror -ffp-contract=off -Iinclude tests/cfd_heated_face_test.c $(HEATED_FACE_SRCS) -lm -o $(BUILD_DIR)/heated-face/test
	$(BUILD_DIR)/heated-face/test
test-heated-face-sanitize:
	python3 -B scripts/atomic_output.py -- $(CC) -std=c11 -O1 -g -Wall -Wextra -Werror -ffp-contract=off -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/cfd_heated_face_test.c $(HEATED_FACE_SRCS) -lm -o $(BUILD_DIR)/heated-face/test-sanitize
	$(BUILD_DIR)/heated-face/test-sanitize
test-open-xy-parity:
	python3 -B scripts/atomic_output.py -- $(CC) -std=c11 -O2 -Wall -Wextra -Werror -ffp-contract=off -Iinclude tests/cfd_open_xy_parity_test.c $(OPEN_ATMOSPHERE3D_SRCS) -lm -o $(BUILD_DIR)/heated-face/xy-parity
	$(BUILD_DIR)/heated-face/xy-parity

.PHONY: prescribed-transport-probe prescribed-transport-sanitize
prescribed-transport-probe:
	python3 -B scripts/atomic_output.py -- $(CC) -std=c11 -O2 -Wall -Wextra -Werror -ffp-contract=off -Iinclude src/tools/physics_sim_prescribed_transport_probe.c $(OPEN_ATMOSPHERE3D_SRCS) -lm -o $(BUILD_DIR)/heated-face/transport-probe
prescribed-transport-sanitize:
	python3 -B scripts/atomic_output.py -- $(CC) -std=c11 -O1 -g -Wall -Wextra -Werror -ffp-contract=off -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude src/tools/physics_sim_prescribed_transport_probe.c $(OPEN_ATMOSPHERE3D_SRCS) -lm -o $(BUILD_DIR)/heated-face/transport-sanitize
.PHONY: test-scalar-mc-native test-scalar-mc-sanitize
test-scalar-mc-native:
	python3 -B scripts/atomic_output.py -- $(CC) -std=c11 -O2 -Wall -Wextra -Werror -ffp-contract=off -Iinclude tests/cfd_scalar_mc_test.c $(OPEN_ATMOSPHERE3D_SRCS) -lm -o $(BUILD_DIR)/heated-face/mc-test
	$(BUILD_DIR)/heated-face/mc-test
test-scalar-mc-sanitize:
	python3 -B scripts/atomic_output.py -- $(CC) -std=c11 -O1 -g -Wall -Wextra -Werror -ffp-contract=off -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/cfd_scalar_mc_test.c $(OPEN_ATMOSPHERE3D_SRCS) -lm -o $(BUILD_DIR)/heated-face/mc-sanitize
	$(BUILD_DIR)/heated-face/mc-sanitize

.PHONY: matched-box-probe test-matched-box-native test-matched-box-sanitize
matched-box-probe:
	python3 -B scripts/atomic_output.py -- $(CC) -std=c11 -O2 -Wall -Wextra -Werror -ffp-contract=off -DPHYSICS_SIM_OPEN_FULL_GRID_QUALIFIER=1 -DPHYSICS_SIM_OPEN_EXPLICIT_SCALAR_DT=.00125 -Iinclude src/tools/physics_sim_matched_box_probe.c $(MATCHED_HEAT_SRCS) -lm -o $(BUILD_DIR)/heated-face/matched-box-probe
test-matched-box-native:
	python3 -B scripts/atomic_output.py -- $(CC) -std=c11 -O2 -Wall -Wextra -Werror -ffp-contract=off -Iinclude tests/cfd_matched_box_test.c $(MATCHED_HEAT_SRCS) -lm -o $(BUILD_DIR)/heated-face/matched-box-test
	$(BUILD_DIR)/heated-face/matched-box-test
test-matched-box-sanitize:
	python3 -B scripts/atomic_output.py -- $(CC) -std=c11 -O1 -g -Wall -Wextra -Werror -ffp-contract=off -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/cfd_matched_box_test.c $(MATCHED_HEAT_SRCS) -lm -o $(BUILD_DIR)/heated-face/matched-box-sanitize
	$(BUILD_DIR)/heated-face/matched-box-sanitize
.PHONY: test-matched-heat-small
test-matched-heat-small:
	python3 -B scripts/atomic_output.py -- $(CC) -std=c11 -O2 -Wall -Wextra -Werror -ffp-contract=off -Iinclude tests/cfd_matched_heat_test.c $(MATCHED_HEAT_SRCS) -lm -o $(BUILD_DIR)/heated-face/matched-heat-small
	$(BUILD_DIR)/heated-face/matched-heat-small
.PHONY: source-edge-probe
source-edge-probe:
	python3 -B scripts/atomic_output.py -- $(CC) -std=c11 -O2 -Wall -Wextra -Werror -ffp-contract=off -Iinclude src/tools/physics_sim_source_edge_probe.c $(MATCHED_HEAT_SRCS) -lm -o $(BUILD_DIR)/heated-face/source-edge-probe
.PHONY: sharp-diffusion-probe sharp-diffusion-sanitize
sharp-diffusion-probe:
	python3 -B scripts/atomic_output.py -- $(CC) -std=c11 -O2 -Wall -Wextra -Werror -ffp-contract=off -Iinclude src/tools/physics_sim_sharp_diffusion_probe.c $(OPEN_ATMOSPHERE3D_SRCS) -lm -o $(BUILD_DIR)/heated-face/sharp-diffusion-probe
sharp-diffusion-sanitize:
	python3 -B scripts/atomic_output.py -- $(CC) -std=c11 -O1 -g -Wall -Wextra -Werror -ffp-contract=off -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude src/tools/physics_sim_sharp_diffusion_probe.c $(OPEN_ATMOSPHERE3D_SRCS) -lm -o $(BUILD_DIR)/heated-face/sharp-diffusion-sanitize
.PHONY: sharp-transport-probe sharp-transport-sanitize
sharp-transport-probe:
	python3 -B scripts/atomic_output.py -- $(CC) -std=c11 -O2 -Wall -Wextra -Werror -ffp-contract=off -Iinclude src/tools/physics_sim_sharp_transport_probe.c $(OPEN_ATMOSPHERE3D_SRCS) -lm -o $(BUILD_DIR)/heated-face/sharp-transport-probe
sharp-transport-sanitize:
	python3 -B scripts/atomic_output.py -- $(CC) -std=c11 -O1 -g -Wall -Wextra -Werror -ffp-contract=off -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude src/tools/physics_sim_sharp_transport_probe.c $(OPEN_ATMOSPHERE3D_SRCS) -lm -o $(BUILD_DIR)/heated-face/sharp-transport-sanitize
.PHONY: scalar-flux-probe
scalar-flux-probe:
	python3 -B scripts/atomic_output.py -- $(CC) -std=c11 -O2 -Wall -Wextra -Werror -ffp-contract=off -Iinclude src/tools/physics_sim_scalar_flux_probe.c $(OPEN_ATMOSPHERE3D_SRCS) -lm -o $(BUILD_DIR)/heated-face/scalar-flux-probe
.PHONY: sharp-transport-thin-probe test-thin-transport-capacity
sharp-transport-thin-probe:
	python3 -B scripts/atomic_output.py -- $(CC) -std=c11 -O2 -Wall -Wextra -Werror -ffp-contract=off -DPHYSICS_SIM_THIN_TRANSPORT_QUALIFIER=1 -Iinclude src/tools/physics_sim_sharp_transport_probe.c $(OPEN_ATMOSPHERE3D_SRCS) -lm -o $(BUILD_DIR)/heated-face/sharp-transport-thin-probe
test-thin-transport-capacity:
	python3 -B scripts/atomic_output.py -- $(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/cfd_thin_transport_capacity_test.c $(OPEN_ATMOSPHERE3D_SRCS) -lm -o $(BUILD_DIR)/heated-face/capacity-default
	$(BUILD_DIR)/heated-face/capacity-default
	python3 -B scripts/atomic_output.py -- $(CC) -std=c11 -O2 -Wall -Wextra -Werror -DPHYSICS_SIM_THIN_TRANSPORT_QUALIFIER=1 -Iinclude tests/cfd_thin_transport_capacity_test.c $(OPEN_ATMOSPHERE3D_SRCS) -lm -o $(BUILD_DIR)/heated-face/capacity-thin
	$(BUILD_DIR)/heated-face/capacity-thin
