# Collider test harness (headless)
COLLIDER_TEST_SRC := tests/collider_test.c \
	$(SRC_DIR)/app/app_config.c \
	$(SRC_DIR)/app/shape_lookup.c \
	$(SRC_DIR)/render/import_project.c \
	$(SRC_DIR)/geo/shape_library.c \
	$(SHAPE_SHARED_SRCS) \
	$(SRC_DIR)/physics/rigid/collider_builder.c \
	$(SRC_DIR)/physics/rigid/collider_geom.c \
	$(SRC_DIR)/physics/rigid/collider_legacy.c \
	$(SRC_DIR)/physics/rigid/collider_tagging.c

collider-test: $(COLLIDER_TEST_SRC)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -Isrc -Iinclude -o $(BUILD_DIR)/collider_test $(COLLIDER_TEST_SRC) $(filter-out -lSDL2 -lSDL2_ttf,$(LIBS))

collider-tests: collider-test
	$(BUILD_DIR)/collider_test

MESH_PREVIEW_BRIDGE_INCS := -I$(CORE_MESH_ASSET_DIR)/include -I$(CORE_MESH_PREVIEW_DIR)/include -I$(CORE_MESH_PREVIEW_DIR)/../../shape/external

KIT_VIZ_FIELD_TEST_SRCS := \
	tests/kit_viz_field_adapter_test.c \
	$(SRC_DIR)/render/kit_viz_field_adapter.c \
	$(KIT_VIZ_DIR)/src/kit_viz.c \
	$(CORE_BASE_DIR)/src/core_base.c

test-kitviz-field-adapter: $(KIT_VIZ_FIELD_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CSTD) $(WARN) $(DEBUG) \
		-I$(INC_DIR) -I$(SRC_DIR) \
		-I$(KIT_VIZ_DIR)/include -I$(CORE_BASE_DIR)/include \
		-o $(BUILD_DIR)/kit_viz_field_adapter_test $(KIT_VIZ_FIELD_TEST_SRCS) -lm
	$(BUILD_DIR)/kit_viz_field_adapter_test

test-atmospheric-field-contract: $(ATMOSPHERIC_FIELD_CONTRACT_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CSTD) $(WARN) $(DEBUG) \
		-I$(INC_DIR) -I$(SRC_DIR) \
		-o $(BUILD_DIR)/atmospheric_field_contract_test $(ATMOSPHERIC_FIELD_CONTRACT_TEST_SRCS) -lm
	$(BUILD_DIR)/atmospheric_field_contract_test

test-soft-body-contract: $(SOFT_BODY_CONTRACT_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CSTD) $(WARN) $(DEBUG) \
		-I$(INC_DIR) -I$(SRC_DIR) \
		-o $(BUILD_DIR)/soft_body_contract_test $(SOFT_BODY_CONTRACT_TEST_SRCS) -lm
	$(BUILD_DIR)/soft_body_contract_test

test-sim-runtime-backend-2d-runtime-fields-contract: $(SIM_RUNTIME_BACKEND_2D_RUNTIME_FIELDS_CONTRACT_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) \
		-o $(BUILD_DIR)/sim_runtime_backend_2d_runtime_fields_contract_test $(SIM_RUNTIME_BACKEND_2D_RUNTIME_FIELDS_CONTRACT_TEST_SRCS) -lm
	$(BUILD_DIR)/sim_runtime_backend_2d_runtime_fields_contract_test

test-sim-runtime-backend-2d-contract: $(SIM_RUNTIME_BACKEND_2D_CONTRACT_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) \
		-o $(BUILD_DIR)/sim_runtime_backend_2d_contract_test $(SIM_RUNTIME_BACKEND_2D_CONTRACT_TEST_SRCS) -lm
	$(BUILD_DIR)/sim_runtime_backend_2d_contract_test

test-atmospheric-warm-start-contract: $(ATMOSPHERIC_WARM_START_CONTRACT_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) \
		-I$(CORE_BASE_DIR)/include -I$(CORE_IO_DIR)/include -I$(CORE_DATA_DIR)/include \
		-I$(CORE_PACK_DIR)/include -I$(CORE_SCENE_DIR)/include -I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include \
		-o $(BUILD_DIR)/atmospheric_warm_start_contract_test $(ATMOSPHERIC_WARM_START_CONTRACT_TEST_SRCS) $(filter-out -lSDL2 -lSDL2_ttf,$(LIBS))
	$(BUILD_DIR)/atmospheric_warm_start_contract_test

PRESET_IO_DIMENSIONAL_TEST_SRCS := \
	tests/preset_io_dimensional_contract_test.c \
	$(SRC_DIR)/app/preset_io.c \
	$(SRC_DIR)/app/preset_io_library.c \
	$(SRC_DIR)/app/preset_io_save.c \
	$(SRC_DIR)/app/scene_presets.c \
	$(SRC_DIR)/app/atmospheric/atmospheric_field.c

ATMOSPHERIC_FIELD_CONTRACT_TEST_SRCS := \
	tests/atmospheric_field_contract_test.c \
	$(SRC_DIR)/app/atmospheric/atmospheric_field.c

SOFT_BODY_CONTRACT_TEST_SRCS := \
	tests/soft_body_contract_test.c \
	$(SRC_DIR)/physics/soft/soft_body.c

SIM_RUNTIME_BACKEND_2D_RUNTIME_FIELDS_CONTRACT_TEST_SRCS := \
	tests/sim_runtime_backend_2d_runtime_fields_contract_test.c \
	$(SRC_DIR)/app/sim_runtime_backend_2d_runtime_fields.c \
	$(SRC_DIR)/physics/fluid2d/fluid2d.c \
	$(SRC_DIR)/physics/fluid2d/fluid2d_boundary.c

SIM_RUNTIME_BACKEND_2D_CONTRACT_TEST_SRCS := \
	tests/sim_runtime_backend_2d_contract_test.c \
	$(SRC_DIR)/app/sim_runtime_backend.c \
	$(SRC_DIR)/app/sim_runtime_backend_2d.c \
	$(SRC_DIR)/app/sim_runtime_backend_2d_runtime_fields.c \
	$(SRC_DIR)/app/atmospheric/atmospheric_field.c \
	$(SRC_DIR)/physics/fluid2d/fluid2d.c \
	$(SRC_DIR)/physics/fluid2d/fluid2d_boundary.c

ATMOSPHERIC_WARM_START_CONTRACT_TEST_SRCS := \
	tests/atmospheric_warm_start_contract_test.c \
	$(SRC_DIR)/app/atmospheric/atmospheric_field.c \
	$(SRC_DIR)/app/atmospheric/atmospheric_warm_start.c \
	$(SRC_DIR)/export/volume_frames.c \
	$(SRC_DIR)/export/volume_frames_vf3d.c \
	$(SRC_DIR)/export/water_surface_artifacts.c \
	$(SRC_DIR)/app/water_object_coupling.c \
	$(SRC_DIR)/export/export_paths.c \
	$(SRC_DIR)/app/sim_runtime_backend.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_scaffold.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_wind.c \
	$(SRC_DIR)/app/wind_tunnel_3d_analysis.c \
	$(SRC_DIR)/app/wind_tunnel_3d_inspector.c \
	$(SRC_DIR)/app/wind_tunnel_3d.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_scaffold_views.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_runtime.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_obstacles.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_obstacle_sources.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_oriented_box.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_emitter_shapes.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_emitters.c \
	$(SRC_DIR)/app/sim_runtime_3d_domain.c \
	$(SRC_DIR)/app/sim_runtime_3d_brick_store.c \
	$(SRC_DIR)/app/sim_runtime_3d_footprint.c \
	$(SRC_DIR)/app/sim_runtime_3d_anchor.c \
	$(SRC_DIR)/app/sim_runtime_3d_space.c \
	$(SRC_DIR)/app/sim_runtime_3d_solver.c \
	$(SRC_DIR)/app/sim_runtime_3d_solver_step.c \
	$(SRC_DIR)/app/sim_runtime_3d_solver_core_sim.c \
	$(SRC_DIR)/app/sim_runtime_emitter.c \
	$(SRC_DIR)/app/sim_runtime_obstacle.c \
	$(CORE_SIM_DIR)/src/core_sim.c \
	$(CORE_DATA_DIR)/src/core_data.c \
	$(CORE_PACK_DIR)/src/core_pack.c \
	$(CORE_PACK_DIR)/src/core_pack_vf2d.c \
	$(CORE_PACK_DIR)/src/core_pack_vf3d.c \
	$(CORE_IO_DIR)/src/core_io.c \
	$(CORE_BASE_DIR)/src/core_base.c \
	$(CORE_OBJECT_DIR)/src/core_object.c \
	$(CORE_UNITS_DIR)/src/core_units.c \
	$(CORE_SCENE_DIR)/src/core_scene.c \
	$(TIMER_HUD_DIR)/external/cJSON.c

SIM_MODE_ROUTE_CONTRACT_TEST_SRCS := \
	tests/sim_mode_route_contract_test.c \
	$(SRC_DIR)/app/sim_modes/sim_mode_dispatch.c \
	$(SRC_DIR)/app/wind_tunnel_3d.c

WIND_TUNNEL_3D_CONTRACT_TEST_SRCS := \
	tests/wind_tunnel_3d_contract_test.c \
	$(SRC_DIR)/app/wind_tunnel_3d.c

WATER_MODE_CONTRACT_TEST_SRCS := \
	tests/water_mode_contract_test.c \
	$(SRC_DIR)/app/water_mode.c

