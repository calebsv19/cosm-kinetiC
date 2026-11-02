# Opt-in native channel/paired-X physics; default atmosphere paths stay in rules-tools.
.PHONY: test-ventilation-native test-ventilation-sanitize test-ventilation test-wind-sensitivity
test-ventilation-native: $(BUILD_DIR)/open-atmosphere/ventilation-test
	$(BUILD_DIR)/open-atmosphere/ventilation-test
$(BUILD_DIR)/open-atmosphere/ventilation-test: tests/cfd_ventilation_test.c $(OPEN_ATMOSPHERE3D_SRCS) include/app/cfd_open_atmosphere3d.h include/app/cfd_open_atmosphere3d_wind.h $(ATMOSPHERE3D_HEADERS)
	@mkdir -p $(BUILD_DIR)/open-atmosphere
	python3 -B scripts/atomic_output.py -- $(CC) -std=c11 -O2 -Wall -Wextra -Werror -ffp-contract=off -Iinclude tests/cfd_ventilation_test.c $(OPEN_ATMOSPHERE3D_SRCS) -lm -o $@
test-ventilation-sanitize: $(BUILD_DIR)/open-atmosphere/ventilation-sanitize
	ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 $(BUILD_DIR)/open-atmosphere/ventilation-sanitize
$(BUILD_DIR)/open-atmosphere/ventilation-sanitize: tests/cfd_ventilation_test.c $(OPEN_ATMOSPHERE3D_SRCS) include/app/cfd_open_atmosphere3d.h include/app/cfd_open_atmosphere3d_wind.h $(ATMOSPHERE3D_HEADERS)
	@mkdir -p $(BUILD_DIR)/open-atmosphere
	python3 -B scripts/atomic_output.py -- $(CC) -std=c11 -O1 -g -Wall -Wextra -Werror -ffp-contract=off -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/cfd_ventilation_test.c $(OPEN_ATMOSPHERE3D_SRCS) -lm -o $@
test-wind-sensitivity:
	python3 -B tests/test_wind_sensitivity.py
test-ventilation: open-atmosphere-worker coupling-framework-worker test-ventilation-native
	FIRE_FLUID_GROWTH_ROOT="$(FIRE_FLUID_GROWTH_ROOT)" FIRE_FLUID_FIRE_WORKER="$(FIRE_FLUID_FIRE_WORKER)" FIRE_FLUID_OPEN_WORKER="$(abspath $(OPEN_ATMOSPHERE3D_WORKER))" FIRE_FLUID_CLOCK_WORKER="$(abspath $(COUPLING_CLOCK_BIN))" python3 -B tests/test_ventilation.py
