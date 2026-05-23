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
include make/rules-runtime.mk
include make/rules-tools.mk
include make/rules-shims.mk
include make/rules-tests-contracts.mk
include make/rules-test-groups.mk
include make/package-macos.mk
include make/package-linux-worker.mk
include make/release.mk

# =========================
#  Diagnostics
# =========================
$(info USING MAKEFILE AT: $(abspath $(firstword $(MAKEFILE_LIST))))


# =========================
#  Auto-generated deps
# =========================
-include $(DEPS)
