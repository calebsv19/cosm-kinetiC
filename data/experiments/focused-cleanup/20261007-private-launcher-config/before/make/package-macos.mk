# =========================
#  macOS desktop packaging
# =========================
PHYSICS_SIM_CANONICAL_DESKTOP_SOURCE := /Users/calebsv/Desktop/CodeWork/physics_sim

package-desktop-refresh-authority:
	@actual="$$(pwd -P)"; test "$$actual" = "$(PHYSICS_SIM_CANONICAL_DESKTOP_SOURCE)" || \
		(echo "Desktop refresh denied: canonical source is $(PHYSICS_SIM_CANONICAL_DESKTOP_SOURCE), got $$actual"; exit 1)
	@branch="$$(git branch --show-current)"; test "$$branch" = "main" || \
		(echo "Desktop refresh denied: canonical branch is main, got $$branch"; exit 1)
	@test -z "$$(git status --porcelain)" || \
		(echo "Desktop refresh denied: canonical main worktree must be clean"; exit 1)
	@test "$$(git worktree list --porcelain | /usr/bin/grep -c '^worktree ')" = "1" || \
		(echo "Desktop refresh denied: PhysicsSim has more than one registered worktree"; exit 1)
	@if /usr/bin/pgrep -f '/kinetiC.app/Contents/MacOS/' >/dev/null; then \
		echo "Desktop refresh denied: close the running kinetiC.app first"; exit 1; \
	fi
	@echo "Desktop refresh authority: canonical clean main, one worktree, no running app"

