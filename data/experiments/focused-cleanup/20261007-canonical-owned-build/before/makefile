# Cleanup inspection must not load compiler or package configuration.
CONTROL_GOALS := clean clean-plan test-top-level-cleanup
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

# =========================
#  Diagnostics
# =========================
$(info USING MAKEFILE AT: $(abspath $(firstword $(MAKEFILE_LIST))))


# =========================
#  Auto-generated deps
# =========================
-include $(DEPS)

endif
