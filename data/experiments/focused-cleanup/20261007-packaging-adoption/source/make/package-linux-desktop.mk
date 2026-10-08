# =========================
#  Linux desktop app packaging
# =========================
LINUX_DESKTOP_PLATFORM ?= linux-x86_64
LINUX_DESKTOP_PACKAGE_EPOCH ?= 0
LINUX_DESKTOP_PACKAGE_CLASS := desktop_app_linux
LINUX_DESKTOP_ARTIFACT_ROLE := desktop_app
LINUX_DESKTOP_RUNTIME := linux_gui
LINUX_DESKTOP_BASENAME := $(RELEASE_PRODUCT_NAME)-$(RELEASE_VERSION)-$(LINUX_DESKTOP_PLATFORM)-desktop-$(RELEASE_CHANNEL)
LINUX_DESKTOP_DIR := $(RELEASE_DIR)/$(LINUX_DESKTOP_BASENAME)
LINUX_DESKTOP_BIN_DIR := $(LINUX_DESKTOP_DIR)/bin
LINUX_DESKTOP_RESOURCES_DIR := $(LINUX_DESKTOP_DIR)/resources
LINUX_DESKTOP_SHARE_DIR := $(LINUX_DESKTOP_DIR)/share
LINUX_DESKTOP_LAUNCHER_SRC := tools/packaging/linux/physics-sim-launcher
LINUX_DESKTOP_ENTRY_SRC := tools/packaging/linux/kinetic.desktop
LINUX_DESKTOP_ICON_SRC := tools/packaging/linux/icons/kinetic.svg
LINUX_DESKTOP_INSTALLER_SRC := tools/packaging/linux/install-desktop-entry.sh
LINUX_DESKTOP_INSTALLER_HELPER_SRC := tools/packaging/linux/install-desktop-entry.py
LINUX_DESKTOP_ENTRY := $(LINUX_DESKTOP_SHARE_DIR)/applications/kinetic.desktop
LINUX_DESKTOP_ICON := $(LINUX_DESKTOP_SHARE_DIR)/icons/hicolor/scalable/apps/kinetic.svg
LINUX_DESKTOP_INSTALLER := $(LINUX_DESKTOP_SHARE_DIR)/install-desktop-entry.sh
LINUX_DESKTOP_MANIFEST_JSON := $(LINUX_DESKTOP_DIR)/manifest.json
LINUX_DESKTOP_PACKAGE_MANIFEST := $(LINUX_DESKTOP_DIR)/package_manifest.json
LINUX_DESKTOP_ARCHIVE := $(RELEASE_DIR)/$(LINUX_DESKTOP_BASENAME).tar.gz
LINUX_DESKTOP_SHA256 := $(LINUX_DESKTOP_ARCHIVE).sha256
LINUX_DESKTOP_SELF_TEST_RUNTIME_DIR ?= $(abspath $(RELEASE_DIR)/linux-desktop-self-test/runtime)
LINUX_DESKTOP_SELF_TEST_STATE_DIR ?= $(abspath $(RELEASE_DIR)/linux-desktop-self-test/state)

.PHONY: package-linux-desktop-contract package-linux-desktop-clean package-linux-desktop-host-check package-linux-desktop package-linux-desktop-self-test package-linux-desktop-determinism-test

package-linux-desktop-contract:
	@echo "Linux desktop package contract"
	@echo "  package class: $(LINUX_DESKTOP_PACKAGE_CLASS)"
	@echo "  artifact role: $(LINUX_DESKTOP_ARTIFACT_ROLE)"
	@echo "  runtime:       $(LINUX_DESKTOP_RUNTIME)"
	@echo "  version:       $(RELEASE_VERSION)"
	@echo "  platform:      $(LINUX_DESKTOP_PLATFORM)"
	@echo "  stage dir:     $(LINUX_DESKTOP_DIR)"
	@echo "  archive:       $(LINUX_DESKTOP_ARCHIVE)"
	@echo "  launcher:      bin/physics-sim-launcher"
	@echo "  binary:        bin/physics-sim-bin"
	@echo "  desktop entry: share/applications/kinetic.desktop"
	@echo "  icon:          share/icons/hicolor/scalable/apps/kinetic.svg"
	@echo "  installer:     share/install-desktop-entry.sh"

package-linux-desktop-clean:
	@python3 -B scripts/package_outputs.py --root "$(RELEASE_DIR)" --directory "$(LINUX_DESKTOP_DIR)" --file "$(LINUX_DESKTOP_ARCHIVE)" --file "$(LINUX_DESKTOP_SHA256)" --directory "$(RELEASE_DIR)/linux-desktop-self-test"
	@echo "No existing Linux desktop artifacts selected for removal"

