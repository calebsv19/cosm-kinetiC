# Diagnostic worker only; the ordinary app/native worker stays unchanged.
FROZEN_TRANSPORT_SRCS = $(filter-out src/app/cfd_open_atmosphere3d.c,$(OPEN_ATMOSPHERE3D_SRCS)) src/tools/physics_sim_frozen_transport.c
.PHONY: transport-replay-workers
transport-replay-workers:
	@mkdir -p $(BUILD_DIR)/transport-replay
	python3 -B scripts/atomic_output.py -- $(CC) -std=c11 -O2 -Wall -Wextra -Werror -ffp-contract=off -DPHYSICS_SIM_FROZEN_TRANSPORT_DIAGNOSTIC -Iinclude $(JSON_CFLAGS) src/tools/physics_sim_open_atmosphere_worker.c $(OPEN_ATMOSPHERE_FORCING_IO_SRCS) $(INERT_OBJECT_IO_SRCS) $(FROZEN_TRANSPORT_SRCS) $(JSON_LIBS) -lm -o $(BUILD_DIR)/transport-replay/frozen-worker
	python3 -B scripts/atomic_output.py -- $(CC) -std=c11 -O1 -g -Wall -Wextra -Werror -ffp-contract=off -DPHYSICS_SIM_FROZEN_TRANSPORT_DIAGNOSTIC -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude $(JSON_CFLAGS) src/tools/physics_sim_open_atmosphere_worker.c $(OPEN_ATMOSPHERE_FORCING_IO_SRCS) $(INERT_OBJECT_IO_SRCS) $(FROZEN_TRANSPORT_SRCS) $(JSON_LIBS) -lm -o $(BUILD_DIR)/transport-replay/frozen-worker-sanitize
