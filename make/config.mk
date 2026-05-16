# =========================
#  Compiler and project identity
# =========================
CC        := cc
CSTD      := -std=c11
PKG_CONFIG ?= pkg-config

SRC_DIR   := src
INC_DIR   := include
BUILD_DIR := build
TARGET    := physics_sim
DIST_DIR  := dist

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
