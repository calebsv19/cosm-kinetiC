# =========================
#  Linux headless worker packaging
# =========================
LINUX_WORKER_HOST_ARCH := $(shell uname -m)
LINUX_WORKER_HOST_OS := $(shell uname -s)
ifndef LINUX_WORKER_PLATFORM
ifeq ($(LINUX_WORKER_HOST_ARCH),x86_64)
LINUX_WORKER_PLATFORM := linux-x86_64
else ifeq ($(LINUX_WORKER_HOST_ARCH),amd64)
LINUX_WORKER_PLATFORM := linux-x86_64
else ifeq ($(LINUX_WORKER_HOST_ARCH),aarch64)
LINUX_WORKER_PLATFORM := linux-aarch64
else ifeq ($(LINUX_WORKER_HOST_ARCH),arm64)
LINUX_WORKER_PLATFORM := linux-aarch64
else
$(error Unsupported Linux worker package host architecture: $(LINUX_WORKER_HOST_ARCH))
endif
endif
LINUX_WORKER_SLUG := physics_sim_headless_worker
LINUX_WORKER_MAX_GLIBC ?= 2.39.0
ifeq ($(LINUX_WORKER_PLATFORM),linux-x86_64)
LINUX_WORKER_PLATFORM_CAPABILITY := platform-linux-x86_64-v1
else ifeq ($(LINUX_WORKER_PLATFORM),linux-aarch64)
LINUX_WORKER_PLATFORM_CAPABILITY := platform-linux-aarch64-v1
else
$(error Unsupported Linux worker package platform: $(LINUX_WORKER_PLATFORM))
endif
LINUX_WORKER_BASENAME := $(RELEASE_PROGRAM_KEY)-$(RELEASE_VERSION)-$(LINUX_WORKER_PLATFORM)-worker
LINUX_WORKER_DIR := $(RELEASE_DIR)/$(LINUX_WORKER_BASENAME)
LINUX_WORKER_BIN_DIR := $(LINUX_WORKER_DIR)/bin
LINUX_WORKER_CONFIG_DIR := $(LINUX_WORKER_DIR)/config
LINUX_WORKER_DOCS_DIR := $(LINUX_WORKER_DIR)/docs
LINUX_WORKER_MANIFEST_JSON := $(LINUX_WORKER_DIR)/manifest.json
LINUX_WORKER_MANIFEST := $(LINUX_WORKER_DIR)/package_manifest.json
LINUX_WORKER_ARCHIVE := $(RELEASE_DIR)/$(LINUX_WORKER_BASENAME).tar.gz
LINUX_WORKER_SHA256 := $(LINUX_WORKER_ARCHIVE).sha256
LINUX_WORKER_ARTIFACT_MANIFEST := $(LINUX_WORKER_ARCHIVE).manifest.txt
LINUX_WORKER_PACKAGE_VALIDATOR := tools/packaging/validate_linux_worker_package.py
LINUX_WORKER_ARTIFACT_MANIFEST_WRITER := tools/packaging/write_linux_worker_artifact_manifest.py

.PHONY: package-linux-worker-contract package-linux-worker-host-check package-linux-worker-clean package-linux-worker package-linux-worker-self-test package-linux-worker-dry-run test-linux-worker-artifact-manifest

package-linux-worker-contract:
	@echo "Linux worker package contract"
	@echo "  worker slug: $(LINUX_WORKER_SLUG)"
	@echo "  version:     $(RELEASE_VERSION)"
	@echo "  platform:    $(LINUX_WORKER_PLATFORM)"
	@echo "  max glibc:   $(LINUX_WORKER_MAX_GLIBC)"
	@echo "  stage dir:   $(LINUX_WORKER_DIR)"
	@echo "  archive:     $(LINUX_WORKER_ARCHIVE)"
	@echo "  binaries:"
	@echo "    - $(PHYSICS_SIM_HEADLESS_TOOL_BIN)"
	@echo "    - $(PHYSICS_SIM_JOB_RUNNER_TOOL_BIN)"

