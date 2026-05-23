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

physics_sim_headless: $(PHYSICS_SIM_HEADLESS_TOOL_OBJ) $(PHYSICS_SIM_APP_OBJS_NO_MAIN)
	$(CC) $(LDFLAGS) -o $(PHYSICS_SIM_HEADLESS_TOOL_BIN) $(PHYSICS_SIM_HEADLESS_TOOL_OBJ) $(PHYSICS_SIM_APP_OBJS_NO_MAIN) $(LIBS)

physics-sim-job-runner: $(PHYSICS_SIM_JOB_RUNNER_TOOL_OBJ) $(PHYSICS_SIM_APP_OBJS_NO_MAIN)
	$(CC) $(LDFLAGS) -o $(PHYSICS_SIM_JOB_RUNNER_TOOL_BIN) $(PHYSICS_SIM_JOB_RUNNER_TOOL_OBJ) $(PHYSICS_SIM_APP_OBJS_NO_MAIN) $(LIBS)

runtime_scene_emitter_diag_tool: $(RUNTIME_SCENE_EMITTER_DIAG_TOOL_SRCS)
	$(CC) $(CFLAGS) \
		-I$(CORE_BASE_DIR)/include -I$(CORE_SCENE_DIR)/include -I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include \
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