WATER_SURFACE_ARTIFACTS_CONTRACT_TEST_SRCS := \
	tests/water_surface_artifacts_contract_test.c \
	$(SRC_DIR)/app/atmospheric/atmospheric_field.c \
	$(SRC_DIR)/export/volume_frames.c \
	$(SRC_DIR)/export/volume_frames_vf3d.c \
	$(SRC_DIR)/export/water_surface_artifacts.c \
	$(SRC_DIR)/app/water_object_coupling.c \
	$(SRC_DIR)/export/export_paths.c \
	$(SRC_DIR)/app/sim_runtime_backend.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_scaffold.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_wind.c \
	$(SRC_DIR)/app/wind_tunnel_3d_analysis.c \
	$(SRC_DIR)/app/wind_tunnel_3d.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_scaffold_views.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_runtime.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_obstacles.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_obstacle_sources.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_oriented_box.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_emitter_shapes.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_emitters.c \
	$(SRC_DIR)/app/sim_runtime_3d_domain.c \
	$(SRC_DIR)/app/sim_runtime_3d_brick_store.c \
	$(SRC_DIR)/app/sim_runtime_3d_footprint.c \
	$(SRC_DIR)/app/sim_runtime_3d_anchor.c \
	$(SRC_DIR)/app/sim_runtime_3d_space.c \
	$(SRC_DIR)/app/sim_runtime_3d_solver.c \
	$(SRC_DIR)/app/sim_runtime_3d_solver_step.c \
	$(SRC_DIR)/app/sim_runtime_3d_solver_core_sim.c \
	$(SRC_DIR)/app/sim_runtime_emitter.c \
	$(SRC_DIR)/app/sim_runtime_obstacle.c \
	$(CORE_SIM_DIR)/src/core_sim.c \
	$(CORE_DATA_DIR)/src/core_data.c \
	$(CORE_PACK_DIR)/src/core_pack.c \
	$(CORE_PACK_DIR)/src/core_pack_vf2d.c \
	$(CORE_PACK_DIR)/src/core_pack_vf3d.c \
	$(CORE_IO_DIR)/src/core_io.c \
	$(CORE_BASE_DIR)/src/core_base.c \
	$(CORE_OBJECT_DIR)/src/core_object.c \
	$(CORE_UNITS_DIR)/src/core_units.c \
	$(CORE_SCENE_DIR)/src/core_scene.c \
	$(TIMER_HUD_DIR)/external/cJSON.c

SIM_RUNTIME_EMITTER_CONTRACT_TEST_SRCS := \
	tests/sim_runtime_emitter_contract_test.c \
	$(SRC_DIR)/app/sim_runtime_emitter.c \
	$(SRC_DIR)/app/sim_runtime_3d_space.c

SIM_RUNTIME_OBSTACLE_CONTRACT_TEST_SRCS := \
	tests/sim_runtime_obstacle_contract_test.c \
	$(SRC_DIR)/app/sim_runtime_obstacle.c

SIM_RUNTIME_BACKEND_3D_EMITTER_TEST_SRCS := \
	tests/sim_runtime_backend_3d_emitter_contract_test.c \
	$(SRC_DIR)/app/atmospheric/atmospheric_field.c \
	$(SRC_DIR)/app/sim_runtime_backend.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_scaffold.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_wind.c \
	$(SRC_DIR)/app/wind_tunnel_3d_analysis.c \
	$(SRC_DIR)/app/wind_tunnel_3d_inspector.c \
	$(SRC_DIR)/app/wind_tunnel_3d.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_scaffold_views.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_runtime.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_obstacles.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_obstacle_sources.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_oriented_box.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_emitter_shapes.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_emitters.c \
		$(SRC_DIR)/app/sim_runtime_3d_domain.c \
		$(SRC_DIR)/app/sim_runtime_3d_brick_store.c \
		$(SRC_DIR)/app/sim_runtime_3d_footprint.c \
		$(SRC_DIR)/app/sim_runtime_3d_anchor.c \
	$(SRC_DIR)/app/sim_runtime_3d_space.c \
	$(SRC_DIR)/app/sim_runtime_3d_solver.c \
	$(SRC_DIR)/app/sim_runtime_3d_solver_step.c \
	$(SRC_DIR)/app/sim_runtime_3d_solver_core_sim.c \
	$(SRC_DIR)/app/sim_runtime_emitter.c \
	$(SRC_DIR)/app/sim_runtime_obstacle.c \
	$(CORE_SIM_DIR)/src/core_sim.c

SIM_RUNTIME_BACKEND_3D_ATTACHED_EMITTER_TEST_SRCS := \
	tests/sim_runtime_backend_3d_attached_emitter_contract_test.c \
	$(SRC_DIR)/app/atmospheric/atmospheric_field.c \
	$(SRC_DIR)/app/sim_runtime_backend.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_scaffold.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_wind.c \
	$(SRC_DIR)/app/wind_tunnel_3d_analysis.c \
	$(SRC_DIR)/app/wind_tunnel_3d_inspector.c \
	$(SRC_DIR)/app/wind_tunnel_3d.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_scaffold_views.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_runtime.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_obstacles.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_obstacle_sources.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_oriented_box.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_emitter_shapes.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_emitters.c \
		$(SRC_DIR)/app/sim_runtime_3d_domain.c \
		$(SRC_DIR)/app/sim_runtime_3d_brick_store.c \
		$(SRC_DIR)/app/sim_runtime_3d_footprint.c \
		$(SRC_DIR)/app/sim_runtime_3d_anchor.c \
	$(SRC_DIR)/app/sim_runtime_3d_space.c \
	$(SRC_DIR)/app/sim_runtime_3d_solver.c \
	$(SRC_DIR)/app/sim_runtime_3d_solver_step.c \
	$(SRC_DIR)/app/sim_runtime_3d_solver_core_sim.c \
	$(SRC_DIR)/app/sim_runtime_emitter.c \
	$(SRC_DIR)/app/sim_runtime_obstacle.c \
	$(CORE_SIM_DIR)/src/core_sim.c

SIM_RUNTIME_BACKEND_3D_RUNTIME_MESH_EMITTER_TEST_SRCS := \
	tests/sim_runtime_backend_3d_runtime_mesh_emitter_contract_test.c \
	$(SRC_DIR)/app/atmospheric/atmospheric_field.c \
	$(SRC_DIR)/app/sim_runtime_backend.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_scaffold.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_wind.c \
	$(SRC_DIR)/app/wind_tunnel_3d_analysis.c \
	$(SRC_DIR)/app/wind_tunnel_3d_inspector.c \
	$(SRC_DIR)/app/wind_tunnel_3d.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_scaffold_views.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_runtime.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_obstacles.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_obstacle_sources.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_oriented_box.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_emitter_shapes.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_emitters.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_mesh_obstacle_proxy.c \
	$(SRC_DIR)/app/sim_runtime_3d_domain.c \
	$(SRC_DIR)/app/sim_runtime_3d_brick_store.c \
	$(SRC_DIR)/app/sim_runtime_3d_footprint.c \
	$(SRC_DIR)/app/sim_runtime_3d_anchor.c \
	$(SRC_DIR)/app/sim_runtime_3d_space.c \
	$(SRC_DIR)/app/sim_runtime_3d_solver.c \
	$(SRC_DIR)/app/sim_runtime_3d_solver_step.c \
	$(SRC_DIR)/app/sim_runtime_3d_solver_core_sim.c \
	$(SRC_DIR)/app/sim_runtime_mesh_accel.c \
	$(SRC_DIR)/app/sim_runtime_mesh_obstacle_proxy.c \
	$(SRC_DIR)/app/sim_runtime_mesh_diagnostics.c \
	$(SRC_DIR)/app/sim_runtime_emitter.c \
	$(SRC_DIR)/app/sim_runtime_obstacle.c \
	$(CORE_SIM_DIR)/src/core_sim.c \
	$(CORE_MESH_ASSET_DIR)/src/core_mesh_asset.c \
	$(CORE_MESH_ASSET_DIR)/src/core_mesh_asset_runtime_document.c \
	$(CORE_OBJECT_DIR)/src/core_object.c \
	$(CORE_UNITS_DIR)/src/core_units.c \
	$(CORE_IO_DIR)/src/core_io.c \
	$(CORE_BASE_DIR)/src/core_base.c \
	$(CORE_MESH_PREVIEW_DIR)/../../shape/external/cjson/cJSON.c

SIM_RUNTIME_BACKEND_3D_OBSTACLE_TEST_SRCS := \
	tests/sim_runtime_backend_3d_obstacle_contract_test.c \
	$(SRC_DIR)/app/atmospheric/atmospheric_field.c \
	$(SRC_DIR)/app/sim_runtime_backend.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_scaffold.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_wind.c \
	$(SRC_DIR)/app/wind_tunnel_3d_analysis.c \
	$(SRC_DIR)/app/wind_tunnel_3d.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_scaffold_views.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_runtime.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_obstacles.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_obstacle_sources.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_oriented_box.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_emitter_shapes.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_emitters.c \
		$(SRC_DIR)/app/sim_runtime_3d_domain.c \
		$(SRC_DIR)/app/sim_runtime_3d_brick_store.c \
		$(SRC_DIR)/app/sim_runtime_3d_footprint.c \
		$(SRC_DIR)/app/sim_runtime_3d_anchor.c \
	$(SRC_DIR)/app/sim_runtime_3d_space.c \
	$(SRC_DIR)/app/sim_runtime_3d_solver.c \
	$(SRC_DIR)/app/sim_runtime_3d_solver_step.c \
	$(SRC_DIR)/app/sim_runtime_3d_solver_core_sim.c \
	$(SRC_DIR)/app/sim_runtime_emitter.c \
	$(SRC_DIR)/app/sim_runtime_obstacle.c \
	$(CORE_SIM_DIR)/src/core_sim.c

SIM_RUNTIME_BACKEND_3D_WIND_TUNNEL_TEST_SRCS := \
	tests/sim_runtime_backend_3d_wind_tunnel_contract_test.c \
	$(SRC_DIR)/app/atmospheric/atmospheric_field.c \
	$(SRC_DIR)/app/app_config.c \
	$(SRC_DIR)/app/data_paths.c \
	$(SRC_DIR)/app/sim_runtime_backend.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_scaffold.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_wind.c \
	$(SRC_DIR)/app/wind_tunnel_3d_analysis.c \
	$(SRC_DIR)/app/wind_tunnel_3d_inspector.c \
	$(SRC_DIR)/app/wind_tunnel_3d.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_scaffold_views.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_runtime.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_obstacles.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_obstacle_sources.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_oriented_box.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_emitter_shapes.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_emitters.c \
	$(SRC_DIR)/app/sim_runtime_3d_domain.c \
	$(SRC_DIR)/app/sim_runtime_3d_brick_store.c \
	$(SRC_DIR)/app/sim_runtime_3d_footprint.c \
	$(SRC_DIR)/app/sim_runtime_3d_anchor.c \
	$(SRC_DIR)/app/sim_runtime_3d_space.c \
	$(SRC_DIR)/app/sim_runtime_3d_solver.c \
	$(SRC_DIR)/app/sim_runtime_3d_solver_step.c \
	$(SRC_DIR)/app/sim_runtime_3d_solver_core_sim.c \
	$(SRC_DIR)/app/sim_runtime_emitter.c \
	$(SRC_DIR)/app/sim_runtime_obstacle.c \
	$(CORE_SIM_DIR)/src/core_sim.c

