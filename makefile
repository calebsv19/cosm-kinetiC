# Cleanup inspection must not load compiler or package configuration.
CONTROL_GOALS := help test-linux-desktop-installer test-macos-bundle-lifecycle clean clean-plan test-top-level-cleanup test-package-launcher-lifecycle
ifneq ($(filter clean,$(MAKECMDGOALS)),)
ifneq ($(filter-out $(CONTROL_GOALS),$(MAKECMDGOALS)),)
$(error Run clean and build as separate operations)
endif
endif
CONTROL_ONLY :=
ifneq ($(strip $(MAKECMDGOALS)),)
ifeq ($(strip $(filter-out $(CONTROL_GOALS),$(MAKECMDGOALS))),)
CONTROL_ONLY := 1
endif
endif
ifeq ($(CONTROL_ONLY),1)
include make/config.mk
include make/sources-tools.mk
include make/rules-runtime.mk
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
