# =========================
#  Build rules
# =========================
all: $(TARGET)

clang-build: $(CLANG_TARGET)

fisics-build: $(FISICS_TARGET)

toolchain-contract:
	@echo "Program root:      /Users/calebsv/Desktop/CodeWork/physics_sim"
	@echo "Clang binary:      $(CLANG_TARGET)"
	@echo "fisiCs binary:     $(FISICS_TARGET)"
	@echo "Package default:   $(PACKAGE_SOURCE_BIN)"
	@echo "clang build:       object build under $(BUILD_DIR)/ then link -> $(CLANG_TARGET)"
	@echo "fisiCs build:      object build under $(FISICS_BUILD_DIR)/ with per-file compat includes, then link -> $(FISICS_TARGET)"
	@echo "dump-sema source:  $(SEMA_SRC)"
	@echo "dump-sema output:  $(SEMA_OUT)"
	@echo "dump-sema 3d src:  $(SEMA_SOLVER_3D_SRC)"
	@echo "dump-sema 3d out:  $(SEMA_SOLVER_3D_OUT)"
	@echo "dump-sema 2d src:  $(SEMA_FLUID2D_SRC)"
	@echo "dump-sema 2d out:  $(SEMA_FLUID2D_OUT)"
	@echo "dump-sema rigid:   $(SEMA_RIGID2D_SRC)"
	@echo "dump-sema rigid o: $(SEMA_RIGID2D_OUT)"
	@echo "dump-sema coll src:$(SEMA_RIGID2D_COLLISION_SRC)"
	@echo "dump-sema coll out:$(SEMA_RIGID2D_COLLISION_OUT)"
	@echo "dump-sema struct src:$(SEMA_STRUCTURAL_RUNTIME_SRC)"
	@echo "dump-sema struct out:$(SEMA_STRUCTURAL_RUNTIME_OUT)"
	@echo "dump-sema parts src:$(SEMA_PARTICLES2D_SRC)"
	@echo "dump-sema parts out:$(SEMA_PARTICLES2D_OUT)"
	@echo "dump-sema struct solver src:$(SEMA_STRUCTURAL_SOLVER_SRC)"
	@echo "dump-sema struct solver out:$(SEMA_STRUCTURAL_SOLVER_OUT)"
	@echo "dump-sema atmos src:$(SEMA_ATMOSPHERIC_FIELD_SRC)"
	@echo "dump-sema atmos out:$(SEMA_ATMOSPHERIC_FIELD_OUT)"
	@echo "dump-sema objects src:$(SEMA_OBJECT_MANAGER_SRC)"
	@echo "dump-sema objects out:$(SEMA_OBJECT_MANAGER_OUT)"
	@echo "dump-sema soft src:$(SEMA_SOFT_BODY_SRC)"
	@echo "dump-sema soft out:$(SEMA_SOFT_BODY_OUT)"
	@echo "dump-sema 2d fields src:$(SEMA_RUNTIME_FIELDS_2D_SRC)"
	@echo "dump-sema 2d fields out:$(SEMA_RUNTIME_FIELDS_2D_OUT)"
	@echo "dump-sema 2d backend src:$(SEMA_RUNTIME_BACKEND_2D_SRC)"
	@echo "dump-sema 2d backend out:$(SEMA_RUNTIME_BACKEND_2D_OUT)"
	@echo "dump-sema 3d emit src:$(SEMA_RUNTIME_BACKEND_3D_EMITTERS_SRC)"
	@echo "dump-sema 3d emit out:$(SEMA_RUNTIME_BACKEND_3D_EMITTERS_OUT)"
	@echo "dump-sema 3d run src: $(SEMA_RUNTIME_BACKEND_3D_RUNTIME_SRC)"
	@echo "dump-sema 3d run out: $(SEMA_RUNTIME_BACKEND_3D_RUNTIME_OUT)"
	@echo "dump-sema 3d obs src: $(SEMA_RUNTIME_BACKEND_3D_OBSTACLES_SRC)"
	@echo "dump-sema 3d obs out: $(SEMA_RUNTIME_BACKEND_3D_OBSTACLES_OUT)"
	@echo "dump-sema emitter src:$(SEMA_RUNTIME_EMITTER_SRC)"
	@echo "dump-sema emitter out:$(SEMA_RUNTIME_EMITTER_OUT)"
	@echo "dump-sema obstacle src:$(SEMA_RUNTIME_OBSTACLE_SRC)"
	@echo "dump-sema obstacle out:$(SEMA_RUNTIME_OBSTACLE_OUT)"

