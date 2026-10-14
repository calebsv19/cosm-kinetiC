# Fire–Fluid inert-box checkpoint

2026-10-10, local Main Edit source milestone. One stationary interior box is
connected to the existing evolving warm plume and accepted-prior wind channel.
It is unlit, impermeable, no slip, and thermally insulated. No material heating,
ignition, motion, forces, radiation, or reciprocal thermal feedback is claimed.

## Geometry and reproducible controls

Domain 1×1×1 m, periodic horizontal sides, solid no-slip floor, open top.
Box bounds [0.125,0.125,0.375] to [0.375,0.375,0.625] m are identical on
8³, 16³ and 32³ grids. Source patch 0.75×0.75 m, 8×8 nodes; injection depth
0.125 m. Fire dt 0.02 s, Fluid dt 0.005 s, exchange 0.1 s, duration 3.2 s.
Both controls start from rest. Source loading is 3; authored heat transfer is
10% in all six spatial cases and both live coupled controls.

The prior 75% optical demonstration was rejected on the 16³ no-box mesh at
1.175 s by temperature-contrast admission. The 330 K ceiling is unchanged.
This is a bounded warm-flow source experiment, not calibrated combustion.

## Results and limits

Live box/no-box runs complete 32 exchanges with prior Fluid wind held for every
Fire window. Same-operation replay, full native checkpoint recovery, and
staged-publication failure preservation pass for both. Lifetime heat balance
residual is below 3e-13 J and smoke balance below 6e-19 kg at the final heads.

Refinement replays one immutable accepted no-box Fire source sequence to isolate
Fluid geometry/transport from changing Fire response. Native masks, all blocked
normal faces, and zero heat/smoke/pressure in solid cells are independently checked.
All 36/240/1728 blocked stored faces are exactly zero on 8³/16³/32³ box grids.
Peak final temperature is below 309.89 K in all matched cases.

On the common 8³ observation partition, box heat differences decrease from
26.15% to 20.27%; smoke from 21.95% to 14.79%. No-box differences likewise
fall from 26.22% to 20.27% and 22.04% to 14.79%. Fixed-world wind-probe L2
differences decrease from 0.007240 to 0.002249 m/s for the box, and from
0.007537 to 0.002591 m/s without it. These pass the declared decreasing-difference
gate; they do not establish an asymptotic order or tight numerical accuracy.
At the finest matched below-box probe, vertical velocity changes from 0.010682
to 0.006187 m/s; tangential components increase. Probes are volumetric flow
observations outside solids, not a qualified wall-stress or thermal sampler.

The new box pressure operator also passes an independently assembled manufactured
pressure equation. Native and ASan/UBSan contracts, full application compilation,
27 focused Python methods, 7 Growth carrier methods and the full stable suite pass.
An evolving nonzero 16³ no-box interval has exact full native field/state parity
against the pre-box worker. Two real 640×480 RayTracing snapshots use identical
camera/optics; no renderer source changes or airborne diagnostic cards.

## Reproduction

Build into a fresh task-owned directory:

```sh
make BUILD_DIR=build/<fresh-box-proof> open-atmosphere-worker \
  test-open-atmosphere-native test-open-atmosphere-sanitize
python3 -B scripts/fire_fluid_box_qualification.py \
  --growth-root <Growth-Main-Edit> --fire-worker <native-Fire-worker> \
  --fluid-worker <fresh-open-atmosphere-worker> --clock-worker <native-core-sim-clock> \
  --output <fresh-task-owned-evidence-root>
```

The runner retains exact source frames, six final native fields/configurations,
paired joint journals, per-window checks, wind probes and summary. Keep the input
and worker paths bound to a saved run; later source edits intentionally refuse
solver continuation under old source pins.

The workbench profiles `box_control` and `inert_box` exactly reproduce the live
controls. Selecting either fills wind 1, source 3, height 0.0625 m, exchange
0.1 s and duration 3.2 s. Start from rest and 10% heat transfer are fixed.
The selected inert-box scene is complete and saved; the no-box scene is also
available in the saved-run selector. The native solid mask is carried in VF3D,
and the six rendered box faces use the same physical bounds.

## Next boundary

Attach separately ignitable surface patches to stationary 3D object faces, with
local tangent frames and obstacle-aware source/return sampling. Cross-object
ignition remains a later surface-energy and material-response qualification.
This source milestone has no canonical adoption, commit, VERSION transition,
package, install, Registry, remote or publication effect.
