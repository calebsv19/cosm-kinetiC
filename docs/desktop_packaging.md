# Physics Sim Desktop Packaging

Last updated: 2026-06-19

## Bundle Targets
- `make -C physics_sim package-desktop`
- `make -C physics_sim package-desktop-smoke`
- `make -C physics_sim package-desktop-self-test`
- `make -C physics_sim package-desktop-copy-desktop`
- `make -C physics_sim package-desktop-sync`
- `make -C physics_sim package-desktop-open`
- `make -C physics_sim package-desktop-remove`
- `make -C physics_sim package-desktop-refresh`
- `make -C physics_sim package-linux-worker-dry-run`

## Bundle Layout
- app path: `physics_sim/dist/kinetiC.app`
- launcher: `Contents/MacOS/physics-sim-launcher`
- binary: `Contents/MacOS/physics-sim-bin`
- resources root: `Contents/Resources`

Bundled resource lanes:
- `config/` (including `config/objects/` and structural preset files)
- optional `AppIcon.icns` when `PACKAGE_APP_ICON_SRC` or `PACKAGE_APP_ICONSET_SRC` is provided
- `vk_renderer/shaders/`
- `shaders/`
- `shared/assets/fonts/` (copied when `third_party/codework_shared/assets/fonts` exists)

Package/artifact trust boundary:
- package and release targets are trusted local operator tools. They use
  destructive cleanup under their configured package/release roots, so
  overrides for `DIST_DIR`, `RELEASE_DIR`, `PACKAGE_APP_DIR`, or
  `DESKTOP_APP_DIR` should stay under explicit build/Desktop artifact roots.
- `package-desktop` stages only the app binary, launcher, `Info.plist`,
  bundled dylibs, `config/`, shader resources, optional app icon, and optional
  shared fonts into `dist/kinetiC.app`.
- `package-linux-worker` stages only the headless/job-runner binaries,
  `config/`, selected public docs, and package manifests into
  `build/release/<worker-bundle>/` before creating the worker `.tar.gz`.
- Private/generated run lanes such as `build/agent_runs/`, `tmp/`,
  `_private_workspace_artifacts/`, detached job outputs, launcher logs, and
  local icon source copies are not package inputs unless a future target
  explicitly stages them.
- Release/notary/signing byproducts live under `build/release/` and remain
  operator artifacts, not public source truth.
- Remote execution, upload, publish, or cross-host worker validation is outside
  these local package targets and must use the appropriate CodeWork handoff
  lane instead of ad hoc SSH/SCP.

Worker package / cross-host handoff boundary:
- `package-linux-worker` creates a local archive only. It does not install,
  upload, run, or register the worker package on any remote host.
- `package-linux-worker-dry-run` is the local package proof target. It stages
  the archive, runs `package-linux-worker-self-test`, validates manifest and
  package-manifest metadata, checks the expected entrypoints/docs/config files,
  confirms the archive excludes private/generated run lanes, and executes only
  the package-manifest declared local self-test command.
- The worker package manifest advertises `worker_slug =
  physics_sim_headless_worker`, `job_types = ["trio_headless_stage"]`, and the
  entrypoint `bin/run_worker.sh`.
- Worker-safe job payloads must stage the runtime scene, run config, runtime
  mesh documents, preview sidecars, and expected output/report roots together.
  Do not rely on author-machine absolute paths or local recovery paths once the
  archive leaves the Mac.
- Linux PC package upload/install/status/fetch work must route through the
  Linux PC handoff lane and its bounded package/install/runtime inventory
  profiles.
- VPS worker-fleet visibility, worker-exchange requests, and VPS-side runtime
  proofs must route through the VPS handoff lane and its bounded worker
  exchange/report-inbox/export-dropbox profiles.
- Cross-host validation should read back manifests, package inventory,
  runtime inventory, reports, and fetched artifacts through those lanes. Do not
  use raw SSH/SCP or ad hoc remote shell from the package docs.

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
- app support root: `${HOME}/Library/Application Support/PhysicsSim`, unless
  `PHYSICS_SIM_APP_SUPPORT_DIR` is set
- writable runtime root: `$PHYSICS_SIM_APP_SUPPORT_DIR/runtime`, unless
  `PHYSICS_SIM_RUNTIME_DIR` is set
- runtime-root fallback: `${TMPDIR:-/tmp}/PhysicsSim/runtime` if the app support
  runtime directory cannot be created
- linked resource lanes under the runtime root:
  - `config -> Contents/Resources/config`
  - `shared -> Contents/Resources/shared`
  - `shaders -> Contents/Resources/shaders`
  - `vk_renderer -> Contents/Resources/vk_renderer`
- writable state lanes created under the runtime root:
  - `data/runtime/`
  - `data/snapshots/`
- `VK_RENDERER_SHADER_ROOT` defaults to the runtime root
- `SHAPE_ASSET_DIR` defaults to `$PHYSICS_SIM_RUNTIME_DIR/config/objects`
- `PHYSICS_SIM_TIMER_HUD`, `PHYSICS_SIM_TIMER_HUD_OVERLAY`, and
  `PHYSICS_SIM_TIMER_HUD_VISUAL_MODE` default to `0`, `0`, and `hybrid`
- MoltenVK uses a generated runtime ICD file at
  `$PHYSICS_SIM_RUNTIME_DIR/vk/MoltenVK_icd.json` when the bundled
  `Contents/Frameworks/libMoltenVK.dylib` is present
- cwd switches to the runtime root before binary exec
- logs write to `~/Library/Logs/PhysicsSim/launcher.log` with
  `${TMPDIR:-/tmp}/physics-sim-launcher.log` fallback

Diagnostic commands:
- `.../physics-sim-launcher --print-config` prints `APP_CONTENTS_DIR`,
  `RES_DIR`, `LOG_FILE`, `PHYSICS_SIM_RUNTIME_DIR`, timer HUD values,
  `VK_RENDERER_SHADER_ROOT`, `SHAPE_ASSET_DIR`, Vulkan ICD/driver files, and
  `MOLTENVK_DYLIB`.
- `.../physics-sim-launcher --self-test` verifies the bundled binary,
  `Info.plist`, linked config files, bundled shape assets, writable runtime
  state directories, runtime and `vk_renderer` shaders, and bundled MoltenVK,
  then prints the same diagnostic environment lanes.

## Verification Sequence
1. `make -C physics_sim package-desktop-refresh`
2. `make -C physics_sim package-desktop-self-test`
3. `/Users/<user>/Desktop/kinetiC.app/Contents/MacOS/physics-sim-launcher --print-config`
4. `open /Users/<user>/Desktop/kinetiC.app`
5. `tail -n 120 ~/Library/Logs/PhysicsSim/launcher.log`

Note:
- a fresh clone will still need an `AppIcon.icns` copied into `tools/packaging/macos/local_app_icon/` before plain packaging picks it up, because that lane is intentionally ignored.
