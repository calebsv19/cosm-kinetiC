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
.PHONY: help clean clean-plan
help:
	@echo "PhysicsSim local source workflow (run each command separately):"
	@echo "  make physics_sim_headless                            Build the selected owned profile"
	@echo "  make test-physics-sim-headless-water-mode             Water smoke; resets its named tmp output"
	@echo "  make test-physics-sim-headless-scene-project-cache-output  Scene-cache proof; resets its named tmp output"
	@echo "  make clean-plan                                      Inspect selected compiler outputs without deletion"
	@echo "  make clean                                           Remove only admitted owned compiler outputs"
	@echo "Selected build profile: $(BUILD_DIR)"
	@echo "Retained evidence: data/experiments/; reference tools: data/tools/"
	@echo "Unknown or changed outputs hold cleanup; preserve them and inspect the refusal."
	@echo "Use disposable checkouts for fixtures when their output directories contain retained work."
	@echo "Read docs/cleanup_operations.md and docs/launcher_configuration_recovery.md for recovery."
	@echo "Packaging, installation and release require their separate operating authority."

clean-plan:
	python3 scripts/clean_outputs.py $(CLEAN_ARGUMENTS)

clean:
	python3 scripts/clean_outputs.py $(CLEAN_ARGUMENTS) --apply

.PHONY: test-top-level-cleanup test-build-identity
test-top-level-cleanup:
	python3 -B tests/test_clean_outputs.py

test-build-identity:
	python3 -B tests/test_build_identity.py

.PHONY: status
status:
	@python3 -B scripts/physics_status.py --build-root "$(BUILD_DIR)" --test-root "$(TEST_TMP_DIR)" --experiments-root "$(EXPERIMENT_DIR)" --tools-root "$(REFERENCE_TOOLS_DIR)" $(foreach output,$(CLEAN_EXECUTABLES),--executable "$(output)")

.PHONY: test-cfd-evidence-lifecycle
test-cfd-evidence-lifecycle:
	python3 -B tests/test_cfd_evidence_lifecycle.py

.PHONY: test-atomic-output
test-atomic-output:
	python3 -B tests/test_atomic_output.py

.PHONY: test-build-output-isolation
test-build-output-isolation:
	python3 -B tests/test_build_output_isolation.py

.PHONY: test-build-owner
test-build-owner:
	python3 -B tests/test_build_owner.py

.PHONY: test-fixture-root
test-fixture-root:
	python3 -B tests/test_fixture_root.py

.PHONY: test-package-outputs
test-package-outputs:
	python3 -B tests/test_package_outputs.py


DOCTOR_PROFILE ?= headless
DOCTOR_REFERENCE_PYTHON ?= $(if $(CFD_REFINED_REFERENCE_PYTHON),$(CFD_REFINED_REFERENCE_PYTHON),$(REFERENCE_TOOLS_DIR)/cfd-reference-venv/bin/python)
.PHONY: doctor test-physics-doctor test-evidence-admission test-semantic-proof test-retained-compile
doctor:
	@python3 -B scripts/physics_doctor.py --profile "$(DOCTOR_PROFILE)" --reference-python "$(DOCTOR_REFERENCE_PYTHON)" --json-compat-prefix "$(PHYSICS_SIM_JSON_COMPAT_PREFIX)" --build-root "$(BUILD_DIR)" --test-root "$(TEST_TMP_DIR)" --experiments-root "$(EXPERIMENT_DIR)" --tools-root "$(REFERENCE_TOOLS_DIR)" --compiler "$(CLANG)" --pkg-config "$(PKG_CONFIG)" --target-arch "$(TARGET_ARCH)" $(foreach output,$(CLEAN_EXECUTABLES),--executable "$(output)")

test-physics-doctor:
	python3 -B tests/test_physics_doctor.py

test-evidence-admission:
	python3 -B tests/test_evidence_admission.py

test-semantic-proof:
	python3 -B tests/test_semantic_proof.py

test-retained-compile:
	python3 -B tests/test_retained_compile.py


.PHONY: cfd-reference-env cfd-reference-amg-env reference-env-plan test-reference-environment-setup
reference-env-plan:
	@python3 -B scripts/reference_environment_setup.py --tools-root "$(REFERENCE_TOOLS_DIR)" --interpreter "$(CFD_REFINED_REFERENCE_PYTHON)"

cfd-reference-env:
	@python3 -B scripts/reference_environment_setup.py --tools-root "$(REFERENCE_TOOLS_DIR)" --interpreter "$(CFD_REFINED_REFERENCE_PYTHON)" --apply