SIM_RUNTIME_BACKEND_3D_RETAINED_OBSTACLE_TEST_SRCS := \
	tests/sim_runtime_backend_3d_retained_obstacle_contract_test.c \
	$(SRC_DIR)/app/atmospheric/atmospheric_field.c \
	$(SRC_DIR)/app/sim_runtime_backend.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_scaffold.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_wind.c \
	$(SRC_DIR)/app/wind_tunnel_3d_analysis.c \
	$(SRC_DIR)/app/wind_tunnel_3d.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_scaffold_views.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_runtime.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_obstacles.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_obstacle_sources.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_oriented_box.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_emitter_shapes.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_emitters.c \
		$(SRC_DIR)/app/sim_runtime_3d_domain.c \
		$(SRC_DIR)/app/sim_runtime_3d_brick_store.c \
		$(SRC_DIR)/app/sim_runtime_3d_footprint.c \
		$(SRC_DIR)/app/sim_runtime_3d_anchor.c \
	$(SRC_DIR)/app/sim_runtime_3d_space.c \
	$(SRC_DIR)/app/sim_runtime_3d_solver.c \
	$(SRC_DIR)/app/sim_runtime_3d_solver_step.c \
	$(SRC_DIR)/app/sim_runtime_3d_solver_core_sim.c \
	$(SRC_DIR)/app/sim_runtime_emitter.c \
	$(SRC_DIR)/app/sim_runtime_obstacle.c \
	$(CORE_SIM_DIR)/src/core_sim.c

SIM_RUNTIME_BACKEND_DISPATCH_TEST_SRCS := \
	tests/sim_runtime_backend_dispatch_contract_test.c \
	$(SRC_DIR)/app/sim_runtime_backend.c

SIM_RUNTIME_BACKEND_REPORTING_TEST_SRCS := \
	tests/sim_runtime_backend_reporting_contract_test.c \
	$(SRC_DIR)/app/atmospheric/atmospheric_field.c \
	$(SRC_DIR)/app/app_config.c \
	$(SRC_DIR)/app/data_paths.c \
	$(SRC_DIR)/app/sim_runtime_backend.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_scaffold.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_wind.c \
	$(SRC_DIR)/app/wind_tunnel_3d_analysis.c \
	$(SRC_DIR)/app/wind_tunnel_3d.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_scaffold_views.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_runtime.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_obstacles.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_obstacle_sources.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_oriented_box.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_emitter_shapes.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_emitters.c \
		$(SRC_DIR)/app/sim_runtime_3d_domain.c \
		$(SRC_DIR)/app/sim_runtime_3d_brick_store.c \
		$(SRC_DIR)/app/sim_runtime_3d_footprint.c \
		$(SRC_DIR)/app/sim_runtime_3d_anchor.c \
	$(SRC_DIR)/app/sim_runtime_3d_space.c \
	$(SRC_DIR)/app/sim_runtime_3d_solver.c \
	$(SRC_DIR)/app/sim_runtime_3d_solver_step.c \
	$(SRC_DIR)/app/sim_runtime_3d_solver_core_sim.c \
	$(SRC_DIR)/app/sim_runtime_emitter.c \
	$(SRC_DIR)/app/sim_runtime_obstacle.c \
	$(CORE_SIM_DIR)/src/core_sim.c

SIM_RUNTIME_3D_ANCHOR_TEST_SRCS := \
	tests/sim_runtime_3d_anchor_contract_test.c \
	$(SRC_DIR)/app/sim_runtime_3d_anchor.c \
	$(SRC_DIR)/app/sim_runtime_3d_space.c

SIM_RUNTIME_3D_FOOTPRINT_TEST_SRCS := \
	tests/sim_runtime_3d_footprint_contract_test.c \
	$(SRC_DIR)/app/sim_runtime_3d_footprint.c

SIM_RUNTIME_3D_SPACE_TEST_SRCS := \
	tests/sim_runtime_3d_space_contract_test.c \
	$(SRC_DIR)/app/sim_runtime_3d_space.c

SIM_RUNTIME_3D_DOMAIN_TEST_SRCS := \
	tests/sim_runtime_3d_domain_contract_test.c \
	$(SRC_DIR)/app/sim_runtime_3d_domain.c

SIM_RUNTIME_3D_SOLVER_TEST_SRCS := \
	tests/sim_runtime_3d_solver_contract_test.c \
	$(SRC_DIR)/app/app_config.c \
	$(SRC_DIR)/app/data_paths.c \
	$(SRC_DIR)/app/sim_runtime_3d_domain.c \
	$(SRC_DIR)/app/sim_runtime_3d_solver.c \
	$(SRC_DIR)/app/sim_runtime_3d_solver_step.c \
	$(SRC_DIR)/app/sim_runtime_3d_solver_core_sim.c \
	$(CORE_SIM_DIR)/src/core_sim.c

SCENE_CORE_SIM_RUNTIME_STEP_TEST_SRCS := \
	tests/scene_core_sim_runtime_step_contract_test.c \
	$(SRC_DIR)/app/scene_core_sim_runtime_step.c \
	$(SRC_DIR)/app/scene_apply.c \
	$(SRC_DIR)/physics/objects/object_manager.c \
	$(SRC_DIR)/physics/rigid/rigid2d.c \
	$(SRC_DIR)/physics/rigid/rigid2d_collision.c \
	$(CORE_SIM_DIR)/src/core_sim.c

MENU_SETTINGS_SHELL_CONTRACT_TEST_SRCS := \
	tests/menu_settings_shell_contract_test.c \
	$(SRC_DIR)/app/menu/menu_settings_schema.c \
	$(SRC_DIR)/app/menu/menu_settings_provider_common.c \
	$(SRC_DIR)/app/menu/menu_settings_provider_2d.c \
	$(SRC_DIR)/app/menu/menu_settings_provider_3d.c \
	$(SRC_DIR)/app/menu/menu_settings_provider_wind.c \
	$(SRC_DIR)/app/menu/menu_settings_provider_structural.c \
	$(SRC_DIR)/app/menu/menu_settings_provider_atmospheric.c \
	$(SRC_DIR)/app/menu/menu_settings_provider_water.c \
	$(SRC_DIR)/app/menu/menu_settings_draft.c \
	$(SRC_DIR)/app/atmospheric/atmospheric_field.c \
	$(SRC_DIR)/app/water_mode.c \
	$(SRC_DIR)/app/quality_profiles.c \
	$(SRC_DIR)/app/app_config.c \
	$(SRC_DIR)/app/data_paths.c \
	$(SRC_DIR)/app/sim_runtime_3d_domain.c \
	$(SRC_DIR)/app/sim_runtime_3d_solver.c \
	$(SRC_DIR)/app/sim_runtime_3d_solver_step.c

SCENE_MENU_LAYOUT_CONTRACT_TEST_SRCS := \
	tests/scene_menu_layout_contract_test.c \
	$(SRC_DIR)/app/scene_menu_layout_helpers.c \
	$(SRC_DIR)/app/menu/menu_settings_layout.c \
	$(SRC_DIR)/app/menu/menu_settings_schema.c \
	$(SRC_DIR)/app/menu/menu_settings_provider_common.c \
	$(SRC_DIR)/app/menu/menu_settings_provider_2d.c \
	$(SRC_DIR)/app/menu/menu_settings_provider_3d.c \
	$(SRC_DIR)/app/menu/menu_settings_provider_wind.c \
	$(SRC_DIR)/app/menu/menu_settings_provider_structural.c \
	$(SRC_DIR)/app/menu/menu_settings_provider_atmospheric.c \
	$(SRC_DIR)/app/menu/menu_settings_provider_water.c \
	$(SRC_DIR)/app/menu/menu_settings_draft.c \
	$(SRC_DIR)/render/text_upload_policy.c \
	$(SRC_DIR)/app/atmospheric/atmospheric_field.c \
	$(SRC_DIR)/app/water_mode.c \
	$(SRC_DIR)/app/quality_profiles.c \
	$(SRC_DIR)/app/app_config.c \
	$(SRC_DIR)/app/data_paths.c \
	$(SRC_DIR)/app/sim_runtime_3d_domain.c \
	$(SRC_DIR)/app/sim_runtime_3d_solver.c \
	$(SRC_DIR)/app/sim_runtime_3d_solver_step.c

QUALITY_PROFILES_CONTRACT_TEST_SRCS := \
	tests/quality_profiles_contract_test.c \
	$(SRC_DIR)/app/quality_profiles.c \
	$(SRC_DIR)/app/app_config.c \
	$(SRC_DIR)/app/data_paths.c

CONFIG_LOADER_CONTRACT_TEST_SRCS := \
	tests/config_loader_contract_test.c \
	$(SRC_DIR)/config/config_loader.c \
	$(SRC_DIR)/app/app_config.c \
	$(SRC_DIR)/app/data_paths.c

