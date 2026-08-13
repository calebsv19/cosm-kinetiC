# Physics Sim Desktop Packaging

Last updated: 2026-08-13

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
- `make -C physics_sim package-linux-desktop-contract`
- `make -C physics_sim package-linux-desktop`
- `make -C physics_sim package-linux-desktop-self-test`
- `make -C physics_sim package-linux-desktop-determinism-test`

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
- `package-linux-desktop` stages only the windowed app binary, Linux launcher,
  `config/`, shader resources, optional shared fonts, manifests, and
  package-local README into a private `desktop_app_linux` archive. It is a
  Linux-only proof target and refuses on non-Linux hosts.
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
  confirms the archive excludes private/generated run lanes, verifies that
  both native executables require no GLIBC symbol newer than the declared
  fleet ceiling, and executes only the package-manifest declared local
  self-test command. The default ceiling is `2.39.0`; override
  `LINUX_WORKER_MAX_GLIBC` only for an explicitly different target fleet.
- The worker package manifest advertises `worker_slug =
  physics_sim_headless_worker`, `job_types = ["trio_headless_stage"]`, and the
  entrypoint `bin/run_worker.sh`. Both manifests also bind
  `max_glibc_version` so compatibility intent remains visible after packaging.
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

Linux desktop package / GUI proof boundary:
- `package-linux-desktop` is private proof scaffolding, not a public release
  target.
- Package class: `desktop_app_linux`.
- Artifact role: `desktop_app`.
- Runtime: `linux_gui`.
- Default platform: `linux-x86_64`.
- Expected private archive:
  `build/release/kinetiC-<version>-linux-x86_64-desktop-stable.tar.gz`.
- Expected checksum sidecar:
  `build/release/kinetiC-<version>-linux-x86_64-desktop-stable.tar.gz.sha256`.
- Package root layout:

```text
kinetiC-<version>-linux-x86_64-desktop-stable/
  bin/physics-sim-launcher
  bin/physics-sim-bin
  resources/config/
  resources/shared/
  resources/shaders/
  resources/vk_renderer/
  resources/data/runtime/
  resources/data/snapshots/
  manifest.json
  package_manifest.json
  README.md
```

- The Linux launcher copies package resources into a writable runtime root on
  first use instead of mutating the unpacked package directory.
- Default writable roots:
  - `${XDG_DATA_HOME:-$HOME/.local/share}/PhysicsSim/runtime`
  - `${XDG_STATE_HOME:-$HOME/.local/state}/PhysicsSim/logs`
- Proof helpers can override with `PHYSICS_SIM_RUNTIME_DIR`,
  `PHYSICS_SIM_LOG_DIR`, and `XDG_STATE_HOME`.
- `--self-test` checks package-local binaries/resources and writable runtime
  root setup without requiring a visible display.
- The deterministic archive gate uses sorted tar entries, POSIX tar format,
  deterministic PAX names, fixed mtime from `LINUX_DESKTOP_PACKAGE_EPOCH`,
  numeric `0:0` ownership, gzip `-n`, and a `.sha256` sidecar.
- The local Mac target can only print the contract and refuse the build; real
  package build/proof must run on Linux x86_64 through the Mac/Linux PC
  handoff lane.
- Linux dependencies must be read back per host, but the expected classes are
  `cc`, `make`, `pkg-config`, SDL2, SDL2_ttf, json-c, Vulkan headers/loader,
  a working Vulkan driver stack, display readback tools, and screenshot/control
  tooling or the existing bounded X11/XTest fallback.
- Minimum Linux PC package proof:
  - build from a named source state
  - run `package-linux-desktop-determinism-test`
  - verify the archive sidecar
  - unpack into a clean proof directory
  - run unpacked `bin/physics-sim-launcher --self-test`
  - launch the unpacked package in the real logged-in desktop session
  - capture app-window screenshots and launcher log markers
  - keep this separate from Linux headless worker proof

Latest Linux PC proof, 2026-07-09:
- proof id: `pslgui4-20260709a`
- report-inbox thread:
  `_private_workspace_artifacts/codework_report_inbox/linux-pc-physics-sim-gui-proof-pslgui4-20260709a/`
- source archive sha256:
  `edaac3204745c8c0db270df6dbf68e660ff65ee86dcbaceaaddf8f60dcbc254f`
- package archive:
  `kinetiC-0.3.0-linux-x86_64-desktop-stable.tar.gz`
- package archive sha256:
  `ebd581a014abfe69f02df8745ca8bf645fea2b3a7551065ce59aeb47683a572e`
- package archive size: `3360882` bytes
- Linux PC dependency readback:
  - SDL2 `2.32.68`
  - SDL2_ttf `2.24.0`
  - json-c `0.18`
  - Vulkan `1.4.350`
  - NVIDIA GeForce RTX 3060 via proprietary NVIDIA driver `580.159.03`
- desktop session readback:
  - `DISPLAY=:0`
  - `XAUTHORITY=/tmp/xauth_SmfAha`
  - KDE/X11 process discovery found `kwin_x11` and `plasmashell`
- proof classification: `gui_menu_captured`
- fetched proof artifacts:
  - `vps/physics_sim_gui_proof/physics_sim_gui_proof.json`
  - `vps/physics_sim_gui_proof/build.log`
  - `vps/physics_sim_gui_proof/physics_sim_menu_window.png`
  - `vps/physics_sim_gui_proof/physics_sim_after_action_window.png`
  - `vps/physics_sim_gui_proof/physics_sim_late_window.png`
- screenshot sanity:
  - menu capture: `1024x720`, nonblank candidate, sha256
    `f9b9588135ecd5bec199f3ecc15886f387efdb0c9e122fcf98ac0e3ed8ac503a`
  - after-action and late captures: `1024x720`, nonblank candidates, sha256
    `6b33743ca12f3ecde8a6cdf5b296055be5bf040f9155bb314b8c89ef3c50ce17`
- scope boundary:
  this proof did not publish, bump `VERSION`, mutate website/download metadata,
  mutate production registry state, install worker packages, or alter
  RayTracing artifacts.

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

## Linux Desktop Proof Sequence
1. Local contract readback:
   - `make -C physics_sim package-linux-desktop-contract`
2. Linux host guard readback:
   - `make -C physics_sim package-linux-desktop-host-check`
   - expected on macOS: fail with a message that the package build must run on
     Linux through the handoff lane
3. Linux PC package proof command inside a bounded Mac/Linux PC handoff helper:
   - `make -C /path/to/physics_sim package-linux-desktop-determinism-test CLANG=cc LINUX_DESKTOP_PLATFORM=linux-x86_64`
4. Clean unpack launcher self-test:
   - `PHYSICS_SIM_RUNTIME_DIR=/tmp/physics-sim-linux-desktop-proof/runtime XDG_STATE_HOME=/tmp/physics-sim-linux-desktop-proof/state ./kinetiC-<version>-linux-x86_64-desktop-stable/bin/physics-sim-launcher --self-test`
5. Real desktop proof:
   - launch the unpacked package in the logged-in Linux PC desktop session and
     fetch app-window screenshots plus launcher logs through the bounded
     handoff lane.