package-linux-worker-host-check:
	@if [ "$(LINUX_WORKER_HOST_OS)" != "Linux" ]; then \
		echo "Linux worker package build requires a Linux host; got $(LINUX_WORKER_HOST_OS)/$(LINUX_WORKER_HOST_ARCH)"; \
		exit 1; \
	fi
	@if { [ "$(LINUX_WORKER_PLATFORM)" = "linux-x86_64" ] && [ "$(LINUX_WORKER_HOST_ARCH)" != "x86_64" ] && [ "$(LINUX_WORKER_HOST_ARCH)" != "amd64" ]; } || \
	   { [ "$(LINUX_WORKER_PLATFORM)" = "linux-aarch64" ] && [ "$(LINUX_WORKER_HOST_ARCH)" != "aarch64" ] && [ "$(LINUX_WORKER_HOST_ARCH)" != "arm64" ]; }; then \
		echo "Linux worker package platform $(LINUX_WORKER_PLATFORM) does not match native host architecture $(LINUX_WORKER_HOST_ARCH); no cross toolchain is configured"; \
		exit 1; \
	fi

package-linux-worker-clean:
	@rm -rf "$(LINUX_WORKER_DIR)" "$(LINUX_WORKER_ARCHIVE)" "$(LINUX_WORKER_SHA256)" "$(LINUX_WORKER_ARTIFACT_MANIFEST)"
	@echo "Removed Linux worker package artifacts: $(LINUX_WORKER_BASENAME)"

