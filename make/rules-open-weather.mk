# Explicit open-domain driver proof; no app/default/installed activation.
OPEN_WEATHER_NATIVE_SRCS = tests/cfd_open_weather_integration_test.c src/tools/physics_sim_inert_coupling.c src/tools/physics_sim_heated_face.c $(OPEN_ATMOSPHERE3D_SRCS)
OPEN_WEATHER_COMPONENT_SRCS = tests/cfd_open_weather_io_test.c src/tools/physics_sim_open_weather_io.c $(OPEN_ATMOSPHERE3D_SRCS)
.PHONY: open-weather-workers test-open-weather-native test-open-weather-sanitize
open-weather-workers:
	@mkdir -p $(BUILD_DIR)/open-weather
	python3 -B scripts/atomic_output.py -- $(CC) -std=c11 -O2 -Wall -Wextra -Werror -ffp-contract=off -Iinclude $(JSON_CFLAGS) src/tools/physics_sim_open_atmosphere_worker.c $(OPEN_ATMOSPHERE_FORCING_IO_SRCS) $(INERT_OBJECT_IO_SRCS) $(OPEN_ATMOSPHERE3D_SRCS) $(JSON_LIBS) -lm -o $(BUILD_DIR)/open-weather/worker
	python3 -B scripts/atomic_output.py -- $(CC) -std=c11 -O1 -g -Wall -Wextra -Werror -ffp-contract=off -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude $(JSON_CFLAGS) src/tools/physics_sim_open_atmosphere_worker.c $(OPEN_ATMOSPHERE_FORCING_IO_SRCS) $(INERT_OBJECT_IO_SRCS) $(OPEN_ATMOSPHERE3D_SRCS) $(JSON_LIBS) -lm -o $(BUILD_DIR)/open-weather/worker-sanitize
test-open-weather-native:
	@mkdir -p $(BUILD_DIR)/open-weather
	python3 -B scripts/atomic_output.py -- $(CC) -std=c11 -O2 -Wall -Wextra -Werror -ffp-contract=off -Iinclude $(OPEN_WEATHER_NATIVE_SRCS) -lm -o $(BUILD_DIR)/open-weather/integration-test
	$(BUILD_DIR)/open-weather/integration-test
	python3 -B scripts/atomic_output.py -- $(CC) -std=c11 -O2 -Wall -Wextra -Werror -ffp-contract=off -Iinclude $(JSON_CFLAGS) $(OPEN_WEATHER_COMPONENT_SRCS) $(JSON_LIBS) -lm -o $(BUILD_DIR)/open-weather/component-test
	$(BUILD_DIR)/open-weather/component-test
test-open-weather-sanitize:
	@mkdir -p $(BUILD_DIR)/open-weather
	python3 -B scripts/atomic_output.py -- $(CC) -std=c11 -O1 -g -Wall -Wextra -Werror -ffp-contract=off -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude $(OPEN_WEATHER_NATIVE_SRCS) -lm -o $(BUILD_DIR)/open-weather/integration-sanitize
	$(BUILD_DIR)/open-weather/integration-sanitize
	python3 -B scripts/atomic_output.py -- $(CC) -std=c11 -O1 -g -Wall -Wextra -Werror -ffp-contract=off -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude $(JSON_CFLAGS) $(OPEN_WEATHER_COMPONENT_SRCS) $(JSON_LIBS) -lm -o $(BUILD_DIR)/open-weather/component-sanitize
	$(BUILD_DIR)/open-weather/component-sanitize

