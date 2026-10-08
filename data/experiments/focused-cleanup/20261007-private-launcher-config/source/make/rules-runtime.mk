# =========================
#  Runtime convenience targets
# =========================
run: $(TARGET)
	./$(TARGET)

run-ide-theme: $(TARGET)
	PHYSICS_SIM_USE_SHARED_THEME_FONT=1 PHYSICS_SIM_USE_SHARED_THEME=1 PHYSICS_SIM_USE_SHARED_FONT=1 PHYSICS_SIM_THEME_PRESET=ide_gray PHYSICS_SIM_FONT_PRESET=ide ./$(TARGET)

run-daw-theme: $(TARGET)
	PHYSICS_SIM_USE_SHARED_THEME_FONT=1 PHYSICS_SIM_USE_SHARED_THEME=1 PHYSICS_SIM_USE_SHARED_FONT=1 PHYSICS_SIM_THEME_PRESET=daw_default PHYSICS_SIM_FONT_PRESET=daw_default ./$(TARGET)

run-headless-smoke: clang-build test-stable test-physics-sim-headless-cli
	@echo "physics_sim headless smoke passed (non-interactive)"

visual-harness: clang-build
	@echo "visual harness binary ready: $(CLANG_TARGET)"

visual-artifact: physics_sim_headless
	bash tests/integration/run_physics_sim_visual_artifact.sh

VIDEO_FPS ?= 30
video:
	ffmpeg -y -framerate $(VIDEO_FPS) -i export/render_frames/frame_%06d.bmp -pix_fmt yuv420p export/render_vid/output.mp4

# One validated inventory owns all normal compilation cleanup.
CLEAN_EXECUTABLES = $(TARGET) $(VF2D_PACK_TOOL_BIN) $(VF2D_DATASET_TOOL_BIN) $(PHYSICS_TRACE_TOOL_BIN) $(PHYSICS_SIM_HEADLESS_TOOL_BIN) $(PHYSICS_SIM_JOB_RUNNER_TOOL_BIN) $(RUNTIME_SCENE_EMITTER_DIAG_TOOL_BIN) $(if $(SESSION_WORKER_BIN),$(SESSION_WORKER_BIN),physics_sim_session_worker) $(SHAPE_SANITY_TOOL_BIN) $(SHAPE_MASK_TOOL_BIN) $(SHAPE_ASSET_TOOL_BIN)
CLEAN_ARGUMENTS = --build-root "$(BUILD_DIR)" --experiment-root "$(EXPERIMENT_DIR)" --tools-root "$(REFERENCE_TOOLS_DIR)" --test-root "$(TEST_TMP_DIR)" $(foreach output,$(CLEAN_EXECUTABLES),--executable "$(output)")
.PHONY: clean clean-plan
clean-plan:
	python3 scripts/clean_outputs.py $(CLEAN_ARGUMENTS)

clean:
	python3 scripts/clean_outputs.py $(CLEAN_ARGUMENTS) --apply

.PHONY: test-top-level-cleanup
test-top-level-cleanup:
	python3 -B tests/test_clean_outputs.py

.PHONY: test-package-launcher-lifecycle
test-package-launcher-lifecycle:
	python3 -B tests/test_package_launcher_lifecycle.py
