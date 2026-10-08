# =========================
#  Release rules
# =========================
release-contract:
	@echo "release-contract:"
	@echo "  product: $(RELEASE_PRODUCT_NAME)"
	@echo "  program: $(RELEASE_PROGRAM_KEY)"
	@echo "  version: $(RELEASE_VERSION)"
	@echo "  channel: $(RELEASE_CHANNEL)"
	@echo "  bundle_id: $(RELEASE_BUNDLE_ID)"
	@echo "  app_name: $(PACKAGE_APP_NAME)"
	@echo "  artifact_base: $(RELEASE_ARTIFACT_BASENAME)"
	@echo "  unsigned_local_zip: $(RELEASE_APP_ZIP)"
	@echo "  authenticated_root: $(RELEASE_PIPELINE_ROOT)"
	@echo "  authenticated_zip: $(RELEASE_PIPELINE_ROOT)/final/$(RELEASE_ARTIFACT_BASENAME).zip"
	@echo "  signing_identity: $(RELEASE_CODESIGN_IDENTITY)"
	@echo "  notary_profile_set: $(if $(strip $(APPLE_NOTARY_PROFILE)),yes,no)"
	@echo "  team_id_set: $(if $(strip $(APPLE_TEAM_ID)),yes,no)"

release-clean:
	@python3 -B scripts/package_outputs.py --root "$(RELEASE_DIR)" --directory "$(RELEASE_DIR)"
	@echo "Release outputs absent; no retained artifact removal performed"

release-build: all
	@echo "Release build complete: $(TARGET)"

RELEASE_PLIST_BUDDY ?= /usr/libexec/PlistBuddy
RELEASE_OTOOL ?= otool
.PHONY: release-bundle-audit _release-bundle-audit
release-bundle-audit: package-desktop-self-test
	+@python3 -B scripts/package_proof.py --root "$(RELEASE_DIR)" --name release-bundle-audit --input "$(PACKAGE_APP_DIR)" --input "$(RELEASE_PLIST_BUDDY)" --input "$$(command -v "$(RELEASE_OTOOL)")" --identity "bundle-id=$(RELEASE_BUNDLE_ID)" --map "RELEASE_AUDIT_DIR=." -- $(MAKE) -f makefile _release-bundle-audit