dump-sema: $(SEMA_SRC) | $(FISICS_BUILD_DIR)
	@mkdir -p $(dir $(SEMA_OUT))
	FISICS_MAX_PROCS=0 $(FISICS) $(FISICS_FLAGS) $(FISICS_CFLAGS) --dump-sema -c $(SEMA_SRC) -o $(SEMA_OBJ) > $(SEMA_OUT) 2>&1
	@echo "Wrote semantic dump to $(SEMA_OUT)"

dump-sema-runtime-3d-solver-step: $(SEMA_SOLVER_3D_SRC) | $(FISICS_BUILD_DIR)
	@mkdir -p $(dir $(SEMA_SOLVER_3D_OUT))
	FISICS_MAX_PROCS=0 $(FISICS) $(FISICS_FLAGS) $(FISICS_CFLAGS) --dump-sema -c $(SEMA_SOLVER_3D_SRC) -o $(SEMA_SOLVER_3D_OBJ) > $(SEMA_SOLVER_3D_OUT) 2>&1
	@echo "Wrote semantic dump to $(SEMA_SOLVER_3D_OUT)"

dump-sema-fluid2d: $(SEMA_FLUID2D_SRC) | $(FISICS_BUILD_DIR)
	@mkdir -p $(dir $(SEMA_FLUID2D_OUT))
	FISICS_MAX_PROCS=0 $(FISICS) $(FISICS_FLAGS) $(FISICS_CFLAGS) --dump-sema -c $(SEMA_FLUID2D_SRC) -o $(SEMA_FLUID2D_OBJ) > $(SEMA_FLUID2D_OUT) 2>&1
	@echo "Wrote semantic dump to $(SEMA_FLUID2D_OUT)"

dump-sema-rigid2d: $(SEMA_RIGID2D_SRC) | $(FISICS_BUILD_DIR)
	@mkdir -p $(dir $(SEMA_RIGID2D_OUT))
	FISICS_MAX_PROCS=0 $(FISICS) $(FISICS_FLAGS) $(FISICS_CFLAGS) --dump-sema -c $(SEMA_RIGID2D_SRC) -o $(SEMA_RIGID2D_OBJ) > $(SEMA_RIGID2D_OUT) 2>&1
	@echo "Wrote semantic dump to $(SEMA_RIGID2D_OUT)"

dump-sema-rigid2d-collision: $(SEMA_RIGID2D_COLLISION_SRC) | $(FISICS_BUILD_DIR)
	@mkdir -p $(dir $(SEMA_RIGID2D_COLLISION_OUT))
	FISICS_MAX_PROCS=0 $(FISICS) $(FISICS_FLAGS) $(FISICS_CFLAGS) --dump-sema -c $(SEMA_RIGID2D_COLLISION_SRC) -o $(SEMA_RIGID2D_COLLISION_OBJ) > $(SEMA_RIGID2D_COLLISION_OUT) 2>&1
	@echo "Wrote semantic dump to $(SEMA_RIGID2D_COLLISION_OUT)"

dump-sema-structural-runtime: $(SEMA_STRUCTURAL_RUNTIME_SRC) | $(FISICS_BUILD_DIR)
	@mkdir -p $(dir $(SEMA_STRUCTURAL_RUNTIME_OUT))
	FISICS_MAX_PROCS=0 $(FISICS) $(FISICS_FLAGS) $(FISICS_CFLAGS) --dump-sema -c $(SEMA_STRUCTURAL_RUNTIME_SRC) -o $(SEMA_STRUCTURAL_RUNTIME_OBJ) > $(SEMA_STRUCTURAL_RUNTIME_OUT) 2>&1
	@echo "Wrote semantic dump to $(SEMA_STRUCTURAL_RUNTIME_OUT)"