package-linux-desktop-host-check:
	@if [ "$$(uname -s)" != "Linux" ]; then \
		echo "package-linux-desktop must run on a Linux host; use the Linux PC handoff lane for proof"; \
		exit 2; \
	fi
	@if [ "$(LINUX_DESKTOP_PLATFORM)" != "linux-x86_64" ]; then \
		echo "unsupported Linux desktop platform: $(LINUX_DESKTOP_PLATFORM)"; \
		exit 2; \
	fi

package-linux-desktop: package-linux-desktop-host-check visual-harness
	+@python3 -B scripts/package_transaction.py --root "$(RELEASE_DIR)" \
		--directory "LINUX_DESKTOP_DIR=$(LINUX_DESKTOP_DIR)" --file "LINUX_DESKTOP_ARCHIVE=$(LINUX_DESKTOP_ARCHIVE)" --file "LINUX_DESKTOP_SHA256=$(LINUX_DESKTOP_SHA256)" \
		--map "LINUX_DESKTOP_BIN_DIR=$(LINUX_DESKTOP_BIN_DIR)" --map "LINUX_DESKTOP_RESOURCES_DIR=$(LINUX_DESKTOP_RESOURCES_DIR)" --map "LINUX_DESKTOP_SHARE_DIR=$(LINUX_DESKTOP_SHARE_DIR)" \
		--map "LINUX_DESKTOP_ENTRY=$(LINUX_DESKTOP_ENTRY)" --map "LINUX_DESKTOP_ICON=$(LINUX_DESKTOP_ICON)" --map "LINUX_DESKTOP_INSTALLER=$(LINUX_DESKTOP_INSTALLER)" \
		--map "LINUX_DESKTOP_MANIFEST_JSON=$(LINUX_DESKTOP_MANIFEST_JSON)" --map "LINUX_DESKTOP_PACKAGE_MANIFEST=$(LINUX_DESKTOP_PACKAGE_MANIFEST)" \
		--identity "program=$(RELEASE_VERSION)" --identity "platform=$(LINUX_DESKTOP_PLATFORM)" --identity "epoch=$(LINUX_DESKTOP_PACKAGE_EPOCH)" --identity "channel=$(RELEASE_CHANNEL)" --identity "program-key=$(RELEASE_PROGRAM_KEY)" --identity "product=$(RELEASE_PRODUCT_NAME)" --identity "class=$(LINUX_DESKTOP_PACKAGE_CLASS)" --identity "role=$(LINUX_DESKTOP_ARTIFACT_ROLE)" --identity "runtime=$(LINUX_DESKTOP_RUNTIME)" \
		--input "$(CLANG_TARGET)" --input "$(SHARED_ASSETS_DIR)/fonts" --input "$(VK_RENDERER_DIR)/shaders" \
		--input "$(LINUX_DESKTOP_LAUNCHER_SRC)" --input "$(LINUX_DESKTOP_ENTRY_SRC)" --input "$(LINUX_DESKTOP_ICON_SRC)" --input "$(LINUX_DESKTOP_INSTALLER_SRC)" --input makefile --input make --input scripts --input tools/packaging --input config --input docs --input README.md --input VERSION --input WORKER_VERSION \
		--tool "$(SHELL)" --tool tar --tool sha256sum --tool cp --tool chmod --tool find --tool sort --tool gzip --tool python3 \
		-- $(MAKE) -f makefile _package-linux-desktop-assemble