RUNTIME_SCENE_BRIDGE_TEST_SRCS := \
	tests/runtime_scene_bridge_contract_test.c \
	tests/runtime_scene_bridge_contract_session_suite.c \
	tests/runtime_scene_bridge_contract_writeback_suite.c \
	$(SRC_DIR)/import/runtime_scene_bridge.c \
	$(SRC_DIR)/import/runtime_mesh_preview_bridge.c \
	$(SRC_DIR)/import/runtime_scene_solver_projection.c \
	$(SRC_DIR)/import/runtime_scene_solver_projection_domain.c \
	$(SRC_DIR)/import/runtime_scene_solver_projection_objects.c \
	$(SRC_DIR)/import/runtime_scene_solver_projection_emitters.c \
	$(SRC_DIR)/app/wind_tunnel_3d.c \
	$(SRC_DIR)/app/scene_objects.c \
	$(SRC_DIR)/app/editor/scene_editor_session.c \
	$(SRC_DIR)/app/editor/scene_editor_session_overlay_edit.c \
	$(SRC_DIR)/app/editor/scene_editor_session_labels.c \
	$(SRC_DIR)/app/atmospheric/atmospheric_field.c \
	$(SRC_DIR)/app/app_config.c \
	$(SRC_DIR)/app/data_paths.c \
	$(SRC_DIR)/app/scene_presets.c \
	$(SRC_DIR)/app/sim_runtime_emitter.c \
	$(SRC_DIR)/app/sim_runtime_3d_anchor.c \
	$(SRC_DIR)/app/sim_runtime_3d_space.c \
	$(SRC_DIR)/physics/objects/object_manager.c \
	$(SRC_DIR)/physics/rigid/rigid2d.c \
	$(SRC_DIR)/physics/rigid/rigid2d_collision.c \
	$(CORE_SCENE_DIR)/src/core_scene.c \
	$(CORE_SCENE_COMPILE_DIR)/src/core_scene_compile.c \
	$(CORE_MESH_PREVIEW_DIR)/src/core_mesh_preview.c \
	$(CORE_MESH_ASSET_DIR)/src/core_mesh_asset.c \
	$(CORE_MESH_ASSET_DIR)/src/core_mesh_asset_runtime_document.c \
	$(CORE_OBJECT_DIR)/src/core_object.c \
	$(CORE_UNITS_DIR)/src/core_units.c \
	$(CORE_IO_DIR)/src/core_io.c \
	$(CORE_BASE_DIR)/src/core_base.c \
	$(CORE_MESH_PREVIEW_DIR)/../../shape/external/cjson/cJSON.c

RUNTIME_SCENE_VIEW_PACKET_READOUT_TEST_SRCS := \
	tests/runtime_scene_view_packet_readout_contract_test.c \
	$(SRC_DIR)/import/runtime_scene_view_packet_readout.c \
	$(CORE_SCENE_VIEW_DIR)/src/core_scene_view.c \
	$(filter-out tests/runtime_scene_bridge_contract_test.c tests/runtime_scene_bridge_contract_session_suite.c tests/runtime_scene_bridge_contract_writeback_suite.c,$(RUNTIME_SCENE_BRIDGE_TEST_SRCS))

RUNTIME_SCENE_VIEW_METADATA_AUTHORITY_TEST_SRCS := \
	tests/runtime_scene_view_metadata_authority_contract_test.c \
	$(SRC_DIR)/import/runtime_scene_view_packet_readout.c \
	$(CORE_SCENE_VIEW_DIR)/src/core_scene_view.c \
	$(filter-out tests/runtime_scene_bridge_contract_test.c tests/runtime_scene_bridge_contract_session_suite.c tests/runtime_scene_bridge_contract_writeback_suite.c,$(RUNTIME_SCENE_BRIDGE_TEST_SRCS))

RUNTIME_MESH_PREVIEW_BRIDGE_TEST_SRCS := \
	tests/runtime_mesh_preview_bridge_contract_test.c \
	$(SRC_DIR)/import/runtime_mesh_preview_bridge.c \
	$(SRC_DIR)/import/runtime_mesh_preview_path_resolver.c \
	$(CORE_MESH_PREVIEW_DIR)/src/core_mesh_preview.c \
	$(CORE_MESH_ASSET_DIR)/src/core_mesh_asset.c \
	$(CORE_MESH_ASSET_DIR)/src/core_mesh_asset_runtime_document.c \
	$(CORE_SCENE_DIR)/src/core_scene.c \
	$(CORE_OBJECT_DIR)/src/core_object.c \
	$(CORE_UNITS_DIR)/src/core_units.c \
	$(CORE_IO_DIR)/src/core_io.c \
	$(CORE_BASE_DIR)/src/core_base.c \
	$(CORE_MESH_PREVIEW_DIR)/../../shape/external/cjson/cJSON.c

RUNTIME_MESH_OBSTACLE_PROXY_TEST_SRCS := \
	tests/runtime_mesh_obstacle_proxy_contract_test.c \
	$(SRC_DIR)/app/sim_runtime_mesh_accel.c \
	$(SRC_DIR)/app/sim_runtime_mesh_obstacle_proxy.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_mesh_obstacle_proxy.c \
	$(SRC_DIR)/app/sim_runtime_3d_domain.c \
	$(SRC_DIR)/app/sim_runtime_3d_space.c \
	$(CORE_MESH_ASSET_DIR)/src/core_mesh_asset.c \
	$(CORE_MESH_ASSET_DIR)/src/core_mesh_asset_runtime_document.c \
	$(CORE_OBJECT_DIR)/src/core_object.c \
	$(CORE_UNITS_DIR)/src/core_units.c \
	$(CORE_IO_DIR)/src/core_io.c \
	$(CORE_BASE_DIR)/src/core_base.c \
	$(CORE_MESH_PREVIEW_DIR)/../../shape/external/cjson/cJSON.c

RUNTIME_SCENE_SOLVER_PROJECTION_TEST_SRCS := \
	tests/runtime_scene_solver_projection_contract_test.c \
	$(SRC_DIR)/app/atmospheric/atmospheric_field.c \
	$(SRC_DIR)/import/runtime_scene_solver_projection.c \
	$(SRC_DIR)/import/runtime_scene_solver_projection_domain.c \
	$(SRC_DIR)/import/runtime_scene_solver_projection_objects.c \
	$(SRC_DIR)/import/runtime_scene_solver_projection_emitters.c \
	$(SRC_DIR)/app/wind_tunnel_3d.c \
	$(SRC_DIR)/app/app_config.c \
	$(SRC_DIR)/app/data_paths.c \
	$(SRC_DIR)/app/scene_presets.c \
	$(SRC_DIR)/app/sim_runtime_3d_anchor.c \
	$(SRC_DIR)/app/sim_runtime_3d_space.c

RUNTIME_SCENE_3D_TRUTH_TEST_SRCS := \
	tests/runtime_scene_3d_truth_contract_test.c \
	$(SRC_DIR)/app/atmospheric/atmospheric_field.c \
	$(SRC_DIR)/import/runtime_scene_solver_projection.c \
	$(SRC_DIR)/import/runtime_scene_solver_projection_domain.c \
	$(SRC_DIR)/import/runtime_scene_solver_projection_objects.c \
	$(SRC_DIR)/import/runtime_scene_solver_projection_emitters.c \
	$(SRC_DIR)/render/retained_runtime_scene_overlay_space.c \
	$(SRC_DIR)/render/retained_runtime_scene_overlay_geom.c \
	$(SRC_DIR)/app/sim_runtime_backend.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_scaffold.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_wind.c \
	$(SRC_DIR)/app/wind_tunnel_3d_analysis.c \
	$(SRC_DIR)/app/wind_tunnel_3d.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_scaffold_views.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_runtime.c \
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
	$(SRC_DIR)/app/sim_runtime_emitter.c \
	$(SRC_DIR)/app/sim_runtime_obstacle.c \
	$(CORE_SIM_DIR)/src/core_sim.c

SCENE_RUNTIME_LAUNCH_PROJECTION_TEST_SRCS := \
	tests/scene_runtime_launch_projection_contract_test.c \
	$(SRC_DIR)/app/atmospheric/atmospheric_field.c \
	$(SRC_DIR)/app/scene_runtime_launch_projection.c \
	$(SRC_DIR)/import/runtime_scene_bridge.c \
	$(SRC_DIR)/import/runtime_mesh_preview_bridge.c \
	$(SRC_DIR)/import/runtime_scene_solver_projection.c \
	$(SRC_DIR)/import/runtime_scene_solver_projection_domain.c \
	$(SRC_DIR)/import/runtime_scene_solver_projection_objects.c \
	$(SRC_DIR)/import/runtime_scene_solver_projection_emitters.c \
	$(SRC_DIR)/app/wind_tunnel_3d.c \
	$(SRC_DIR)/app/app_config.c \
	$(SRC_DIR)/app/data_paths.c \
	$(SRC_DIR)/app/scene_presets.c \
	$(SRC_DIR)/app/sim_runtime_3d_anchor.c \
	$(SRC_DIR)/app/sim_runtime_3d_space.c \
	$(SRC_DIR)/app/scene_objects.c \
	$(SRC_DIR)/app/editor/scene_editor_session.c \
	$(SRC_DIR)/app/editor/scene_editor_session_labels.c \
	$(SRC_DIR)/physics/objects/object_manager.c \
	$(SRC_DIR)/physics/rigid/rigid2d.c \
	$(SRC_DIR)/physics/rigid/rigid2d_collision.c \
	$(CORE_SCENE_DIR)/src/core_scene.c \
	$(CORE_SCENE_COMPILE_DIR)/src/core_scene_compile.c \
	$(CORE_MESH_PREVIEW_DIR)/src/core_mesh_preview.c \
	$(CORE_MESH_ASSET_DIR)/src/core_mesh_asset.c \
	$(CORE_MESH_ASSET_DIR)/src/core_mesh_asset_runtime_document.c \
	$(CORE_OBJECT_DIR)/src/core_object.c \
	$(CORE_UNITS_DIR)/src/core_units.c \
	$(CORE_IO_DIR)/src/core_io.c \
	$(CORE_BASE_DIR)/src/core_base.c \
	$(CORE_MESH_PREVIEW_DIR)/../../shape/external/cjson/cJSON.c

SCENE_OBJECTS_RUNTIME_TEST_SRCS := \
	tests/scene_objects_runtime_contract_test.c \
	$(SRC_DIR)/app/scene_objects.c \
	$(SRC_DIR)/physics/objects/object_manager.c \
	$(SRC_DIR)/physics/rigid/rigid2d.c \
	$(SRC_DIR)/physics/rigid/rigid2d_collision.c

RIGID2D_COLLISION_CONTRACT_TEST_SRCS := \
	tests/rigid2d_collision_contract_test.c \
	$(SRC_DIR)/physics/rigid/rigid2d_collision.c

