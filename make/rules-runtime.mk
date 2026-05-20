# =========================
#  Runtime convenience targets
# =========================
run: $(TARGET)
	./$(TARGET)

run-ide-theme: $(TARGET)
	PHYSICS_SIM_USE_SHARED_THEME_FONT=1 PHYSICS_SIM_USE_SHARED_THEME=1 PHYSICS_SIM_USE_SHARED_FONT=1 PHYSICS_SIM_THEME_PRESET=ide_gray PHYSICS_SIM_FONT_PRESET=ide ./$(TARGET)

run-daw-theme: $(TARGET)
	PHYSICS_SIM_USE_SHARED_THEME_FONT=1 PHYSICS_SIM_USE_SHARED_THEME=1 PHYSICS_SIM_USE_SHARED_FONT=1 PHYSICS_SIM_THEME_PRESET=daw_default PHYSICS_SIM_FONT_PRESET=daw_default ./$(TARGET)

run-headless-smoke: all test-stable test-physics-sim-headless-cli
	@echo "physics_sim headless smoke passed (non-interactive)"

visual-harness: $(TARGET)
	@echo "visual harness binary ready: $(TARGET)"

VIDEO_FPS ?= 30
video:
	ffmpeg -y -framerate $(VIDEO_FPS) -i export/render_frames/frame_%06d.bmp -pix_fmt yuv420p export/render_vid/output.mp4

clean:
	rm -rf $(BUILD_DIR) $(TARGET) $(VF2D_PACK_TOOL_BIN) $(VF2D_DATASET_TOOL_BIN) $(PHYSICS_TRACE_TOOL_BIN) $(PHYSICS_SIM_HEADLESS_TOOL_BIN) $(PHYSICS_SIM_JOB_RUNNER_TOOL_BIN) $(RUNTIME_SCENE_EMITTER_DIAG_TOOL_BIN)
