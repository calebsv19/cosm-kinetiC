#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PACKAGE_MK="${ROOT_DIR}/make/package-macos.mk"
PATHS_MK="${ROOT_DIR}/make/package-paths.mk"
LAUNCHER="${ROOT_DIR}/tools/packaging/macos/physics-sim-launcher"

fail() {
    echo "Main Edit package contract check failed: $1" >&2
    exit 1
}

check_contains() {
    local pattern="$1"
    local file="$2"
    rg --fixed-strings --quiet "${pattern}" "${file}" || fail "missing '${pattern}' in ${file}"
}

check_contains "package-desktop-main-edit" "${PACKAGE_MK}"
check_contains "package-desktop-main-edit-self-test" "${PACKAGE_MK}"
check_contains "package-desktop-main-edit-refresh" "${PACKAGE_MK}"
check_contains "MAIN_EDIT_APP_NAME := kinetiC Main Edit.app" "${PATHS_MK}"
check_contains "MAIN_EDIT_BUNDLE_ID := com.cosm.kinetic.main-edit" "${PATHS_MK}"
check_contains "MAIN_EDIT_RUNTIME_NAMESPACE := PhysicsSim-Main-Edit" "${PATHS_MK}"
check_contains "MAIN_EDIT_LOG_NAMESPACE := PhysicsSim-Main-Edit" "${PATHS_MK}"
check_contains "write-identity" "${PACKAGE_MK}"
check_contains "verify-identity" "${PACKAGE_MK}"
check_contains "Source changed during Main Edit packaging" "${PACKAGE_MK}"
check_contains "Refusing canonical Desktop destination" "${PACKAGE_MK}"
check_contains "process-audit" "${PACKAGE_MK}"
check_contains "PACKAGE_PROFILE=" "${LAUNCHER}"
check_contains "RUNTIME_NAMESPACE=" "${LAUNCHER}"
check_contains "LOG_NAMESPACE=" "${LAUNCHER}"
check_contains "BUILD_LABEL=" "${LAUNCHER}"

echo "Main Edit package contract checks passed"