STRUCTURAL_RUNTIME_SPLIT_TEST_SRCS := \
	tests/structural_runtime_split_contract_test.c \
	$(SRC_DIR)/app/structural/structural_controller_runtime.c \
	$(SRC_DIR)/physics/structural/structural_scene.c \
	$(SRC_DIR)/physics/structural/structural_solver.c \
	$(SRC_DIR)/physics/structural/structural_sparse_matrix.c

SCENE_EDITOR_SCENE_LIBRARY_TEST_SRCS := \
	tests/scene_editor_scene_library_contract_test.c \
	$(SRC_DIR)/app/editor/scene_editor_scene_library.c \
	$(SRC_DIR)/app/data_paths.c \
	$(SRC_DIR)/app/scene_presets.c

SCENE_PROJECT_CACHE_OUTPUT_STATUS_TEST_SRCS := \
	tests/scene_project_cache_output_status_contract_test.c \
	$(SRC_DIR)/app/scene_project_cache_output.c \
	$(SRC_DIR)/app/physics_sim_json_helpers.c

SCENE_EDITOR_RETAINED_DOCUMENT_TEST_SRCS := \
	tests/scene_editor_retained_document_contract_test.c \
	$(SRC_DIR)/app/editor/scene_editor_retained_document.c

PHYSICS_SIM_WORKSPACE_AUTHORING_HOST_TEST_SRCS := \
	tests/physics_sim_workspace_authoring_host_test.c \
	$(SRC_DIR)/app/menu/workspace_authoring/physics_sim_workspace_authoring_host.c \
	$(SRC_DIR)/app/menu/workspace_authoring/physics_sim_workspace_authoring_overlay_model.c \
	$(SRC_DIR)/app/menu/shared_theme_font_adapter.c \
	$(SRC_DIR)/app/app_config.c \
	$(SRC_DIR)/app/data_paths.c \
	$(SRC_DIR)/app/editor/scene_editor_pane_host.c \
	$(KIT_WORKSPACE_AUTHORING_DIR)/src/kit_workspace_authoring.c \
	$(KIT_WORKSPACE_AUTHORING_DIR)/src/ui/kit_workspace_authoring_ui_overlay.c \
	$(KIT_WORKSPACE_AUTHORING_DIR)/src/ui/kit_workspace_authoring_ui_font_theme.c \
	$(CORE_PANE_DIR)/src/core_pane.c \
	$(KIT_PANE_DIR)/src/kit_pane.c \
	$(KIT_RENDER_DIR)/src/kit_render.c \
	$(KIT_RENDER_DIR)/src/kit_render_backend_null.c \
	$(KIT_RENDER_DIR)/src/kit_render_backend_vk.c \
	$(CORE_THEME_DIR)/src/core_theme.c \
	$(CORE_FONT_DIR)/src/core_font.c \
	$(CORE_BASE_DIR)/src/core_base.c

SCENE_EDITOR_PANE_HOST_TEST_SRCS := \
	tests/scene_editor_pane_host_contract_test.c \
	$(SRC_DIR)/app/editor/scene_editor_pane_host.c \
	$(CORE_PANE_DIR)/src/core_pane.c \
	$(KIT_PANE_DIR)/src/kit_pane.c \
	$(KIT_RENDER_DIR)/src/kit_render.c \
	$(KIT_RENDER_DIR)/src/kit_render_backend_null.c \
	$(KIT_RENDER_DIR)/src/kit_render_backend_vk.c \
	$(CORE_THEME_DIR)/src/core_theme.c \
	$(CORE_FONT_DIR)/src/core_font.c \
	$(CORE_BASE_DIR)/src/core_base.c

SCENE_EDITOR_VIEWPORT_TEST_SRCS := \
	tests/scene_editor_viewport_contract_test.c \
	$(SRC_DIR)/app/editor/scene_editor_viewport.c \
	$(CORE_VIEWPORT2D_DIR)/src/core_viewport2d.c \
	$(CORE_BASE_DIR)/src/core_base.c

SCENE_EDITOR_WIND_SETUP_TEST_SRCS := \
	tests/scene_editor_wind_setup_contract_test.c \
	$(SRC_DIR)/app/editor/scene_editor_wind_setup.c \
	$(SRC_DIR)/app/editor/scene_editor_session.c \
	$(SRC_DIR)/app/editor/scene_editor_session_overlay_edit.c \
	$(SRC_DIR)/app/wind_tunnel_3d.c

RETAINED_RUNTIME_SCENE_OVERLAY_GEOM_TEST_SRCS := \
	tests/retained_runtime_scene_overlay_geom_contract_test.c \
	$(SRC_DIR)/render/retained_runtime_scene_overlay_geom.c

RETAINED_RUNTIME_SCENE_OVERLAY_READOUT_TEST_SRCS := \
	tests/retained_runtime_scene_overlay_readout_contract_test.c \
	$(SRC_DIR)/render/retained_runtime_scene_overlay_readout.c \
	$(SRC_DIR)/app/editor/scene_editor_viewport.c \
	$(CORE_VIEWPORT2D_DIR)/src/core_viewport2d.c \
	$(CORE_BASE_DIR)/src/core_base.c

RETAINED_RUNTIME_SCENE_OVERLAY_SPACE_TEST_SRCS := \
	tests/retained_runtime_scene_overlay_space_contract_test.c \
	$(SRC_DIR)/render/retained_runtime_scene_overlay_space.c \
	$(SRC_DIR)/render/retained_runtime_scene_overlay_geom.c \
	$(SRC_DIR)/app/sim_runtime_3d_anchor.c \
	$(SRC_DIR)/app/sim_runtime_3d_space.c

ifeq ($(UNAME_S),Darwin)
RETAINED_RUNTIME_SCENE_OVERLAY_GEOM_TEST_PLATFORM_CFLAGS := -I/opt/homebrew/include -D_THREAD_SAFE
STRUCTURAL_RUNTIME_SPLIT_TEST_PLATFORM_CFLAGS := -I/opt/homebrew/include -D_THREAD_SAFE
else
RETAINED_RUNTIME_SCENE_OVERLAY_GEOM_TEST_PLATFORM_CFLAGS := $(if $(SDL_CFLAGS),$(SDL_CFLAGS),-I/usr/include/SDL2)
STRUCTURAL_RUNTIME_SPLIT_TEST_PLATFORM_CFLAGS := $(if $(SDL_CFLAGS),$(SDL_CFLAGS),-I/usr/include/SDL2)
endif

test-sim-mode-route-contract: $(SIM_MODE_ROUTE_CONTRACT_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CSTD) $(WARN) $(DEBUG) \
		-I$(INC_DIR) -I$(SRC_DIR) \
		-o $(BUILD_DIR)/sim_mode_route_contract_test $(SIM_MODE_ROUTE_CONTRACT_TEST_SRCS) -lm
	$(BUILD_DIR)/sim_mode_route_contract_test

test-wind-tunnel-3d-contract: $(WIND_TUNNEL_3D_CONTRACT_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CSTD) $(WARN) $(DEBUG) \
		-I$(INC_DIR) -I$(SRC_DIR) \
		-o $(BUILD_DIR)/wind_tunnel_3d_contract_test $(WIND_TUNNEL_3D_CONTRACT_TEST_SRCS) -lm
	$(BUILD_DIR)/wind_tunnel_3d_contract_test

test-water-mode-contract: $(WATER_MODE_CONTRACT_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CSTD) $(WARN) $(DEBUG) \
		-I$(INC_DIR) -I$(SRC_DIR) \
		-o $(BUILD_DIR)/water_mode_contract_test $(WATER_MODE_CONTRACT_TEST_SRCS) -lm
	$(BUILD_DIR)/water_mode_contract_test

test-water-surface-artifacts-contract: $(WATER_SURFACE_ARTIFACTS_CONTRACT_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) \
		-I$(CORE_BASE_DIR)/include -I$(CORE_IO_DIR)/include -I$(CORE_DATA_DIR)/include \
		-I$(CORE_PACK_DIR)/include -I$(CORE_SCENE_DIR)/include -I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include \
		-o $(BUILD_DIR)/water_surface_artifacts_contract_test $(WATER_SURFACE_ARTIFACTS_CONTRACT_TEST_SRCS) $(filter-out -lSDL2 -lSDL2_ttf,$(LIBS))
	$(BUILD_DIR)/water_surface_artifacts_contract_test

test-sim-runtime-emitter-contract: $(SIM_RUNTIME_EMITTER_CONTRACT_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CSTD) $(WARN) $(DEBUG) \
		-I$(INC_DIR) -I$(SRC_DIR) \
		-o $(BUILD_DIR)/sim_runtime_emitter_contract_test $(SIM_RUNTIME_EMITTER_CONTRACT_TEST_SRCS) -lm
	$(BUILD_DIR)/sim_runtime_emitter_contract_test

test-sim-runtime-obstacle-contract: $(SIM_RUNTIME_OBSTACLE_CONTRACT_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CSTD) $(WARN) $(DEBUG) \
		-I$(INC_DIR) -I$(SRC_DIR) \
		-o $(BUILD_DIR)/sim_runtime_obstacle_contract_test $(SIM_RUNTIME_OBSTACLE_CONTRACT_TEST_SRCS) -lm
	$(BUILD_DIR)/sim_runtime_obstacle_contract_test

test-sim-runtime-backend-3d-emitter-contract: $(SIM_RUNTIME_BACKEND_3D_EMITTER_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) \
		-I$(CORE_BASE_DIR)/include -I$(CORE_SCENE_DIR)/include -I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include \
		-o $(BUILD_DIR)/sim_runtime_backend_3d_emitter_contract_test $(SIM_RUNTIME_BACKEND_3D_EMITTER_TEST_SRCS) -lm
	$(BUILD_DIR)/sim_runtime_backend_3d_emitter_contract_test

test-sim-runtime-backend-3d-attached-emitter-contract: $(SIM_RUNTIME_BACKEND_3D_ATTACHED_EMITTER_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) \
		-I$(CORE_BASE_DIR)/include -I$(CORE_SCENE_DIR)/include -I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include \
		-o $(BUILD_DIR)/sim_runtime_backend_3d_attached_emitter_contract_test $(SIM_RUNTIME_BACKEND_3D_ATTACHED_EMITTER_TEST_SRCS) -lm
	$(BUILD_DIR)/sim_runtime_backend_3d_attached_emitter_contract_test

