# =========================
#  CLI tool source declarations
# =========================
SHAPE_MASK_TOOL_SRC   := $(SRC_DIR)/tools/cli/shape_import_tool.c
SHAPE_ASSET_TOOL_SRC  := $(SRC_DIR)/tools/cli/shape_asset_tool.c
SHAPE_SANITY_TOOL_SRC := $(SRC_DIR)/tools/cli/shape_sanity_tool.c
VF2D_PACK_TOOL_SRC := $(SRC_DIR)/tools/cli/vf2d_pack_tool.c
VF2D_PACK_TOOL_BIN := vf2d_pack_tool
VF2D_DATASET_TOOL_SRC := $(SRC_DIR)/tools/cli/vf2d_dataset_tool.c
VF2D_DATASET_TOOL_BIN := vf2d_dataset_tool
PHYSICS_TRACE_TOOL_SRC := $(SRC_DIR)/tools/cli/physics_trace_tool.c
PHYSICS_TRACE_TOOL_BIN := physics_trace_tool
PHYSICS_SIM_HEADLESS_TOOL_SRC := $(SRC_DIR)/tools/cli/physics_sim_headless.c
PHYSICS_SIM_HEADLESS_TOOL_BIN := physics_sim_headless
PHYSICS_SIM_JOB_RUNNER_TOOL_SRC := $(SRC_DIR)/tools/cli/physics_sim_job_runner.c
PHYSICS_SIM_JOB_RUNNER_TOOL_BIN := physics_sim_job_runner
RUNTIME_SCENE_EMITTER_DIAG_TOOL_BIN := runtime_scene_emitter_diag_tool
RUNTIME_SCENE_EMITTER_DIAG_TOOL_SRCS = \
	$(SRC_DIR)/tools/cli/runtime_scene_emitter_diag_tool.c \
	$(SRC_DIR)/app/atmospheric/atmospheric_field.c \
	$(SRC_DIR)/import/runtime_scene_bridge.c \
	$(SRC_DIR)/import/runtime_mesh_preview_bridge.c \
	$(SRC_DIR)/import/runtime_scene_solver_projection.c \
	$(SRC_DIR)/import/runtime_scene_solver_projection_domain.c \
	$(SRC_DIR)/import/runtime_scene_solver_projection_objects.c \
	$(SRC_DIR)/import/runtime_scene_solver_projection_emitters.c \
	$(SRC_DIR)/render/retained_runtime_scene_overlay_geom.c \
	$(SRC_DIR)/app/sim_runtime_backend.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_scaffold.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_wind.c \
	$(SRC_DIR)/app/wind_tunnel_3d.c \
	$(SRC_DIR)/app/wind_tunnel_3d_analysis.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_scaffold_views.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_runtime.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_mesh_obstacle_proxy.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_obstacles.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_obstacle_sources.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_oriented_box.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_emitter_shapes.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_emitters.c \
	$(SRC_DIR)/app/app_config.c \
	$(SRC_DIR)/app/data_paths.c \
	$(SRC_DIR)/app/scene_presets.c \
	$(SRC_DIR)/app/sim_runtime_3d_domain.c \
	$(SRC_DIR)/app/sim_runtime_3d_brick_store.c \
	$(SRC_DIR)/app/sim_runtime_3d_footprint.c \
	$(SRC_DIR)/app/sim_runtime_3d_anchor.c \
	$(SRC_DIR)/app/sim_runtime_3d_space.c \
	$(SRC_DIR)/app/sim_runtime_3d_solver.c \
	$(SRC_DIR)/app/sim_runtime_3d_solver_step.c \
	$(SRC_DIR)/app/sim_runtime_3d_solver_core_sim.c \
	$(SRC_DIR)/app/sim_runtime_mesh_diagnostics.c \
	$(SRC_DIR)/app/sim_runtime_mesh_accel.c \
	$(SRC_DIR)/app/sim_runtime_mesh_obstacle_proxy.c \
	$(SRC_DIR)/app/sim_runtime_emitter.c \
	$(SRC_DIR)/app/sim_runtime_obstacle.c \
	$(SRC_DIR)/app/scene_objects.c \
	$(SRC_DIR)/app/editor/scene_editor_session.c \
	$(SRC_DIR)/app/editor/scene_editor_session_labels.c \
	$(SRC_DIR)/physics/objects/object_manager.c \
	$(SRC_DIR)/physics/rigid/rigid2d.c \
	$(SRC_DIR)/physics/rigid/rigid2d_collision.c \
	$(CORE_SCENE_DIR)/src/core_scene.c \
	$(CORE_SCENE_COMPILE_DIR)/src/core_scene_compile.c \
	$(CORE_SIM_DIR)/src/core_sim.c \
	$(CORE_MESH_PREVIEW_DIR)/src/core_mesh_preview.c \
	$(CORE_MESH_ASSET_DIR)/src/core_mesh_asset.c \
	$(CORE_MESH_ASSET_DIR)/src/core_mesh_asset_runtime_document.c \
	$(CORE_OBJECT_DIR)/src/core_object.c \
	$(CORE_UNITS_DIR)/src/core_units.c \
	$(CORE_IO_DIR)/src/core_io.c \
	$(CORE_BASE_DIR)/src/core_base.c \
	$(CORE_MESH_PREVIEW_DIR)/../../shape/external/cjson/cJSON.c

