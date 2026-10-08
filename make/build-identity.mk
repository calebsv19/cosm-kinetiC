# Configuration changes are prerequisites, not a reason to ask operators to clean.
BUILD_IDENTITY_KEYS := RUNTIME_SCENE_EMITTER_DIAG_TOOL_CFLAGS VF2D_PACK_TOOL_CFLAGS VF2D_DATASET_TOOL_CFLAGS PHYSICS_TRACE_TOOL_CFLAGS CC CLANG CFLAGS LDFLAGS LIBS CSTD TARGET_ARCH FISICS FISICS_FLAGS FISICS_CFLAGS FISICS_COMPILE_FLAGS FISICS_MEMCHECK_LINK_LIBS CFD_BUILD_OPT HEADLESS_WORKER_LIBS TARGET CLANG_TARGET FISICS_TARGET SESSION_WORKER_BIN PHYSICS_SIM_HEADLESS_TOOL_BIN PHYSICS_SIM_JOB_RUNNER_TOOL_BIN VF2D_PACK_TOOL_BIN VF2D_DATASET_TOOL_BIN PHYSICS_TRACE_TOOL_BIN RUNTIME_SCENE_EMITTER_DIAG_TOOL_BIN SHAPE_SANITY_TOOL_BIN SHAPE_MASK_TOOL_BIN SHAPE_ASSET_TOOL_BIN PASSIVE3D_WORKER ATMOSPHERE3D_WORKER OPEN_ATMOSPHERE3D_WORKER JSON_CFLAGS JSON_LIBS
$(foreach key,$(BUILD_IDENTITY_KEYS),$(eval export PHYSICS_BUILD_$(key) := $($(key))))
# Safe literal shell arguments, including embedded single quotes in flags.
build_identity_quote = '$(subst ','"'"',$(1))'
BUILD_IDENTITY_ENV = $(foreach key,$(BUILD_IDENTITY_KEYS),PHYSICS_BUILD_$(key)=$(call build_identity_quote,$($(key))))
# Object rebuilds must invalidate links even on whole-second Make timestamps.
BUILD_OBJECT_LINKS := $(sort $(CLANG_TARGET) $(FISICS_TARGET) $(SHAPE_SANITY_TOOL_BIN) $(SHAPE_MASK_TOOL_BIN) $(SHAPE_ASSET_TOOL_BIN) $(PHYSICS_SIM_HEADLESS_TOOL_BIN) $(PHYSICS_SIM_JOB_RUNNER_TOOL_BIN) $(SESSION_WORKER_BIN) $(VF2D_PACK_TOOL_BIN) $(VF2D_DATASET_TOOL_BIN) $(PHYSICS_TRACE_TOOL_BIN) $(RUNTIME_SCENE_EMITTER_DIAG_TOOL_BIN))
# Darwin object-only links emit actual linker selection manifests.
# Direct source-to-binary commands need a separate source/header closure contract.
ifeq ($(UNAME_S),Darwin)
BUILD_LINK_OUTPUTS := $(BUILD_OBJECT_LINKS)
LINK_PROVENANCE_FLAG := --link-provenance
endif
BUILD_IDENTITY_LINKS := $(foreach file,$(BUILD_LINK_OUTPUTS),--link-output $(call build_identity_quote,$(file)))
BUILD_IDENTITY_DEPENDENCIES := $(foreach file,$(DEPS),--dependency $(call build_identity_quote,$(file)))
BUILD_IDENTITY_FILES := $(foreach file,$(MAKEFILE_LIST),--makefile $(call build_identity_quote,$(file)))
# Digest-named prerequisites work even with Make versions using whole-second mtimes.
# Clean-only commands must not initialize a compiler environment or build output.
ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),clean-plan)
BUILD_CONFIGURATION_ADMISSION := $(shell env $(BUILD_IDENTITY_ENV) python3 scripts/build_identity.py --digest-only --selection-root "$(BUILD_DIR)" $(BUILD_IDENTITY_FILES) $(BUILD_IDENTITY_DEPENDENCIES) $(BUILD_IDENTITY_LINKS))
BUILD_CONFIGURATION_DIGEST := $(firstword $(BUILD_CONFIGURATION_ADMISSION))
BUILD_DEPENDENCY_REBUILDS := $(wordlist 2,$(words $(BUILD_CONFIGURATION_ADMISSION)),$(BUILD_CONFIGURATION_ADMISSION))
ifneq ($(strip $(BUILD_DEPENDENCY_REBUILDS)),)
.PHONY: build-dependency-content-changed
build-dependency-content-changed:
$(BUILD_DEPENDENCY_REBUILDS): build-dependency-content-changed
build_link_content_rebuild = $(if $(filter $(2),$(BUILD_DEPENDENCY_REBUILDS)),$(eval $(1): build-dependency-content-changed))
$(call build_link_content_rebuild,$(CLANG_TARGET),$(OBJS))
$(call build_link_content_rebuild,$(FISICS_TARGET),$(FISICS_OBJS))
$(call build_link_content_rebuild,$(SHAPE_SANITY_TOOL_BIN),$(SHAPE_SANITY_TOOL_OBJ))
$(call build_link_content_rebuild,$(SHAPE_MASK_TOOL_BIN),$(SHAPE_MASK_TOOL_OBJ) $(SHAPE_SHARED_OBJS) $(SHAPE_OUTPUT_OBJS) $(SHAPE_INPUT_OBJS))
$(call build_link_content_rebuild,$(SHAPE_ASSET_TOOL_BIN),$(SHAPE_ASSET_TOOL_OBJ) $(SHAPE_SHARED_OBJS) $(SHAPE_OUTPUT_OBJS) $(SHAPE_INPUT_OBJS))
$(call build_link_content_rebuild,$(PHYSICS_SIM_HEADLESS_TOOL_BIN),$(PHYSICS_SIM_HEADLESS_TOOL_OBJ) $(PHYSICS_SIM_HEADLESS_WORKER_OBJS))
$(call build_link_content_rebuild,$(PHYSICS_SIM_JOB_RUNNER_TOOL_BIN),$(PHYSICS_SIM_JOB_RUNNER_TOOL_OBJ) $(PHYSICS_SIM_HEADLESS_WORKER_OBJS))
$(call build_link_content_rebuild,$(SESSION_WORKER_BIN),$(BUILD_DIR)/tools/cli/physics_sim_session_worker.o $(PHYSICS_SIM_HEADLESS_WORKER_OBJS))
$(call build_link_content_rebuild,$(VF2D_PACK_TOOL_BIN),$(VF2D_PACK_TOOL_OBJS))
$(call build_link_content_rebuild,$(VF2D_DATASET_TOOL_BIN),$(VF2D_DATASET_TOOL_OBJS))
$(call build_link_content_rebuild,$(PHYSICS_TRACE_TOOL_BIN),$(PHYSICS_TRACE_TOOL_OBJS))
$(call build_link_content_rebuild,$(RUNTIME_SCENE_EMITTER_DIAG_TOOL_BIN),$(RUNTIME_SCENE_EMITTER_DIAG_TOOL_OBJS))
endif
ifeq ($(strip $(BUILD_CONFIGURATION_DIGEST)),)
$(error Cannot establish build configuration identity)
endif
BUILD_CONFIGURATION_STAMP := $(BUILD_DIR)/.configuration/$(BUILD_CONFIGURATION_DIGEST).json
$(BUILD_CONFIGURATION_STAMP): scripts/build_identity.py
	python3 scripts/build_identity.py --output "$@" --expected-digest "$(BUILD_CONFIGURATION_DIGEST)" --selection-root "$(BUILD_DIR)" $(BUILD_IDENTITY_FILES) $(BUILD_IDENTITY_DEPENDENCIES) $(BUILD_IDENTITY_LINKS)