test-sim-runtime-backend-3d-runtime-mesh-emitter-contract: $(SIM_RUNTIME_BACKEND_3D_RUNTIME_MESH_EMITTER_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) \
		-I$(CORE_BASE_DIR)/include -I$(CORE_SCENE_DIR)/include -I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include \
		-I$(CORE_MESH_ASSET_DIR)/include -I$(CORE_MESH_PREVIEW_DIR)/include \
		-I$(CORE_MESH_PREVIEW_DIR)/../../shape/external -I$(CORE_IO_DIR)/include \
		-o $(BUILD_DIR)/sim_runtime_backend_3d_runtime_mesh_emitter_contract_test $(SIM_RUNTIME_BACKEND_3D_RUNTIME_MESH_EMITTER_TEST_SRCS) $(JSON_LIBS) -lm
	$(BUILD_DIR)/sim_runtime_backend_3d_runtime_mesh_emitter_contract_test

test-sim-runtime-backend-3d-obstacle-contract: $(SIM_RUNTIME_BACKEND_3D_OBSTACLE_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) \
		-I$(CORE_BASE_DIR)/include -I$(CORE_SCENE_DIR)/include -I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include \
		-o $(BUILD_DIR)/sim_runtime_backend_3d_obstacle_contract_test $(SIM_RUNTIME_BACKEND_3D_OBSTACLE_TEST_SRCS) -lm
	$(BUILD_DIR)/sim_runtime_backend_3d_obstacle_contract_test

test-sim-runtime-backend-3d-wind-tunnel-contract: $(SIM_RUNTIME_BACKEND_3D_WIND_TUNNEL_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) \
		-I$(CORE_BASE_DIR)/include -I$(CORE_SCENE_DIR)/include -I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include \
		-o $(BUILD_DIR)/sim_runtime_backend_3d_wind_tunnel_contract_test $(SIM_RUNTIME_BACKEND_3D_WIND_TUNNEL_TEST_SRCS) -lm
	$(BUILD_DIR)/sim_runtime_backend_3d_wind_tunnel_contract_test

test-sim-runtime-backend-3d-retained-obstacle-contract: $(SIM_RUNTIME_BACKEND_3D_RETAINED_OBSTACLE_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) \
		-I$(CORE_BASE_DIR)/include -I$(CORE_SCENE_DIR)/include -I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include \
		-o $(BUILD_DIR)/sim_runtime_backend_3d_retained_obstacle_contract_test $(SIM_RUNTIME_BACKEND_3D_RETAINED_OBSTACLE_TEST_SRCS) -lm
	$(BUILD_DIR)/sim_runtime_backend_3d_retained_obstacle_contract_test

test-sim-runtime-3d-anchor-contract: $(SIM_RUNTIME_3D_ANCHOR_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) \
		-I$(CORE_BASE_DIR)/include -I$(CORE_SCENE_DIR)/include -I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include \
		-o $(BUILD_DIR)/sim_runtime_3d_anchor_contract_test $(SIM_RUNTIME_3D_ANCHOR_TEST_SRCS) -lm
	$(BUILD_DIR)/sim_runtime_3d_anchor_contract_test

test-sim-runtime-3d-footprint-contract: $(SIM_RUNTIME_3D_FOOTPRINT_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CSTD) $(WARN) $(DEBUG) \
		-I$(INC_DIR) -I$(SRC_DIR) \
		-o $(BUILD_DIR)/sim_runtime_3d_footprint_contract_test $(SIM_RUNTIME_3D_FOOTPRINT_TEST_SRCS) -lm
	$(BUILD_DIR)/sim_runtime_3d_footprint_contract_test

test-sim-runtime-3d-space-contract: $(SIM_RUNTIME_3D_SPACE_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CSTD) $(WARN) $(DEBUG) \
		-I$(INC_DIR) -I$(SRC_DIR) \
		-I$(CORE_BASE_DIR)/include -I$(CORE_SCENE_DIR)/include -I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include \
		-o $(BUILD_DIR)/sim_runtime_3d_space_contract_test $(SIM_RUNTIME_3D_SPACE_TEST_SRCS) -lm
	$(BUILD_DIR)/sim_runtime_3d_space_contract_test

test-sim-runtime-3d-domain-contract: $(SIM_RUNTIME_3D_DOMAIN_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CSTD) $(WARN) $(DEBUG) \
		-I$(INC_DIR) -I$(SRC_DIR) \
		-I$(CORE_BASE_DIR)/include -I$(CORE_SCENE_DIR)/include $(MESH_PREVIEW_BRIDGE_INCS) -I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include \
		-o $(BUILD_DIR)/sim_runtime_3d_domain_contract_test $(SIM_RUNTIME_3D_DOMAIN_TEST_SRCS) -lm
	$(BUILD_DIR)/sim_runtime_3d_domain_contract_test

test-sim-runtime-3d-solver-contract: $(SIM_RUNTIME_3D_SOLVER_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CSTD) $(WARN) $(DEBUG) \
		-I$(INC_DIR) -I$(SRC_DIR) \
		-I$(CORE_BASE_DIR)/include -I$(CORE_SCENE_DIR)/include $(MESH_PREVIEW_BRIDGE_INCS) -I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include -I$(CORE_SIM_DIR)/include \
		-o $(BUILD_DIR)/sim_runtime_3d_solver_contract_test $(SIM_RUNTIME_3D_SOLVER_TEST_SRCS) -lm
	$(BUILD_DIR)/sim_runtime_3d_solver_contract_test

test-scene-core-sim-runtime-step-contract: $(SCENE_CORE_SIM_RUNTIME_STEP_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) \
		-o $(BUILD_DIR)/scene_core_sim_runtime_step_contract_test $(SCENE_CORE_SIM_RUNTIME_STEP_TEST_SRCS) -lm
	$(BUILD_DIR)/scene_core_sim_runtime_step_contract_test

test-menu-settings-shell-contract: $(MENU_SETTINGS_SHELL_CONTRACT_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CSTD) $(WARN) $(DEBUG) \
		-I$(INC_DIR) -I$(SRC_DIR) -I$(SRC_DIR)/tools \
		-I$(CORE_BASE_DIR)/include -I$(CORE_SCENE_DIR)/include $(MESH_PREVIEW_BRIDGE_INCS) -I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include \
		-o $(BUILD_DIR)/menu_settings_shell_contract_test $(MENU_SETTINGS_SHELL_CONTRACT_TEST_SRCS) -lm
	$(BUILD_DIR)/menu_settings_shell_contract_test

test-scene-menu-layout-contract: $(SCENE_MENU_LAYOUT_CONTRACT_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) \
		-I$(SRC_DIR)/tools \
		-I$(CORE_BASE_DIR)/include -I$(CORE_SCENE_DIR)/include $(MESH_PREVIEW_BRIDGE_INCS) -I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include \
		-o $(BUILD_DIR)/scene_menu_layout_contract_test $(SCENE_MENU_LAYOUT_CONTRACT_TEST_SRCS) $(LIBS) -lm
	$(BUILD_DIR)/scene_menu_layout_contract_test

test-quality-profiles-contract: $(QUALITY_PROFILES_CONTRACT_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CSTD) $(WARN) $(DEBUG) \
		-I$(INC_DIR) -I$(SRC_DIR) \
		-o $(BUILD_DIR)/quality_profiles_contract_test $(QUALITY_PROFILES_CONTRACT_TEST_SRCS) -lm
	$(BUILD_DIR)/quality_profiles_contract_test

test-config-loader-contract: $(CONFIG_LOADER_CONTRACT_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CSTD) $(WARN) $(DEBUG) \
		-I$(INC_DIR) -I$(SRC_DIR) \
		-o $(BUILD_DIR)/config_loader_contract_test $(CONFIG_LOADER_CONTRACT_TEST_SRCS) -lm
	$(BUILD_DIR)/config_loader_contract_test

test-sim-runtime-backend-reporting-contract: $(SIM_RUNTIME_BACKEND_REPORTING_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) \
		-I$(CORE_BASE_DIR)/include -I$(CORE_SCENE_DIR)/include -I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include \
		-o $(BUILD_DIR)/sim_runtime_backend_reporting_contract_test $(SIM_RUNTIME_BACKEND_REPORTING_TEST_SRCS) -lm
	$(BUILD_DIR)/sim_runtime_backend_reporting_contract_test

test-sim-runtime-backend-dispatch-contract: $(SIM_RUNTIME_BACKEND_DISPATCH_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) \
		-o $(BUILD_DIR)/sim_runtime_backend_dispatch_contract_test $(SIM_RUNTIME_BACKEND_DISPATCH_TEST_SRCS) -lm
	$(BUILD_DIR)/sim_runtime_backend_dispatch_contract_test

test-preset-io-dimensional-contract: $(PRESET_IO_DIMENSIONAL_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CSTD) $(WARN) $(DEBUG) \
		-I$(INC_DIR) -I$(SRC_DIR) \
		-o $(BUILD_DIR)/preset_io_dimensional_contract_test $(PRESET_IO_DIMENSIONAL_TEST_SRCS) -lm
	$(BUILD_DIR)/preset_io_dimensional_contract_test

test-scene-objects-runtime-contract: $(SCENE_OBJECTS_RUNTIME_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) \
		-o $(BUILD_DIR)/scene_objects_runtime_contract_test $(SCENE_OBJECTS_RUNTIME_TEST_SRCS) -lm
	$(BUILD_DIR)/scene_objects_runtime_contract_test

test-rigid2d-collision-contract: $(RIGID2D_COLLISION_CONTRACT_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) \
		-o $(BUILD_DIR)/rigid2d_collision_contract_test $(RIGID2D_COLLISION_CONTRACT_TEST_SRCS) -lm
	$(BUILD_DIR)/rigid2d_collision_contract_test

test-scene-editor-retained-document-contract: $(SCENE_EDITOR_RETAINED_DOCUMENT_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CSTD) $(WARN) $(DEBUG) \
		-I$(INC_DIR) -I$(SRC_DIR) $(MESH_PREVIEW_BRIDGE_INCS) \
		-o $(BUILD_DIR)/scene_editor_retained_document_contract_test $(SCENE_EDITOR_RETAINED_DOCUMENT_TEST_SRCS) -lm
	$(BUILD_DIR)/scene_editor_retained_document_contract_test

