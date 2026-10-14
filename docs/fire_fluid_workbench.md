# Local Fire–Fluid authoring and coupled playback

This source-development workbench provides one usable bounded coupled scene,
native field inspection, saved-state playback and local RayTracing previews.
It uses the established native Fire → heat/smoke → Fluid path and optional
accepted-prior Fluid → held tangent wind → Fire response. It does not enable
reciprocal thermal feedback or adopt the experimental mixture solver.

## Build and launch

Use the registered PhysicsSim and GrowthSim Main Edit checkouts. Choose fresh
build roots; never clean retained numerical evidence to rebuild these workers.

```sh
make -C <physics-main-edit> BUILD_DIR=build/<fresh> \
  coupling-framework-worker open-atmosphere-worker
make -C <growth-main-edit> BUILD_TOOLCHAIN=clang \
  FIRE_SOURCE_BUILD_DIR=build/<fresh>/fire fire-source-build
make -C <ray-main-edit> BUILD_DIR_BASE=build/<fresh> ray-tracing-render-headless
python3 -B <physics-main-edit>/scripts/fire_fluid_workbench.py \
  --root <owned-scene-root> --growth-root <growth-main-edit> \
  --fire-worker <fresh-growth-worker> --fluid-worker <fresh-open-worker> \
  --clock-worker <fresh-clock-worker> --renderer <existing-ray-renderer>
```

Open `http://127.0.0.1:8841`. The optional `--port` selects another loopback
port. Omitting `--renderer` still permits authoring, simulation and export;
the preview button reports that the renderer is unavailable. This trusted local
tool is not a public server or an installed desktop workflow. Stop with Ctrl-C.

## Authoring and playback

The initial scene has a 0.75 × 0.75 m source with 8 × 8 native Fire nodes,
an 8 × 8 × 4 fluid grid in a 1 × 1 × 0.5 m domain, a solid floor, open top
and periodic horizontal boundaries. Fire steps at 0.02 s, Fluid at 0.005 s.
Default baseline playback covers 2 s in twenty 0.1 s exchanges.
The optional Warm plume profile uses a 1 m tall domain with eight vertical cells
and 75% authored heat transfer. Its qualified synthetic scene uses wind multiplier
1, source loading 3 and duration 3.2 s (32 exchanges). These are explicit authored
inputs, not measured burner calibration; native temperature admission remains active. No internal solid bodies
or newly qualified combustion physics are implied by this scene.

- Set wind-feedback enablement, initial shear multiplier, source fuel-loading
  multiplier, return sampling height, exchange interval and duration.
- **Create new scene** saves a new immutable configuration. Existing runs are
  preserved; controls do not rewrite an accepted run's inputs. Source loading
  changes the authored fuel-to-heat/smoke calibration, not a measured material.
- **Run**, **Pause** and **Single exchange** operate on native joint windows.
  Pause takes effect between complete exchanges; it never splits publication of
  the two participants. Solver supervision runs in a child main interpreter.
- Seek or replay accepted checkpoints without advancing either solver. Select
  Fire heat/fuel/burn/ash, inspect native fluid column temperature and smoke,
  and hover each top map for a numerical value. A native side projection and the
  smoke fraction above the first layer expose vertical transport independently
  of the rendered appearance. Wind arrows show consumed tangent
  velocity. Expand provenance to inspect the prior checkpoint and hold window.
- Open a saved scene to recover its accepted head. Server restarts begin paused.
  Configuration, native worker or framework input drift refuses reopening;
  it does not silently migrate checkpoints. Use a fresh scene root for changed
  incompatible source inputs.

Supported exchange intervals are 0.02, 0.04 and 0.1 s. Duration must align exactly
and cover 3–32 exchanges. Sampling height must be inside the 0.5 m domain.
Native admission and journal/work limits remain in force. Failed native steps
preserve the accepted head; step retries retain the original operation identity.

## Local rendering