dump-sema-particles2d: $(SEMA_PARTICLES2D_SRC) | $(FISICS_BUILD_DIR)
	@mkdir -p $(dir $(SEMA_PARTICLES2D_OUT))
	FISICS_MAX_PROCS=0 $(FISICS) $(FISICS_FLAGS) $(FISICS_CFLAGS) --dump-sema -c $(SEMA_PARTICLES2D_SRC) -o $(SEMA_PARTICLES2D_OBJ) > $(SEMA_PARTICLES2D_OUT) 2>&1
	@echo "Wrote semantic dump to $(SEMA_PARTICLES2D_OUT)"

dump-sema-structural-solver: $(SEMA_STRUCTURAL_SOLVER_SRC) | $(FISICS_BUILD_DIR)
	@mkdir -p $(dir $(SEMA_STRUCTURAL_SOLVER_OUT))
	FISICS_MAX_PROCS=0 $(FISICS) $(FISICS_FLAGS) $(FISICS_CFLAGS) --dump-sema -c $(SEMA_STRUCTURAL_SOLVER_SRC) -o $(SEMA_STRUCTURAL_SOLVER_OBJ) > $(SEMA_STRUCTURAL_SOLVER_OUT) 2>&1
	@echo "Wrote semantic dump to $(SEMA_STRUCTURAL_SOLVER_OUT)"

dump-sema-atmospheric-field: $(SEMA_ATMOSPHERIC_FIELD_SRC) | $(FISICS_BUILD_DIR)
	@mkdir -p $(dir $(SEMA_ATMOSPHERIC_FIELD_OUT))
	FISICS_MAX_PROCS=0 $(FISICS) $(FISICS_FLAGS) $(FISICS_CFLAGS) --dump-sema -c $(SEMA_ATMOSPHERIC_FIELD_SRC) -o $(SEMA_ATMOSPHERIC_FIELD_OBJ) > $(SEMA_ATMOSPHERIC_FIELD_OUT) 2>&1
	@echo "Wrote semantic dump to $(SEMA_ATMOSPHERIC_FIELD_OUT)"

dump-sema-object-manager: $(SEMA_OBJECT_MANAGER_SRC) | $(FISICS_BUILD_DIR)
	@mkdir -p $(dir $(SEMA_OBJECT_MANAGER_OUT))
	FISICS_MAX_PROCS=0 $(FISICS) $(FISICS_FLAGS) $(FISICS_CFLAGS) --dump-sema -c $(SEMA_OBJECT_MANAGER_SRC) -o $(SEMA_OBJECT_MANAGER_OBJ) > $(SEMA_OBJECT_MANAGER_OUT) 2>&1
	@echo "Wrote semantic dump to $(SEMA_OBJECT_MANAGER_OUT)"

dump-sema-soft-body: $(SEMA_SOFT_BODY_SRC) | $(FISICS_BUILD_DIR)
	@mkdir -p $(dir $(SEMA_SOFT_BODY_OUT))
	FISICS_MAX_PROCS=0 $(FISICS) $(FISICS_FLAGS) $(FISICS_CFLAGS) --dump-sema -c $(SEMA_SOFT_BODY_SRC) -o $(SEMA_SOFT_BODY_OBJ) > $(SEMA_SOFT_BODY_OUT) 2>&1
	@echo "Wrote semantic dump to $(SEMA_SOFT_BODY_OUT)"

dump-sema-runtime-fields-2d: $(SEMA_RUNTIME_FIELDS_2D_SRC) | $(FISICS_BUILD_DIR)
	@mkdir -p $(dir $(SEMA_RUNTIME_FIELDS_2D_OUT))
	FISICS_MAX_PROCS=0 $(FISICS) $(FISICS_FLAGS) $(FISICS_CFLAGS) --dump-sema -c $(SEMA_RUNTIME_FIELDS_2D_SRC) -o $(SEMA_RUNTIME_FIELDS_2D_OBJ) > $(SEMA_RUNTIME_FIELDS_2D_OUT) 2>&1
	@echo "Wrote semantic dump to $(SEMA_RUNTIME_FIELDS_2D_OUT)"