test-scene-editor-scene-library-contract: $(SCENE_EDITOR_SCENE_LIBRARY_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CSTD) $(WARN) $(DEBUG) \
		-I$(INC_DIR) -I$(SRC_DIR) -I$(CORE_BASE_DIR)/include -I$(CORE_SCENE_DIR)/include $(MESH_PREVIEW_BRIDGE_INCS) -I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include \
		-o $(BUILD_DIR)/scene_editor_scene_library_contract_test $(SCENE_EDITOR_SCENE_LIBRARY_TEST_SRCS) -lm
	$(BUILD_DIR)/scene_editor_scene_library_contract_test

test-scene-project-cache-output-status-contract: $(SCENE_PROJECT_CACHE_OUTPUT_STATUS_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CSTD) $(WARN) $(DEBUG) \
		-I$(INC_DIR) -I$(SRC_DIR) -Itests \
		-o $(BUILD_DIR)/scene_project_cache_output_status_contract_test $(SCENE_PROJECT_CACHE_OUTPUT_STATUS_TEST_SRCS) -lm
	$(BUILD_DIR)/scene_project_cache_output_status_contract_test

test-physics-sim-workspace-authoring-host: $(PHYSICS_SIM_WORKSPACE_AUTHORING_HOST_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) \
		-I$(KIT_WORKSPACE_AUTHORING_DIR)/include -I$(CORE_BASE_DIR)/include -I$(CORE_PANE_DIR)/include -I$(KIT_PANE_DIR)/include -I$(KIT_RENDER_DIR)/include -I$(CORE_THEME_DIR)/include -I$(CORE_FONT_DIR)/include \
		-o $(BUILD_DIR)/physics_sim_workspace_authoring_host_test $(PHYSICS_SIM_WORKSPACE_AUTHORING_HOST_TEST_SRCS) $(LIBS)
	$(BUILD_DIR)/physics_sim_workspace_authoring_host_test

test-scene-editor-pane-host-contract: $(SCENE_EDITOR_PANE_HOST_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CSTD) $(WARN) $(DEBUG) \
		-I$(INC_DIR) -I$(SRC_DIR) -I$(CORE_PANE_DIR)/include -I$(KIT_PANE_DIR)/include -I$(KIT_RENDER_DIR)/include -I$(CORE_THEME_DIR)/include -I$(CORE_FONT_DIR)/include -I$(CORE_BASE_DIR)/include \
		-o $(BUILD_DIR)/scene_editor_pane_host_contract_test $(SCENE_EDITOR_PANE_HOST_TEST_SRCS) -lm
	$(BUILD_DIR)/scene_editor_pane_host_contract_test

test-scene-editor-viewport-contract: $(SCENE_EDITOR_VIEWPORT_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CSTD) $(WARN) $(DEBUG) \
		-I$(INC_DIR) -I$(SRC_DIR) -I$(CORE_VIEWPORT2D_DIR)/include -I$(CORE_BASE_DIR)/include \
		-o $(BUILD_DIR)/scene_editor_viewport_contract_test $(SCENE_EDITOR_VIEWPORT_TEST_SRCS) -lm
	$(BUILD_DIR)/scene_editor_viewport_contract_test

test-scene-editor-wind-setup-contract: $(SCENE_EDITOR_WIND_SETUP_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CSTD) $(WARN) $(DEBUG) \
		$(JSON_CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) -I$(CORE_SCENE_DIR)/include $(MESH_PREVIEW_BRIDGE_INCS) -I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include -I$(CORE_BASE_DIR)/include \
		-o $(BUILD_DIR)/scene_editor_wind_setup_contract_test $(SCENE_EDITOR_WIND_SETUP_TEST_SRCS) $(JSON_LIBS) -lm
	$(BUILD_DIR)/scene_editor_wind_setup_contract_test

test-retained-runtime-scene-overlay-geom-contract: $(RETAINED_RUNTIME_SCENE_OVERLAY_GEOM_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) \
		-I$(CORE_BASE_DIR)/include -I$(CORE_SCENE_DIR)/include -I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include \
		-o $(BUILD_DIR)/retained_runtime_scene_overlay_geom_contract_test $(RETAINED_RUNTIME_SCENE_OVERLAY_GEOM_TEST_SRCS) -lm
	$(BUILD_DIR)/retained_runtime_scene_overlay_geom_contract_test