**Export RayTracing playback** writes same-clock scene snapshots for every
accepted noninitial revision. **Render viewed state** exports a fresh payload
and renders one selected accepted revision at 640 × 480 with one temporal frame
and one worker. Each preflight/render command has a 180 s cap. The last rendered
preview is saved per run and remains available after page reload/server restart.

Exports preserve the authoritative combined-scene sidecars. The separate
`playback_scene_runtime.json` uses surface fuel/burn/ash colors, with all diagnostic
emission cards removed. There are no airborne proxy particles or flame geometry.
The lower camera shows volume height. Native smoke is carried by VF3D using the
existing renderer's cell-centred layout and physical_smoke_v1 single-scattering
profile: concentration reference 0.0002 kg/m³, candidate extinction 7500 m²/kg,
albedo 0.65, anisotropy 0.35, sky enabled and sky omission budget 0.00001.
These optical candidates are uncalibrated; no legacy density amplification is
applied. No render carrier feeds the solvers. Fire heat remains an ignition proxy.

For a small external render batch:

```sh
python3 -B scripts/fire_fluid_render.py --export <fresh-playback-export> \
  --renderer <existing-headless-renderer> --revisions 1,10,20
```

The CLI allows at most four distinct accepted states, refuses reused render
outputs, verifies input and renderer hashes, and records image/summary/checkpoint
identities. No RayTracing edits, remote jobs, publication or latest-good promotion
are part of this workflow.

## Bounded qualification

`tests/test_fire_fluid_workbench.py` checks baseline and warm-plume authoring admission, preserved runs,
idempotent steps/stale refusal, prior-state wind provenance, exact replay/reopen,
pause/completion, control changes and validated multi-state export. It requires
`FIRE_FLUID_GROWTH_ROOT`, `FIRE_FLUID_FIRE_WORKER`, `FIRE_FLUID_OPEN_WORKER` and
`FIRE_FLUID_CLOCK_WORKER`. Use fresh workers and an owned evidence root.

`scripts/fire_fluid_workbench_qualification.py` retains the full two-second
enabled/repeated/disabled scenarios. The 2026-10-10 checkpoint reproduces the
enabled native fields and checkpoint identities exactly. Stored fluid heat is
33.326536 J enabled versus 25.002402 J disabled. These differences demonstrate
functional wind feedback, not physical accuracy. Native conservation, causality,
rollback and sampling regression tests pass separately. Local RayTracing scene
and volume attachment, accepted-state timing and changing preview frames are
verified independently from the numerical result.

The original three-frame baseline proved renderer attachment but was inadequate
as a smoke-depth demonstration: 97% of smoke stayed in its lowest layer, and
diagnostic emission cards obscured the interpretation. The visual correction
removes those cards and qualifies actual native upward smoke transport in the
separate warm-plume scene. The milestone is local usable integration. Physical calibration, combustion,
radiation, thermal return, native desktop integration and human visual/usability
acceptance remain separate. Methanol-specific expansion is paused. The next
boundary is operator review of this workflow, followed by a selected measurable
realism requirement rather than an automatic chemistry expansion.

## Paired inert-box controls

`box_control` and `inert_box` expose the matched stationary-box qualification.
Both start from rest and use 10% authored heat transfer; the box is unlit,
insulated and fixed. Selecting either preset fills its required settings.
Existing baseline and 75% warm-plume profiles are preserved. See
[results, reproduction and limits](fire_fluid_box.md).

## Isolated plume review (2026-10-10)

`isolated_control` and `isolated_box` center the source and select ambient open
sides, limited transport and the same held wind channel. Their live 8³ grid is
for development review. The paired fine-grid spatial accuracy gate is explicitly
unqualified; see [qualification](fire_fluid_isolated.md) before interpreting
local heat or smoke predictions. Finer prescribed-source renders are separate
review artifacts and do not replace the live journal's numerical fields.