dump-sema-runtime-backend-2d: $(SEMA_RUNTIME_BACKEND_2D_SRC) | $(FISICS_BUILD_DIR)
	@mkdir -p $(dir $(SEMA_RUNTIME_BACKEND_2D_OUT))
	FISICS_MAX_PROCS=0 $(FISICS) $(FISICS_FLAGS) $(FISICS_CFLAGS) --dump-sema -c $(SEMA_RUNTIME_BACKEND_2D_SRC) -o $(SEMA_RUNTIME_BACKEND_2D_OBJ) > $(SEMA_RUNTIME_BACKEND_2D_OUT) 2>&1
	@echo "Wrote semantic dump to $(SEMA_RUNTIME_BACKEND_2D_OUT)"

dump-sema-runtime-backend-3d-emitters: $(SEMA_RUNTIME_BACKEND_3D_EMITTERS_SRC) | $(FISICS_BUILD_DIR)
	@mkdir -p $(dir $(SEMA_RUNTIME_BACKEND_3D_EMITTERS_OUT))
	FISICS_MAX_PROCS=0 $(FISICS) $(FISICS_FLAGS) $(FISICS_CFLAGS) --dump-sema -c $(SEMA_RUNTIME_BACKEND_3D_EMITTERS_SRC) -o $(SEMA_RUNTIME_BACKEND_3D_EMITTERS_OBJ) > $(SEMA_RUNTIME_BACKEND_3D_EMITTERS_OUT) 2>&1
	@echo "Wrote semantic dump to $(SEMA_RUNTIME_BACKEND_3D_EMITTERS_OUT)"

dump-sema-runtime-backend-3d-runtime: $(SEMA_RUNTIME_BACKEND_3D_RUNTIME_SRC) | $(FISICS_BUILD_DIR)
	@mkdir -p $(dir $(SEMA_RUNTIME_BACKEND_3D_RUNTIME_OUT))
	FISICS_MAX_PROCS=0 $(FISICS) $(FISICS_FLAGS) $(FISICS_CFLAGS) --dump-sema -c $(SEMA_RUNTIME_BACKEND_3D_RUNTIME_SRC) -o $(SEMA_RUNTIME_BACKEND_3D_RUNTIME_OBJ) > $(SEMA_RUNTIME_BACKEND_3D_RUNTIME_OUT) 2>&1
	@echo "Wrote semantic dump to $(SEMA_RUNTIME_BACKEND_3D_RUNTIME_OUT)"

dump-sema-runtime-backend-3d-obstacles: $(SEMA_RUNTIME_BACKEND_3D_OBSTACLES_SRC) | $(FISICS_BUILD_DIR)
	@mkdir -p $(dir $(SEMA_RUNTIME_BACKEND_3D_OBSTACLES_OUT))
	FISICS_MAX_PROCS=0 $(FISICS) $(FISICS_FLAGS) $(FISICS_CFLAGS) --dump-sema -c $(SEMA_RUNTIME_BACKEND_3D_OBSTACLES_SRC) -o $(SEMA_RUNTIME_BACKEND_3D_OBSTACLES_OBJ) > $(SEMA_RUNTIME_BACKEND_3D_OBSTACLES_OUT) 2>&1
	@echo "Wrote semantic dump to $(SEMA_RUNTIME_BACKEND_3D_OBSTACLES_OUT)"

dump-sema-runtime-emitter: $(SEMA_RUNTIME_EMITTER_SRC) | $(FISICS_BUILD_DIR)
	@mkdir -p $(dir $(SEMA_RUNTIME_EMITTER_OUT))
	FISICS_MAX_PROCS=0 $(FISICS) $(FISICS_FLAGS) $(FISICS_CFLAGS) --dump-sema -c $(SEMA_RUNTIME_EMITTER_SRC) -o $(SEMA_RUNTIME_EMITTER_OBJ) > $(SEMA_RUNTIME_EMITTER_OUT) 2>&1
	@echo "Wrote semantic dump to $(SEMA_RUNTIME_EMITTER_OUT)"

