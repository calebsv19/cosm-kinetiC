# Wind Orientation Probe

`tools/wind_orientation_probe.py` is a headless debug workflow for comparing
the current Wind drag-pressure proxy across object orientations. It is not a
drag-coefficient calculator and does not perform final surface-force
integration.

The helper:

- loads a `scene_runtime_v1` Wind scene
- optionally lists probe-eligible objects
- clones the scene once per orientation into an output directory
- applies absolute rotations or relative rotation deltas to one selected object
- runs `physics_sim_headless`
- writes text and JSON summaries with object drag-pressure proxy, projected
  area, pressure delta, throughput, vorticity, and final diagnostic frame paths

## Fixture Smoke

```sh
make -C physics_sim test-physics-sim-headless-wind-orientation-probe
```

This is the portable probe. It runs the checked-in
`tests/fixtures/runtime_scene_wind_tunnel_3d_mesh_wedge_wide.json` mesh wedge
fixture with the `roll-sweep` preset and may be used as a local deterministic
headless gate.

## DragonWind Smoke

```sh
make -C physics_sim test-physics-sim-headless-dragonwind-orientation-probe
```

This is a local-system probe, not a portable stable gate. It expects the
persisted DragonWind runtime scene at:

```text
/Users/calebsv/Desktop/Simulations/scenes/dragon_wind/scene_runtime.json
```

That scene must contain an active `extensions.physics_sim.wind_tunnel` block.
Use `DRAGONWIND_ORIENTATION_PROBE_SCENE=/path/to/scene_runtime.json` to run a
different scene.

Do not add the DragonWind target to `test-fast`, `test-stable`, package
self-tests, or worker validation unless a future pass adds a checked-in
synthetic stand-in or stages the scene through an explicit bundle.

## Object Discovery

```sh
python3 physics_sim/tools/wind_orientation_probe.py \
  --runtime-scene /path/to/scene_runtime.json \
  --output-root /tmp/wind_probe \
  --list-objects
```

Supported probe object types are `mesh_asset_instance`, `box`, and `sphere`.

## Common Options

- `--object-id <id>`: select the object to rotate.
- `--preset roll-sweep`: run baseline, roll45, and roll90.
- `--preset dragonwind-roll`: run authored, roll45, and roll90.
- `--preset axis-check`: run baseline plus pitch/yaw/roll checks.
- `--rotation-mode absolute`: replace the object's rotation.
- `--rotation-mode relative`: add each orientation to the authored rotation.
- `--require-wind-tunnel`: fail unless the runtime scene has active Wind setup.

## Interpretation

The reported `object_drag_pressure_proxy` is a solver/analyzer debug proxy. It
is useful for comparing current object orientation behavior inside this Wind
lane, but it is not a final aerodynamic coefficient. True coefficient work
should wait for the force, units, and surface-integration contract.