.PHONY: _package-linux-desktop-assemble
_package-linux-desktop-assemble:
	@echo "Preparing Linux desktop package..."
	@python3 -B scripts/package_outputs.py --root "$(RELEASE_DIR)" --directory "$(LINUX_DESKTOP_DIR)" --file "$(LINUX_DESKTOP_ARCHIVE)" --file "$(LINUX_DESKTOP_SHA256)" --declare
	@mkdir -p "$(LINUX_DESKTOP_BIN_DIR)" "$(LINUX_DESKTOP_RESOURCES_DIR)" "$(dir $(LINUX_DESKTOP_ENTRY))" "$(dir $(LINUX_DESKTOP_ICON))" "$(LINUX_DESKTOP_RESOURCES_DIR)/data/runtime" "$(LINUX_DESKTOP_RESOURCES_DIR)/data/snapshots"
	@cp "$(CLANG_TARGET)" "$(LINUX_DESKTOP_BIN_DIR)/physics-sim-bin"
	@cp tools/packaging/runtime-config.sh "$(LINUX_DESKTOP_RESOURCES_DIR)/runtime-config.sh"
	@cp "$(LINUX_DESKTOP_LAUNCHER_SRC)" "$(LINUX_DESKTOP_BIN_DIR)/physics-sim-launcher"
	@cp "$(LINUX_DESKTOP_ENTRY_SRC)" "$(LINUX_DESKTOP_ENTRY)"
	@cp "$(LINUX_DESKTOP_ICON_SRC)" "$(LINUX_DESKTOP_ICON)"
	@cp "$(LINUX_DESKTOP_INSTALLER_SRC)" "$(LINUX_DESKTOP_INSTALLER)"
	@cp "$(LINUX_DESKTOP_INSTALLER_HELPER_SRC)" "$(LINUX_DESKTOP_SHARE_DIR)/install-desktop-entry.py"
	@chmod +x "$(LINUX_DESKTOP_BIN_DIR)/physics-sim-bin" "$(LINUX_DESKTOP_BIN_DIR)/physics-sim-launcher" "$(LINUX_DESKTOP_INSTALLER)"
	@cp -R config "$(LINUX_DESKTOP_RESOURCES_DIR)/"
	@if [ -d "$(SHARED_ASSETS_DIR)/fonts" ]; then \
		mkdir -p "$(LINUX_DESKTOP_RESOURCES_DIR)/shared/assets"; \
		cp -R "$(SHARED_ASSETS_DIR)/fonts" "$(LINUX_DESKTOP_RESOURCES_DIR)/shared/assets/"; \
	fi
	@mkdir -p "$(LINUX_DESKTOP_RESOURCES_DIR)/vk_renderer" "$(LINUX_DESKTOP_RESOURCES_DIR)/shaders"
	@cp -R "$(VK_RENDERER_DIR)/shaders" "$(LINUX_DESKTOP_RESOURCES_DIR)/vk_renderer/"
	@cp -R "$(VK_RENDERER_DIR)/shaders/." "$(LINUX_DESKTOP_RESOURCES_DIR)/shaders/"
	@printf '{\n' > "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '  "schema_version": "codework-desktop-package/v1",\n' >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '  "package_class": "%s",\n' "$(LINUX_DESKTOP_PACKAGE_CLASS)" >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '  "artifact_role": "%s",\n' "$(LINUX_DESKTOP_ARTIFACT_ROLE)" >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '  "runtime": "%s",\n' "$(LINUX_DESKTOP_RUNTIME)" >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '  "program": "%s",\n' "$(RELEASE_PROGRAM_KEY)" >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '  "product": "%s",\n' "$(RELEASE_PRODUCT_NAME)" >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '  "version": "%s",\n' "$(RELEASE_VERSION)" >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '  "platform": "%s",\n' "$(LINUX_DESKTOP_PLATFORM)" >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '  "entrypoint": "bin/physics-sim-launcher",\n' >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '  "desktop_entry": "share/applications/kinetic.desktop",\n' >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '  "icon": "share/icons/hicolor/scalable/apps/kinetic.svg",\n' >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '  "desktop_installer": "share/install-desktop-entry.sh",\n' >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '  "runtime_dependencies": ["glibc", "libgcc_s", "libm", "SDL2", "SDL2_ttf", "json-c", "vulkan-loader", "vulkan-driver"]\n' >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '}\n' >> "$(LINUX_DESKTOP_MANIFEST_JSON)"
	@printf '{\n' > "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  "schema_version": "codework_package_manifest_v1",\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  "package_class": "%s",\n' "$(LINUX_DESKTOP_PACKAGE_CLASS)" >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  "package_role": "%s",\n' "$(LINUX_DESKTOP_ARTIFACT_ROLE)" >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  "runtime": "%s",\n' "$(LINUX_DESKTOP_RUNTIME)" >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  "program": "%s",\n' "$(RELEASE_PROGRAM_KEY)" >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  "product": "%s",\n' "$(RELEASE_PRODUCT_NAME)" >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  "version": "%s",\n' "$(RELEASE_VERSION)" >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  "platform": "%s",\n' "$(LINUX_DESKTOP_PLATFORM)" >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  "entrypoints": {\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '    "desktop_launcher": "bin/physics-sim-launcher",\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '    "runtime_binary": "bin/physics-sim-bin"\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  },\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  "desktop_integration": {\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '    "desktop_entry": "share/applications/kinetic.desktop",\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '    "icon": "share/icons/hicolor/scalable/apps/kinetic.svg",\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '    "installer": "share/install-desktop-entry.sh"\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  },\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  "self_test": {\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '    "type": "command",\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '    "argv": ["bin/physics-sim-launcher", "--self-test"]\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  },\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '  "runtime_dependencies": ["glibc", "libgcc_s", "libm", "SDL2", "SDL2_ttf", "json-c", "vulkan-loader", "vulkan-driver"]\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '}\n' >> "$(LINUX_DESKTOP_PACKAGE_MANIFEST)"
	@printf '# kinetiC Linux desktop package\n\n' > "$(LINUX_DESKTOP_DIR)/README.md"
	@printf 'Private proof package for the PhysicsSim windowed Linux GUI.\n\n' >> "$(LINUX_DESKTOP_DIR)/README.md"
	@printf 'Run `bin/physics-sim-launcher --self-test` after unpacking. Launching the GUI requires a real desktop session with SDL2 and Vulkan runtime support.\n' >> "$(LINUX_DESKTOP_DIR)/README.md"
	@printf 'Optional per-user desktop installation requires Python 3: `share/install-desktop-entry.sh --plan`, then `share/install-desktop-entry.sh`. Interrupted attempts are retained; use `--recover ID` to resume the exact attempt.\n' >> "$(LINUX_DESKTOP_DIR)/README.md"
	@cd "$(RELEASE_DIR)" && find "$(LINUX_DESKTOP_BASENAME)" -print0 | LC_ALL=C sort -z | tar --null --no-recursion --files-from - --format=posix --pax-option=exthdr.name=%d/PaxHeaders/%f,delete=atime,delete=ctime --mtime="@$(LINUX_DESKTOP_PACKAGE_EPOCH)" --owner=0 --group=0 --numeric-owner -cf - | gzip -n > "$(abspath $(LINUX_DESKTOP_ARCHIVE))"
	@cd "$(RELEASE_DIR)" && sha256sum "$(notdir $(LINUX_DESKTOP_ARCHIVE))" > "$(notdir $(LINUX_DESKTOP_SHA256))"
	@echo "Linux desktop package ready: $(LINUX_DESKTOP_ARCHIVE)"

