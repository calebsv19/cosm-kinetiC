# Isolated Fire–Fluid plume qualification

2026-10-10 Main Edit development. This original 8³/16³/32³ checkpoint is
preserved. The subsequent [source/transport accuracy checkpoint](fire_fluid_accuracy.md)
passes the same coarse observation gate at 64³ while retaining finer heat limits.
Boundary transport verification passes.
The isolated plume and inert-box spatial accuracy gate is **unqualified**.
Do not promote this checkpoint as calibrated or sufficiently resolved fire flow.

## Boundary model and verification

The opt-in `ambient_open_xy` policy replaces horizontal periodic seams with
zero relative pressure at half-cell lateral reservoirs, zero-gradient predictor
velocity and reference-temperature clean inflow. Scalar diffusion has zero
normal flux at reservoirs. The no-slip insulated floor and open top remain.
This is a pressure-reservoir model, not a qualified full-traction/nonreflecting
boundary. All six boundary-face heat/smoke inflow/outflow ledgers persist in
native checkpoints. X/Y upper normal faces are appended after the existing
Z top faces; legacy periodic state layout and numerical path remain supported.

Independent manufactured pressure equations with/without the box pass native
and sanitizer gates. Periodic sinusoidal cell-average translation crosses a
complete period in ±X and ±Y: relative L1 error decreases across 8/16/32 cells
and is 2.63% at 32. No lateral mass loss occurs. Compact prescribed packets
leave ambient sides in all four directions, never appear at the opposite edge,
and reproduce exact full native checkpoint recovery. Their initial-mass-normalized
L1 shape error decreases from 47.0% at 16 cells to 24.8% at 32 and 10.25% at 64.
This meets the declared 15% fine packet threshold; it is not a high-accuracy
wall-flux or hot-gas qualification. The earlier coarse failed result is retained.

## Centered coupled controls

Domain 1×1×1 m. A centered 0.5×0.5 m Fire patch starts at [0.25,0.25,0] m;
its central 0.25×0.25 m region is ignited. Fixed 8×8 Fire nodes, source loading
3, 10% synthetic heat transfer, 0.125 m injection depth. Fire dt 0.02 s, Fluid
dt 0.005 s, exchange 0.1 s, duration 3.2 s. Both controls start from rest.
Limited MUSCL transport is selected. Accepted-prior wind feedback is enabled;
reciprocal thermal feedback remains disabled. The box is stationary, unlit,
insulated and no slip, [0.375,0.375,0.375]–[0.625,0.625,0.625] m.

Both live controls pass immutable replay, exact recovery and staged-publication
rollback. Six matched native 8³/16³/32³ runs use exactly the same accepted no-box
Fire source frames. Solid fields and all blocked normal faces remain exactly
zero. Final heat balance residuals are below 4e-13 J and smoke below 6e-19 kg.
Peak matched temperature stays below 310.25 K and the unchanged contrast limit.
At the finest below-box probe, vertical wind changes from 0.08096 to
0.06101 m/s. Fixed-world probe differences decrease with refinement.

## Failed spatial gate and passed domain-size gate

The declared successive-grid scalar threshold is 10% on a common 8³ observation
partition. Fine-pair heat/smoke relative L1 differences are:

| Case | Heat | Smoke | Spatial gate |
| --- | --- | --- | --- |
| No box, 16³→32³ | 22.11% | 16.64% | Failed |
| Box, 16³→32³ | 22.30% | 16.66% | Failed |

Differences decrease but remain too large. For no box, the heat in the first
0.125 m slab changes from 85.09 J to 67.73 J, while the next slab changes
from 65.16 J to 82.63 J. Accounting closes; the heat distribution is unresolved.
This isolates the source-layer/buoyant transport boundary for subsequent work;
it does not establish whether temporal lag, forcing or scalar reconstruction
is the dominant cause.

A separate 1.5×1.5×1 m case adds equal lateral margins, keeps 0.0625 m cells,
and translates the identical source amounts and observation coordinates
together. In the original central region, heat differs by 0.200% and smoke by
0.178%. This passes the 10% domain-size check for this duration/source only.
Very little scalar reaches the plume's outer boundary in 3.2 s; the prescribed
packet tests establish actual escaping-scalar accounting separately.

## Review and next boundary

The workbench exposes `isolated_control` and `isolated_box` as development
review profiles. Its live grid is 8³ and is explicitly not accuracy qualified.
Independent 32³ prescribed-source native snapshots use actual accepted fields,
exact mask geometry and matching camera/optics; they are not live 32³ feedback
journals. Earlier periodic checkpoints/renders remain preserved.

Next: qualify source-layer heating, buoyancy forcing and scalar transport with
known solutions and separate time/grid refinements before ignitable object
faces, material response or room-scale acceptance. No canonical adoption,
commit, VERSION transition, package, install, Registry or publication occurred.

## Reproduction

```sh
make BUILD_DIR=build/<fresh-isolated-proof> open-atmosphere-worker \
  test-open-atmosphere-native test-open-atmosphere-sanitize \
  test-open-lateral-native test-open-lateral-sanitize
python3 -B scripts/fire_fluid_isolated_qualification.py --stage boundary \
  --growth-root <Growth-Main-Edit> --fire-worker <native-Fire-worker> \
  --fluid-worker <fresh-open-worker> --clock-worker <native-clock-worker> \
  --output <fresh-boundary-evidence>
python3 -B scripts/fire_fluid_isolated_qualification.py --stage plume \
  --growth-root <Growth-Main-Edit> --fire-worker <native-Fire-worker> \
  --fluid-worker <fresh-open-worker> --clock-worker <native-clock-worker> \
  --output <fresh-plume-evidence>
```

Read the summary's status: successful execution of the qualification runner
does not mean an accuracy gate passed. Preserve a failed summary and reproducer.