_release-bundle-audit:
	@python3 -B scripts/package_outputs.py --root "$(RELEASE_AUDIT_DIR)" --directory "$(RELEASE_AUDIT_DIR)/framework-reports" --file "$(RELEASE_AUDIT_DIR)/bundle_id.txt" --file "$(RELEASE_AUDIT_DIR)/print_config.txt" --file "$(RELEASE_AUDIT_DIR)/otool_physics_sim_bin.txt" --declare
	@"$(RELEASE_PLIST_BUDDY)" -c 'Print :CFBundleIdentifier' "$(PACKAGE_CONTENTS_DIR)/Info.plist" > "$(RELEASE_AUDIT_DIR)/bundle_id.txt"
	@test "$$(cat "$(RELEASE_AUDIT_DIR)/bundle_id.txt")" = "$(RELEASE_BUNDLE_ID)" || (echo "bundle id mismatch: expected $(RELEASE_BUNDLE_ID), got $$(cat "$(RELEASE_AUDIT_DIR)/bundle_id.txt")"; exit 1)
	@env -i HOME="$(HOME)" PATH="$(PATH)" "$(PACKAGE_MACOS_DIR)/physics-sim-launcher" --print-config > "$(RELEASE_AUDIT_DIR)/print_config.txt"
	@runtime_dir="$$(/usr/bin/grep '^PHYSICS_SIM_RUNTIME_DIR=' "$(RELEASE_AUDIT_DIR)/print_config.txt" | /usr/bin/cut -d= -f2-)"; \
	if [ -z "$$runtime_dir" ]; then echo "runtime dir missing from print-config"; exit 1; fi; \
	case "$$runtime_dir" in *"/Contents/Resources"*) echo "runtime dir incorrectly points into app bundle: $$runtime_dir"; exit 1;; esac; \
	case "$$runtime_dir" in /tmp/*|/var/*|"$(HOME)"/*) ;; *) echo "runtime dir is not user-writable rooted: $$runtime_dir"; exit 1;; esac
	@/usr/bin/grep -q '^VK_ICD_FILENAMES=' "$(RELEASE_AUDIT_DIR)/print_config.txt" || (echo "missing VK_ICD_FILENAMES in print-config"; exit 1)
	@/usr/bin/grep -q '^VK_DRIVER_FILES=' "$(RELEASE_AUDIT_DIR)/print_config.txt" || (echo "missing VK_DRIVER_FILES in print-config"; exit 1)
	@"$(RELEASE_OTOOL)" -L "$(PACKAGE_MACOS_DIR)/physics-sim-bin" > "$(RELEASE_AUDIT_DIR)/otool_physics_sim_bin.txt"
	@python3 -B scripts/release_framework_audit.py --verify-report "$(RELEASE_AUDIT_DIR)/otool_physics_sim_bin.txt" --input-binary "$(PACKAGE_MACOS_DIR)/physics-sim-bin"
	@python3 -B scripts/release_framework_audit.py --frameworks "$(PACKAGE_FRAMEWORKS_DIR)" --reports "$(RELEASE_AUDIT_DIR)/framework-reports" --otool "$(RELEASE_OTOOL)"
	@echo "release-bundle-audit passed."

# Unsigned, local package evidence for Decision 1. Authentication and
# publication remain separate later stages.
RELEASE_DITTO ?= /usr/bin/ditto
RELEASE_SHASUM ?= shasum
RELEASE_APP_SHA256 ?= $(RELEASE_APP_ZIP).sha256
.PHONY: release-local-artifact _release-local-artifact
release-local-artifact: release-bundle-audit
	+@python3 -B scripts/package_transaction.py --root "$(RELEASE_DIR)" \
		--file "RELEASE_APP_ZIP=$(RELEASE_APP_ZIP)" --file "RELEASE_APP_SHA256=$(RELEASE_APP_SHA256)" --file "RELEASE_MANIFEST=$(RELEASE_MANIFEST)" \
		--input "$(PACKAGE_APP_DIR)" --input makefile --input make --input scripts \
		--identity "product=$(RELEASE_PRODUCT_NAME)" --identity "program=$(RELEASE_PROGRAM_KEY)" --identity "version=$(RELEASE_VERSION)" --identity "platform=$(RELEASE_PLATFORM)" --identity "arch=$(RELEASE_ARCH)" --identity "channel=$(RELEASE_CHANNEL)" --identity unsigned-local-artifact \
		--tool "$(RELEASE_DITTO)" --tool "$(RELEASE_SHASUM)" --tool "$(SHELL)" -- $(MAKE) -f makefile _release-local-artifact

_release-local-artifact:
	@python3 -B scripts/package_outputs.py --root "$(RELEASE_DIR)" --file "$(RELEASE_APP_ZIP)" --file "$(RELEASE_APP_SHA256)" --file "$(RELEASE_MANIFEST)" --declare
	@mkdir -p "$$(dirname "$(RELEASE_APP_ZIP)")" "$$(dirname "$(RELEASE_APP_SHA256)")" "$$(dirname "$(RELEASE_MANIFEST)")"
	@"$(RELEASE_DITTO)" -c -k --sequesterRsrc --keepParent "$(PACKAGE_APP_DIR)" "$(RELEASE_APP_ZIP)"
	@archive_dir="$$(dirname "$(RELEASE_APP_ZIP)")"; archive_name="$$(basename "$(RELEASE_APP_ZIP)")"; (cd "$$archive_dir" && "$(RELEASE_SHASUM)" -a 256 "$$archive_name") > "$(RELEASE_APP_SHA256)"
	@printf 'product=%s\nprogram=%s\nversion=%s\nplatform=%s\narch=%s\nformat=zip\nchannel=%s\nartifact=%s\nsha256=%s\nsigned=false\nnotarized=false\n' \
		"$(RELEASE_PRODUCT_NAME)" "$(RELEASE_PROGRAM_KEY)" "$(RELEASE_VERSION)" \
		"$(RELEASE_PLATFORM)" "$(RELEASE_ARCH)" "$(RELEASE_CHANNEL)" \
		"$$(basename "$(RELEASE_APP_ZIP)")" \
		"$$(cut -d' ' -f1 "$(RELEASE_APP_SHA256)")" > "$(RELEASE_MANIFEST)"
	@python3 -B scripts/verify_release_local_artifact.py --archive "$(RELEASE_APP_ZIP)" --checksum "$(RELEASE_APP_SHA256)" --manifest "$(RELEASE_MANIFEST)"
	@echo "release-local-artifact complete: $(RELEASE_APP_ZIP)"

# Separate immutable phase roots. No source app is signed or stapled in place.
# Output paths are returned in exact transaction receipts, not guessed by callers.
RELEASE_PIPELINE_ROOT ?= $(RELEASE_DIR)/authenticated
RELEASE_CODESIGN ?= codesign
RELEASE_XCRUN ?= xcrun
RELEASE_SPCTL ?= spctl
RELEASE_LIPO ?= lipo
RELEASE_PIPELINE_ARGS = --source "$(PACKAGE_APP_DIR)" --root "$(RELEASE_PIPELINE_ROOT)" \
 --signing-identity="$(RELEASE_CODESIGN_IDENTITY)" --profile "$(APPLE_NOTARY_PROFILE)" \
 --archive-name "$(RELEASE_ARTIFACT_BASENAME).zip" \
 --identity "product=$(RELEASE_PRODUCT_NAME)" --identity "program=$(RELEASE_PROGRAM_KEY)" \
 --identity "bundle_id=$(RELEASE_BUNDLE_ID)" --identity "version=$(RELEASE_VERSION)" \
 --identity "channel=$(RELEASE_CHANNEL)" --identity "platform=$(RELEASE_PLATFORM)" --identity "arch=$(RELEASE_ARCH)" \
 --tool "codesign=$(RELEASE_CODESIGN)" --tool "xcrun=$(RELEASE_XCRUN)" \
 --tool "spctl=$(RELEASE_SPCTL)" --tool "lipo=$(RELEASE_LIPO)" --tool "ditto=$(RELEASE_DITTO)"

release-sign: release-bundle-audit
	@python3 -B scripts/release_pipeline.py sign $(RELEASE_PIPELINE_ARGS)

# The sign transaction performs strict codesign verification. Gatekeeper is a
# mandatory final-artifact gate after same-ID acceptance and stapling.
release-verify release-verify-signed: release-sign
	@echo "Signed app verified by retained signing transaction"

release-notarize: release-bundle-audit
	@python3 -B scripts/release_pipeline.py notarize $(RELEASE_PIPELINE_ARGS)

release-staple: release-bundle-audit
	@python3 -B scripts/release_pipeline.py staple $(RELEASE_PIPELINE_ARGS)

release-verify-notarized: release-staple
	@echo "Stapled app verified by retained stapling transaction"

release-artifact: release-bundle-audit
	@python3 -B scripts/release_pipeline.py artifact $(RELEASE_PIPELINE_ARGS)

release-distribute: release-artifact
	@echo "Local authenticated artifact complete; publication remains Release Control owned"

# Refresh requires the exact completed final receipt; never select a raw app or
# discover the newest transaction. Existing authority target stays mandatory.
release-desktop-refresh: package-desktop-refresh-authority
	@test -n "$(RELEASE_FINAL_RECEIPT)" || (echo "RELEASE_FINAL_RECEIPT is required"; exit 1)
	@python3 -B scripts/release_final_artifact.py refresh --final-receipt "$(RELEASE_FINAL_RECEIPT)" --destination "$(DESKTOP_APP_DIR)"