package-linux-desktop-self-test: package-linux-desktop
	+@python3 -B scripts/package_proof.py --root "$(RELEASE_DIR)" --name linux-desktop-self-test --input "$(LINUX_DESKTOP_DIR)" --input "$(LINUX_DESKTOP_ARCHIVE)" --input "$(LINUX_DESKTOP_SHA256)" --map "LINUX_DESKTOP_PROOF_DIR=." --map "LINUX_DESKTOP_SELF_TEST_RUNTIME_DIR=runtime" --map "LINUX_DESKTOP_SELF_TEST_STATE_DIR=state" -- $(MAKE) -f makefile _package-linux-desktop-proof

.PHONY: _package-linux-desktop-proof
_package-linux-desktop-proof:
	@test -n "$(PACKAGE_PROOF_DIR)" || (echo "Use the public package proof target"; exit 2)
	@$(MAKE) test-physics-sim-file-picker
	@test -x "$(LINUX_DESKTOP_BIN_DIR)/physics-sim-launcher" || (echo "Missing Linux launcher"; exit 1)
	@test -x "$(LINUX_DESKTOP_BIN_DIR)/physics-sim-bin" || (echo "Missing app binary"; exit 1)
	@test -f "$(LINUX_DESKTOP_ENTRY)" || (echo "Missing Linux desktop entry"; exit 1)
	@test -f "$(LINUX_DESKTOP_ICON)" || (echo "Missing Linux desktop icon"; exit 1)
	@test -x "$(LINUX_DESKTOP_INSTALLER)" || (echo "Missing Linux desktop installer"; exit 1)
	@test -f "$(LINUX_DESKTOP_SHARE_DIR)/install-desktop-entry.py" || (echo "Missing Linux desktop installer helper"; exit 1)
	@test -f "$(LINUX_DESKTOP_MANIFEST_JSON)" || (echo "Missing manifest.json"; exit 1)
	@test -f "$(LINUX_DESKTOP_PACKAGE_MANIFEST)" || (echo "Missing package_manifest.json"; exit 1)
	@test -f "$(LINUX_DESKTOP_RESOURCES_DIR)/config/app.json" || (echo "Missing config/app.json"; exit 1)
	@test -f "$(LINUX_DESKTOP_RESOURCES_DIR)/config/custom_preset.txt" || (echo "Missing config/custom_preset.txt"; exit 1)
	@test -f "$(LINUX_DESKTOP_RESOURCES_DIR)/config/structural_scene.txt" || (echo "Missing config/structural_scene.txt"; exit 1)
	@test -f "$(LINUX_DESKTOP_RESOURCES_DIR)/config/objects/Hexagon.asset.json" || (echo "Missing bundled shape assets"; exit 1)
	@test -f "$(LINUX_DESKTOP_RESOURCES_DIR)/vk_renderer/shaders/textured.vert.spv" || (echo "Missing bundled vk_renderer shader"; exit 1)
	@test -f "$(LINUX_DESKTOP_RESOURCES_DIR)/shaders/textured.vert.spv" || (echo "Missing bundled runtime shader"; exit 1)
	@test -f "$(LINUX_DESKTOP_ARCHIVE)" || (echo "Missing Linux desktop archive"; exit 1)
	@test -f "$(LINUX_DESKTOP_SHA256)" || (echo "Missing Linux desktop checksum"; exit 1)
	@mkdir -p "$(LINUX_DESKTOP_PROOF_DIR)/unpack" "$(LINUX_DESKTOP_PROOF_DIR)/home" "$(LINUX_DESKTOP_PROOF_DIR)/xdg-data" "$(LINUX_DESKTOP_SELF_TEST_RUNTIME_DIR)" "$(LINUX_DESKTOP_SELF_TEST_STATE_DIR)"
	@cd "$(RELEASE_DIR)" && sha256sum -c "$(notdir $(LINUX_DESKTOP_SHA256))"
	@tar -xzf "$(LINUX_DESKTOP_ARCHIVE)" -C "$(LINUX_DESKTOP_PROOF_DIR)/unpack"
	@HOME="$(abspath $(LINUX_DESKTOP_PROOF_DIR)/home)" XDG_DATA_HOME="$(abspath $(LINUX_DESKTOP_PROOF_DIR)/xdg-data)" "$(LINUX_DESKTOP_PROOF_DIR)/unpack/$(LINUX_DESKTOP_BASENAME)/share/install-desktop-entry.sh" >/dev/null
	@test -f "$(LINUX_DESKTOP_PROOF_DIR)/xdg-data/applications/kinetic.desktop" || (echo "Installer did not write kinetic.desktop"; exit 1)
	@test -f "$(LINUX_DESKTOP_PROOF_DIR)/xdg-data/icons/hicolor/scalable/apps/kinetic.svg" || (echo "Installer did not write kinetic.svg"; exit 1)
	@PHYSICS_SIM_RUNTIME_DIR="$(LINUX_DESKTOP_SELF_TEST_RUNTIME_DIR)" XDG_STATE_HOME="$(LINUX_DESKTOP_SELF_TEST_STATE_DIR)" "$(LINUX_DESKTOP_PROOF_DIR)/unpack/$(LINUX_DESKTOP_BASENAME)/bin/physics-sim-launcher" --self-test
	@echo "package-linux-desktop-self-test passed."