cfd-reference-amg-env:
	@python3 -B scripts/reference_environment_setup.py --tools-root "$(REFERENCE_TOOLS_DIR)" --interpreter "$(CFD_REFINED_REFERENCE_PYTHON)" --amg --apply

test-reference-environment-setup:
	python3 -B tests/test_reference_environment_setup.py

.PHONY: test-contract-writer-lifecycle
test-contract-writer-lifecycle:
	python3 -B tests/test_contract_writer_lifecycle.py

.PHONY: test-configuration-admission
test-configuration-admission:
	python3 -B tests/test_configuration_admission.py

.PHONY: test-tool-probe
test-tool-probe:
	python3 -B tests/test_tool_probe.py

.PHONY: test-contract-proof
test-contract-proof:
	python3 -B tests/test_contract_proof.py

.PHONY: retention-audit test-retention-audit
retention-audit:
	@python3 -B scripts/retention_audit.py --path "$(BUILD_DIR)" --path "$(TEST_TMP_DIR)" --path "$(EXPERIMENT_DIR)" --path "$(REFERENCE_TOOLS_DIR)"

test-retention-audit:
	python3 -B tests/test_retention_audit.py

.PHONY: test-fixture-session
test-fixture-session:
	python3 -B tests/test_fixture_session.py

.PHONY: test-restore-rehearsal
test-restore-rehearsal:
	python3 -B tests/test_restore_rehearsal.py

.PHONY: test-atmosphere-worker-selection
test-atmosphere-worker-selection:
	python3 -B tests/test_atmosphere_worker_selection.py

.PHONY: test-worker-capture
test-worker-capture:
	python3 -B tests/test_worker_capture.py

.PHONY: test-artifact-inventory
test-artifact-inventory:
	python3 -B tests/test_artifact_inventory.py

.PHONY: test-package-transaction test-package-proof
test-package-transaction:
	python3 -B tests/test_package_transaction.py
test-package-proof:
	python3 -B tests/test_package_proof.py

.PHONY: test-release-audit
test-release-audit:
	python3 -B tests/test_release_audit.py

.PHONY: test-release-local-artifact
test-release-local-artifact:
	python3 -B tests/test_release_local_artifact.py

.PHONY: test-release-app-stage
test-release-app-stage:
	python3 -B tests/test_release_app_stage.py

.PHONY: test-release-notary
test-release-notary:
	python3 -B tests/test_release_notary.py

.PHONY: test-release-notary-archive
test-release-notary-archive:
	python3 -B tests/test_release_notary_archive.py

.PHONY: test-release-zip-validation
test-release-zip-validation:
	python3 -B tests/test_release_zip_validation.py

.PHONY: test-release-final-artifact
test-release-final-artifact:
	python3 -B tests/test_release_final_artifact.py

.PHONY: test-release-pipeline
test-release-pipeline:
	@python3 -B tests/test_release_pipeline.py

.PHONY: test-sample-retention
test-sample-retention:
	@python3 -B tests/test_sample_retention.py

.PHONY: test-session-paths
test-session-paths:
	@python3 -B tests/test_session_paths.py

.PHONY: test-session-attempts
test-session-attempts:
	@python3 -B tests/test_session_attempts.py

.PHONY: test-evidence-retirement-plan
test-evidence-retirement-plan:
	PYTHONPATH=tests python3 -B -m unittest test_evidence_retirement_plan

.PHONY: test-cfd-run-supervision
test-cfd-run-supervision:
	python3 -B tests/test_cfd_run_supervision.py

.PHONY: test-reference-supervision
test-reference-supervision:
	python3 -B tests/test_reference_supervision.py

.PHONY: test-factor-reference-supervision
test-factor-reference-supervision:
	python3 -B tests/test_factor_reference_supervision.py

.PHONY: test-factor-build-transaction
test-factor-build-transaction:
	python3 -B tests/test_factor_build_transaction.py

.PHONY: test-reference-record-admission
test-reference-record-admission:
	python3 -B tests/test_reference_record_admission.py

.PHONY: test-reference-root-admission
test-reference-root-admission:
	python3 -B tests/test_reference_root_admission.py

.PHONY: test-reference-ownership
test-reference-ownership:
	python3 -B tests/test_reference_ownership.py

.PHONY: test-reference-make-borrowing
test-reference-make-borrowing:
	python3 -B tests/test_reference_make_borrowing.py

.PHONY: test-reference-receipt-publication
test-reference-receipt-publication:
	python3 -B tests/test_reference_receipt_publication.py

.PHONY: test-reference-receipt-recovery
test-reference-receipt-recovery:
	python3 -B tests/test_reference_receipt_recovery.py

