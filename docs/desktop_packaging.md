# Physics Sim Desktop Packaging

Last updated: 2026-04-25

## Bundle Targets
- `make -C physics_sim package-desktop`
- `make -C physics_sim package-desktop-smoke`
- `make -C physics_sim package-desktop-self-test`
- `make -C physics_sim package-desktop-copy-desktop`
- `make -C physics_sim package-desktop-sync`
- `make -C physics_sim package-desktop-open`
- `make -C physics_sim package-desktop-remove`
- `make -C physics_sim package-desktop-refresh`

## Bundle Layout
- app path: `physics_sim/dist/kinetiC.app`
- launcher: `Contents/MacOS/physics-sim-launcher`
- binary: `Contents/MacOS/physics-sim-bin`
- resources root: `Contents/Resources`

Bundled resource lanes:
- `config/` (including `config/objects/` and structural preset files)
- `data/runtime/` (created)
- `data/snapshots/` (created)
- optional `AppIcon.icns` when `PACKAGE_APP_ICON_SRC` or `PACKAGE_APP_ICONSET_SRC` is provided
- `vk_renderer/shaders/`
- `shaders/`
- `shared/assets/fonts/` (copied when `third_party/codework_shared/assets/fonts` exists)

Default local icon store:
- `physics_sim/tools/packaging/macos/local_app_icon/AppIcon.icns`
- `physics_sim/tools/packaging/macos/local_app_icon/AppIcon.iconset`

Optional icon inputs:
- `make -C physics_sim package-desktop-refresh PACKAGE_APP_ICON_SRC="/absolute/path/to/kinetic.icns"`
- `make -C physics_sim package-desktop-refresh PACKAGE_APP_ICONSET_SRC="/absolute/path/to/kinetic.iconset"`

When present, packaging now:
- bundles `Contents/Resources/AppIcon.icns`
- declares `CFBundleIconFile=AppIcon`
- preserves the signed Desktop copy path with `ditto`

Plain `make -C physics_sim package-desktop-refresh` and `package-desktop-self-test` now look in that local store first. The local icon store is gitignored so refreshed icon copies do not dirty the normal repo worktree.

## Launcher Contract
- config dump:
  - `.../physics-sim-launcher --print-config`
- self test:
  - `.../physics-sim-launcher --self-test`

Runtime defaults set by launcher:
- `VK_RENDERER_SHADER_ROOT=$RES_DIR`
- `SHAPE_ASSET_DIR=$RES_DIR/config/objects`
- cwd switched to `$RES_DIR` before binary exec
- logs written to `~/Library/Logs/PhysicsSim/launcher.log` with tmp fallback

## Verification Sequence
1. `make -C physics_sim package-desktop-self-test`
2. `make -C physics_sim package-desktop-refresh`
3. `/Users/<user>/Desktop/kinetiC.app/Contents/MacOS/physics-sim-launcher --print-config`
4. `open /Users/<user>/Desktop/kinetiC.app`
5. `tail -n 120 ~/Library/Logs/PhysicsSim/launcher.log`

Note:
- a fresh clone will still need an `AppIcon.icns` copied into `tools/packaging/macos/local_app_icon/` before plain packaging picks it up, because that lane is intentionally ignored.
