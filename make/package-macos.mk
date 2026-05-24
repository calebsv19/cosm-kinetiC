# =========================
#  macOS desktop packaging
# =========================
package-desktop: $(PACKAGE_SOURCE_BIN)
	@echo "Preparing desktop package..."
	@rm -rf "$(PACKAGE_APP_DIR)"
	@mkdir -p "$(PACKAGE_MACOS_DIR)" "$(PACKAGE_RESOURCES_DIR)" "$(PACKAGE_FRAMEWORKS_DIR)"
	@cp "$(PACKAGE_INFO_PLIST_SRC)" "$(PACKAGE_CONTENTS_DIR)/Info.plist"
	@cp "$(PACKAGE_SOURCE_BIN)" "$(PACKAGE_MACOS_DIR)/physics-sim-bin"
	@cp "$(PACKAGE_LAUNCHER_SRC)" "$(PACKAGE_MACOS_DIR)/physics-sim-launcher"
	@PACKAGE_DEP_SEARCH_ROOTS="$(TARGET_DEP_SEARCH_ROOTS)" "$(PACKAGE_DYLIB_BUNDLER)" "$(PACKAGE_MACOS_DIR)/physics-sim-bin" "$(PACKAGE_FRAMEWORKS_DIR)"
	@chmod +x "$(PACKAGE_MACOS_DIR)/physics-sim-bin" "$(PACKAGE_MACOS_DIR)/physics-sim-launcher"
	@cp -R config "$(PACKAGE_RESOURCES_DIR)/"
	@mkdir -p "$(PACKAGE_RESOURCES_DIR)/data/runtime" "$(PACKAGE_RESOURCES_DIR)/data/snapshots"
	@if [ -d "$(SHARED_ASSETS_DIR)/fonts" ]; then \
		mkdir -p "$(PACKAGE_RESOURCES_DIR)/shared/assets"; \
		cp -R "$(SHARED_ASSETS_DIR)/fonts" "$(PACKAGE_RESOURCES_DIR)/shared/assets/"; \
	fi
	@if [ -f "$(PACKAGE_APP_ICON_SRC)" ]; then \
		cp "$(PACKAGE_APP_ICON_SRC)" "$(PACKAGE_BUNDLED_ICON_PATH)"; \
		echo "Bundled app icon from $(PACKAGE_APP_ICON_SRC)"; \
	elif [ -d "$(PACKAGE_APP_ICONSET_SRC)" ]; then \
		iconutil -c icns "$(PACKAGE_APP_ICONSET_SRC)" -o "$(PACKAGE_BUNDLED_ICON_PATH)"; \
		echo "Bundled app icon from $(PACKAGE_APP_ICONSET_SRC)"; \
	else \
		echo "Warning: no app icon input found; continuing without bundled AppIcon.icns"; \
	fi
	@mkdir -p "$(PACKAGE_RESOURCES_DIR)/vk_renderer" "$(PACKAGE_RESOURCES_DIR)/shaders"
	@cp -R "$(VK_RENDERER_DIR)/shaders" "$(PACKAGE_RESOURCES_DIR)/vk_renderer/"
	@cp -R "$(VK_RENDERER_DIR)/shaders/." "$(PACKAGE_RESOURCES_DIR)/shaders/"
	@for dylib in $$(/usr/bin/find "$(PACKAGE_FRAMEWORKS_DIR)" -type f -name '*.dylib' 2>/dev/null); do \
		/usr/bin/codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" --timestamp=none "$$dylib"; \
	done
	@/usr/bin/codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" --timestamp=none "$(PACKAGE_MACOS_DIR)/physics-sim-bin"
	@/usr/bin/codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" --timestamp=none "$(PACKAGE_MACOS_DIR)/physics-sim-launcher"
	@/usr/bin/codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" --timestamp=none "$(PACKAGE_APP_DIR)"
	@echo "Desktop package ready: $(PACKAGE_APP_DIR)"

package-desktop-smoke: package-desktop
	@test -x "$(PACKAGE_MACOS_DIR)/physics-sim-launcher" || (echo "Missing launcher"; exit 1)
	@test -x "$(PACKAGE_MACOS_DIR)/physics-sim-bin" || (echo "Missing app binary"; exit 1)
	@test -f "$(PACKAGE_FRAMEWORKS_DIR)/libvulkan.1.dylib" || (echo "Missing bundled libvulkan.1.dylib"; exit 1)
	@test -f "$(PACKAGE_FRAMEWORKS_DIR)/libMoltenVK.dylib" || (echo "Missing bundled libMoltenVK.dylib"; exit 1)
	@test -f "$(PACKAGE_CONTENTS_DIR)/Info.plist" || (echo "Missing Info.plist"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/config/app.json" || (echo "Missing config/app.json"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/config/custom_preset.txt" || (echo "Missing config/custom_preset.txt"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/config/structural_scene.txt" || (echo "Missing config/structural_scene.txt"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/config/objects/Hexagon.asset.json" || (echo "Missing bundled shape assets"; exit 1)
	@test -d "$(PACKAGE_RESOURCES_DIR)/data/runtime" || (echo "Missing runtime dir"; exit 1)
	@test -d "$(PACKAGE_RESOURCES_DIR)/data/snapshots" || (echo "Missing snapshots dir"; exit 1)
	@if [ -f "$(PACKAGE_APP_ICON_SRC)" ] || [ -d "$(PACKAGE_APP_ICONSET_SRC)" ]; then \
		test -f "$(PACKAGE_BUNDLED_ICON_PATH)" || (echo "Missing bundled AppIcon.icns"; exit 1); \
	fi
	@test -f "$(PACKAGE_RESOURCES_DIR)/vk_renderer/shaders/textured.vert.spv" || (echo "Missing bundled vk_renderer shader"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/shaders/textured.vert.spv" || (echo "Missing bundled runtime shader"; exit 1)
	@echo "package-desktop-smoke passed."

package-desktop-self-test: package-desktop-smoke
	@"$(PACKAGE_MACOS_DIR)/physics-sim-launcher" --self-test || (echo "package-desktop self-test failed."; exit 1)
	@echo "package-desktop-self-test passed."

package-desktop-copy-desktop: package-desktop
	@mkdir -p "$(dir $(DESKTOP_APP_DIR))"
	@rm -rf "$(DESKTOP_APP_DIR)"
	@/usr/bin/ditto "$(PACKAGE_APP_DIR)" "$(DESKTOP_APP_DIR)"
	@echo "Copied $(PACKAGE_APP_NAME) to $(DESKTOP_APP_DIR)"

package-desktop-sync: package-desktop-copy-desktop
	@echo "Desktop package synchronized: $(DESKTOP_APP_DIR)"

package-desktop-open: package-desktop
	@open "$(PACKAGE_APP_DIR)"

package-desktop-remove:
	@rm -rf "$(PACKAGE_APP_DIR)"
	@echo "Removed desktop package: $(PACKAGE_APP_DIR)"

package-desktop-refresh: package-desktop
	@mkdir -p "$(dir $(DESKTOP_APP_DIR))"
	@rm -rf "$(DESKTOP_APP_DIR)"
	@/usr/bin/ditto "$(PACKAGE_APP_DIR)" "$(DESKTOP_APP_DIR)"
	@echo "Refreshed $(PACKAGE_APP_NAME) at $(DESKTOP_APP_DIR)"