package-desktop: $(PACKAGE_SOURCE_BIN) physics_sim_session_worker
	@echo "Preparing desktop package..."
	@rm -rf "$(PACKAGE_APP_DIR)"
	@mkdir -p "$(PACKAGE_MACOS_DIR)" "$(PACKAGE_RESOURCES_DIR)" "$(PACKAGE_FRAMEWORKS_DIR)"
	@cp "$(PACKAGE_INFO_PLIST_SRC)" "$(PACKAGE_CONTENTS_DIR)/Info.plist"
	@/usr/libexec/PlistBuddy -c "Set :CFBundleIdentifier $(PACKAGE_BUNDLE_ID)" "$(PACKAGE_CONTENTS_DIR)/Info.plist"
	@/usr/libexec/PlistBuddy -c "Set :CFBundleName $(PACKAGE_DISPLAY_NAME)" "$(PACKAGE_CONTENTS_DIR)/Info.plist"
	@/usr/libexec/PlistBuddy -c "Add :CFBundleDisplayName string $(PACKAGE_DISPLAY_NAME)" "$(PACKAGE_CONTENTS_DIR)/Info.plist"
	@/usr/libexec/PlistBuddy -c "Set :CFBundleShortVersionString $(RELEASE_VERSION)" "$(PACKAGE_CONTENTS_DIR)/Info.plist"
	@/usr/libexec/PlistBuddy -c "Add :PhysicsSimPackageProfile string $(PACKAGE_PROFILE)" "$(PACKAGE_CONTENTS_DIR)/Info.plist"
	@/usr/libexec/PlistBuddy -c "Add :PhysicsSimRuntimeNamespace string $(PACKAGE_RUNTIME_NAMESPACE)" "$(PACKAGE_CONTENTS_DIR)/Info.plist"
	@/usr/libexec/PlistBuddy -c "Add :PhysicsSimLogNamespace string $(PACKAGE_LOG_NAMESPACE)" "$(PACKAGE_CONTENTS_DIR)/Info.plist"
	@/usr/libexec/PlistBuddy -c "Add :PhysicsSimBuildLabel string $(PACKAGE_BUILD_LABEL)" "$(PACKAGE_CONTENTS_DIR)/Info.plist"
	@cp "$(PACKAGE_SOURCE_BIN)" "$(PACKAGE_MACOS_DIR)/physics-sim-bin"
	@cp physics_sim_session_worker "$(PACKAGE_MACOS_DIR)/physics_sim_session_worker"
	@mkdir -p "$(PACKAGE_RESOURCES_DIR)/scripts/agent_session"
	@cp scripts/physics_sim_session.py "$(PACKAGE_RESOURCES_DIR)/scripts/"
	@cp scripts/agent_session/*.py "$(PACKAGE_RESOURCES_DIR)/scripts/agent_session/"
	@PACKAGE_DEP_SEARCH_ROOTS="$(TARGET_DEP_SEARCH_ROOTS)" "$(PACKAGE_DYLIB_BUNDLER)" "$(PACKAGE_MACOS_DIR)/physics_sim_session_worker" "$(PACKAGE_FRAMEWORKS_DIR)"
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
	@/usr/bin/find "$(PACKAGE_FRAMEWORKS_DIR)" -type f -name '*.dylib' \
		-exec /usr/bin/codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" --timestamp=none {} \;
	@/usr/bin/codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" --timestamp=none "$(PACKAGE_MACOS_DIR)/physics-sim-bin"
	@/usr/bin/codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" --timestamp=none "$(PACKAGE_MACOS_DIR)/physics_sim_session_worker"
	@/usr/bin/codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" --timestamp=none "$(PACKAGE_MACOS_DIR)/physics-sim-launcher"
	@if [ "$(PACKAGE_EMBED_BUILD_IDENTITY)" = "1" ]; then \
		python3 "$(MEW1_TOOL)" write-identity \
			--output "$(PACKAGE_RESOURCES_DIR)/build_identity.json" \
			--source-root "$(CURDIR)" \
			--binary "$(PACKAGE_MACOS_DIR)/physics-sim-bin" \
			--profile "$(PACKAGE_PROFILE)" \
			--program physics_sim \
			--product kinetiC \
			--version "$(RELEASE_VERSION)" \
			--architecture "$(TARGET_ARCH)" \
			--toolchain "$(PACKAGE_TOOLCHAIN)" \
			--build-label "$(PACKAGE_BUILD_LABEL)"; \
	fi
	@/usr/bin/codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" --timestamp=none "$(PACKAGE_APP_DIR)"
	@echo "Desktop package ready: $(PACKAGE_APP_DIR)"

package-desktop-smoke: package-desktop
	@test -x "$(PACKAGE_MACOS_DIR)/physics-sim-launcher" || (echo "Missing launcher"; exit 1)
	@test -x "$(PACKAGE_MACOS_DIR)/physics-sim-bin" || (echo "Missing app binary"; exit 1)
	@test -f "$(PACKAGE_FRAMEWORKS_DIR)/libvulkan.1.dylib" || (echo "Missing bundled libvulkan.1.dylib"; exit 1)
	@test -f "$(PACKAGE_FRAMEWORKS_DIR)/libMoltenVK.dylib" || (echo "Missing bundled libMoltenVK.dylib"; exit 1)
	@test -f "$(PACKAGE_CONTENTS_DIR)/Info.plist" || (echo "Missing Info.plist"; exit 1)
	@test "$$('/usr/libexec/PlistBuddy' -c 'Print :CFBundleIdentifier' "$(PACKAGE_CONTENTS_DIR)/Info.plist")" = "$(PACKAGE_BUNDLE_ID)" || (echo "Bundle id mismatch"; exit 1)
	@test "$$('/usr/libexec/PlistBuddy' -c 'Print :CFBundleDisplayName' "$(PACKAGE_CONTENTS_DIR)/Info.plist")" = "$(PACKAGE_DISPLAY_NAME)" || (echo "Bundle display name mismatch"; exit 1)
	@test "$$('/usr/libexec/PlistBuddy' -c 'Print :CFBundleShortVersionString' "$(PACKAGE_CONTENTS_DIR)/Info.plist")" = "$(RELEASE_VERSION)" || (echo "Bundle version mismatch"; exit 1)
	@test "$$('/usr/libexec/PlistBuddy' -c 'Print :PhysicsSimPackageProfile' "$(PACKAGE_CONTENTS_DIR)/Info.plist")" = "$(PACKAGE_PROFILE)" || (echo "Package profile mismatch"; exit 1)
	@test "$$('/usr/libexec/PlistBuddy' -c 'Print :PhysicsSimRuntimeNamespace' "$(PACKAGE_CONTENTS_DIR)/Info.plist")" = "$(PACKAGE_RUNTIME_NAMESPACE)" || (echo "Runtime namespace mismatch"; exit 1)
	@test "$$('/usr/libexec/PlistBuddy' -c 'Print :PhysicsSimLogNamespace' "$(PACKAGE_CONTENTS_DIR)/Info.plist")" = "$(PACKAGE_LOG_NAMESPACE)" || (echo "Log namespace mismatch"; exit 1)
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
	@test -x "$(PACKAGE_MACOS_DIR)/physics_sim_session_worker"
	@test -f "$(PACKAGE_RESOURCES_DIR)/scripts/agent_session/service.py"
	@echo "package-desktop-smoke passed."

package-desktop-self-test: package-desktop-smoke
	@python3 tools/packaging/validate_macos_session.py --app "$(PACKAGE_APP_DIR)"
	@support="$$(mktemp -d "$(CURDIR)/$(BUILD_DIR)/package-self-test.XXXXXX")"; \
	PHYSICS_SIM_APP_SUPPORT_DIR="$$support" PHYSICS_SIM_LOG_DIR="$$support/logs" \
		"$(PACKAGE_MACOS_DIR)/physics-sim-launcher" --self-test; result=$$?; \
	if [ "$$result" = 0 ]; then rm -rf "$$support"; else echo "package-desktop self-test failed; evidence: $$support"; fi; \
	exit "$$result"
	@echo "package-desktop-self-test passed."

package-desktop-copy-desktop: package-desktop-refresh-authority package-desktop
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

package-desktop-refresh: package-desktop-refresh-authority package-desktop
	@mkdir -p "$(dir $(DESKTOP_APP_DIR))"
	@rm -rf "$(DESKTOP_APP_DIR)"
	@/usr/bin/ditto "$(PACKAGE_APP_DIR)" "$(DESKTOP_APP_DIR)"
	@echo "Refreshed $(PACKAGE_APP_NAME) at $(DESKTOP_APP_DIR)"

package-desktop-main-edit:
	@test -f "$(MEW1_TOOL)" || (echo "Missing shared MEW1 helper: $(MEW1_TOOL)"; exit 1)
	@before="$$(python3 "$(MEW1_TOOL)" fingerprint --repo "$(CURDIR)")"; \
	$(MAKE) package-desktop-smoke \
		DIST_DIR="$(MAIN_EDIT_DIST_DIR)" \
		PACKAGE_APP_NAME="$(MAIN_EDIT_APP_NAME)" \
		PACKAGE_DISPLAY_NAME="$(MAIN_EDIT_DISPLAY_NAME)" \
		PACKAGE_BUNDLE_ID="$(MAIN_EDIT_BUNDLE_ID)" \
		PACKAGE_PROFILE="$(MAIN_EDIT_PROFILE)" \
		PACKAGE_RUNTIME_NAMESPACE="$(MAIN_EDIT_RUNTIME_NAMESPACE)" \
		PACKAGE_LOG_NAMESPACE="$(MAIN_EDIT_LOG_NAMESPACE)" \
		PACKAGE_BUILD_LABEL="$(MAIN_EDIT_BUILD_LABEL)" \
		PACKAGE_EMBED_BUILD_IDENTITY=1 || exit 1; \
	after="$$(python3 "$(MEW1_TOOL)" fingerprint --repo "$(CURDIR)")"; \
	if [ "$$before" != "$$after" ]; then \
		rm -rf "$(MAIN_EDIT_APP_DIR)"; \
		echo "Source changed during Main Edit packaging; discarded generated package."; \
		exit 1; \
	fi
	@echo "Main Edit desktop package ready: $(MAIN_EDIT_APP_DIR)"

package-desktop-main-edit-self-test: package-desktop-main-edit
	@test "$$('/usr/libexec/PlistBuddy' -c 'Print :CFBundleIdentifier' "$(MAIN_EDIT_APP_DIR)/Contents/Info.plist")" = "$(MAIN_EDIT_BUNDLE_ID)"
	@test "$$('/usr/libexec/PlistBuddy' -c 'Print :CFBundleDisplayName' "$(MAIN_EDIT_APP_DIR)/Contents/Info.plist")" = "$(MAIN_EDIT_DISPLAY_NAME)"
	@test "$$('/usr/libexec/PlistBuddy' -c 'Print :PhysicsSimPackageProfile' "$(MAIN_EDIT_APP_DIR)/Contents/Info.plist")" = "$(MAIN_EDIT_PROFILE)"
	@test "$$('/usr/libexec/PlistBuddy' -c 'Print :PhysicsSimRuntimeNamespace' "$(MAIN_EDIT_APP_DIR)/Contents/Info.plist")" = "$(MAIN_EDIT_RUNTIME_NAMESPACE)"
	@test "$$('/usr/libexec/PlistBuddy' -c 'Print :PhysicsSimLogNamespace' "$(MAIN_EDIT_APP_DIR)/Contents/Info.plist")" = "$(MAIN_EDIT_LOG_NAMESPACE)"
	@test -f "$(MAIN_EDIT_APP_DIR)/Contents/Resources/build_identity.json"
	@python3 "$(MEW1_TOOL)" verify-identity \
		--identity "$(MAIN_EDIT_APP_DIR)/Contents/Resources/build_identity.json" \
		--source-root "$(CURDIR)" \
		--binary "$(MAIN_EDIT_APP_DIR)/Contents/MacOS/physics-sim-bin" \
		--profile "$(MAIN_EDIT_PROFILE)" \
		--program physics_sim \
		--product kinetiC \
		--version "$(RELEASE_VERSION)"
	@set -e; \
	fake_home="$(CURDIR)/$(MAIN_EDIT_SELF_TEST_DIR)/home"; \
	rm -rf "$$fake_home"; \
	mkdir -p "$$fake_home"; \
	HOME="$$fake_home" "$(MAIN_EDIT_APP_DIR)/Contents/MacOS/physics-sim-launcher" --self-test; \
	config="$$(HOME="$$fake_home" "$(MAIN_EDIT_APP_DIR)/Contents/MacOS/physics-sim-launcher" --print-config)"; \
	printf '%s\n' "$$config"; \
	printf '%s\n' "$$config" | grep -Fqx "PACKAGE_PROFILE=$(MAIN_EDIT_PROFILE)"; \
	printf '%s\n' "$$config" | grep -Fqx "RUNTIME_NAMESPACE=$(MAIN_EDIT_RUNTIME_NAMESPACE)"; \
	printf '%s\n' "$$config" | grep -Fqx "LOG_NAMESPACE=$(MAIN_EDIT_LOG_NAMESPACE)"; \
	printf '%s\n' "$$config" | grep -Fqx "BUILD_LABEL=$(MAIN_EDIT_BUILD_LABEL)"; \
	printf '%s\n' "$$config" | grep -Fqx "PHYSICS_SIM_RUNTIME_DIR=$$fake_home/Library/Application Support/$(MAIN_EDIT_RUNTIME_NAMESPACE)/runtime"; \
	printf '%s\n' "$$config" | grep -Fqx "LOG_FILE=$$fake_home/Library/Logs/$(MAIN_EDIT_LOG_NAMESPACE)/launcher.log"
	@/usr/bin/codesign --verify --deep --strict "$(MAIN_EDIT_APP_DIR)"
	@echo "package-desktop-main-edit-self-test passed."

package-desktop-main-edit-refresh: package-desktop-main-edit-self-test
	@test "$(MAIN_EDIT_DESKTOP_APP_DIR)" != "$(DESKTOP_APP_DIR)" || (echo "Refusing canonical Desktop destination"; exit 1)
	@mkdir -p "$(dir $(MAIN_EDIT_PROCESS_RECEIPT))"
	@python3 "$(MEW1_TOOL)" process-audit --match "$(MAIN_EDIT_DISPLAY_NAME)" --path "$(MAIN_EDIT_DESKTOP_APP_DIR)" > "$(MAIN_EDIT_PROCESS_RECEIPT)"
	@if grep -Fq '"running": true' "$(MAIN_EDIT_PROCESS_RECEIPT)"; then \
		echo "Refusing to replace a running $(MAIN_EDIT_APP_NAME); process receipt: $(MAIN_EDIT_PROCESS_RECEIPT)"; \
		exit 1; \
	fi
	@mkdir -p "$$(dirname "$(MAIN_EDIT_DESKTOP_APP_DIR)")"
	@rm -rf "$(MAIN_EDIT_DESKTOP_APP_DIR)"
	@/usr/bin/ditto "$(MAIN_EDIT_APP_DIR)" "$(MAIN_EDIT_DESKTOP_APP_DIR)"
	@echo "Refreshed $(MAIN_EDIT_APP_NAME) at $(MAIN_EDIT_DESKTOP_APP_DIR)"

main-edit-package-contract-checks:
	@./tests/run_main_edit_package_contract_checks.sh
