# =========================
#  Test groups and phony surface
# =========================
STABLE_TEST_TARGETS := \
	test-volume-frames-3d-export-contract \
	test-volume-frames-3d-tiny-parity-contract \
	test-atmospheric-warm-start-contract \
	test-manifest-to-trace-export \
	test-vf2d-pack-dataset-parity \
	test-trio-scene-contract-diff \
	test-sim-runtime-backend-2d-runtime-fields-contract \
	test-atmospheric-field-contract \
	test-soft-body-contract \
	test-kitviz-field-adapter \
	test-sim-mode-route-contract \
	test-wind-tunnel-3d-contract \
	test-sim-runtime-emitter-contract \
	test-sim-runtime-obstacle-contract \
	test-sim-runtime-backend-3d-emitter-contract \
	test-sim-runtime-backend-3d-attached-emitter-contract \
	test-sim-runtime-backend-3d-obstacle-contract \
	test-sim-runtime-backend-3d-wind-tunnel-contract \
	test-sim-runtime-backend-3d-retained-obstacle-contract \
	test-sim-runtime-3d-anchor-contract \
	test-sim-runtime-3d-footprint-contract \
	test-sim-runtime-3d-space-contract \
	test-sim-runtime-3d-domain-contract \
	test-sim-runtime-3d-solver-contract \
	test-scene-core-sim-runtime-step-contract \
	test-menu-settings-shell-contract \
	test-quality-profiles-contract \
	test-config-loader-contract \
	test-sim-runtime-backend-reporting-contract \
	test-sim-runtime-backend-dispatch-contract \
	test-preset-io-dimensional-contract \
	test-rigid2d-collision-contract \
	test-scene-objects-runtime-contract \
	test-scene-editor-retained-document-contract \
	test-scene-editor-scene-library-contract \
	test-physics-sim-workspace-authoring-host \
	test-scene-editor-pane-host-contract \
	test-scene-editor-viewport-contract \
	test-retained-runtime-scene-overlay-geom-contract \
	test-retained-runtime-scene-overlay-readout-contract \
	test-retained-runtime-scene-overlay-space-contract \
	test-scene-runtime-launch-projection-contract \
	test-runtime-scene-3d-truth-contract \
	test-runtime-scene-solver-projection-contract \
	test-runtime-scene-bridge-contract \
	test-structural-runtime-split-contract \
	test-physics-sim-ui-button-contract \
	test-physics-sim-headless-wind-analysis

LEGACY_TEST_TARGETS := \
	test-shared-theme-font-adapter