package-linux-worker: package-linux-worker-host-check physics_sim_headless physics-sim-job-runner
	@echo "Preparing Linux worker package..."
	@rm -rf "$(LINUX_WORKER_DIR)"
	@mkdir -p "$(LINUX_WORKER_BIN_DIR)" "$(LINUX_WORKER_CONFIG_DIR)" "$(LINUX_WORKER_DOCS_DIR)"
	@cp "$(PHYSICS_SIM_HEADLESS_TOOL_BIN)" "$(LINUX_WORKER_BIN_DIR)/physics_sim_headless"
	@cp "$(PHYSICS_SIM_JOB_RUNNER_TOOL_BIN)" "$(LINUX_WORKER_BIN_DIR)/physics_sim_job_runner"
	@printf '#!/usr/bin/env bash\n' > "$(LINUX_WORKER_BIN_DIR)/run_worker.sh"
	@printf 'set -euo pipefail\n' >> "$(LINUX_WORKER_BIN_DIR)/run_worker.sh"
	@printf 'SCRIPT_DIR="$$(cd "$$(dirname "$${BASH_SOURCE[0]}")" && pwd)"\n' >> "$(LINUX_WORKER_BIN_DIR)/run_worker.sh"
	@printf 'exec "$$SCRIPT_DIR/physics_sim_headless" "$$@"\n' >> "$(LINUX_WORKER_BIN_DIR)/run_worker.sh"
	@chmod +x "$(LINUX_WORKER_BIN_DIR)/run_worker.sh"
	@cp -R config/. "$(LINUX_WORKER_CONFIG_DIR)/"
	@cp README.md "$(LINUX_WORKER_DIR)/"
	@cp docs/README.md docs/headless_cli.md "$(LINUX_WORKER_DOCS_DIR)/"
	@printf '{\n' > "$(LINUX_WORKER_MANIFEST_JSON)"
	@printf '  "schema_version": "codework-worker-package/v1",\n' >> "$(LINUX_WORKER_MANIFEST_JSON)"
	@printf '  "worker_slug": "%s",\n' "$(LINUX_WORKER_SLUG)" >> "$(LINUX_WORKER_MANIFEST_JSON)"
	@printf '  "version": "%s",\n' "$(RELEASE_VERSION)" >> "$(LINUX_WORKER_MANIFEST_JSON)"
	@printf '  "platform": "%s",\n' "$(LINUX_WORKER_PLATFORM)" >> "$(LINUX_WORKER_MANIFEST_JSON)"
	@printf '  "program": "%s",\n' "$(RELEASE_PROGRAM_KEY)" >> "$(LINUX_WORKER_MANIFEST_JSON)"
	@printf '  "job_types": ["trio_headless_stage"],\n' >> "$(LINUX_WORKER_MANIFEST_JSON)"
	@printf '  "capabilities": ["trio-headless-v1", "scene-project-portable-v1", "physics-cache-project-local-v1", "%s"],\n' "$(LINUX_WORKER_PLATFORM_CAPABILITY)" >> "$(LINUX_WORKER_MANIFEST_JSON)"
	@printf '  "entrypoint": "bin/run_worker.sh",\n' >> "$(LINUX_WORKER_MANIFEST_JSON)"
	@printf '  "default_args": [],\n' >> "$(LINUX_WORKER_MANIFEST_JSON)"
	@printf '  "max_glibc_version": "%s",\n' "$(LINUX_WORKER_MAX_GLIBC)" >> "$(LINUX_WORKER_MANIFEST_JSON)"
	@printf '  "runtime_dependencies": ["glibc", "libgcc_s", "libm", "SDL2", "SDL2_ttf"]\n' >> "$(LINUX_WORKER_MANIFEST_JSON)"
	@printf '}\n' >> "$(LINUX_WORKER_MANIFEST_JSON)"
	@printf '{\n' > "$(LINUX_WORKER_MANIFEST)"
	@printf '  "schema_version": "codework_worker_package_manifest_v1",\n' >> "$(LINUX_WORKER_MANIFEST)"
	@printf '  "package_role": "headless-worker",\n' >> "$(LINUX_WORKER_MANIFEST)"
	@printf '  "worker_slug": "%s",\n' "$(LINUX_WORKER_SLUG)" >> "$(LINUX_WORKER_MANIFEST)"
	@printf '  "program": "%s",\n' "$(RELEASE_PROGRAM_KEY)" >> "$(LINUX_WORKER_MANIFEST)"
	@printf '  "version": "%s",\n' "$(RELEASE_VERSION)" >> "$(LINUX_WORKER_MANIFEST)"
	@printf '  "platform": "%s",\n' "$(LINUX_WORKER_PLATFORM)" >> "$(LINUX_WORKER_MANIFEST)"
	@printf '  "max_glibc_version": "%s",\n' "$(LINUX_WORKER_MAX_GLIBC)" >> "$(LINUX_WORKER_MANIFEST)"
	@printf '  "entrypoints": {\n' >> "$(LINUX_WORKER_MANIFEST)"
	@printf '    "headless_cli": "bin/physics_sim_headless",\n' >> "$(LINUX_WORKER_MANIFEST)"
	@printf '    "job_runner": "bin/physics_sim_job_runner"\n' >> "$(LINUX_WORKER_MANIFEST)"
	@printf '  },\n' >> "$(LINUX_WORKER_MANIFEST)"
	@printf '  "capabilities": ["trio-headless-v1", "scene-project-portable-v1", "physics-cache-project-local-v1", "%s"],\n' "$(LINUX_WORKER_PLATFORM_CAPABILITY)" >> "$(LINUX_WORKER_MANIFEST)"
	@printf '  "runtime_dependencies": ["glibc", "libgcc_s", "libm"],\n' >> "$(LINUX_WORKER_MANIFEST)"
	@printf '  "self_test": {\n' >> "$(LINUX_WORKER_MANIFEST)"
	@printf '    "type": "command",\n' >> "$(LINUX_WORKER_MANIFEST)"
	@printf '    "argv": ["bin/physics_sim_job_runner", "submit", "--help"]\n' >> "$(LINUX_WORKER_MANIFEST)"
	@printf '  }\n' >> "$(LINUX_WORKER_MANIFEST)"
	@printf '}\n' >> "$(LINUX_WORKER_MANIFEST)"
	@mkdir -p "$(RELEASE_DIR)"
	@tar -czf "$(LINUX_WORKER_ARCHIVE)" -C "$(RELEASE_DIR)" "$(LINUX_WORKER_BASENAME)"
	@sha256sum "$(LINUX_WORKER_ARCHIVE)" > "$(LINUX_WORKER_SHA256)"
	@python3 "$(LINUX_WORKER_ARTIFACT_MANIFEST_WRITER)" \
		--archive "$(LINUX_WORKER_ARCHIVE)" \
		--checksum "$(LINUX_WORKER_SHA256)" \
		--output "$(LINUX_WORKER_ARTIFACT_MANIFEST)" \
		--program "$(RELEASE_PROGRAM_KEY)" \
		--version "$(RELEASE_VERSION)" \
		--platform "$(LINUX_WORKER_PLATFORM)" \
		--worker-slug "$(LINUX_WORKER_SLUG)" \
		--max-glibc-version "$(LINUX_WORKER_MAX_GLIBC)"
	@echo "Linux worker package ready: $(LINUX_WORKER_ARCHIVE)"