package-linux-desktop-determinism-test: package-linux-desktop-self-test
	+@python3 -B scripts/package_proof.py --root "$(RELEASE_DIR)" --name linux-desktop-determinism --input "$(LINUX_DESKTOP_DIR)" --input "$(LINUX_DESKTOP_ARCHIVE)" --input "$(LINUX_DESKTOP_SHA256)" --map "LINUX_DESKTOP_COMPARISON_DIR=comparison" -- $(MAKE) -f makefile _package-linux-desktop-determinism-proof

.PHONY: _package-linux-desktop-determinism-proof
_package-linux-desktop-determinism-proof:
	@test -n "$(PACKAGE_PROOF_DIR)" || (echo "Use the public determinism proof target"; exit 2)
	@release_dir="$(abspath $(RELEASE_DIR))"; \
		first_sha="$$(cut -d ' ' -f 1 "$(abspath $(LINUX_DESKTOP_SHA256))")"; \
		comparison_dir="$(LINUX_DESKTOP_COMPARISON_DIR)"; mkdir -p "$$comparison_dir" || exit 2; \
		comparison_archive="$$comparison_dir/second.tar.gz"; \
		cd "$$release_dir" && find "$(LINUX_DESKTOP_BASENAME)" -print0 | LC_ALL=C sort -z | tar --null --no-recursion --files-from - --format=posix --pax-option=exthdr.name=%d/PaxHeaders/%f,delete=atime,delete=ctime --mtime="@$(LINUX_DESKTOP_PACKAGE_EPOCH)" --owner=0 --group=0 --numeric-owner -cf - | gzip -n > "$$comparison_archive"; \
		second_sha="$$(sha256sum "$$comparison_archive" | cut -d ' ' -f 1)"; \
		if [ "$$first_sha" != "$$second_sha" ]; then \
			echo "package-linux-desktop determinism failed: $$first_sha != $$second_sha"; \
			exit 1; \
		fi; \
		echo "package-linux-desktop-determinism-test passed: $$second_sha"