test-retained-runtime-scene-overlay-readout-contract: $(RETAINED_RUNTIME_SCENE_OVERLAY_READOUT_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(VK_RENDERER_SDL_COMPAT_CFLAGS) \
		-I$(CORE_BASE_DIR)/include -I$(CORE_SCENE_DIR)/include -I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include -I$(CORE_VIEWPORT2D_DIR)/include \
		-o $(BUILD_DIR)/retained_runtime_scene_overlay_readout_contract_test $(RETAINED_RUNTIME_SCENE_OVERLAY_READOUT_TEST_SRCS) -lm
	$(BUILD_DIR)/retained_runtime_scene_overlay_readout_contract_test

test-retained-runtime-scene-overlay-space-contract: $(RETAINED_RUNTIME_SCENE_OVERLAY_SPACE_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) \
		-I$(CORE_BASE_DIR)/include -I$(CORE_SCENE_DIR)/include -I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include \
		-o $(BUILD_DIR)/retained_runtime_scene_overlay_space_contract_test $(RETAINED_RUNTIME_SCENE_OVERLAY_SPACE_TEST_SRCS) -lm
	$(BUILD_DIR)/retained_runtime_scene_overlay_space_contract_test

test-scene-runtime-launch-projection-contract: $(SCENE_RUNTIME_LAUNCH_PROJECTION_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) \
		-I$(CORE_BASE_DIR)/include -I$(CORE_SCENE_DIR)/include -I$(CORE_SCENE_COMPILE_DIR)/include \
		-I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include -I$(CORE_IO_DIR)/include \
		-o $(BUILD_DIR)/scene_runtime_launch_projection_contract_test $(SCENE_RUNTIME_LAUNCH_PROJECTION_TEST_SRCS) $(JSON_LIBS) -lm
	$(BUILD_DIR)/scene_runtime_launch_projection_contract_test

test-runtime-scene-3d-truth-contract: $(RUNTIME_SCENE_3D_TRUTH_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) \
		-I$(CORE_BASE_DIR)/include -I$(CORE_SCENE_DIR)/include -I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include \
		-I/opt/homebrew/Cellar/json-c/0.18/include -I/opt/homebrew/Cellar/json-c/0.18/include/json-c \
		-o $(BUILD_DIR)/runtime_scene_3d_truth_contract_test $(RUNTIME_SCENE_3D_TRUTH_TEST_SRCS) $(JSON_LIBS) -lm
	$(BUILD_DIR)/runtime_scene_3d_truth_contract_test

test-runtime-scene-solver-projection-contract: $(RUNTIME_SCENE_SOLVER_PROJECTION_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) \
		-I$(CORE_BASE_DIR)/include -I$(CORE_SCENE_DIR)/include -I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include \
		-I/opt/homebrew/Cellar/json-c/0.18/include -I/opt/homebrew/Cellar/json-c/0.18/include/json-c \
		-o $(BUILD_DIR)/runtime_scene_solver_projection_contract_test $(RUNTIME_SCENE_SOLVER_PROJECTION_TEST_SRCS) $(JSON_LIBS) -lm
	$(BUILD_DIR)/runtime_scene_solver_projection_contract_test

test-runtime-scene-bridge-contract: $(RUNTIME_SCENE_BRIDGE_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) \
		-o $(BUILD_DIR)/runtime_scene_bridge_contract_test $(RUNTIME_SCENE_BRIDGE_TEST_SRCS) $(JSON_LIBS) -lm
	$(BUILD_DIR)/runtime_scene_bridge_contract_test

test-runtime-scene-view-packet-readout-contract: $(RUNTIME_SCENE_VIEW_PACKET_READOUT_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) \
		-I$(CORE_SCENE_VIEW_DIR)/include -I$(CORE_SCENE_VIEW_DIR)/../../shape/external \
		-I$(CORE_SCENE_DIR)/include -I$(CORE_MESH_PREVIEW_DIR)/include -I$(CORE_MESH_ASSET_DIR)/include \
		-I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include -I$(CORE_IO_DIR)/include -I$(CORE_BASE_DIR)/include \
		-o $(BUILD_DIR)/runtime_scene_view_packet_readout_contract_test $(RUNTIME_SCENE_VIEW_PACKET_READOUT_TEST_SRCS) $(JSON_LIBS) -lm
	$(BUILD_DIR)/runtime_scene_view_packet_readout_contract_test

test-runtime-scene-view-metadata-authority-contract: $(RUNTIME_SCENE_VIEW_METADATA_AUTHORITY_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) \
		-I$(CORE_SCENE_VIEW_DIR)/include -I$(CORE_SCENE_VIEW_DIR)/../../shape/external \
		-I$(CORE_SCENE_DIR)/include -I$(CORE_MESH_PREVIEW_DIR)/include -I$(CORE_MESH_ASSET_DIR)/include \
		-I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include -I$(CORE_IO_DIR)/include -I$(CORE_BASE_DIR)/include \
		-o $(BUILD_DIR)/runtime_scene_view_metadata_authority_contract_test $(RUNTIME_SCENE_VIEW_METADATA_AUTHORITY_TEST_SRCS) $(JSON_LIBS) -lm
	$(BUILD_DIR)/runtime_scene_view_metadata_authority_contract_test

test-runtime-mesh-preview-bridge-contract: $(RUNTIME_MESH_PREVIEW_BRIDGE_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -DPHYSICS_SIM_RUNTIME_MESH_PATH_RESOLVER_FORCE_LINUX=1 \
		-I$(CORE_MESH_ASSET_DIR)/include -I$(CORE_MESH_PREVIEW_DIR)/include \
		-I$(CORE_MESH_PREVIEW_DIR)/../../shape/external \
		-I$(CORE_SCENE_DIR)/include -I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include \
		-I$(CORE_IO_DIR)/include -I$(CORE_BASE_DIR)/include \
		-o $(BUILD_DIR)/runtime_mesh_preview_bridge_contract_test $(RUNTIME_MESH_PREVIEW_BRIDGE_TEST_SRCS) $(JSON_LIBS) -lm
	$(BUILD_DIR)/runtime_mesh_preview_bridge_contract_test

test-runtime-mesh-obstacle-proxy-contract: $(RUNTIME_MESH_OBSTACLE_PROXY_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) \
		-I$(CORE_MESH_ASSET_DIR)/include -I$(CORE_MESH_PREVIEW_DIR)/include \
		-I$(CORE_MESH_PREVIEW_DIR)/../../shape/external \
		-I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include \
		-I$(CORE_IO_DIR)/include -I$(CORE_BASE_DIR)/include \
		-o $(BUILD_DIR)/runtime_mesh_obstacle_proxy_contract_test $(RUNTIME_MESH_OBSTACLE_PROXY_TEST_SRCS) $(JSON_LIBS) -lm
	$(BUILD_DIR)/runtime_mesh_obstacle_proxy_contract_test

test-structural-runtime-split-contract: $(STRUCTURAL_RUNTIME_SPLIT_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CSTD) $(WARN) $(DEBUG) \
		-I$(INC_DIR) -I$(SRC_DIR) $(STRUCTURAL_RUNTIME_SPLIT_TEST_PLATFORM_CFLAGS) \
		-o $(BUILD_DIR)/structural_runtime_split_contract_test $(STRUCTURAL_RUNTIME_SPLIT_TEST_SRCS) -lm
	$(BUILD_DIR)/structural_runtime_split_contract_test

# =========================
#  Convenience targets
# =========================
VOLUME_FRAMES_3D_EXPORT_TEST_SRCS := \
	tests/volume_frames_3d_export_contract_test.c \
	$(SRC_DIR)/app/atmospheric/atmospheric_field.c \
	$(SRC_DIR)/export/volume_frames.c \
	$(SRC_DIR)/export/volume_frames_vf3d.c \
	$(SRC_DIR)/export/water_surface_artifacts.c \
	$(SRC_DIR)/app/water_object_coupling.c \
	$(SRC_DIR)/export/export_paths.c \
	$(SRC_DIR)/app/sim_runtime_backend.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_scaffold.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_wind.c \
	$(SRC_DIR)/app/wind_tunnel_3d_analysis.c \
	$(SRC_DIR)/app/wind_tunnel_3d.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_scaffold_views.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_runtime.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_obstacles.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_obstacle_sources.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_oriented_box.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_emitter_shapes.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_emitters.c \
	$(SRC_DIR)/app/sim_runtime_3d_domain.c \
	$(SRC_DIR)/app/sim_runtime_3d_brick_store.c \
	$(SRC_DIR)/app/sim_runtime_3d_footprint.c \
	$(SRC_DIR)/app/sim_runtime_3d_anchor.c \
	$(SRC_DIR)/app/sim_runtime_3d_space.c \
	$(SRC_DIR)/app/sim_runtime_3d_solver.c \
	$(SRC_DIR)/app/sim_runtime_3d_solver_step.c \
	$(SRC_DIR)/app/sim_runtime_3d_solver_core_sim.c \
	$(SRC_DIR)/app/sim_runtime_emitter.c \
	$(SRC_DIR)/app/sim_runtime_obstacle.c \
	$(CORE_SIM_DIR)/src/core_sim.c \
	$(CORE_DATA_DIR)/src/core_data.c \
	$(CORE_PACK_DIR)/src/core_pack.c \
	$(CORE_PACK_DIR)/src/core_pack_vf2d.c \
	$(CORE_PACK_DIR)/src/core_pack_vf3d.c \
	$(CORE_IO_DIR)/src/core_io.c \
	$(CORE_BASE_DIR)/src/core_base.c \
	$(CORE_OBJECT_DIR)/src/core_object.c \
	$(CORE_UNITS_DIR)/src/core_units.c \
	$(CORE_SCENE_DIR)/src/core_scene.c \
	$(TIMER_HUD_DIR)/external/cJSON.c

test-volume-frames-3d-export-contract: $(VOLUME_FRAMES_3D_EXPORT_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) \
		-I$(CORE_BASE_DIR)/include -I$(CORE_IO_DIR)/include -I$(CORE_DATA_DIR)/include \
		-I$(CORE_PACK_DIR)/include -I$(CORE_SCENE_DIR)/include -I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include \
		-o $(BUILD_DIR)/volume_frames_3d_export_contract_test $(VOLUME_FRAMES_3D_EXPORT_TEST_SRCS) $(filter-out -lSDL2 -lSDL2_ttf,$(LIBS))
	$(BUILD_DIR)/volume_frames_3d_export_contract_test

VOLUME_FRAMES_3D_TINY_PARITY_TEST_SRCS := \
	tests/volume_frames_3d_tiny_parity_contract_test.c \
	$(SRC_DIR)/app/atmospheric/atmospheric_field.c \
	$(SRC_DIR)/export/volume_frames.c \
	$(SRC_DIR)/export/volume_frames_vf3d.c \
	$(SRC_DIR)/export/water_surface_artifacts.c \
	$(SRC_DIR)/app/water_object_coupling.c \
	$(SRC_DIR)/export/export_paths.c \
	$(SRC_DIR)/app/sim_runtime_backend.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_scaffold.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_wind.c \
	$(SRC_DIR)/app/wind_tunnel_3d_analysis.c \
	$(SRC_DIR)/app/wind_tunnel_3d.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_scaffold_views.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_runtime.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_obstacles.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_obstacle_sources.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_oriented_box.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_emitter_shapes.c \
	$(SRC_DIR)/app/sim_runtime_backend_3d_emitters.c \
		$(SRC_DIR)/app/sim_runtime_3d_domain.c \
		$(SRC_DIR)/app/sim_runtime_3d_brick_store.c \
		$(SRC_DIR)/app/sim_runtime_3d_footprint.c \
	$(SRC_DIR)/app/sim_runtime_3d_anchor.c \
	$(SRC_DIR)/app/sim_runtime_3d_space.c \
	$(SRC_DIR)/app/sim_runtime_3d_solver.c \
	$(SRC_DIR)/app/sim_runtime_3d_solver_step.c \
	$(SRC_DIR)/app/sim_runtime_3d_solver_core_sim.c \
	$(SRC_DIR)/app/sim_runtime_emitter.c \
	$(SRC_DIR)/app/sim_runtime_obstacle.c \
	$(CORE_SIM_DIR)/src/core_sim.c \
	$(CORE_DATA_DIR)/src/core_data.c \
	$(CORE_PACK_DIR)/src/core_pack.c \
	$(CORE_PACK_DIR)/src/core_pack_vf2d.c \
	$(CORE_PACK_DIR)/src/core_pack_vf3d.c \
	$(CORE_IO_DIR)/src/core_io.c \
	$(CORE_BASE_DIR)/src/core_base.c \
	$(CORE_OBJECT_DIR)/src/core_object.c \
	$(CORE_UNITS_DIR)/src/core_units.c \
	$(CORE_SCENE_DIR)/src/core_scene.c \
	$(TIMER_HUD_DIR)/external/cJSON.c

test-volume-frames-3d-tiny-parity-contract: $(VOLUME_FRAMES_3D_TINY_PARITY_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) \
		-I$(CORE_BASE_DIR)/include -I$(CORE_IO_DIR)/include -I$(CORE_DATA_DIR)/include \
		-I$(CORE_PACK_DIR)/include -I$(CORE_SCENE_DIR)/include -I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include \
		-o $(BUILD_DIR)/volume_frames_3d_tiny_parity_contract_test $(VOLUME_FRAMES_3D_TINY_PARITY_TEST_SRCS) $(filter-out -lSDL2 -lSDL2_ttf,$(LIBS))
	$(BUILD_DIR)/volume_frames_3d_tiny_parity_contract_test

SHARED_THEME_FONT_ADAPTER_TEST_SRCS := \
	tests/shared_theme_font_adapter_test.c \
	src/app/menu/shared_theme_font_adapter.c \
	$(CORE_THEME_DIR)/src/core_theme.c \
	$(CORE_FONT_DIR)/src/core_font.c \
	$(CORE_BASE_DIR)/src/core_base.c

PHYSICS_SIM_UI_BUTTON_CONTRACT_TEST_SRCS := \
	tests/physics_sim_ui_button_contract_test.c \
	src/app/ui/physics_sim_ui_button.c \
	src/app/menu/shared_theme_font_adapter.c \
	$(KIT_UI_DIR)/src/kit_ui_button.c \
	$(KIT_RENDER_DIR)/src/kit_render.c \
	$(KIT_RENDER_DIR)/src/kit_render_backend_null.c \
	$(KIT_RENDER_DIR)/src/kit_render_backend_vk.c \
	$(CORE_THEME_DIR)/src/core_theme.c \
	$(CORE_FONT_DIR)/src/core_font.c \
	$(CORE_BASE_DIR)/src/core_base.c

test-shared-theme-font-adapter: $(SHARED_THEME_FONT_ADAPTER_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) \
		-I$(INC_DIR) -I$(SRC_DIR) \
		-I$(CORE_THEME_DIR)/include -I$(CORE_FONT_DIR)/include -I$(CORE_BASE_DIR)/include \
		-o $(BUILD_DIR)/shared_theme_font_adapter_test $(SHARED_THEME_FONT_ADAPTER_TEST_SRCS) $(filter-out -lSDL2 -lSDL2_ttf,$(LIBS))
	$(BUILD_DIR)/shared_theme_font_adapter_test

test-physics-sim-ui-button-contract: $(PHYSICS_SIM_UI_BUTTON_CONTRACT_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) \
		-I$(INC_DIR) -I$(SRC_DIR) \
		-I$(KIT_UI_DIR)/include -I$(KIT_RENDER_DIR)/include \
		-I$(CORE_THEME_DIR)/include -I$(CORE_FONT_DIR)/include -I$(CORE_BASE_DIR)/include \
		-o $(BUILD_DIR)/physics_sim_ui_button_contract_test $(PHYSICS_SIM_UI_BUTTON_CONTRACT_TEST_SRCS) $(LIBS)
	$(BUILD_DIR)/physics_sim_ui_button_contract_test
