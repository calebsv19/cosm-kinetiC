# The Registry owns credentials, same-ID recovery and native ticket attachment.
# These targets do not call the independent standalone release pipeline.
.PHONY: release-native-signed release-native-notarize release-artifact
release-native-signed: release-bundle-audit
	@test -n "$(APPLE_SIGN_IDENTITY)" && test "$(APPLE_SIGN_IDENTITY)" != '-'
	@python3 -B scripts/release_app_stage.py sign --source "$(abspath $(PACKAGE_APP_DIR))" --root "$(abspath $(RELEASE_ROOT))" --bundle-id "$(RELEASE_BUNDLE_ID)" --tool codesign --identity="$(APPLE_SIGN_IDENTITY)"

release-native-notarize: release-native-signed
	@test -n "$(APPLE_NOTARY_PROFILE)"
	@python3 -B scripts/release_native.py upload --root "$(abspath $(RELEASE_ROOT))" --version "$(RELEASE_VERSION)"
	@xcrun notarytool submit "$(RELEASE_ROOT)/$(RELEASE_ARTIFACT_BASENAME)-notary-submit.zip" --keychain-profile "$(APPLE_NOTARY_PROFILE)" --wait --output-format json > "$(RELEASE_ROOT)/notary_submit.json"
	@python3 -B scripts/release_native.py accepted --root "$(abspath $(RELEASE_ROOT))" --version "$(RELEASE_VERSION)"

# The native owner has already attached and independently validated the ticket.
# Revalidate locally and assemble under the existing package transaction owner.
ifneq ($(strip $(PHYSICS_SIM_LAYOUT_BUILD)),)
release-artifact:
	@python3 -B scripts/release_native.py package --root "$(abspath $(RELEASE_ROOT))" --version "$(RELEASE_VERSION)"
endif