BUILD_CONFIGURATION_OBJECTS := $(sort $(CLANG_DEPENDENCY_OBJECTS) $(OBJS) $(FISICS_OBJS))
ifneq ($(strip $(BUILD_CONFIGURATION_OBJECTS)),)
$(BUILD_CONFIGURATION_OBJECTS): $(BUILD_CONFIGURATION_STAMP)
ifeq ($(wildcard $(BUILD_CONFIGURATION_STAMP)),)
.PHONY: build-configuration-changed
build-configuration-changed:
$(BUILD_OBJECT_LINKS): build-configuration-changed
$(BUILD_CONFIGURATION_OBJECTS): build-configuration-changed
endif
endif
$(CLANG_TARGET) $(FISICS_TARGET) $(PHYSICS_SIM_HEADLESS_TOOL_BIN) $(PHYSICS_SIM_JOB_RUNNER_TOOL_BIN) $(SESSION_WORKER_BIN) $(VF2D_PACK_TOOL_BIN) $(VF2D_DATASET_TOOL_BIN) $(PHYSICS_TRACE_TOOL_BIN) $(RUNTIME_SCENE_EMITTER_DIAG_TOOL_BIN): $(BUILD_CONFIGURATION_STAMP)
$(CFD_REFERENCE_SUPPORT_LIBS): $(BUILD_CONFIGURATION_STAMP)
ifeq ($(wildcard $(BUILD_CONFIGURATION_STAMP)),)
$(CFD_REFERENCE_SUPPORT_LIBS): build-configuration-changed
endif
BUILD_CONFIGURATION_WORKERS := $(PASSIVE3D_WORKER) $(ATMOSPHERE3D_WORKER) $(OPEN_ATMOSPHERE3D_WORKER)
ifneq ($(strip $(BUILD_CONFIGURATION_WORKERS)),)
$(BUILD_CONFIGURATION_WORKERS): $(BUILD_CONFIGURATION_STAMP)
ifeq ($(wildcard $(BUILD_CONFIGURATION_STAMP)),)
$(BUILD_CONFIGURATION_WORKERS): build-configuration-changed
endif
endif
.DELETE_ON_ERROR:
endif
endif
