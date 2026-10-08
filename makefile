# Read-only/cleanup control must work without compiler or optional library setup.
CONTROL_GOALS := test-package-launcher-lifecycle test-linux-desktop-installer test-macos-bundle-lifecycle test-wind-probe-lifecycle test-frame-retention-entrypoint test-emitter-cli-arguments test-cli-object-identity test-shape-link-objects test-link-input-identity test-object-dependency-coverage test-compiler-input-boundary test-build-dependency-admission test-build-environment test-clean-reservation-schema test-clean-reservations test-clean-inventory-budget test-clean-json-bounds test-output-read-admission test-contract-fixture-ownership test-cache-payload-validation test-cache-inventory test-cache-status-admission test-cache-publication-recovery test-cache-publish-transaction test-cache-publish-admission test-snapshot-publication test-config-option-persistence test-config-json-contract test-config-read-admission test-runtime-directory-lifecycle test-config-save-lifecycle test-native-persistence test-writer-inventory test-reference-receipt-recovery test-reference-receipt-publication test-reference-make-borrowing test-reference-ownership test-reference-root-admission test-reference-record-admission test-factor-build-transaction test-factor-reference-supervision test-reference-supervision test-cfd-run-supervision test-evidence-retirement-plan test-session-attempts test-session-paths test-sample-retention test-release-pipeline test-release-final-artifact test-release-zip-validation test-release-notary-archive test-release-notary test-release-app-stage test-release-local-artifact test-release-audit release-contract test-package-transaction test-package-proof test-artifact-inventory test-worker-capture test-atmosphere-worker-selection test-restore-rehearsal test-fixture-session retention-audit test-retention-audit test-contract-proof test-tool-probe test-configuration-admission test-contract-writer-lifecycle cfd-reference-env cfd-reference-amg-env reference-env-plan test-reference-environment-setup doctor test-physics-doctor test-evidence-admission test-semantic-proof test-retained-compile test-package-outputs test-fixture-root test-build-owner test-build-output-isolation test-atomic-output clean clean-plan status test-top-level-cleanup test-build-identity test-cfd-evidence-lifecycle
CONTROL_ONLY :=
ifneq ($(strip $(MAKECMDGOALS)),)
ifeq ($(strip $(filter-out $(CONTROL_GOALS),$(MAKECMDGOALS))),)
CONTROL_ONLY := 1
endif
endif

ifneq ($(filter clean,$(MAKECMDGOALS)),)
ifneq ($(filter-out $(CONTROL_GOALS),$(MAKECMDGOALS)),)
$(error Run clean and build as separate operations; mixed goals cannot own both lifecycles)
endif
endif

ifeq ($(CONTROL_ONLY),1)
include make/config.mk
include make/sources-tools.mk
SESSION_WORKER_BIN ?= physics_sim_session_worker
include make/rules-runtime.mk
ifneq ($(filter release-contract,$(MAKECMDGOALS)),)
include make/target.mk
include make/package-paths.mk
include make/release.mk
endif
else
include make/config.mk
# Build mutations enter ownership before compiler probes/configuration selection.
owner_quote = '$(subst ','"'"',$(1))'
OWNER_FLAGS := $(firstword $(MAKEFLAGS))
OWNER_READ_ONLY :=
ifeq ($(filter --%,$(OWNER_FLAGS)),)
OWNER_READ_ONLY := $(or $(findstring n,$(OWNER_FLAGS)),$(findstring q,$(OWNER_FLAGS)))
endif
OWNER_STATE := $(shell python3 -B scripts/build_owner.py --root $(call owner_quote,$(BUILD_DIR)) --check-inherited)
ifeq ($(strip $(OWNER_READ_ONLY)$(filter owned,$(OWNER_STATE))),)
.DEFAULT_GOAL := owned-build
.PHONY: owned-build $(MAKECMDGOALS)
owned-build:
	+python3 -B scripts/build_owner.py --root $(call owner_quote,$(BUILD_DIR)) -- $(MAKE) -f $(call owner_quote,$(firstword $(MAKEFILE_LIST))) $(foreach goal,$(MAKECMDGOALS),$(call owner_quote,$(goal)))
$(MAKECMDGOALS): owned-build
	@:
else
include make/target.mk
include make/package-paths.mk
include make/shared-roots.mk
include make/flags.mk
include make/sources-app.mk
include make/sources-shared.mk
include make/sources-tools.mk
include make/objects.mk
include make/rules-build.mk
include make/rules-memory-check.mk
include make/rules-runtime.mk
include make/rules-tools.mk
include make/cfd-reference-support.mk
include make/rules-shims.mk
include make/rules-tests-contracts.mk
include make/rules-test-file-picker.mk
include make/rules-test-path-opener.mk
include make/rules-test-groups.mk
include make/rules-vulkan-runtime.mk
include make/package-macos.mk
include make/package-linux-worker.mk
include make/package-linux-desktop.mk
include make/release.mk
include make/build-identity.mk

# =========================
#  Diagnostics
# =========================
$(info USING MAKEFILE AT: $(abspath $(firstword $(MAKEFILE_LIST))))


# =========================
#  Auto-generated deps
# =========================
-include $(DEPS)

endif

endif