CORE_PACK_TOOL_SRCS := \
	$(VF2D_PACK_TOOL_SRC) \
	$(CORE_PACK_DIR)/src/core_pack.c \
	$(CORE_PACK_DIR)/src/core_pack_vf2d.c \
	$(CORE_IO_DIR)/src/core_io.c \
	$(CORE_BASE_DIR)/src/core_base.c \
	$(CORE_OBJECT_DIR)/src/core_object.c \
	$(CORE_UNITS_DIR)/src/core_units.c \
	$(CORE_SCENE_DIR)/src/core_scene.c
CORE_PACK_TOOL_INCS := -I$(CORE_PACK_DIR)/include -I$(CORE_IO_DIR)/include -I$(CORE_BASE_DIR)/include -I$(CORE_SCENE_DIR)/include -I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include
VF2D_DATASET_TOOL_SRCS := \
	$(VF2D_DATASET_TOOL_SRC) \
	$(SRC_DIR)/export/volume_frames.c \
	$(SRC_DIR)/export/export_paths.c \
	$(CORE_DATA_DIR)/src/core_data.c \
	$(CORE_PACK_DIR)/src/core_pack.c \
	$(CORE_PACK_DIR)/src/core_pack_vf2d.c \
	$(CORE_IO_DIR)/src/core_io.c \
	$(CORE_BASE_DIR)/src/core_base.c \
	$(CORE_OBJECT_DIR)/src/core_object.c \
	$(CORE_UNITS_DIR)/src/core_units.c \
	$(CORE_SCENE_DIR)/src/core_scene.c \
	$(TIMER_HUD_DIR)/external/cJSON.c
VF2D_DATASET_TOOL_INCS := -I$(INC_DIR) -I$(SRC_DIR) -I$(SRC_DIR)/tools -I$(CORE_DATA_DIR)/include -I$(CORE_PACK_DIR)/include -I$(CORE_IO_DIR)/include -I$(CORE_BASE_DIR)/include -I$(CORE_SCENE_DIR)/include -I$(CORE_MESH_ASSET_DIR)/include -I$(CORE_MESH_PREVIEW_DIR)/include -I$(CORE_MESH_PREVIEW_DIR)/../../shape/external -I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include -I$(CORE_SIM_DIR)/include -I$(TIMER_HUD_DIR)/external -DVOLUME_FRAMES_DATASET_TOOL_ONLY=1 $(SDL_CFLAGS)
ifeq ($(UNAME_S),Darwin)
VF2D_DATASET_TOOL_INCS += -I/opt/homebrew/include -D_THREAD_SAFE
endif
PHYSICS_TRACE_TOOL_SRCS := \
	$(PHYSICS_TRACE_TOOL_SRC) \
	$(CORE_TRACE_DIR)/src/core_trace.c \
	$(CORE_PACK_DIR)/src/core_pack.c \
	$(CORE_IO_DIR)/src/core_io.c \
	$(CORE_BASE_DIR)/src/core_base.c \
	$(CORE_OBJECT_DIR)/src/core_object.c \
	$(CORE_UNITS_DIR)/src/core_units.c \
	$(CORE_SCENE_DIR)/src/core_scene.c \
	$(TIMER_HUD_DIR)/external/cJSON.c
PHYSICS_TRACE_TOOL_INCS := -I$(CORE_TRACE_DIR)/include -I$(CORE_PACK_DIR)/include -I$(CORE_IO_DIR)/include -I$(CORE_BASE_DIR)/include -I$(CORE_SCENE_DIR)/include -I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include -I$(TIMER_HUD_DIR)/external

SHAPE_SHARED_SRCS := \
	$(SRC_DIR)/tools/ShapeLib/shape_core.c \
	$(SRC_DIR)/tools/ShapeLib/shape_flatten.c \
	$(SRC_DIR)/tools/ShapeLib/shape_json.c \
	$(SRC_DIR)/import/shape_import.c \
	$(SRC_DIR)/geo/shape_asset.c \
	$(TIMER_HUD_DIR)/external/cJSON.c