.PHONY: all run run-ide-theme run-daw-theme run-headless-smoke visual-harness package-desktop package-desktop-smoke package-desktop-self-test package-desktop-copy-desktop package-desktop-sync package-desktop-open package-desktop-remove package-desktop-refresh release-contract release-clean release-build release-bundle-audit release-sign release-verify release-verify-signed release-notarize release-staple release-verify-notarized release-artifact release-distribute release-desktop-refresh clean video vf2d_pack_tool vf2d_to_pack vf2d_dataset_tool physics_trace_tool physics_sim_headless physics-sim-job-runner runtime_scene_emitter_diag_tool manifest_to_trace test-stable test-legacy test-sim-runtime-backend-2d-contract test-sim-runtime-backend-2d-runtime-fields-contract test-atmospheric-field-contract test-soft-body-contract test-atmospheric-warm-start-contract test-kitviz-field-adapter test-sim-mode-route-contract test-wind-tunnel-3d-contract test-sim-runtime-emitter-contract test-sim-runtime-obstacle-contract test-sim-runtime-backend-3d-emitter-contract test-sim-runtime-backend-3d-attached-emitter-contract test-sim-runtime-backend-3d-obstacle-contract test-sim-runtime-backend-3d-wind-tunnel-contract test-sim-runtime-backend-3d-retained-obstacle-contract test-sim-runtime-3d-anchor-contract test-sim-runtime-3d-footprint-contract test-sim-runtime-3d-space-contract test-sim-runtime-3d-domain-contract test-sim-runtime-3d-solver-contract test-scene-core-sim-runtime-step-contract test-menu-settings-shell-contract test-quality-profiles-contract test-config-loader-contract test-sim-runtime-backend-reporting-contract test-sim-runtime-backend-dispatch-contract test-preset-io-dimensional-contract test-rigid2d-collision-contract test-scene-objects-runtime-contract test-scene-editor-retained-document-contract test-scene-editor-scene-library-contract test-physics-sim-workspace-authoring-host test-scene-editor-pane-host-contract test-scene-editor-viewport-contract test-retained-runtime-scene-overlay-geom-contract test-retained-runtime-scene-overlay-readout-contract test-retained-runtime-scene-overlay-space-contract test-scene-runtime-launch-projection-contract test-runtime-scene-3d-truth-contract test-runtime-scene-solver-projection-contract test-runtime-scene-bridge-contract test-structural-runtime-split-contract test-physics-sim-ui-button-contract test-vf2d-dataset-export test-manifest-to-trace-export test-vf2d-pack-dataset-parity test-trio-scene-contract-diff test-physics-sim-headless-cli test-physics-sim-headless-wind-analysis test-physics-sim-headless-wind-long-tunnel-visual test-physics-sim-headless-wind-long-tunnel-video test-physics-sim-headless-wind-object-comparison test-physics-sim-job-runner-smoke test-physics-sim-job-runner-policy test-volume-frames-3d-export-contract test-volume-frames-3d-tiny-parity-contract shim-parse-smoke shim-parse-parity shim-compile-subset shim-gate test-shared-theme-font-adapter
.PHONY: all clang-build fisics-build dump-sema dump-sema-runtime-3d-solver-step dump-sema-fluid2d dump-sema-rigid2d dump-sema-rigid2d-collision dump-sema-structural-runtime dump-sema-particles2d dump-sema-structural-solver dump-sema-atmospheric-field dump-sema-object-manager dump-sema-soft-body dump-sema-runtime-fields-2d dump-sema-runtime-backend-2d dump-sema-runtime-backend-3d-emitters dump-sema-runtime-backend-3d-runtime dump-sema-runtime-backend-3d-obstacles dump-sema-runtime-emitter dump-sema-runtime-obstacle toolchain-contract run run-ide-theme run-daw-theme run-headless-smoke visual-harness package-desktop package-desktop-smoke package-desktop-self-test package-desktop-copy-desktop package-desktop-sync package-desktop-open package-desktop-remove package-desktop-refresh release-contract release-clean release-build release-bundle-audit release-sign release-verify release-verify-signed release-notarize release-staple release-verify-notarized release-artifact release-distribute release-desktop-refresh clean video vf2d_pack_tool vf2d_to_pack vf2d_dataset_tool physics_trace_tool physics_sim_headless physics-sim-job-runner runtime_scene_emitter_diag_tool manifest_to_trace test-stable test-legacy test-sim-runtime-backend-2d-contract test-sim-runtime-backend-2d-runtime-fields-contract test-atmospheric-field-contract test-soft-body-contract test-atmospheric-warm-start-contract test-kitviz-field-adapter test-sim-mode-route-contract test-wind-tunnel-3d-contract test-sim-runtime-emitter-contract test-sim-runtime-obstacle-contract test-sim-runtime-backend-3d-emitter-contract test-sim-runtime-backend-3d-attached-emitter-contract test-sim-runtime-backend-3d-obstacle-contract test-sim-runtime-backend-3d-wind-tunnel-contract test-sim-runtime-backend-3d-retained-obstacle-contract test-sim-runtime-3d-anchor-contract test-sim-runtime-3d-footprint-contract test-sim-runtime-3d-space-contract test-sim-runtime-3d-domain-contract test-sim-runtime-3d-solver-contract test-scene-core-sim-runtime-step-contract test-menu-settings-shell-contract test-quality-profiles-contract test-config-loader-contract test-sim-runtime-backend-reporting-contract test-sim-runtime-backend-dispatch-contract test-preset-io-dimensional-contract test-rigid2d-collision-contract test-scene-objects-runtime-contract test-scene-editor-retained-document-contract test-scene-editor-scene-library-contract test-physics-sim-workspace-authoring-host test-scene-editor-pane-host-contract test-scene-editor-viewport-contract test-retained-runtime-scene-overlay-geom-contract test-retained-runtime-scene-overlay-readout-contract test-retained-runtime-scene-overlay-space-contract test-scene-runtime-launch-projection-contract test-runtime-scene-3d-truth-contract test-runtime-scene-solver-projection-contract test-runtime-scene-bridge-contract test-structural-runtime-split-contract test-physics-sim-ui-button-contract test-vf2d-dataset-export test-manifest-to-trace-export test-vf2d-pack-dataset-parity test-trio-scene-contract-diff test-physics-sim-headless-cli test-physics-sim-headless-wind-analysis test-physics-sim-headless-wind-long-tunnel-visual test-physics-sim-headless-wind-long-tunnel-video test-physics-sim-headless-wind-object-comparison test-physics-sim-job-runner-smoke test-physics-sim-job-runner-bundle-smoke test-physics-sim-job-runner-policy test-volume-frames-3d-export-contract test-volume-frames-3d-tiny-parity-contract shim-parse-smoke shim-parse-parity shim-compile-subset shim-gate test-shared-theme-font-adapter

test-stable:
	@$(MAKE) $(STABLE_TEST_TARGETS)
	@echo "physics_sim stable test lane passed"

test-legacy:
	@set +e; \
	fails=0; \
	for t in $(LEGACY_TEST_TARGETS); do \
		echo "[legacy] running $$t"; \
		$(MAKE) $$t || fails=1; \
	done; \
	if [ $$fails -ne 0 ]; then \
		echo "[legacy] one or more legacy tests failed"; \
		exit 1; \
	fi