.PHONY: scalar-gust128-workers
scalar-gust128-workers:
	@mkdir -p $(BUILD_DIR)/scalar-gust128
	python3 -B scripts/atomic_output.py -- $(CC) -std=c11 -DPHYSICS_SIM_OPEN_FULL_GRID_QUALIFIER -DPHYSICS_SIM_SCALAR_GUST128 -O2 -Wall -Wextra -Werror -ffp-contract=off -Iinclude $(JSON_CFLAGS) src/tools/physics_sim_open_atmosphere_worker.c $(OPEN_ATMOSPHERE_FORCING_IO_SRCS) $(INERT_OBJECT_IO_SRCS) $(OPEN_ATMOSPHERE3D_SRCS) $(JSON_LIBS) -lm -o $(BUILD_DIR)/scalar-gust128/worker
	python3 -B scripts/atomic_output.py -- $(CC) -std=c11 -DPHYSICS_SIM_OPEN_FULL_GRID_QUALIFIER -DPHYSICS_SIM_SCALAR_GUST128 -O1 -g -Wall -Wextra -Werror -ffp-contract=off -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude $(JSON_CFLAGS) src/tools/physics_sim_open_atmosphere_worker.c $(OPEN_ATMOSPHERE_FORCING_IO_SRCS) $(INERT_OBJECT_IO_SRCS) $(OPEN_ATMOSPHERE3D_SRCS) $(JSON_LIBS) -lm -o $(BUILD_DIR)/scalar-gust128/worker-sanitize

.PHONY: scalar-wake128-workers
scalar-wake128-workers:
	@mkdir -p $(BUILD_DIR)/scalar-wake128
	python3 -B scripts/atomic_output.py -- $(CC) -std=c11 -DPHYSICS_SIM_OPEN_FULL_GRID_QUALIFIER -DPHYSICS_SIM_SCALAR_WAKE128 -O2 -Wall -Wextra -Werror -ffp-contract=off -Iinclude $(JSON_CFLAGS) src/tools/physics_sim_open_atmosphere_worker.c $(OPEN_ATMOSPHERE_FORCING_IO_SRCS) $(INERT_OBJECT_IO_SRCS) $(OPEN_ATMOSPHERE3D_SRCS) $(JSON_LIBS) -lm -o $(BUILD_DIR)/scalar-wake128/worker
	python3 -B scripts/atomic_output.py -- $(CC) -std=c11 -DPHYSICS_SIM_OPEN_FULL_GRID_QUALIFIER -DPHYSICS_SIM_SCALAR_WAKE128 -O1 -g -Wall -Wextra -Werror -ffp-contract=off -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude $(JSON_CFLAGS) src/tools/physics_sim_open_atmosphere_worker.c $(OPEN_ATMOSPHERE_FORCING_IO_SRCS) $(INERT_OBJECT_IO_SRCS) $(OPEN_ATMOSPHERE3D_SRCS) $(JSON_LIBS) -lm -o $(BUILD_DIR)/scalar-wake128/worker-sanitize

# Explicit four-cycle lifetime work profile; ordinary and v1 workers stay closed.
.PHONY: scalar-wake128-work-workers
scalar-wake128-work-workers:
	@mkdir -p $(BUILD_DIR)/work128
	python3 -B scripts/atomic_output.py -- $(CC) -std=c11 -DPHYSICS_SIM_OPEN_FULL_GRID_QUALIFIER -DPHYSICS_SIM_SCALAR_WAKE128_WORK -O2 -Wall -Wextra -Werror -ffp-contract=off -Iinclude $(JSON_CFLAGS) src/tools/physics_sim_open_atmosphere_worker.c $(OPEN_ATMOSPHERE_FORCING_IO_SRCS) $(INERT_OBJECT_IO_SRCS) $(OPEN_ATMOSPHERE3D_SRCS) $(JSON_LIBS) -lm -o $(BUILD_DIR)/work128/worker
	python3 -B scripts/atomic_output.py -- $(CC) -std=c11 -DPHYSICS_SIM_OPEN_FULL_GRID_QUALIFIER -DPHYSICS_SIM_SCALAR_WAKE128_WORK -O1 -g -Wall -Wextra -Werror -ffp-contract=off -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude $(JSON_CFLAGS) src/tools/physics_sim_open_atmosphere_worker.c $(OPEN_ATMOSPHERE_FORCING_IO_SRCS) $(INERT_OBJECT_IO_SRCS) $(OPEN_ATMOSPHERE3D_SRCS) $(JSON_LIBS) -lm -o $(BUILD_DIR)/work128/worker-sanitize