.PHONY: test-writer-inventory
test-writer-inventory:
	python3 -B tests/test_writer_inventory.py

.PHONY: test-native-persistence
test-native-persistence:
	python3 -B tests/test_native_persistence.py

.PHONY: test-config-save-lifecycle
test-config-save-lifecycle:
	python3 -B tests/test_config_save_lifecycle.py

.PHONY: test-runtime-directory-lifecycle
test-runtime-directory-lifecycle:
	python3 -B tests/test_runtime_directory_lifecycle.py

.PHONY: test-config-read-admission
test-config-read-admission:
	python3 -B tests/test_config_read_admission.py

.PHONY: test-config-json-contract
test-config-json-contract:
	python3 -B tests/test_config_json_contract.py

.PHONY: test-config-option-persistence
test-config-option-persistence:
	python3 -B tests/test_config_option_persistence.py

.PHONY: test-snapshot-publication
test-snapshot-publication:
	python3 -B tests/test_snapshot_publication.py

.PHONY: test-cache-publish-admission
test-cache-publish-admission:
	python3 -B tests/test_cache_publish_admission.py

.PHONY: test-cache-publish-transaction
test-cache-publish-transaction:
	python3 -B tests/test_cache_publish_transaction.py

.PHONY: test-cache-publication-recovery
test-cache-publication-recovery:
	python3 -B tests/test_cache_publication_recovery.py

.PHONY: test-cache-status-admission
test-cache-status-admission:
	python3 -B tests/test_cache_status_admission.py

.PHONY: test-cache-inventory
test-cache-inventory:
	python3 -B tests/test_cache_inventory.py

.PHONY: test-cache-payload-validation
test-cache-payload-validation:
	python3 -B tests/test_cache_payload_validation.py

.PHONY: test-cache-compiler-profiles
test-cache-compiler-profiles:
	python3 -B -m unittest discover -s tests -p test_cache_compiler_profiles.py -v

.PHONY: test-contract-fixture-ownership
test-contract-fixture-ownership:
	python3 -B tests/test_contract_fixture_ownership.py

.PHONY: test-output-read-admission
test-output-read-admission:
	python3 -B tests/test_output_read_admission.py

.PHONY: test-clean-json-bounds
test-clean-json-bounds:
	python3 -B tests/test_clean_json_bounds.py

.PHONY: test-clean-inventory-budget
test-clean-inventory-budget:
	python3 -B tests/test_clean_inventory_budget.py

.PHONY: test-clean-reservations
test-clean-reservations:
	python3 -B tests/test_clean_reservations.py

.PHONY: test-clean-reservation-schema
test-clean-reservation-schema:
	python3 -B tests/test_clean_reservation_schema.py

.PHONY: test-build-environment
test-build-environment:
	python3 -B tests/test_build_environment.py

.PHONY: test-build-dependency-admission
test-build-dependency-admission:
	python3 -B tests/test_build_dependency_admission.py

.PHONY: test-compiler-input-boundary
test-compiler-input-boundary:
	python3 -B tests/test_compiler_input_boundary.py

.PHONY: test-object-dependency-coverage
test-object-dependency-coverage:
	python3 -B tests/test_object_dependency_coverage.py

.PHONY: test-link-input-identity
test-link-input-identity:
	PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tests -p test_link_input_identity.py -v

.PHONY: test-shape-link-objects
test-shape-link-objects:
	PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tests -p test_shape_link_objects.py -v

.PHONY: test-cli-object-identity
test-cli-object-identity:
	PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tests -p test_cli_object_identity.py -v

.PHONY: test-emitter-cli-arguments
test-emitter-cli-arguments:
	@test -n "$$PHYSICS_SIM_EMITTER_ARGUMENT_TEST_BIN" || { echo "Set PHYSICS_SIM_EMITTER_ARGUMENT_TEST_BIN to the native binary to qualify" >&2; exit 2; }
	PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tests -p test_emitter_cli_arguments.py -v

.PHONY: test-frame-retention-entrypoint
test-frame-retention-entrypoint:
	python3 -B tests/test_frame_retention_entrypoint.py

.PHONY: test-wind-probe-lifecycle
test-wind-probe-lifecycle:
	python3 -B tests/test_wind_probe_lifecycle.py

.PHONY: test-macos-bundle-lifecycle
test-macos-bundle-lifecycle:
	python3 -B tests/test_macos_bundle_lifecycle.py

.PHONY: test-linux-desktop-installer
test-linux-desktop-installer:
	python3 -B tests/test_linux_desktop_installer.py

.PHONY: test-package-launcher-lifecycle
test-package-launcher-lifecycle:
	python3 -B tests/test_package_launcher_lifecycle.py
