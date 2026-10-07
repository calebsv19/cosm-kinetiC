# =========================
#  CLI tool rules
# =========================
shape_sanity_tool: $(SHAPE_SANITY_TOOL_OBJ)
	@mkdir -p $(dir $(SHAPE_SANITY_TOOL_OBJ))
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

shape_mask_tool: $(SHAPE_MASK_TOOL_OBJ) $(SHAPE_SHARED_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(SHAPE_MASK_TOOL_OBJ) $(SHAPE_SHARED_OBJS) -lm

shape_asset_tool: $(SHAPE_ASSET_TOOL_OBJ) $(SHAPE_SHARED_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(SHAPE_ASSET_TOOL_OBJ) $(SHAPE_SHARED_OBJS) -lm

# legacy alias
shape_import_tool: shape_mask_tool

vf2d_pack_tool: $(CORE_PACK_TOOL_SRCS)
	$(CC) $(CSTD) $(WARN) $(DEBUG) $(CORE_PACK_TOOL_INCS) -o $(VF2D_PACK_TOOL_BIN) $(CORE_PACK_TOOL_SRCS)

vf2d_dataset_tool: $(VF2D_DATASET_TOOL_SRCS)
	$(CC) $(CSTD) $(WARN) $(DEBUG) $(VF2D_DATASET_TOOL_INCS) -o $(VF2D_DATASET_TOOL_BIN) $(VF2D_DATASET_TOOL_SRCS) $(filter-out -lSDL2 -lSDL2_ttf,$(LIBS))

physics_trace_tool: $(PHYSICS_TRACE_TOOL_SRCS)
	$(CC) $(CSTD) $(WARN) $(DEBUG) $(PHYSICS_TRACE_TOOL_INCS) -o $(PHYSICS_TRACE_TOOL_BIN) $(PHYSICS_TRACE_TOOL_SRCS)

physics_sim_headless: $(PHYSICS_SIM_HEADLESS_TOOL_OBJ) $(PHYSICS_SIM_HEADLESS_WORKER_OBJS)
	$(CC) $(LDFLAGS) -o $(PHYSICS_SIM_HEADLESS_TOOL_BIN) $(PHYSICS_SIM_HEADLESS_TOOL_OBJ) $(PHYSICS_SIM_HEADLESS_WORKER_OBJS) $(HEADLESS_WORKER_LIBS)

physics-sim-job-runner: $(PHYSICS_SIM_JOB_RUNNER_TOOL_OBJ) $(PHYSICS_SIM_HEADLESS_WORKER_OBJS)
	$(CC) $(LDFLAGS) -o $(PHYSICS_SIM_JOB_RUNNER_TOOL_BIN) $(PHYSICS_SIM_JOB_RUNNER_TOOL_OBJ) $(PHYSICS_SIM_HEADLESS_WORKER_OBJS) $(HEADLESS_WORKER_LIBS)

runtime_scene_emitter_diag_tool: $(RUNTIME_SCENE_EMITTER_DIAG_TOOL_SRCS)
	$(CC) $(CFLAGS) \
		-I$(CORE_BASE_DIR)/include -I$(CORE_SCENE_DIR)/include -I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include \
		-I$(CORE_MESH_ASSET_DIR)/include -I$(CORE_MESH_PREVIEW_DIR)/include -I$(CORE_MESH_PREVIEW_DIR)/../../shape/external -I$(CORE_IO_DIR)/include \
		-I/opt/homebrew/Cellar/json-c/0.18/include -I/opt/homebrew/Cellar/json-c/0.18/include/json-c \
		-o $(RUNTIME_SCENE_EMITTER_DIAG_TOOL_BIN) $(RUNTIME_SCENE_EMITTER_DIAG_TOOL_SRCS) $(JSON_LIBS) -lm

test-vf2d-dataset-export: vf2d_dataset_tool
	tests/integration/run_vf2d_dataset_export.sh

test-manifest-to-trace-export: physics_trace_tool
	tests/integration/run_manifest_to_trace_export.sh

test-vf2d-pack-dataset-parity: vf2d_pack_tool vf2d_dataset_tool
	tests/integration/run_vf2d_pack_dataset_parity.sh

test-trio-scene-contract-diff:
	tests/integration/run_trio_scene_contract_diff.sh

test-physics-sim-headless-cli: physics_sim_headless
	tests/integration/run_physics_sim_headless_cli.sh

test-physics-sim-headless-scene-project-cache-output: physics_sim_headless
	bash tests/integration/run_physics_sim_headless_scene_project_cache_output.sh

test-physics-sim-headless-wind-analysis: physics_sim_headless
	tests/integration/run_physics_sim_headless_wind_analysis.sh

test-physics-sim-headless-water-mode: physics_sim_headless
	tests/integration/run_physics_sim_headless_water_mode.sh

test-physics-sim-headless-water-object-coupling: physics_sim_headless
	bash tests/integration/run_physics_sim_headless_water_object_coupling.sh

test-physics-sim-headless-water-object-quality-compare: physics_sim_headless
	bash tests/integration/run_physics_sim_headless_water_object_quality_compare.sh

test-physics-sim-headless-wind-long-tunnel-visual: physics_sim_headless
	tests/integration/run_physics_sim_headless_wind_long_tunnel_visual.sh

test-physics-sim-headless-wind-long-tunnel-video: physics_sim_headless
	bash tests/integration/run_physics_sim_headless_wind_long_tunnel_video.sh

test-physics-sim-headless-wind-object-comparison: physics_sim_headless
	bash tests/integration/run_physics_sim_headless_wind_object_comparison.sh

test-physics-sim-headless-wind-mesh-probe: physics_sim_headless
	bash tests/integration/run_physics_sim_headless_wind_mesh_probe.sh

test-physics-sim-headless-wind-orientation-probe: physics_sim_headless
	bash tests/integration/run_physics_sim_headless_wind_orientation_probe.sh

test-physics-sim-headless-dragonwind-orientation-probe: physics_sim_headless
	bash tests/integration/run_physics_sim_headless_dragonwind_orientation_probe.sh

test-physics-sim-job-runner-smoke: physics-sim-job-runner physics_sim_headless
	tests/integration/run_physics_sim_job_runner_smoke.sh

test-physics-sim-job-runner-bundle-smoke: physics-sim-job-runner physics_sim_headless
	tests/integration/run_physics_sim_job_runner_bundle_smoke.sh

test-physics-sim-job-runner-policy: physics-sim-job-runner physics_sim_headless
	tests/integration/run_physics_sim_job_runner_policy.sh

vf2d_to_pack: vf2d_pack_tool
	@if [ -z "$(VF2D)" ] || [ -z "$(PACK)" ]; then \
		echo "usage: make vf2d_to_pack VF2D=/path/frame.vf2d PACK=/path/out.pack [MANIFEST=/path/manifest.json]"; \
		exit 1; \
	fi
	@if [ ! -f "$(VF2D)" ]; then \
		echo "vf2d_to_pack: VF2D file not found: $(VF2D)"; \
		exit 1; \
	fi
	@if [ -n "$(MANIFEST)" ] && [ ! -f "$(MANIFEST)" ]; then \
		echo "vf2d_to_pack: MANIFEST file not found: $(MANIFEST)"; \
		exit 1; \
	fi
	@mkdir -p "$$(dirname "$(PACK)")"
	@if [ -n "$(MANIFEST)" ]; then \
		./$(VF2D_PACK_TOOL_BIN) "$(VF2D)" "$(PACK)" "$(MANIFEST)"; \
	else \
		./$(VF2D_PACK_TOOL_BIN) "$(VF2D)" "$(PACK)"; \
	fi

manifest_to_trace: physics_trace_tool
	@if [ -z "$(MANIFEST)" ] || [ -z "$(TRACE)" ]; then \
		echo "usage: make manifest_to_trace MANIFEST=/path/manifest.json TRACE=/path/output.trace.pack [ITER=20]"; \
		exit 1; \
	fi
	@if [ ! -f "$(MANIFEST)" ]; then \
		echo "manifest_to_trace: MANIFEST file not found: $(MANIFEST)"; \
		exit 1; \
	fi
	@mkdir -p "$$(dirname "$(TRACE)")"
	@if [ -n "$(ITER)" ]; then \
		./$(PHYSICS_TRACE_TOOL_BIN) "$(MANIFEST)" "$(TRACE)" "$(ITER)"; \
	else \
		./$(PHYSICS_TRACE_TOOL_BIN) "$(MANIFEST)" "$(TRACE)"; \
	fi

# Trusted-local S1 background simulation owner.
SESSION_WORKER_BIN ?= physics_sim_session_worker
$(SESSION_WORKER_BIN): $(BUILD_DIR)/tools/cli/physics_sim_session_worker.o $(PHYSICS_SIM_HEADLESS_WORKER_OBJS) make/flags.mk
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) -o $@ $(filter %.o,$^) $(HEADLESS_WORKER_LIBS)

.PHONY: physics-sim-session-worker-optimized
physics-sim-session-worker-optimized: test-json-container-ownership
	$(MAKE) BUILD_DIR=build/cfd-optimized CFD_BUILD_OPT=1 SESSION_WORKER_BIN=$(abspath build/cfd-optimized/physics_sim_session_worker) $(abspath build/cfd-optimized/physics_sim_session_worker)


test-agent-session: physics_sim_session_worker
	python3 -m unittest discover -s tests -p 'test_agent_session*.py' -v

.PHONY: test-agent-session test-session-observation
test-session-observation: $(PHYSICS_SIM_HEADLESS_WORKER_OBJS)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $(BUILD_DIR)/session_observation_test tests/session_observation_test.c $^ $(HEADLESS_WORKER_LIBS)
	$(BUILD_DIR)/session_observation_test

.PHONY: test-solver-qualification test-agent-qualification
test-solver-qualification: $(PHYSICS_SIM_HEADLESS_WORKER_OBJS)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $(BUILD_DIR)/solver_qualification_test tests/solver_qualification_test.c $^ $(HEADLESS_WORKER_LIBS)
	$(BUILD_DIR)/solver_qualification_test

test-agent-qualification: physics_sim_session_worker
	python3 -B tests/test_agent_qualification.py

.PHONY: test-cfd-channel test-agent-channel
test-cfd-channel:
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Iinclude -o $(BUILD_DIR)/cfd_channel_test tests/cfd_channel_test.c src/app/cfd_channel.c -lm
	$(BUILD_DIR)/cfd_channel_test

test-agent-channel: physics_sim_session_worker
	python3 -B tests/test_agent_channel.py

.PHONY: test-cfd-mac2d
test-cfd-mac2d:
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Iinclude -o $(BUILD_DIR)/cfd_mac2d_test tests/cfd_mac2d_test.c src/app/cfd_mac2d.c src/app/cfd_channel.c -lm
	$(BUILD_DIR)/cfd_mac2d_test

.PHONY: test-agent-mac2d
test-agent-mac2d: physics_sim_session_worker
	python3 -B tests/test_agent_mac2d.py

.PHONY: test-cfd-mac2d-transient
test-cfd-mac2d-transient:
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude -o $(BUILD_DIR)/cfd_mac2d_transient tests/cfd_mac2d_transient.c src/app/cfd_mac2d.c src/app/cfd_channel.c -lm
	python3 -B scripts/qualify_mac2d_transient.py

.PHONY: test-cfd-mac2d-manufactured
test-cfd-mac2d-manufactured:
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude -o $(BUILD_DIR)/cfd_mac2d_manufactured_upwind tests/cfd_mac2d_manufactured.c src/app/cfd_mac2d.c src/app/cfd_channel.c -lm
	$(CC) -std=c11 -O2 -Wall -Wextra -Wpedantic -DCFD_MAC2D_VERIFY_CENTERED -Iinclude -o $(BUILD_DIR)/cfd_mac2d_manufactured_centered tests/cfd_mac2d_manufactured.c src/app/cfd_mac2d.c src/app/cfd_channel.c -lm
	$(CC) -std=c11 -O2 -Wall -Wextra -Wpedantic -DCFD_MAC2D_VERIFY_LIMITED -Iinclude -o $(BUILD_DIR)/cfd_mac2d_manufactured_limited tests/cfd_mac2d_manufactured.c src/app/cfd_mac2d.c src/app/cfd_channel.c -lm
	python3 -B scripts/qualify_mac2d_manufactured.py

.PHONY: test-cfd-surface-force
test-cfd-surface-force:
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude -o $(BUILD_DIR)/cfd_surface_force_test tests/cfd_surface_force_test.c src/app/cfd_surface_force.c src/app/cfd_channel.c -lm
	$(BUILD_DIR)/cfd_surface_force_test

.PHONY: test-cfd-mac2d-obstacle
test-cfd-mac2d-obstacle:
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude -o $(BUILD_DIR)/cfd_mac2d_obstacle_test tests/cfd_mac2d_obstacle_test.c src/app/cfd_mac2d.c src/app/cfd_channel.c -lm
	$(BUILD_DIR)/cfd_mac2d_obstacle_test

.PHONY: test-cfd-mac2d-obstacle-step
test-cfd-mac2d-obstacle-step:
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude -o $(BUILD_DIR)/cfd_mac2d_obstacle_step_test tests/cfd_mac2d_obstacle_step_test.c src/app/cfd_mac2d.c src/app/cfd_channel.c -lm
	$(BUILD_DIR)/cfd_mac2d_obstacle_step_test

.PHONY: test-cfd-mac2d-boundary-pressure
test-cfd-mac2d-boundary-pressure:
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude -o $(BUILD_DIR)/cfd_mac2d_boundary_pressure_test tests/cfd_mac2d_boundary_pressure_test.c src/app/cfd_mac2d.c src/app/cfd_channel.c -lm
	$(BUILD_DIR)/cfd_mac2d_boundary_pressure_test

.PHONY: test-cfd-mac2d-force-accuracy
test-cfd-mac2d-force-accuracy:
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude -o $(BUILD_DIR)/cfd_mac2d_force_accuracy_test tests/cfd_mac2d_force_accuracy_test.c src/app/cfd_mac2d_force_check.c src/app/cfd_mac2d.c src/app/cfd_channel.c -lm
	$(BUILD_DIR)/cfd_mac2d_force_accuracy_test

.PHONY: test-cfd-open2d
test-cfd-open2d:
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude -o $(BUILD_DIR)/cfd_open2d_test tests/cfd_open2d_test.c src/app/cfd_open2d.c src/app/cfd_pressure_mg.c -lm
	$(BUILD_DIR)/cfd_open2d_test

.PHONY: qualify-cfd-mac2d-force
qualify-cfd-mac2d-force:
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude -o $(BUILD_DIR)/cfd_mac2d_force_accuracy_test tests/cfd_mac2d_force_accuracy_test.c src/app/cfd_mac2d_force_check.c src/app/cfd_mac2d.c src/app/cfd_channel.c -lm
	python3 -B scripts/qualify_cfd_force.py

.PHONY: test-agent-cfd-lab
test-agent-cfd-lab: physics_sim_session_worker
	python3 -B tests/test_agent_cfd_lab.py

.PHONY: test-cfd-open2d-exit
test-cfd-open2d-exit:
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude -o $(BUILD_DIR)/cfd_open2d_exit_test tests/cfd_open2d_exit_test.c src/app/cfd_open2d.c src/app/cfd_pressure_mg.c src/app/cfd_open2d_budget.c -lm
	$(BUILD_DIR)/cfd_open2d_exit_test
	$(BUILD_DIR)/cfd_open2d_exit_test 32
	$(BUILD_DIR)/cfd_open2d_exit_test 32 .001

.PHONY: test-cfd-mac2d-corner
test-cfd-mac2d-corner:
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude -o $(BUILD_DIR)/cfd_mac2d_corner_test tests/cfd_mac2d_corner_test.c src/app/cfd_mac2d.c src/app/cfd_channel.c -lm
	$(BUILD_DIR)/cfd_mac2d_corner_test

.PHONY: test-cfd-open2d-obstacle
test-cfd-open2d-obstacle:
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude -o $(BUILD_DIR)/cfd_open2d_obstacle_test tests/cfd_open2d_obstacle_test.c src/app/cfd_open2d.c src/app/cfd_pressure_mg.c src/app/cfd_open2d_force_check.c src/app/cfd_mac2d_force_check.c src/app/cfd_mac2d.c src/app/cfd_channel.c -lm
	$(BUILD_DIR)/cfd_open2d_obstacle_test

.PHONY: test-cfd-open2d-budget
test-cfd-open2d-budget:
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude -o $(BUILD_DIR)/cfd_open2d_budget_test tests/cfd_open2d_budget_test.c src/app/cfd_open2d_budget.c src/app/cfd_open2d.c src/app/cfd_pressure_mg.c -lm
	$(BUILD_DIR)/cfd_open2d_budget_test

.PHONY: test-cfd-open2d-exit-centered
test-cfd-open2d-exit-centered:
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Wpedantic -DCFD_OPEN2D_VERIFY_CENTERED -DCFD_OPEN2D_VERIFY_COPY_OUTLET -Iinclude -o $(BUILD_DIR)/cfd_open2d_exit_centered tests/cfd_open2d_exit_test.c src/app/cfd_open2d.c src/app/cfd_pressure_mg.c src/app/cfd_open2d_budget.c -lm
	$(BUILD_DIR)/cfd_open2d_exit_centered 24
	$(BUILD_DIR)/cfd_open2d_exit_centered 32
	$(BUILD_DIR)/cfd_open2d_exit_centered 32 .001

.PHONY: test-cfd-momentum-flux
test-cfd-momentum-flux:
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude -o $(BUILD_DIR)/cfd_momentum_flux_test tests/cfd_momentum_flux_test.c -lm
	$(BUILD_DIR)/cfd_momentum_flux_test

.PHONY: test-cfd-open2d-exit-donor
test-cfd-open2d-exit-donor:
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Wpedantic -DCFD_OPEN2D_VERIFY_DONOR -DCFD_OPEN2D_VERIFY_COPY_OUTLET -Iinclude -o $(BUILD_DIR)/cfd_open2d_exit_donor tests/cfd_open2d_exit_test.c src/app/cfd_open2d.c src/app/cfd_pressure_mg.c src/app/cfd_open2d_budget.c -lm
	$(BUILD_DIR)/cfd_open2d_exit_donor 24
	$(BUILD_DIR)/cfd_open2d_exit_donor 32

.PHONY: test-cfd-open2d-reference
test-cfd-open2d-reference:
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude -o $(BUILD_DIR)/cfd_open2d_reference_test tests/cfd_open2d_reference_test.c src/app/cfd_open2d_budget.c src/app/cfd_open2d.c src/app/cfd_pressure_mg.c src/app/cfd_open2d_force_check.c src/app/cfd_mac2d_force_check.c src/app/cfd_mac2d.c src/app/cfd_channel.c -lm
	$(BUILD_DIR)/cfd_open2d_reference_test 16
	$(BUILD_DIR)/cfd_open2d_reference_test 32
	$(BUILD_DIR)/cfd_open2d_reference_test 64

.PHONY: test-cfd-open2d-force-check
test-cfd-open2d-force-check:
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude -o $(BUILD_DIR)/cfd_open2d_force_check_test tests/cfd_open2d_force_check_test.c src/app/cfd_open2d.c src/app/cfd_pressure_mg.c src/app/cfd_open2d_force_check.c src/app/cfd_mac2d_force_check.c src/app/cfd_mac2d.c src/app/cfd_channel.c -lm
	$(BUILD_DIR)/cfd_open2d_force_check_test

.PHONY: test-agent-open2d
test-agent-open2d: physics_sim_session_worker
	python3 -B tests/test_agent_open2d.py

.PHONY: test-cfd-run-acceptance test-cfd-masked-energy
test-cfd-run-acceptance: physics_sim_session_worker
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude tests/cfd_steady_monitor_test.c src/app/cfd_steady_monitor.c -lm -o $(BUILD_DIR)/cfd_steady_monitor_test
	$(BUILD_DIR)/cfd_steady_monitor_test
	python3 -B tests/test_cfd_acceptance.py
	python3 -B tests/test_agent_open2d.py

test-cfd-masked-energy:
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude tests/cfd_masked_energy_test.c src/app/cfd_open2d_budget.c src/app/cfd_open2d.c src/app/cfd_pressure_mg.c -lm -o $(BUILD_DIR)/cfd_masked_energy_test
	$(BUILD_DIR)/cfd_masked_energy_test

.PHONY: test-cfd-masked-energy-transient
test-cfd-masked-energy-transient:
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude tests/cfd_masked_energy_transient.c src/app/cfd_open2d_budget.c src/app/cfd_open2d.c src/app/cfd_pressure_mg.c -lm -o $(BUILD_DIR)/cfd_masked_energy_transient
	$(BUILD_DIR)/cfd_masked_energy_transient

.PHONY: test-cfd-pressure-guess
test-cfd-pressure-guess:
	$(CC) -std=c11 -O2 -Iinclude -DCFD_OPEN2D_VERIFY_COLD_PRESSURE -Dcfd_open2d_init=cold_init -Dcfd_open2d_step=cold_step -Dcfd_open2d_set_obstacle=cold_obstacle -Dcfd_open2d_destroy=cold_destroy -c src/app/cfd_open2d.c -o $(BUILD_DIR)/cfd_open2d_cold.o
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude tests/cfd_pressure_guess_test.c src/app/cfd_open2d.c src/app/cfd_pressure_mg.c $(BUILD_DIR)/cfd_open2d_cold.o -lm -o $(BUILD_DIR)/cfd_pressure_guess_test
	$(BUILD_DIR)/cfd_pressure_guess_test

.PHONY: test-cfd-pressure-scaling
test-cfd-pressure-scaling:
	$(CC) -std=c11 -O2 -Iinclude -DCFD_OPEN2D_GRID_LIMIT=128 -DCFD_OPEN2D_VERIFY_UNPRECONDITIONED -Dcfd_open2d_init=plain_init -Dcfd_open2d_step=plain_step -Dcfd_open2d_set_obstacle=plain_obstacle -Dcfd_open2d_destroy=plain_destroy -c src/app/cfd_open2d.c -o $(BUILD_DIR)/cfd_open2d_plain.o
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude -DCFD_OPEN2D_GRID_LIMIT=128 tests/cfd_pressure_scaling_test.c src/app/cfd_open2d.c src/app/cfd_pressure_mg.c $(BUILD_DIR)/cfd_open2d_plain.o -lm -o $(BUILD_DIR)/cfd_pressure_scaling_test
	$(BUILD_DIR)/cfd_pressure_scaling_test

.PHONY: test-cfd-pressure-mg
test-cfd-pressure-mg:
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude tests/cfd_pressure_mg_test.c src/app/cfd_pressure_mg.c -lm -o $(BUILD_DIR)/cfd_pressure_mg_test
	$(BUILD_DIR)/cfd_pressure_mg_test

.PHONY: test-cfd-refined-mesh test-cfd-refined-diffusion
test-cfd-refined-mesh:
	@mkdir -p build
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/cfd_refined_mesh_test.c src/app/cfd_refined_mesh.c src/app/cfd_refined_mesh_local.c src/app/cfd_memory.c -lm -o build/cfd_refined_mesh_test
	build/cfd_refined_mesh_test

test-cfd-refined-diffusion:
	@mkdir -p build
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/cfd_refined_diffusion_test.c src/app/cfd_refined_mesh.c src/app/cfd_refined_mesh_local.c src/app/cfd_refined_diffusion.c src/app/cfd_memory.c -lm -o build/cfd_refined_diffusion_test
	build/cfd_refined_diffusion_test

.PHONY: test-cfd-refined-projection
test-cfd-refined-projection:
	@mkdir -p build
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/cfd_refined_projection_test.c src/app/cfd_refined_mesh.c src/app/cfd_refined_mesh_local.c src/app/cfd_refined_diffusion.c src/app/cfd_memory.c -lm -o build/cfd_refined_projection_test
	build/cfd_refined_projection_test

.PHONY: test-cfd-refined-transient test-cfd-refined-transport test-cfd-refined-channel
test-cfd-refined-transient:
	@mkdir -p build
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/cfd_refined_transient_test.c src/app/cfd_refined_mesh.c src/app/cfd_refined_mesh_local.c src/app/cfd_refined_diffusion.c src/app/cfd_memory.c -lm -o build/cfd_refined_transient_test
	build/cfd_refined_transient_test

test-cfd-refined-transport:
	@mkdir -p build
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/cfd_refined_transport_test.c src/app/cfd_refined_mesh.c src/app/cfd_refined_mesh_local.c src/app/cfd_refined_transport.c src/app/cfd_memory.c -lm -o build/cfd_refined_transport_test
	build/cfd_refined_transport_test

test-cfd-refined-channel:
	@mkdir -p build
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/cfd_refined_channel_test.c src/app/cfd_refined_mesh.c src/app/cfd_refined_mesh_local.c src/app/cfd_refined_diffusion.c src/app/cfd_refined_transport.c src/app/cfd_refined_mixed.c src/app/cfd_sparse_mg.c src/app/cfd_refined_channel.c src/app/cfd_memory.c -lm -o build/cfd_refined_channel_test
	build/cfd_refined_channel_test

.PHONY: probe-cfd-refined-flow-transient test-cfd-refined-mixed-coupling
probe-cfd-refined-flow-transient:
	@mkdir -p build
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/cfd_refined_flow_transient_test.c src/app/cfd_refined_mesh.c src/app/cfd_refined_mesh_local.c src/app/cfd_refined_diffusion.c src/app/cfd_refined_transport.c src/app/cfd_refined_mixed.c src/app/cfd_sparse_mg.c src/app/cfd_refined_channel.c src/app/cfd_memory.c -lm -o build/cfd_refined_flow_transient_test
	build/cfd_refined_flow_transient_test

CFD_REFINED_REFERENCE_PYTHON ?= build/cfd-reference-venv/bin/python
test-cfd-refined-mixed-coupling:
	@mkdir -p build/s4-resolution/local-refinement
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/cfd_refined_matrix_probe.c src/app/cfd_refined_mesh.c src/app/cfd_refined_mesh_local.c src/app/cfd_refined_diffusion.c src/app/cfd_memory.c -lm -o build/cfd_refined_matrix_probe
	build/cfd_refined_matrix_probe 8 > build/s4-resolution/local-refinement/matrix-8.json
	build/cfd_refined_matrix_probe 16 > build/s4-resolution/local-refinement/matrix-16.json
	build/cfd_refined_matrix_probe 32 > build/s4-resolution/local-refinement/matrix-32.json
	$(CFD_REFINED_REFERENCE_PYTHON) scripts/verify_cfd_refined_coupling.py --root build/s4-resolution/local-refinement

.PHONY: test-cfd-refined-mixed test-cfd-refined-flow-transient
test-cfd-refined-mixed:
	@mkdir -p build/s4-resolution/local-refinement
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/cfd_refined_mixed_test.c src/app/cfd_refined_mesh.c src/app/cfd_refined_mesh_local.c src/app/cfd_refined_diffusion.c src/app/cfd_refined_mixed.c src/app/cfd_sparse_mg.c src/app/cfd_memory.c -lm -o build/cfd_refined_mixed_test
	build/cfd_refined_mixed_test 8 build/s4-resolution/local-refinement/native-mixed-8.json
	build/cfd_refined_mixed_test 16 build/s4-resolution/local-refinement/native-mixed-16.json
	build/cfd_refined_mixed_test 32 build/s4-resolution/local-refinement/native-mixed-32.json

test-cfd-refined-flow-transient: probe-cfd-refined-flow-transient

.PHONY: test-cfd-refined-split-channel
test-cfd-refined-split-channel:
	@mkdir -p build
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -DCFD_REFINED_VERIFY_SPLIT -Iinclude tests/cfd_refined_channel_test.c src/app/cfd_refined_mesh.c src/app/cfd_refined_mesh_local.c src/app/cfd_refined_diffusion.c src/app/cfd_refined_transport.c src/app/cfd_refined_mixed.c src/app/cfd_sparse_mg.c src/app/cfd_refined_channel.c src/app/cfd_memory.c -lm -o build/cfd_refined_split_channel_test
	build/cfd_refined_split_channel_test

.PHONY: test-cfd-refined-channel-units
test-cfd-refined-channel-units:
	@mkdir -p build
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/cfd_refined_channel_units_test.c src/app/cfd_refined_mesh.c src/app/cfd_refined_mesh_local.c src/app/cfd_refined_diffusion.c src/app/cfd_refined_transport.c src/app/cfd_refined_mixed.c src/app/cfd_sparse_mg.c src/app/cfd_refined_channel.c src/app/cfd_memory.c -lm -o build/cfd_refined_channel_units_test
	build/cfd_refined_channel_units_test

.PHONY: test-cfd-refined-energy test-cfd-refined-obstacle-evolution probe-cfd-refined-obstacle
test-cfd-refined-energy:
	@mkdir -p build
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/cfd_refined_energy_test.c src/app/cfd_refined_mesh.c src/app/cfd_refined_mesh_local.c src/app/cfd_refined_diffusion.c src/app/cfd_refined_mixed.c src/app/cfd_sparse_mg.c src/app/cfd_memory.c -lm -o build/cfd_refined_energy_test
	build/cfd_refined_energy_test

test-cfd-refined-obstacle-evolution:
	@mkdir -p build
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/cfd_refined_obstacle_evolution_test.c src/app/cfd_refined_mesh.c src/app/cfd_refined_mesh_local.c src/app/cfd_refined_diffusion.c src/app/cfd_refined_transport.c src/app/cfd_refined_mixed.c src/app/cfd_sparse_mg.c src/app/cfd_refined_channel.c src/app/cfd_memory.c -lm -o build/cfd_refined_obstacle_evolution_test
	build/cfd_refined_obstacle_evolution_test

probe-cfd-refined-obstacle:
	@mkdir -p build
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/cfd_refined_obstacle_test.c src/app/cfd_refined_mesh.c src/app/cfd_refined_mesh_local.c src/app/cfd_refined_diffusion.c src/app/cfd_refined_mixed.c src/app/cfd_sparse_mg.c src/app/cfd_memory.c -lm -o build/cfd_refined_obstacle_test
	build/cfd_refined_obstacle_test 8
	build/cfd_refined_obstacle_test 16
	build/cfd_refined_obstacle_test 32

.PHONY: test-cfd-sparse-mg test-cfd-refined-fine-linear
test-cfd-sparse-mg:
	@mkdir -p build
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/cfd_sparse_mg_test.c src/app/cfd_sparse_mg.c src/app/cfd_memory.c -lm -o build/cfd_sparse_mg_test
	build/cfd_sparse_mg_test

# The fixture rejects failed linear solves; printed physical qualification is
# separate. This target does not certify its force components.
test-cfd-refined-fine-linear:
	@mkdir -p build/s4-resolution/refined-obstacle
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/cfd_refined_obstacle_test.c src/app/cfd_refined_mesh.c src/app/cfd_refined_mesh_local.c src/app/cfd_refined_diffusion.c src/app/cfd_refined_mixed.c src/app/cfd_sparse_mg.c src/app/cfd_memory.c -lm -o build/cfd_refined_obstacle_test
	build/cfd_refined_obstacle_test 64

.PHONY: benchmark-cfd-refined-preconditioner
benchmark-cfd-refined-preconditioner:
	@mkdir -p build/s4-resolution/refined-obstacle
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -DCFD_REFINED_VERIFY_ILU -Iinclude tests/cfd_refined_obstacle_test.c src/app/cfd_refined_mesh.c src/app/cfd_refined_mesh_local.c src/app/cfd_refined_diffusion.c src/app/cfd_refined_mixed.c src/app/cfd_memory.c -lm -o build/cfd_refined_obstacle_ilu_test
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/cfd_refined_obstacle_test.c src/app/cfd_refined_mesh.c src/app/cfd_refined_mesh_local.c src/app/cfd_refined_diffusion.c src/app/cfd_refined_mixed.c src/app/cfd_sparse_mg.c src/app/cfd_memory.c -lm -o build/cfd_refined_obstacle_test
	python3 scripts/benchmark_cfd_refined_preconditioner.py --baseline build/cfd_refined_obstacle_ilu_test --multilevel build/cfd_refined_obstacle_test --output build/s4-resolution/refined-obstacle/multilevel-cost

.PHONY: test-cfd-refined-mesh-local
test-cfd-refined-mesh-local:
	@mkdir -p build
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/cfd_refined_mesh_local_test.c src/app/cfd_refined_mesh.c src/app/cfd_refined_mesh_local.c src/app/cfd_refined_diffusion.c src/app/cfd_memory.c -lm -o build/cfd_refined_mesh_local_test
	build/cfd_refined_mesh_local_test

.PHONY: test-cfd-refined-local-obstacle
test-cfd-refined-local-obstacle:
	@mkdir -p build
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/cfd_refined_obstacle_test.c src/app/cfd_refined_mesh.c src/app/cfd_refined_mesh_local.c src/app/cfd_refined_diffusion.c src/app/cfd_refined_mixed.c src/app/cfd_sparse_mg.c src/app/cfd_memory.c -lm -o build/cfd_refined_obstacle_local_test
	build/cfd_refined_obstacle_local_test 64 0 7 1 4 192

.PHONY: test-cfd-refined-local-transient test-cfd-refined-local-obstacle-evolution
test-cfd-refined-local-transient:
	@mkdir -p build
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -DCFD_REFINED_VERIFY_LOCAL -Iinclude tests/cfd_refined_flow_transient_test.c src/app/cfd_refined_mesh.c src/app/cfd_refined_mesh_local.c src/app/cfd_refined_diffusion.c src/app/cfd_refined_transport.c src/app/cfd_refined_mixed.c src/app/cfd_sparse_mg.c src/app/cfd_refined_channel.c src/app/cfd_memory.c -lm -o build/cfd_refined_local_flow_transient_test
	build/cfd_refined_local_flow_transient_test

test-cfd-refined-local-obstacle-evolution:
	@mkdir -p build
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -DCFD_REFINED_VERIFY_LOCAL -Iinclude tests/cfd_refined_obstacle_evolution_test.c src/app/cfd_refined_mesh.c src/app/cfd_refined_mesh_local.c src/app/cfd_refined_diffusion.c src/app/cfd_refined_transport.c src/app/cfd_refined_mixed.c src/app/cfd_sparse_mg.c src/app/cfd_refined_channel.c src/app/cfd_memory.c -lm -o build/cfd_refined_local_obstacle_evolution_test
	build/cfd_refined_local_obstacle_evolution_test

.PHONY: test-cfd-refined-outlet-sensitivity
test-cfd-refined-outlet-sensitivity:
	@mkdir -p build/s4-resolution/refined-obstacle
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/cfd_refined_obstacle_test.c src/app/cfd_refined_mesh.c src/app/cfd_refined_mesh_local.c src/app/cfd_refined_diffusion.c src/app/cfd_refined_mixed.c src/app/cfd_sparse_mg.c src/app/cfd_memory.c -lm -o build/cfd_refined_obstacle_outlet_test
	build/cfd_refined_obstacle_outlet_test 64 0 7 1 4 > build/s4-resolution/refined-obstacle/local-n64-l7.log
	build/cfd_refined_obstacle_outlet_test 64 0 7 0 6 > build/s4-resolution/refined-obstacle/outlet-local-n64-L6.log
	build/cfd_refined_obstacle_outlet_test 64 0 7 0 8 > build/s4-resolution/refined-obstacle/outlet-local-n64-L8.log
	build/cfd_refined_obstacle_outlet_test 128 0 7 1 4 > build/s4-resolution/refined-obstacle/local-n128-l7.log
	build/cfd_refined_obstacle_outlet_test 128 0 7 0 6 > build/s4-resolution/refined-obstacle/outlet-local-n128-L6.log
	python3 scripts/verify_cfd_refined_outlet.py --root build/s4-resolution/refined-obstacle --output build/s4-resolution/refined-obstacle/outlet-qualification.json

.PHONY: test-cfd-memory
test-cfd-memory:
	@mkdir -p build
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/cfd_memory_test.c src/app/cfd_memory.c src/app/cfd_refined_mesh.c src/app/cfd_refined_mesh_local.c src/app/cfd_refined_diffusion.c src/app/cfd_refined_transport.c src/app/cfd_refined_mixed.c src/app/cfd_sparse_mg.c src/app/cfd_refined_channel.c -lm -o build/cfd_memory_test
	build/cfd_memory_test

.PHONY: benchmark-cfd-refined-accuracy
benchmark-cfd-refined-accuracy:
	@mkdir -p build
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/cfd_refined_obstacle_test.c src/app/cfd_refined_mesh.c src/app/cfd_refined_mesh_local.c src/app/cfd_refined_diffusion.c src/app/cfd_refined_mixed.c src/app/cfd_sparse_mg.c src/app/cfd_memory.c -lm -o build/cfd_refined_accuracy_cost
	python3 scripts/benchmark_cfd_refined_accuracy.py --binary build/cfd_refined_accuracy_cost --output build/s4-resolution/refined-obstacle/accuracy-cost

.PHONY: test-cfd-refined-qualification-evidence
test-cfd-refined-qualification-evidence:
	python3 tests/test_refined_qualification_evidence.py

.PHONY: benchmark-cfd-refined-residual
benchmark-cfd-refined-residual:
	@mkdir -p build
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -DCFD_REFINED_VERIFY_LOCAL -DCFD_REFINED_VERIFY_RESTART_ONLY -Iinclude tests/cfd_refined_flow_transient_test.c src/app/cfd_refined_mesh.c src/app/cfd_refined_mesh_local.c src/app/cfd_refined_diffusion.c src/app/cfd_refined_transport.c src/app/cfd_refined_mixed.c src/app/cfd_sparse_mg.c src/app/cfd_refined_channel.c src/app/cfd_memory.c -lm -o build/cfd_refined_restart_only_transient
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -DCFD_REFINED_VERIFY_LOCAL -Iinclude tests/cfd_refined_flow_transient_test.c src/app/cfd_refined_mesh.c src/app/cfd_refined_mesh_local.c src/app/cfd_refined_diffusion.c src/app/cfd_refined_transport.c src/app/cfd_refined_mixed.c src/app/cfd_sparse_mg.c src/app/cfd_refined_channel.c src/app/cfd_memory.c -lm -o build/cfd_refined_candidate_flow_transient
	python3 scripts/benchmark_cfd_refined_residual.py --baseline build/cfd_refined_restart_only_transient --candidate build/cfd_refined_candidate_flow_transient --output build/s4-resolution/local-refinement/residual-cost

.PHONY: benchmark-cfd-refined-transient-accuracy
benchmark-cfd-refined-transient-accuracy:
	@mkdir -p build
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -DCFD_REFINED_VERIFY_UNIFORM -Iinclude tests/cfd_refined_flow_transient_test.c src/app/cfd_refined_mesh.c src/app/cfd_refined_mesh_local.c src/app/cfd_refined_diffusion.c src/app/cfd_refined_transport.c src/app/cfd_refined_mixed.c src/app/cfd_sparse_mg.c src/app/cfd_refined_channel.c src/app/cfd_memory.c -lm -o build/cfd_refined_uniform_flow_transient
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -DCFD_REFINED_VERIFY_LOCAL -Iinclude tests/cfd_refined_flow_transient_test.c src/app/cfd_refined_mesh.c src/app/cfd_refined_mesh_local.c src/app/cfd_refined_diffusion.c src/app/cfd_refined_transport.c src/app/cfd_refined_mixed.c src/app/cfd_sparse_mg.c src/app/cfd_refined_channel.c src/app/cfd_memory.c -lm -o build/cfd_refined_candidate_flow_transient
	python3 scripts/benchmark_cfd_refined_transient_accuracy.py --uniform build/cfd_refined_uniform_flow_transient --local build/cfd_refined_candidate_flow_transient --output build/s4-resolution/local-refinement/matched-transient-cost

.PHONY: test-cfd-refined-assessment
test-cfd-refined-assessment:
	python3 tests/test_refined_assessment.py

CFD_3D_SRCS := src/app/cfd_cartesian3d.c src/app/cfd_duct3d.c src/app/cfd_sparse_mg.c src/app/cfd_memory.c
.PHONY: test-cfd-cartesian3d test-cfd-duct3d
test-cfd-cartesian3d:
	@mkdir -p build/c3d
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/cfd_cartesian3d_test.c $(CFD_3D_SRCS) -lm -o build/c3d/cartesian3d_test
	build/c3d/cartesian3d_test > build/c3d/operators.log
test-cfd-duct3d:
	@mkdir -p build/c3d
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/cfd_duct3d_test.c $(CFD_3D_SRCS) -lm -o build/c3d/duct3d_test
	build/c3d/duct3d_test > build/c3d/duct.jsonl

.PHONY: test-cfd-periodic3d
test-cfd-periodic3d:
	@mkdir -p build/c3d
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/cfd_periodic3d_test.c src/app/cfd_periodic3d.c $(CFD_3D_SRCS) -lm -o build/c3d/periodic3d_test
	build/c3d/periodic3d_test > build/c3d/transient-spatial.jsonl

.PHONY: test-cfd-3d-session test-cfd-3d-session-sanitize qualify-cfd-3d
CFD_3D_SESSION_SRCS := src/app/cfd_obstacle3d.c src/app/cfd_obstacle3d_box.c src/app/cfd_obstacle3d_reconstruction.c src/app/cfd_obstacle3d_mixed.c src/app/cfd_obstacle3d_observation.c src/app/cfd_transient3d_observation.c src/app/cfd_3d_harmonic.c src/app/cfd_wall3d.c src/app/cfd_wall3d_reference.c src/app/cfd_startup3d.c src/app/cfd_startup3d_reference.c src/app/cfd_mixed3d.c $(CFD_3D_SRCS) src/app/cfd_open3d.c src/app/cfd_open3d_observation.c src/app/cfd_periodic3d.c src/app/cfd_3d_session.c src/app/cfd_3d_observation.c src/app/cfd_channel.c src/app/cfd_channel_observation.c
test-cfd-3d-session:
	@mkdir -p build/c3d
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude $$(pkg-config --cflags json-c) tests/cfd_3d_session_test.c $(CFD_3D_SESSION_SRCS) $$(pkg-config --libs json-c) -lm -o build/c3d/session_test
	build/c3d/session_test > build/c3d/session.log
test-cfd-3d-session-sanitize:
	@mkdir -p build/c3d
	$(CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude $$(pkg-config --cflags json-c) tests/cfd_3d_session_test.c $(CFD_3D_SESSION_SRCS) $$(pkg-config --libs json-c) -lm -o build/c3d/session_sanitize
	ASAN_OPTIONS=detect_leaks=0 build/c3d/session_sanitize > build/c3d/sanitize.log
qualify-cfd-3d: test-cfd-cartesian3d test-cfd-duct3d test-cfd-periodic3d test-cfd-3d-session
	python3 scripts/qualify_cfd_3d.py

.PHONY: verify-cfd-3d-agent-evidence
verify-cfd-3d-agent-evidence: physics-sim-session-worker-optimized
	PHYSICS_SIM_SESSION_WORKER="$(CURDIR)/build/cfd-optimized/physics_sim_session_worker" python3 scripts/verify_cfd_3d_agent.py

.PHONY: test-cfd-open3d qualify-cfd-open3d test-cfd-open3d-sanitize
build/c3d-open/open3d_test: tests/cfd_open3d_test.c src/app/cfd_open3d.c $(CFD_3D_SRCS) include/app/cfd_open3d.h
	@mkdir -p build/c3d-open
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude -DCFD_OPEN3D_VERIFY tests/cfd_open3d_test.c src/app/cfd_open3d.c $(CFD_3D_SRCS) -lm -o $@
test-cfd-open3d: build/c3d-open/open3d_test
	build/c3d-open/open3d_test 8
qualify-cfd-open3d: build/c3d-open/open3d_test
	python3 scripts/qualify_cfd_open3d.py

test-cfd-open3d-sanitize:
	@mkdir -p build/c3d-open
	$(CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude $$(pkg-config --cflags json-c) tests/cfd_3d_session_test.c $(CFD_3D_SESSION_SRCS) $$(pkg-config --libs json-c) -lm -o build/c3d-open/session_sanitize
	ASAN_OPTIONS=detect_leaks=0 build/c3d-open/session_sanitize > build/c3d-open/sanitize.log
.PHONY: verify-cfd-open3d-agent-evidence test-cfd-open3d-agent-session
verify-cfd-open3d-agent-evidence: physics-sim-session-worker-optimized
	PHYSICS_SIM_SESSION_WORKER="$(CURDIR)/build/cfd-optimized/physics_sim_session_worker" python3 scripts/verify_cfd_open3d_agent.py
test-cfd-open3d-agent-session: physics-sim-session-worker-optimized
	PHYSICS_SIM_SESSION_WORKER="$(CURDIR)/build/cfd-optimized/physics_sim_session_worker" python3 tests/test_agent_open3d.py

CFD_WALL3D_SRCS := src/app/cfd_wall3d.c src/app/cfd_wall3d_reference.c src/app/cfd_mixed3d.c src/app/cfd_cartesian3d.c src/app/cfd_sparse_mg.c src/app/cfd_memory.c
.PHONY: test-cfd-wall3d qualify-cfd-wall3d
build/c3d-wall/wall3d_test: tests/cfd_wall3d_test.c $(CFD_WALL3D_SRCS) include/app/cfd_wall3d.h include/app/cfd_mixed3d.h
	@mkdir -p build/c3d-wall
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/cfd_wall3d_test.c $(CFD_WALL3D_SRCS) -lm -o $@
test-cfd-wall3d: build/c3d-wall/wall3d_test
	build/c3d-wall/wall3d_test 8 .01 .4
qualify-cfd-wall3d: build/c3d-wall/wall3d_test
	python3 scripts/qualify_cfd_wall3d.py

.PHONY: test-cfd-wall3d-contract test-cfd-wall3d-sanitize
test-cfd-wall3d-contract:
	@mkdir -p build/c3d-wall
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -DCFD_MIXED3D_VERIFY -Iinclude tests/cfd_wall3d_contract_test.c $(CFD_WALL3D_SRCS) -lm -o build/c3d-wall/contract_test
	build/c3d-wall/contract_test
test-cfd-wall3d-sanitize:
	@mkdir -p build/c3d-wall
	$(CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -DCFD_MIXED3D_VERIFY -Iinclude tests/cfd_wall3d_contract_test.c $(CFD_WALL3D_SRCS) -lm -o build/c3d-wall/contract_sanitize
	ASAN_OPTIONS=detect_leaks=0 build/c3d-wall/contract_sanitize

CFD_STARTUP3D_SRCS := src/app/cfd_startup3d.c src/app/cfd_startup3d_reference.c src/app/cfd_duct3d.c src/app/cfd_mixed3d.c src/app/cfd_cartesian3d.c src/app/cfd_sparse_mg.c src/app/cfd_memory.c
.PHONY: test-cfd-startup3d-contract test-cfd-startup3d-sanitize qualify-cfd-startup3d
build/c3d-wall/startup3d_test: tests/cfd_startup3d_test.c $(CFD_STARTUP3D_SRCS) include/app/cfd_startup3d.h include/app/cfd_mixed3d.h
	@mkdir -p build/c3d-wall
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/cfd_startup3d_test.c $(CFD_STARTUP3D_SRCS) -lm -o $@
test-cfd-startup3d-contract:
	@mkdir -p build/c3d-wall
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -DCFD_MIXED3D_VERIFY -Iinclude tests/cfd_startup3d_contract_test.c $(CFD_STARTUP3D_SRCS) -lm -o build/c3d-wall/startup_contract
	build/c3d-wall/startup_contract
test-cfd-startup3d-sanitize:
	@mkdir -p build/c3d-wall
	$(CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -DCFD_MIXED3D_VERIFY -Iinclude tests/cfd_startup3d_contract_test.c $(CFD_STARTUP3D_SRCS) -lm -o build/c3d-wall/startup_sanitize
	ASAN_OPTIONS=detect_leaks=0 build/c3d-wall/startup_sanitize
qualify-cfd-startup3d: build/c3d-wall/startup3d_test
	python3 scripts/qualify_cfd_startup3d.py

.PHONY: test-cfd-open-transient3d
build/c3d-wall/open_transient3d_test: tests/cfd_open_transient3d_test.c $(CFD_WALL3D_SRCS) include/app/cfd_wall3d.h include/app/cfd_mixed3d.h
	@mkdir -p build/c3d-wall
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/cfd_open_transient3d_test.c $(CFD_WALL3D_SRCS) -lm -o $@
test-cfd-open-transient3d: build/c3d-wall/open_transient3d_test
	build/c3d-wall/open_transient3d_test 8 .01 .4 build/c3d-wall/open-smoke.bin 4

.PHONY: qualify-cfd-open-transient3d
qualify-cfd-open-transient3d: build/c3d-wall/open_transient3d_test
	python3 scripts/qualify_cfd_open_transient3d.py

.PHONY: test-cfd-wall3d-reconstruction
test-cfd-wall3d-reconstruction:
	@mkdir -p build/c3d-wall
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/cfd_wall3d_reconstruction_test.c $(CFD_WALL3D_SRCS) -lm -o build/c3d-wall/reconstruction_test
	build/c3d-wall/reconstruction_test > build/c3d-wall/reconstruction.jsonl

CFD_TRANSIENT3D_SRCS := $(CFD_WALL3D_SRCS) src/app/cfd_startup3d.c src/app/cfd_startup3d_reference.c src/app/cfd_duct3d.c
.PHONY: test-cfd-transient3d-budget test-cfd-transient3d-budget-sanitize
test-cfd-transient3d-budget:
	@mkdir -p build/c3d-wall
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/cfd_transient3d_budget_test.c $(CFD_TRANSIENT3D_SRCS) -lm -o build/c3d-wall/budget_test
	build/c3d-wall/budget_test
test-cfd-transient3d-budget-sanitize:
	@mkdir -p build/c3d-wall
	$(CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/cfd_transient3d_budget_test.c $(CFD_TRANSIENT3D_SRCS) -lm -o build/c3d-wall/budget_sanitize
	ASAN_OPTIONS=detect_leaks=0 build/c3d-wall/budget_sanitize

.PHONY: test-cfd-transient3d-session test-cfd-transient3d-session-sanitize
test-cfd-transient3d-session:
	@mkdir -p build/c3d-wall
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude $(JSON_CFLAGS) tests/cfd_transient3d_session_test.c $(CFD_3D_SESSION_SRCS) $(JSON_LIBS) -lm -o build/c3d-wall/session_test
	build/c3d-wall/session_test > build/c3d-wall/transient-session.log
test-cfd-transient3d-session-sanitize:
	@mkdir -p build/c3d-wall
	$(CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude $(JSON_CFLAGS) tests/cfd_transient3d_session_test.c $(CFD_3D_SESSION_SRCS) $(JSON_LIBS) -lm -o build/c3d-wall/session_sanitize
	ASAN_OPTIONS=detect_leaks=0 build/c3d-wall/session_sanitize > build/c3d-wall/transient-session-sanitize.log

.PHONY: test-cfd-transient3d-agent-session verify-cfd-transient3d-agent-evidence audit-cfd-transient3d-agent-evidence
test-cfd-transient3d-agent-session: physics-sim-session-worker-optimized
	PHYSICS_SIM_SESSION_WORKER="$(CURDIR)/build/cfd-optimized/physics_sim_session_worker" python3 tests/test_agent_transient3d.py
verify-cfd-transient3d-agent-evidence: physics-sim-session-worker-optimized
	PHYSICS_SIM_SESSION_WORKER="$(CURDIR)/build/cfd-optimized/physics_sim_session_worker" python3 scripts/verify_cfd_transient3d_agent.py
audit-cfd-transient3d-agent-evidence:
	python3 scripts/audit_cfd_transient3d_agent.py

# Observe container cleanup independently of the numerical memory allocator.
.PHONY: test-json-container-ownership
test-json-container-ownership:
	@mkdir -p build/c3d-wall
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror $(JSON_CFLAGS) tests/json_container_ownership_test.c $(JSON_LIBS) -o build/c3d-wall/json_container_ownership_test
	build/c3d-wall/json_container_ownership_test

.PHONY: audit-cfd-transient3d-completion
audit-cfd-transient3d-completion:
	python3 scripts/audit_cfd_transient3d_completion.py

CFD_OBSTACLE3D_SRCS := src/app/cfd_obstacle3d.c src/app/cfd_obstacle3d_box.c src/app/cfd_obstacle3d_pressure_trace.c src/app/cfd_obstacle3d_reconstruction.c src/app/cfd_obstacle3d_mixed.c src/app/cfd_cartesian3d.c src/app/cfd_sparse_mg.c src/app/cfd_memory.c
build/c3d-obstacle/obstacle3d_test: tests/cfd_obstacle3d_test.c $(CFD_OBSTACLE3D_SRCS) include/app/cfd_obstacle3d.h
	@mkdir -p build/c3d-obstacle
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -DCFD_MIXED3D_VERIFY -Iinclude tests/cfd_obstacle3d_test.c $(CFD_OBSTACLE3D_SRCS) -lm -o $@

.PHONY: test-cfd-obstacle3d-contract test-cfd-obstacle3d-sanitize test-cfd-obstacle3d-agent-session
test-cfd-obstacle3d-contract:
	@mkdir -p build/c3d-obstacle
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -DCFD_MIXED3D_VERIFY -Iinclude tests/cfd_obstacle3d_contract_test.c $(CFD_OBSTACLE3D_SRCS) -lm -o build/c3d-obstacle/contract
	build/c3d-obstacle/contract
test-cfd-obstacle3d-sanitize:
	@mkdir -p build/c3d-obstacle
	$(CC) -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -DCFD_MIXED3D_VERIFY -Iinclude tests/cfd_obstacle3d_contract_test.c $(CFD_OBSTACLE3D_SRCS) -lm -o build/c3d-obstacle/contract-sanitize
	build/c3d-obstacle/contract-sanitize
test-cfd-obstacle3d-agent-session: physics-sim-session-worker-optimized
	PHYSICS_SIM_SESSION_WORKER="$(CURDIR)/build/cfd-optimized/physics_sim_session_worker" python3 tests/test_agent_obstacle3d.py

.PHONY: verify-cfd-obstacle3d-agent-evidence
verify-cfd-obstacle3d-agent-evidence: physics-sim-session-worker-optimized build/c3d-obstacle/obstacle3d_test
	PHYSICS_SIM_SESSION_WORKER="$(CURDIR)/build/cfd-optimized/physics_sim_session_worker" python3 scripts/verify_cfd_obstacle3d_agent.py

.PHONY: audit-cfd-obstacle3d
audit-cfd-obstacle3d:
	python3 scripts/audit_cfd_obstacle3d_reference_refinement.py

.PHONY: test-cfd-obstacle3d-reconstruction
build/c3d-obstacle/correction-v1/reconstruction-probe-corrected: tests/cfd_obstacle3d_reconstruction_probe.c $(CFD_OBSTACLE3D_SRCS) include/app/cfd_obstacle3d.h
	@mkdir -p build/c3d-obstacle/correction-v1
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/cfd_obstacle3d_reconstruction_probe.c $(CFD_OBSTACLE3D_SRCS) -lm -o $@
test-cfd-obstacle3d-reconstruction: build/c3d-obstacle/correction-v1/reconstruction-probe-corrected
	build/cfd-reference-venv/bin/python tests/test_cfd_obstacle3d_reconstruction.py

.PHONY: cfd-obstacle3d-correction-reference verify-cfd-obstacle3d-extended40 audit-cfd-obstacle3d-correction
cfd-obstacle3d-correction-reference:
	python3 scripts/cfd_obstacle3d_correction_reference.py
verify-cfd-obstacle3d-extended40: physics-sim-session-worker-optimized build/c3d-obstacle/obstacle3d_test
	PHYSICS_SIM_SESSION_WORKER="$(CURDIR)/build/cfd-optimized/physics_sim_session_worker" python3 scripts/verify_cfd_obstacle3d_extended40.py
audit-cfd-obstacle3d-correction:
	python3 scripts/audit_cfd_obstacle3d_reference_refinement.py

.PHONY: test-cfd-obstacle3d-reference-measurement test-cfd-obstacle3d-pressure-trace audit-cfd-obstacle3d-reference-refinement
build/c3d-obstacle/refinement-v2/pressure_trace_probe: tests/cfd_obstacle3d_pressure_trace_probe.c $(CFD_OBSTACLE3D_SRCS) include/app/cfd_obstacle3d.h
	@mkdir -p build/c3d-obstacle/refinement-v2
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/cfd_obstacle3d_pressure_trace_probe.c $(CFD_OBSTACLE3D_SRCS) -lm -o $@
test-cfd-obstacle3d-pressure-trace: build/c3d-obstacle/refinement-v2/pressure_trace_probe
	python3 tests/test_cfd_obstacle3d_pressure_trace.py
test-cfd-obstacle3d-reference-measurement:
	build/cfd-reference-venv/bin/python tests/test_cfd_reference3d_measurement.py
audit-cfd-obstacle3d-reference-refinement:
	python3 scripts/audit_cfd_obstacle3d_reference_refinement.py

.PHONY: test-cfd-reference3d-consistency run-cfd-reference3d-consistency
test-cfd-reference3d-consistency:
	PYTHONDONTWRITEBYTECODE=1 build/cfd-reference-venv/bin/python tests/test_cfd_reference3d_solved.py
	PYTHONDONTWRITEBYTECODE=1 build/cfd-reference-venv/bin/python tests/test_cfd_reference3d_consistency.py
run-cfd-reference3d-consistency:
	PYTHONDONTWRITEBYTECODE=1 python3 scripts/run_cfd_reference3d_consistency.py

.PHONY: audit-cfd-3d-initial-improvements
audit-cfd-3d-initial-improvements:
	PYTHONDONTWRITEBYTECODE=1 python3 scripts/audit_cfd_3d_initial_improvements.py

.PHONY: test-cfd-reference3d-method audit-cfd-3d-reference-method
test-cfd-reference3d-method:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python tests/test_cfd_reference3d_p3.py
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python tests/test_cfd_reference3d_p3_solved.py
	PYTHONDONTWRITEBYTECODE=1 $(CURDIR)/build/cfd-reference-venv/bin/python tests/test_cfd_reference3d_method_receipts.py

audit-cfd-3d-reference-method:
	PYTHONDONTWRITEBYTECODE=1 $(CURDIR)/build/cfd-reference-venv/bin/python scripts/audit_cfd_3d_reference_method.py

.PHONY: test-cfd-reference3d-preconditioner audit-cfd-3d-preconditioner
test-cfd-reference3d-preconditioner:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python tests/test_cfd_reference3d_preconditioner.py
	PYTHONDONTWRITEBYTECODE=1 $(CURDIR)/build/cfd-reference-venv/bin/python tests/test_cfd_reference3d_preconditioner_receipts.py

audit-cfd-3d-preconditioner:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python scripts/audit_cfd_3d_preconditioner.py

.PHONY: test-cfd-reference3d-spatial audit-cfd-3d-spatial
test-cfd-reference3d-spatial:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python tests/test_cfd_reference3d_spatial_mesh.py
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python tests/test_cfd_reference3d_chunked.py
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python tests/test_cfd_reference3d_pressure_modes.py
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python tests/test_cfd_reference3d_macro_preconditioner.py

audit-cfd-3d-spatial:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python scripts/audit_cfd_3d_spatial.py

.PHONY: test-cfd-reference3d-equilibrium audit-cfd-3d-equilibrium
test-cfd-reference3d-equilibrium:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python tests/test_cfd_reference3d_equilibrium.py
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python tests/test_cfd_reference3d_adaptive_mesh.py

audit-cfd-3d-equilibrium:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python scripts/audit_cfd_3d_equilibrium.py

.PHONY: test-cfd-reference3d-quartic audit-cfd-3d-quartic
test-cfd-reference3d-quartic:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python tests/test_cfd_reference3d_p4.py
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python tests/test_cfd_reference3d_p4_solved.py

audit-cfd-3d-quartic:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python scripts/audit_cfd_3d_quartic.py

.PHONY: test-cfd-reference3d-graded audit-cfd-3d-graded
test-cfd-reference3d-graded:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python tests/test_cfd_reference3d_quartic_equilibrium.py
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python tests/test_cfd_reference3d_graded_mesh.py

audit-cfd-3d-graded:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python scripts/audit_cfd_3d_graded.py

.PHONY: test-cfd-reference3d-condensed audit-cfd-3d-condensed
test-cfd-reference3d-condensed:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python tests/test_cfd_reference3d_condensed.py
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python tests/test_cfd_reference3d_condensed_solved.py
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python tests/test_cfd_reference3d_condensed_pc.py

audit-cfd-3d-condensed:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python scripts/audit_cfd_3d_condensed.py

.PHONY: test-cfd-reference3d-cholesky audit-cfd-3d-cholesky
test-cfd-reference3d-cholesky:
	mkdir -p $(CURDIR)/build/c3d-cholesky/support
	/usr/bin/clang -std=c11 -O2 -Wall -Wextra -Werror -dynamiclib scripts/cfd_reference3d_accelerate.c -framework Accelerate -o $(CURDIR)/build/c3d-cholesky/support/factor.dylib
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python tests/test_cfd_reference3d_accelerate.py

audit-cfd-3d-cholesky:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python scripts/audit_cfd_3d_cholesky.py

.PHONY: test-cfd-reference3d-domain audit-cfd-3d-domain
test-cfd-reference3d-domain:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python tests/test_cfd_reference3d_domain_mesh.py
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python tests/test_cfd_reference3d_domain_budget.py

audit-cfd-3d-domain:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python scripts/audit_cfd_3d_domain.py

.PHONY: test-cfd-reference3d-selective audit-cfd-3d-selective
test-cfd-reference3d-selective:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python tests/test_cfd_reference3d_selective_mesh.py

audit-cfd-3d-selective:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python scripts/audit_cfd_3d_selective.py

.PHONY: test-cfd-reference3d-corner audit-cfd-3d-corner
test-cfd-reference3d-corner:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python tests/test_cfd_reference3d_corner_geometry.py

audit-cfd-3d-corner:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python scripts/audit_cfd_3d_corner.py

.PHONY: test-cfd-reference3d-bounded audit-cfd-3d-bounded
test-cfd-reference3d-bounded:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python tests/test_cfd_reference3d_bounded_condensed.py

audit-cfd-3d-bounded:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python scripts/audit_cfd_3d_bounded.py

.PHONY: test-cfd-reference3d-triangle audit-cfd-3d-triangle
test-cfd-reference3d-triangle:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python tests/test_cfd_reference3d_triangle.py

audit-cfd-3d-triangle:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python scripts/audit_cfd_3d_triangle.py

.PHONY: test-cfd-reference3d-shared-factor audit-cfd-3d-shared-factor
test-cfd-reference3d-shared-factor:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python tests/test_cfd_reference3d_shared_factor.py

audit-cfd-3d-shared-factor:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python scripts/audit_cfd_3d_shared_factor.py

.PHONY: test-cfd-reference3d-factor-peak audit-cfd-3d-factor-peak
test-cfd-reference3d-factor-peak:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python tests/test_cfd_reference3d_allocator_pressure.py

audit-cfd-3d-factor-peak:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python scripts/audit_cfd_3d_factor_peak.py

.PHONY: test-cfd-reference3d-symbolic audit-cfd-3d-symbolic
test-cfd-reference3d-symbolic:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python tests/test_cfd_reference3d_symbolic.py

audit-cfd-3d-symbolic:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python scripts/audit_cfd_3d_symbolic.py

.PHONY: test-cfd-reference3d-component-cholesky audit-cfd-3d-component-cholesky
test-cfd-reference3d-component-cholesky:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python tests/test_cfd_reference3d_component_cholesky.py

audit-cfd-3d-component-cholesky:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python scripts/audit_cfd_3d_component_cholesky.py

.PHONY: test-cfd-reference3d-block-cholesky audit-cfd-3d-block-cholesky
test-cfd-reference3d-block-cholesky:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python tests/test_cfd_reference3d_block_cholesky.py

audit-cfd-3d-block-cholesky:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python scripts/audit_cfd_3d_block_cholesky.py

.PHONY: test-cfd-reference3d-coarse-velocity audit-cfd-3d-coarse-velocity
test-cfd-reference3d-coarse-velocity:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python tests/test_cfd_reference3d_coarse_velocity.py

audit-cfd-3d-coarse-velocity:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python scripts/audit_cfd_3d_coarse_velocity.py

.PHONY: test-cfd-reference3d-quadratic-coarse audit-cfd-3d-quadratic-coarse
test-cfd-reference3d-quadratic-coarse:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python tests/test_cfd_reference3d_quadratic_coarse.py

audit-cfd-3d-quadratic-coarse:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python scripts/audit_cfd_3d_quadratic_coarse.py

.PHONY: test-cfd-reference3d-cubic-coarse audit-cfd-3d-cubic-coarse
test-cfd-reference3d-cubic-coarse:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python tests/test_cfd_reference3d_cubic_coarse.py

audit-cfd-3d-cubic-coarse:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python scripts/audit_cfd_3d_cubic_coarse.py

.PHONY: test-cfd-reference3d-additive-coarse audit-cfd-3d-additive-coarse
test-cfd-reference3d-additive-coarse:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python tests/test_cfd_reference3d_additive_coarse.py

audit-cfd-3d-additive-coarse:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python scripts/audit_cfd_3d_additive_coarse.py

.PHONY: test-cfd-reference3d-hierarchical audit-cfd-3d-hierarchical
test-cfd-reference3d-hierarchical:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python tests/test_cfd_reference3d_hierarchical.py

audit-cfd-3d-hierarchical:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python scripts/audit_cfd_3d_hierarchical.py

.PHONY: test-cfd-reference3d-workspace-cholesky test-cfd-reference3d-workspace-stage audit-cfd-3d-workspace-cholesky
test-cfd-reference3d-workspace-cholesky:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python tests/test_cfd_reference3d_workspace_cholesky.py

test-cfd-reference3d-workspace-stage:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python tests/test_cfd_reference3d_workspace_stage.py

audit-cfd-3d-workspace-cholesky:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python scripts/audit_cfd_3d_workspace_cholesky.py

.PHONY: test-cfd-reference3d-vector-storage audit-cfd-3d-vector-storage
test-cfd-reference3d-vector-storage:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python -m unittest discover -s tests -p 'test_cfd_reference3d_vector*.py'

audit-cfd-3d-vector-storage:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python scripts/audit_cfd_3d_vector_storage.py

.PHONY: test-cfd-reference3d-sparse-load audit-cfd-3d-sparse-load
test-cfd-reference3d-sparse-load:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python -m unittest discover -s tests -p 'test_cfd_reference3d_sparse_load*.py'

audit-cfd-3d-sparse-load:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python scripts/audit_cfd_3d_sparse_load.py

.PHONY: test-cfd-reference3d-collect-pressure audit-cfd-3d-collect-pressure
test-cfd-reference3d-collect-pressure:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python -m unittest discover -s tests -p 'test_cfd_reference3d_collect*.py'

audit-cfd-3d-collect-pressure:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python scripts/audit_cfd_3d_collect_pressure.py

.PHONY: test-cfd-reference3d-factor-metadata audit-cfd-3d-factor-metadata
test-cfd-reference3d-factor-metadata:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python -m unittest discover -s tests -p 'test_cfd_reference3d_factor_metadata*.py'

audit-cfd-3d-factor-metadata:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python scripts/audit_cfd_3d_factor_metadata.py

.PHONY: test-cfd-reference3d-factor-catalog audit-cfd-3d-factor-catalog
test-cfd-reference3d-factor-catalog:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python -m unittest discover -s tests -p 'test_cfd_reference3d_factor_catalog*.py'

audit-cfd-3d-factor-catalog:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python scripts/audit_cfd_3d_factor_catalog.py

.PHONY: test-cfd-reference3d-normal-domain audit-cfd-3d-normal-domain
test-cfd-reference3d-normal-domain:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python -m unittest discover -s tests -p 'test_cfd_reference3d_normal_domain*.py'

audit-cfd-3d-normal-domain:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python scripts/audit_cfd_3d_normal_domain.py

.PHONY: test-cfd-reference3d-end-slab
test-cfd-reference3d-end-slab:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python -m unittest discover -s tests -p 'test_cfd_reference3d_end_slab*.py'

.PHONY: test-cfd-reference3d-nodal-hierarchy audit-cfd-3d-nodal-hierarchy
test-cfd-reference3d-nodal-hierarchy:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python -m unittest discover -s tests -p 'test_cfd_reference3d_nodal_hierarchy.py'

audit-cfd-3d-nodal-hierarchy:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python scripts/audit_cfd_3d_nodal_hierarchy.py

.PHONY: test-cfd-reference3d-mixed-precision audit-cfd-3d-mixed-precision
test-cfd-reference3d-mixed-precision:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python -m unittest discover -s tests -p 'test_cfd_reference3d_mixed_precision.py'

audit-cfd-3d-mixed-precision:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python scripts/audit_cfd_3d_mixed_precision.py

.PHONY: test-cfd-reference3d-signed-force test-cfd-reference3d-force-local
test-cfd-reference3d-signed-force:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python -m unittest discover -s tests -p 'test_cfd_reference3d_signed_force.py'

test-cfd-reference3d-force-local:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python -m unittest discover -s tests -p 'test_cfd_reference3d_force_local.py'

.PHONY: test-cfd-reference3d-force-transition test-cfd-reference3d-uniform-normal
test-cfd-reference3d-force-transition:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python -m unittest discover -s tests -p 'test_cfd_reference3d_force_transition.py'

test-cfd-reference3d-uniform-normal:
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CURDIR)/build/cfd-reference-venv/bin/python -m unittest discover -s tests -p 'test_cfd_reference3d_uniform_normal.py'

# Optional pressure reconstruction diagnostic; original force defaults remain unchanged.
.PHONY: test-cfd-obstacle3d-pressure-trace-api test-cfd-obstacle3d-pressure-trace-api-sanitize
test-cfd-obstacle3d-pressure-trace-api:
	@mkdir -p build/c3d-pressure-trace-api
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/cfd_obstacle3d_pressure_trace_api_test.c $(CFD_OBSTACLE3D_SRCS) -lm -o build/c3d-pressure-trace-api/contract
	build/c3d-pressure-trace-api/contract
test-cfd-obstacle3d-pressure-trace-api-sanitize:
	@mkdir -p build/c3d-pressure-trace-api
	$(CC) -std=c11 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/cfd_obstacle3d_pressure_trace_api_test.c $(CFD_OBSTACLE3D_SRCS) -lm -o build/c3d-pressure-trace-api/contract-sanitize
	build/c3d-pressure-trace-api/contract-sanitize

.PHONY: test-cfd-obstacle3d-pressure-trace-anisotropic test-cfd-obstacle3d-pressure-trace-anisotropic-sanitize
test-cfd-obstacle3d-pressure-trace-anisotropic:
	@mkdir -p build/c3d-pressure-trace-api
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/cfd_obstacle3d_pressure_trace_anisotropic_test.c $(CFD_OBSTACLE3D_SRCS) -lm -o build/c3d-pressure-trace-api/anisotropic
	build/c3d-pressure-trace-api/anisotropic
test-cfd-obstacle3d-pressure-trace-anisotropic-sanitize:
	@mkdir -p build/c3d-pressure-trace-api
	$(CC) -std=c11 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/cfd_obstacle3d_pressure_trace_anisotropic_test.c $(CFD_OBSTACLE3D_SRCS) -lm -o build/c3d-pressure-trace-api/anisotropic-sanitize
	build/c3d-pressure-trace-api/anisotropic-sanitize

# Physical whole-field parameter laws of the stationary steady-Stokes backend.
.PHONY: test-cfd-obstacle3d-material-scaling test-cfd-obstacle3d-material-scaling-sanitize
test-cfd-obstacle3d-material-scaling:
	@mkdir -p build/c3d-material-scaling
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/cfd_obstacle3d_material_scaling_test.c $(CFD_OBSTACLE3D_SRCS) -lm -o build/c3d-material-scaling/make-normal
	build/c3d-material-scaling/make-normal
test-cfd-obstacle3d-material-scaling-sanitize:
	@mkdir -p build/c3d-material-scaling
	$(CC) -std=c11 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/cfd_obstacle3d_material_scaling_test.c $(CFD_OBSTACLE3D_SRCS) -lm -o build/c3d-material-scaling/make-sanitize
	build/c3d-material-scaling/make-sanitize

.PHONY: test-cfd-periodic3d-unforced test-cfd-periodic3d-unforced-sanitize
test-cfd-periodic3d-unforced:
	@mkdir -p build/c3d-unforced-periodic
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/cfd_periodic3d_unforced_test.c src/app/cfd_periodic3d.c $(CFD_3D_SRCS) -lm -o build/c3d-unforced-periodic/unforced-test
	build/c3d-unforced-periodic/unforced-test > build/c3d-unforced-periodic/results.jsonl
	python3 scripts/assess_cfd_periodic3d_unforced.py build/c3d-unforced-periodic/results.jsonl
test-cfd-periodic3d-unforced-sanitize:
	@mkdir -p build/c3d-unforced-periodic
	$(CC) -std=c11 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/cfd_periodic3d_unforced_test.c src/app/cfd_periodic3d.c $(CFD_3D_SRCS) -lm -o build/c3d-unforced-periodic/unforced-sanitize
	build/c3d-unforced-periodic/unforced-sanitize small > build/c3d-unforced-periodic/sanitizer.jsonl

.PHONY: test-cfd-obstacle3d-box test-cfd-obstacle3d-box-sanitize
test-cfd-obstacle3d-box:
	@mkdir -p build/c3d-box
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -DCFD_MIXED3D_VERIFY -Iinclude tests/cfd_obstacle3d_box_contract_test.c $(CFD_OBSTACLE3D_SRCS) -lm -o build/c3d-box/contract
	build/c3d-box/contract > build/c3d-box/make-contract.jsonl
test-cfd-obstacle3d-box-sanitize:
	@mkdir -p build/c3d-box
	$(CC) -std=c11 -O1 -g -Wall -Wextra -Werror -DCFD_MIXED3D_VERIFY -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/cfd_obstacle3d_box_contract_test.c $(CFD_OBSTACLE3D_SRCS) -lm -o build/c3d-box/contract-sanitize
	build/c3d-box/contract-sanitize > build/c3d-box/make-sanitize.jsonl

.PHONY: test-cfd-obstacle3d-box-material test-cfd-obstacle3d-box-material-sanitize
test-cfd-obstacle3d-box-material:
	@mkdir -p build/c3d-box
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/cfd_obstacle3d_box_material_test.c $(CFD_OBSTACLE3D_SRCS) -lm -o build/c3d-box/material
	build/c3d-box/material > build/c3d-box/material.jsonl
test-cfd-obstacle3d-box-material-sanitize:
	@mkdir -p build/c3d-box
	$(CC) -std=c11 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/cfd_obstacle3d_box_material_test.c $(CFD_OBSTACLE3D_SRCS) -lm -o build/c3d-box/material-sanitize
	build/c3d-box/material-sanitize > build/c3d-box/material-sanitize.jsonl

.PHONY: test-cfd-3d-box-session test-cfd-3d-box-session-sanitize
test-cfd-3d-box-session:
	@mkdir -p build/c3d-box
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude $$(pkg-config --cflags json-c) tests/cfd_3d_box_session_test.c $(CFD_3D_SESSION_SRCS) $$(pkg-config --libs json-c) -lm -o build/c3d-box/session
	build/c3d-box/session > build/c3d-box/session.log
test-cfd-3d-box-session-sanitize:
	@mkdir -p build/c3d-box
	$(CC) -std=c11 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude $$(pkg-config --cflags json-c) tests/cfd_3d_box_session_test.c $(CFD_3D_SESSION_SRCS) $$(pkg-config --libs json-c) -lm -o build/c3d-box/session-sanitize
	build/c3d-box/session-sanitize > build/c3d-box/session-sanitize.log

.PHONY: test-agent-box3d
test-agent-box3d: $(SESSION_WORKER_BIN)
	PYTHONDONTWRITEBYTECODE=1 PHYSICS_SIM_SESSION_WORKER="$(abspath $(SESSION_WORKER_BIN))" python3 -m unittest discover -s tests -p test_agent_box3d.py -v

# Offline general-fluid/atmosphere source contract; no native solver mutation.
.PHONY: test-surface-source-receiver
test-surface-source-receiver:
	PYTHONDONTWRITEBYTECODE=1 python3 -B -m unittest discover -s tests -p test_surface_source_receiver.py -v

# Qualified body-free passive scalar lane; existing native momentum sources unchanged.
PASSIVE3D_SRCS := src/app/cfd_passive3d.c src/app/cfd_cartesian3d.c src/app/cfd_sparse_mg.c src/app/cfd_memory.c
PASSIVE3D_WORKER := build/passive-atmosphere/physics_sim_passive_worker
$(PASSIVE3D_WORKER): src/tools/physics_sim_passive_worker.c $(PASSIVE3D_SRCS) include/app/cfd_passive3d.h include/app/cfd_cartesian3d.h include/app/cfd_memory.h include/app/cfd_sparse_mg.h
	@mkdir -p build/passive-atmosphere
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -ffp-contract=off -Iinclude $$(pkg-config --cflags json-c) src/tools/physics_sim_passive_worker.c $(PASSIVE3D_SRCS) $$(pkg-config --libs json-c) -lm -o $@
.PHONY: passive-atmosphere-worker
passive-atmosphere-worker: $(PASSIVE3D_WORKER)

.PHONY: test-passive-atmosphere-native test-passive-atmosphere-sanitize test-passive-atmosphere
test-passive-atmosphere-native:
	@mkdir -p build/passive-atmosphere
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -ffp-contract=off -Iinclude tests/cfd_passive3d_contract_test.c src/app/cfd_periodic3d.c $(PASSIVE3D_SRCS) -lm -o build/passive-atmosphere/contract-test
	build/passive-atmosphere/contract-test
test-passive-atmosphere-sanitize:
	@mkdir -p build/passive-atmosphere
	$(CC) -std=c11 -O1 -g -Wall -Wextra -Werror -ffp-contract=off -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/cfd_passive3d_contract_test.c src/app/cfd_periodic3d.c $(PASSIVE3D_SRCS) -lm -o build/passive-atmosphere/contract-sanitize
	build/passive-atmosphere/contract-sanitize
test-passive-atmosphere: $(PASSIVE3D_WORKER) test-passive-atmosphere-native
	PYTHONDONTWRITEBYTECODE=1 python3 -B -m unittest discover -s tests -p test_passive_atmosphere.py -v

.PHONY: test-coupled-passive
test-coupled-passive: $(PASSIVE3D_WORKER)
	PYTHONDONTWRITEBYTECODE=1 python3 -B -m unittest discover -s tests -p test_coupled_passive.py -v

ATMOSPHERE3D_SRCS := src/app/cfd_atmosphere3d.c src/app/cfd_periodic3d.c $(PASSIVE3D_SRCS)
ATMOSPHERE3D_WORKER := build/evolving-atmosphere/physics_sim_atmosphere_worker
ATMOSPHERE3D_HEADERS := include/app/cfd_atmosphere3d.h include/app/cfd_periodic3d.h include/app/cfd_passive3d.h include/app/cfd_cartesian3d.h include/app/cfd_memory.h include/app/cfd_sparse_mg.h
$(ATMOSPHERE3D_WORKER): src/tools/physics_sim_atmosphere_worker.c $(ATMOSPHERE3D_SRCS) $(ATMOSPHERE3D_HEADERS)
	@mkdir -p build/evolving-atmosphere
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -ffp-contract=off -Iinclude $$(pkg-config --cflags json-c) src/tools/physics_sim_atmosphere_worker.c $(ATMOSPHERE3D_SRCS) $$(pkg-config --libs json-c) -lm -o $@
.PHONY: evolving-atmosphere-worker
evolving-atmosphere-worker: $(ATMOSPHERE3D_WORKER)
.PHONY: test-evolving-atmosphere-native test-evolving-atmosphere-sanitize test-evolving-atmosphere
test-evolving-atmosphere-native:
	@mkdir -p build/evolving-atmosphere
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -ffp-contract=off -Iinclude tests/cfd_atmosphere3d_contract_test.c $(ATMOSPHERE3D_SRCS) -lm -o build/evolving-atmosphere/contract-test
	build/evolving-atmosphere/contract-test
test-evolving-atmosphere-sanitize:
	@mkdir -p build/evolving-atmosphere
	$(CC) -std=c11 -O1 -g -Wall -Wextra -Werror -ffp-contract=off -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/cfd_atmosphere3d_contract_test.c $(ATMOSPHERE3D_SRCS) -lm -o build/evolving-atmosphere/contract-sanitize
	build/evolving-atmosphere/contract-sanitize
test-evolving-atmosphere: $(ATMOSPHERE3D_WORKER) test-evolving-atmosphere-native
	PYTHONDONTWRITEBYTECODE=1 python3 -B -m unittest discover -s tests -p test_evolving_atmosphere.py -v
.PHONY: test-coupled-atmosphere
test-coupled-atmosphere: $(ATMOSPHERE3D_WORKER)
	PYTHONDONTWRITEBYTECODE=1 python3 -B -m unittest discover -s tests -p test_coupled_atmosphere.py -v

OPEN_ATMOSPHERE3D_SRCS := src/app/cfd_open_atmosphere3d.c $(PASSIVE3D_SRCS)
.PHONY: test-open-atmosphere-native test-open-atmosphere-sanitize
test-open-atmosphere-native:
	@mkdir -p build/open-atmosphere
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -ffp-contract=off -Iinclude tests/cfd_open_atmosphere3d_test.c $(OPEN_ATMOSPHERE3D_SRCS) -lm -o build/open-atmosphere/contract-test
	build/open-atmosphere/contract-test
test-open-atmosphere-sanitize:
	@mkdir -p build/open-atmosphere
	$(CC) -std=c11 -O1 -g -Wall -Wextra -Werror -ffp-contract=off -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/cfd_open_atmosphere3d_test.c $(OPEN_ATMOSPHERE3D_SRCS) -lm -o build/open-atmosphere/contract-sanitize
	build/open-atmosphere/contract-sanitize
OPEN_ATMOSPHERE3D_WORKER := build/open-atmosphere/physics_sim_open_atmosphere_worker
$(OPEN_ATMOSPHERE3D_WORKER): src/tools/physics_sim_open_atmosphere_worker.c src/tools/physics_sim_plume_qualification.h $(OPEN_ATMOSPHERE3D_SRCS) include/app/cfd_open_atmosphere3d.h $(ATMOSPHERE3D_HEADERS)
	@mkdir -p build/open-atmosphere
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -ffp-contract=off -Iinclude $$(pkg-config --cflags json-c) src/tools/physics_sim_open_atmosphere_worker.c $(OPEN_ATMOSPHERE3D_SRCS) $$(pkg-config --libs json-c) -lm -o $@
.PHONY: open-atmosphere-worker
open-atmosphere-worker: $(OPEN_ATMOSPHERE3D_WORKER)
.PHONY: test-open-atmosphere
test-open-atmosphere: $(OPEN_ATMOSPHERE3D_WORKER) test-open-atmosphere-native
	PYTHONDONTWRITEBYTECODE=1 python3 -B -m unittest discover -s tests -p test_open_atmosphere.py -v
.PHONY: test-coupled-open-atmosphere
test-coupled-open-atmosphere: $(OPEN_ATMOSPHERE3D_WORKER)
	PYTHONDONTWRITEBYTECODE=1 python3 -B -m unittest discover -s tests -p test_coupled_open_atmosphere.py -v

.PHONY: test-open-atmosphere-convergence
test-open-atmosphere-convergence: $(OPEN_ATMOSPHERE3D_WORKER)
	PYTHONDONTWRITEBYTECODE=1 python3 -B tests/test_open_atmosphere_convergence.py --report build/open-atmosphere/convergence/metrics.json
