# =========================
#  Compiler and project identity
# =========================
CLANG     ?= clang
FISICS    ?= /Users/calebsv/Desktop/CodeWork/fisiCs/fisics
CC        := $(CLANG)
CSTD      := -std=c11
PKG_CONFIG ?= pkg-config

UNAME_S   := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
CSTD      := -std=c2x
endif

SRC_DIR   := src
INC_DIR   := include
BUILD_DIR := build
TARGET    := physics_sim
DIST_DIR  := dist
CLANG_BUILD_DIR := $(BUILD_DIR)/clang
FISICS_BUILD_DIR := $(BUILD_DIR)/fisics
CLANG_TARGET := $(CLANG_BUILD_DIR)/$(TARGET)
FISICS_TARGET := $(FISICS_BUILD_DIR)/$(TARGET)
PACKAGE_TOOLCHAIN ?= clang
SEMA_SRC := $(SRC_DIR)/import/runtime_scene_solver_projection_domain.c
SEMA_OBJ := $(FISICS_BUILD_DIR)/runtime_scene_solver_projection_domain.o
SEMA_OUT := $(FISICS_BUILD_DIR)/runtime_scene_solver_projection_domain.sema.txt
SEMA_SOLVER_3D_SRC := $(SRC_DIR)/app/sim_runtime_3d_solver_step.c
SEMA_SOLVER_3D_OBJ := $(FISICS_BUILD_DIR)/sim_runtime_3d_solver_step.o
SEMA_SOLVER_3D_OUT := $(FISICS_BUILD_DIR)/sim_runtime_3d_solver_step.sema.txt
SEMA_FLUID2D_SRC := $(SRC_DIR)/physics/fluid2d/fluid2d.c
SEMA_FLUID2D_OBJ := $(FISICS_BUILD_DIR)/fluid2d.o
SEMA_FLUID2D_OUT := $(FISICS_BUILD_DIR)/fluid2d.sema.txt
SEMA_RIGID2D_SRC := $(SRC_DIR)/physics/rigid/rigid2d.c
SEMA_RIGID2D_OBJ := $(FISICS_BUILD_DIR)/rigid2d.o
SEMA_RIGID2D_OUT := $(FISICS_BUILD_DIR)/rigid2d.sema.txt
SEMA_RIGID2D_COLLISION_SRC := $(SRC_DIR)/physics/rigid/rigid2d_collision.c
SEMA_RIGID2D_COLLISION_OBJ := $(FISICS_BUILD_DIR)/rigid2d_collision.o
SEMA_RIGID2D_COLLISION_OUT := $(FISICS_BUILD_DIR)/rigid2d_collision.sema.txt
SEMA_STRUCTURAL_RUNTIME_SRC := $(SRC_DIR)/app/structural/structural_controller_runtime.c
SEMA_STRUCTURAL_RUNTIME_OBJ := $(FISICS_BUILD_DIR)/structural_controller_runtime.o
SEMA_STRUCTURAL_RUNTIME_OUT := $(FISICS_BUILD_DIR)/structural_controller_runtime.sema.txt
SEMA_PARTICLES2D_SRC := $(SRC_DIR)/physics/particles/particles2d.c
SEMA_PARTICLES2D_OBJ := $(FISICS_BUILD_DIR)/particles2d.o
SEMA_PARTICLES2D_OUT := $(FISICS_BUILD_DIR)/particles2d.sema.txt
SEMA_STRUCTURAL_SOLVER_SRC := $(SRC_DIR)/physics/structural/structural_solver.c
SEMA_STRUCTURAL_SOLVER_OBJ := $(FISICS_BUILD_DIR)/structural_solver.o
SEMA_STRUCTURAL_SOLVER_OUT := $(FISICS_BUILD_DIR)/structural_solver.sema.txt
SEMA_ATMOSPHERIC_FIELD_SRC := $(SRC_DIR)/app/atmospheric/atmospheric_field.c
SEMA_ATMOSPHERIC_FIELD_OBJ := $(FISICS_BUILD_DIR)/atmospheric_field.o
SEMA_ATMOSPHERIC_FIELD_OUT := $(FISICS_BUILD_DIR)/atmospheric_field.sema.txt
SEMA_OBJECT_MANAGER_SRC := $(SRC_DIR)/physics/objects/object_manager.c
SEMA_OBJECT_MANAGER_OBJ := $(FISICS_BUILD_DIR)/object_manager.o
SEMA_OBJECT_MANAGER_OUT := $(FISICS_BUILD_DIR)/object_manager.sema.txt
SEMA_SOFT_BODY_SRC := $(SRC_DIR)/physics/soft/soft_body.c
SEMA_SOFT_BODY_OBJ := $(FISICS_BUILD_DIR)/soft_body.o
SEMA_SOFT_BODY_OUT := $(FISICS_BUILD_DIR)/soft_body.sema.txt
SEMA_RUNTIME_FIELDS_2D_SRC := $(SRC_DIR)/app/sim_runtime_backend_2d_runtime_fields.c
SEMA_RUNTIME_FIELDS_2D_OBJ := $(FISICS_BUILD_DIR)/sim_runtime_backend_2d_runtime_fields.o
SEMA_RUNTIME_FIELDS_2D_OUT := $(FISICS_BUILD_DIR)/sim_runtime_backend_2d_runtime_fields.sema.txt
SEMA_RUNTIME_BACKEND_2D_SRC := $(SRC_DIR)/app/sim_runtime_backend_2d.c
SEMA_RUNTIME_BACKEND_2D_OBJ := $(FISICS_BUILD_DIR)/sim_runtime_backend_2d.o
SEMA_RUNTIME_BACKEND_2D_OUT := $(FISICS_BUILD_DIR)/sim_runtime_backend_2d.sema.txt
SEMA_RUNTIME_BACKEND_3D_EMITTERS_SRC := $(SRC_DIR)/app/sim_runtime_backend_3d_emitters.c
SEMA_RUNTIME_BACKEND_3D_EMITTERS_OBJ := $(FISICS_BUILD_DIR)/sim_runtime_backend_3d_emitters.o
SEMA_RUNTIME_BACKEND_3D_EMITTERS_OUT := $(FISICS_BUILD_DIR)/sim_runtime_backend_3d_emitters.sema.txt
SEMA_RUNTIME_BACKEND_3D_RUNTIME_SRC := $(SRC_DIR)/app/sim_runtime_backend_3d_runtime.c
SEMA_RUNTIME_BACKEND_3D_RUNTIME_OBJ := $(FISICS_BUILD_DIR)/sim_runtime_backend_3d_runtime.o
SEMA_RUNTIME_BACKEND_3D_RUNTIME_OUT := $(FISICS_BUILD_DIR)/sim_runtime_backend_3d_runtime.sema.txt
SEMA_RUNTIME_BACKEND_3D_OBSTACLES_SRC := $(SRC_DIR)/app/sim_runtime_backend_3d_obstacles.c
SEMA_RUNTIME_BACKEND_3D_OBSTACLES_OBJ := $(FISICS_BUILD_DIR)/sim_runtime_backend_3d_obstacles.o
SEMA_RUNTIME_BACKEND_3D_OBSTACLES_OUT := $(FISICS_BUILD_DIR)/sim_runtime_backend_3d_obstacles.sema.txt
SEMA_RUNTIME_EMITTER_SRC := $(SRC_DIR)/app/sim_runtime_emitter.c
SEMA_RUNTIME_EMITTER_OBJ := $(FISICS_BUILD_DIR)/sim_runtime_emitter.o
SEMA_RUNTIME_EMITTER_OUT := $(FISICS_BUILD_DIR)/sim_runtime_emitter.sema.txt
SEMA_RUNTIME_OBSTACLE_SRC := $(SRC_DIR)/app/sim_runtime_obstacle.c
SEMA_RUNTIME_OBSTACLE_OBJ := $(FISICS_BUILD_DIR)/sim_runtime_obstacle.o
SEMA_RUNTIME_OBSTACLE_OUT := $(FISICS_BUILD_DIR)/sim_runtime_obstacle.sema.txt

# RL0 release contract.
RELEASE_VERSION_FILE ?= VERSION
RELEASE_VERSION ?= $(strip $(shell cat "$(RELEASE_VERSION_FILE)" 2>/dev/null))
ifeq ($(RELEASE_VERSION),)
RELEASE_VERSION := 0.1.0
endif
RELEASE_CHANNEL ?= stable
RELEASE_PRODUCT_NAME := kinetiC
RELEASE_PROGRAM_KEY := physics_sim
RELEASE_BUNDLE_ID := com.cosm.kinetic