package-linux-worker-self-test: package-linux-worker
	@test -x "$(LINUX_WORKER_BIN_DIR)/physics_sim_headless" || (echo "Missing physics_sim_headless"; exit 1)
	@test -x "$(LINUX_WORKER_BIN_DIR)/physics_sim_job_runner" || (echo "Missing physics_sim_job_runner"; exit 1)
	@test -x "$(LINUX_WORKER_BIN_DIR)/run_worker.sh" || (echo "Missing run_worker.sh"; exit 1)
	@test -f "$(LINUX_WORKER_MANIFEST_JSON)" || (echo "Missing manifest.json"; exit 1)
	@test -f "$(LINUX_WORKER_MANIFEST)" || (echo "Missing package_manifest.json"; exit 1)
	@test -f "$(LINUX_WORKER_DOCS_DIR)/headless_cli.md" || (echo "Missing docs/headless_cli.md"; exit 1)
	@test -f "$(LINUX_WORKER_CONFIG_DIR)/app.json" || (echo "Missing config/app.json"; exit 1)
	@test -f "$(LINUX_WORKER_ARCHIVE)" || (echo "Missing worker archive"; exit 1)
	@test -f "$(LINUX_WORKER_SHA256)" || (echo "Missing worker checksum"; exit 1)
	@test -f "$(LINUX_WORKER_ARTIFACT_MANIFEST)" || (echo "Missing worker artifact sidecar manifest"; exit 1)
	@python3 "$(LINUX_WORKER_ARTIFACT_MANIFEST_WRITER)" \
		--archive "$(LINUX_WORKER_ARCHIVE)" \
		--checksum "$(LINUX_WORKER_SHA256)" \
		--output "$(LINUX_WORKER_ARTIFACT_MANIFEST)" \
		--program "$(RELEASE_PROGRAM_KEY)" \
		--version "$(RELEASE_VERSION)" \
		--platform "$(LINUX_WORKER_PLATFORM)" \
		--worker-slug "$(LINUX_WORKER_SLUG)" \
		--max-glibc-version "$(LINUX_WORKER_MAX_GLIBC)" \
		--verify
	@echo "package-linux-worker-self-test passed."

package-linux-worker-dry-run: package-linux-worker-self-test
	@python3 "$(LINUX_WORKER_PACKAGE_VALIDATOR)" \
		--stage-dir "$(LINUX_WORKER_DIR)" \
		--archive "$(LINUX_WORKER_ARCHIVE)" \
		--program "$(RELEASE_PROGRAM_KEY)" \
		--version "$(RELEASE_VERSION)" \
		--platform "$(LINUX_WORKER_PLATFORM)" \
		--worker-slug "$(LINUX_WORKER_SLUG)" \
		--max-glibc-version "$(LINUX_WORKER_MAX_GLIBC)"

test-linux-worker-artifact-manifest:
	@python3 -m unittest -v tests.test_linux_worker_artifact_manifest