dump-sema-runtime-obstacle: $(SEMA_RUNTIME_OBSTACLE_SRC) | $(FISICS_BUILD_DIR)
	@mkdir -p $(dir $(SEMA_RUNTIME_OBSTACLE_OUT))
	FISICS_MAX_PROCS=0 $(FISICS) $(FISICS_FLAGS) $(FISICS_CFLAGS) --dump-sema -c $(SEMA_RUNTIME_OBSTACLE_SRC) -o $(SEMA_RUNTIME_OBSTACLE_OBJ) > $(SEMA_RUNTIME_OBSTACLE_OUT) 2>&1
	@echo "Wrote semantic dump to $(SEMA_RUNTIME_OBSTACLE_OUT)"

$(CLANG_BUILD_DIR):
	@mkdir -p $@

$(FISICS_BUILD_DIR):
	@mkdir -p $@

$(CLANG_TARGET): $(OBJS) | $(CLANG_BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

$(TARGET): $(CLANG_TARGET)
	cp $(CLANG_TARGET) $(TARGET)

$(FISICS_TARGET): $(FISICS_OBJS) | $(FISICS_BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CLANG) $(LDFLAGS) -o $@ $(FISICS_OBJS) $(LIBS) $(FISICS_MEMCHECK_LINK_LIBS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(FISICS_BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	FISICS_MAX_PROCS=0 $(FISICS) $(FISICS_FLAGS) $(FISICS_CFLAGS) $(FISICS_COMPILE_FLAGS) -c $< -o $@

$(BUILD_DIR)/timer_hud/%.o: $(TIMER_HUD_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(FISICS_BUILD_DIR)/timer_hud/%.o: $(TIMER_HUD_DIR)/src/%.c
	@mkdir -p $(dir $@)
	FISICS_MAX_PROCS=0 $(FISICS) $(FISICS_FLAGS) $(FISICS_CFLAGS) $(FISICS_COMPILE_FLAGS) -c $< -o $@

$(BUILD_DIR)/timer_hud_external/%.o: $(TIMER_HUD_DIR)/external/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(FISICS_BUILD_DIR)/timer_hud_external/%.o: $(TIMER_HUD_DIR)/external/%.c
	@mkdir -p $(dir $@)
	FISICS_MAX_PROCS=0 $(FISICS) $(FISICS_FLAGS) $(FISICS_CFLAGS) $(FISICS_COMPILE_FLAGS) -c $< -o $@

$(BUILD_DIR)/vk_renderer/%.o: $(VK_RENDERER_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(FISICS_BUILD_DIR)/vk_renderer/%.o: $(VK_RENDERER_DIR)/src/%.c
	@mkdir -p $(dir $@)
	FISICS_MAX_PROCS=0 $(FISICS) $(FISICS_FLAGS) $(FISICS_CFLAGS) $(FISICS_COMPILE_FLAGS) -c $< -o $@

$(BUILD_DIR)/kit_workspace_authoring/%.o: $(KIT_WORKSPACE_AUTHORING_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(FISICS_BUILD_DIR)/kit_workspace_authoring/%.o: $(KIT_WORKSPACE_AUTHORING_DIR)/src/%.c
	@mkdir -p $(dir $@)
	FISICS_MAX_PROCS=0 $(FISICS) $(FISICS_FLAGS) $(FISICS_CFLAGS) $(FISICS_COMPILE_FLAGS) -c $< -o $@

$(BUILD_DIR)/core_base/%.o: $(CORE_BASE_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(FISICS_BUILD_DIR)/core_base/%.o: $(CORE_BASE_DIR)/src/%.c
	@mkdir -p $(dir $@)
	FISICS_MAX_PROCS=0 $(FISICS) $(FISICS_FLAGS) $(FISICS_CFLAGS) $(FISICS_COMPILE_FLAGS) -c $< -o $@

$(BUILD_DIR)/core_io/%.o: $(CORE_IO_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(FISICS_BUILD_DIR)/core_io/%.o: $(CORE_IO_DIR)/src/%.c
	@mkdir -p $(dir $@)
	FISICS_MAX_PROCS=0 $(FISICS) $(FISICS_FLAGS) $(FISICS_CFLAGS) $(FISICS_COMPILE_FLAGS) -c $< -o $@

$(BUILD_DIR)/core_data/%.o: $(CORE_DATA_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(FISICS_BUILD_DIR)/core_data/%.o: $(CORE_DATA_DIR)/src/%.c
	@mkdir -p $(dir $@)
	FISICS_MAX_PROCS=0 $(FISICS) $(FISICS_FLAGS) $(FISICS_CFLAGS) $(FISICS_COMPILE_FLAGS) -c $< -o $@

$(BUILD_DIR)/core_pack/%.o: $(CORE_PACK_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(FISICS_BUILD_DIR)/core_pack/%.o: $(CORE_PACK_DIR)/src/%.c
	@mkdir -p $(dir $@)
	FISICS_MAX_PROCS=0 $(FISICS) $(FISICS_FLAGS) $(FISICS_CFLAGS) $(FISICS_COMPILE_FLAGS) -c $< -o $@

$(BUILD_DIR)/core_scene/%.o: $(CORE_SCENE_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(FISICS_BUILD_DIR)/core_scene/%.o: $(CORE_SCENE_DIR)/src/%.c
	@mkdir -p $(dir $@)
	FISICS_MAX_PROCS=0 $(FISICS) $(FISICS_FLAGS) $(FISICS_CFLAGS) $(FISICS_COMPILE_FLAGS) -c $< -o $@

$(BUILD_DIR)/core_scene_compile/%.o: $(CORE_SCENE_COMPILE_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(FISICS_BUILD_DIR)/core_scene_compile/%.o: $(CORE_SCENE_COMPILE_DIR)/src/%.c
	@mkdir -p $(dir $@)
	FISICS_MAX_PROCS=0 $(FISICS) $(FISICS_FLAGS) $(FISICS_CFLAGS) $(FISICS_COMPILE_FLAGS) -c $< -o $@

$(BUILD_DIR)/core_scene_view/%.o: $(CORE_SCENE_VIEW_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(FISICS_BUILD_DIR)/core_scene_view/%.o: $(CORE_SCENE_VIEW_DIR)/src/%.c
	@mkdir -p $(dir $@)
	FISICS_MAX_PROCS=0 $(FISICS) $(FISICS_FLAGS) $(FISICS_CFLAGS) $(FISICS_COMPILE_FLAGS) -c $< -o $@

$(BUILD_DIR)/core_mesh_asset/%.o: $(CORE_MESH_ASSET_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(FISICS_BUILD_DIR)/core_mesh_asset/%.o: $(CORE_MESH_ASSET_DIR)/src/%.c
	@mkdir -p $(dir $@)
	FISICS_MAX_PROCS=0 $(FISICS) $(FISICS_FLAGS) $(FISICS_CFLAGS) $(FISICS_COMPILE_FLAGS) -c $< -o $@

$(BUILD_DIR)/core_mesh_preview/%.o: $(CORE_MESH_PREVIEW_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(FISICS_BUILD_DIR)/core_mesh_preview/%.o: $(CORE_MESH_PREVIEW_DIR)/src/%.c
	@mkdir -p $(dir $@)
	FISICS_MAX_PROCS=0 $(FISICS) $(FISICS_FLAGS) $(FISICS_CFLAGS) $(FISICS_COMPILE_FLAGS) -c $< -o $@

$(BUILD_DIR)/core_object/%.o: $(CORE_OBJECT_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(FISICS_BUILD_DIR)/core_object/%.o: $(CORE_OBJECT_DIR)/src/%.c
	@mkdir -p $(dir $@)
	FISICS_MAX_PROCS=0 $(FISICS) $(FISICS_FLAGS) $(FISICS_CFLAGS) $(FISICS_COMPILE_FLAGS) -c $< -o $@

$(BUILD_DIR)/core_units/%.o: $(CORE_UNITS_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(FISICS_BUILD_DIR)/core_units/%.o: $(CORE_UNITS_DIR)/src/%.c
	@mkdir -p $(dir $@)
	FISICS_MAX_PROCS=0 $(FISICS) $(FISICS_FLAGS) $(FISICS_CFLAGS) $(FISICS_COMPILE_FLAGS) -c $< -o $@

$(BUILD_DIR)/core_viewport2d/%.o: $(CORE_VIEWPORT2D_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/core_screen_pick/%.o: $(CORE_SCREEN_PICK_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(FISICS_BUILD_DIR)/core_viewport2d/%.o: $(CORE_VIEWPORT2D_DIR)/src/%.c
	@mkdir -p $(dir $@)
	FISICS_MAX_PROCS=0 $(FISICS) $(FISICS_FLAGS) $(FISICS_CFLAGS) $(FISICS_COMPILE_FLAGS) -c $< -o $@

$(BUILD_DIR)/core_pane/%.o: $(CORE_PANE_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(FISICS_BUILD_DIR)/core_pane/%.o: $(CORE_PANE_DIR)/src/%.c
	@mkdir -p $(dir $@)
	FISICS_MAX_PROCS=0 $(FISICS) $(FISICS_FLAGS) $(FISICS_CFLAGS) $(FISICS_COMPILE_FLAGS) -c $< -o $@

$(BUILD_DIR)/core_sim/%.o: $(CORE_SIM_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(FISICS_BUILD_DIR)/core_sim/%.o: $(CORE_SIM_DIR)/src/%.c
	@mkdir -p $(dir $@)
	FISICS_MAX_PROCS=0 $(FISICS) $(FISICS_FLAGS) $(FISICS_CFLAGS) $(FISICS_COMPILE_FLAGS) -c $< -o $@

$(BUILD_DIR)/core_theme/%.o: $(CORE_THEME_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(FISICS_BUILD_DIR)/core_theme/%.o: $(CORE_THEME_DIR)/src/%.c
	@mkdir -p $(dir $@)
	FISICS_MAX_PROCS=0 $(FISICS) $(FISICS_FLAGS) $(FISICS_CFLAGS) $(FISICS_COMPILE_FLAGS) -c $< -o $@

$(BUILD_DIR)/core_font/%.o: $(CORE_FONT_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(FISICS_BUILD_DIR)/core_font/%.o: $(CORE_FONT_DIR)/src/%.c
	@mkdir -p $(dir $@)
	FISICS_MAX_PROCS=0 $(FISICS) $(FISICS_FLAGS) $(FISICS_CFLAGS) $(FISICS_COMPILE_FLAGS) -c $< -o $@

$(BUILD_DIR)/core_headless_job/%.o: $(CORE_HEADLESS_JOB_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(FISICS_BUILD_DIR)/core_headless_job/%.o: $(CORE_HEADLESS_JOB_DIR)/src/%.c
	@mkdir -p $(dir $@)
	FISICS_MAX_PROCS=0 $(FISICS) $(FISICS_FLAGS) $(FISICS_CFLAGS) $(FISICS_COMPILE_FLAGS) -c $< -o $@

$(BUILD_DIR)/kit_viz/%.o: $(KIT_VIZ_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(FISICS_BUILD_DIR)/kit_viz/%.o: $(KIT_VIZ_DIR)/src/%.c
	@mkdir -p $(dir $@)
	FISICS_MAX_PROCS=0 $(FISICS) $(FISICS_FLAGS) $(FISICS_CFLAGS) $(FISICS_COMPILE_FLAGS) -c $< -o $@

$(BUILD_DIR)/kit_render/%.o: $(KIT_RENDER_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(FISICS_BUILD_DIR)/kit_render/%.o: $(KIT_RENDER_DIR)/src/%.c
	@mkdir -p $(dir $@)
	FISICS_MAX_PROCS=0 $(FISICS) $(FISICS_FLAGS) $(FISICS_CFLAGS) $(FISICS_COMPILE_FLAGS) -c $< -o $@

$(BUILD_DIR)/kit_pane/%.o: $(KIT_PANE_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(FISICS_BUILD_DIR)/kit_pane/%.o: $(KIT_PANE_DIR)/src/%.c
	@mkdir -p $(dir $@)
	FISICS_MAX_PROCS=0 $(FISICS) $(FISICS_FLAGS) $(FISICS_CFLAGS) $(FISICS_COMPILE_FLAGS) -c $< -o $@

$(BUILD_DIR)/kit_ui/%.o: $(KIT_UI_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(FISICS_BUILD_DIR)/kit_ui/%.o: $(KIT_UI_DIR)/src/%.c
	@mkdir -p $(dir $@)
	FISICS_MAX_PROCS=0 $(FISICS) $(FISICS_FLAGS) $(FISICS_CFLAGS) $(FISICS_COMPILE_FLAGS) -c $< -o $@
