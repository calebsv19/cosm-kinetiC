# Gust/wake diagnostic and pressure-backflow limitation

Completed18fixed-source native diagnostic cases at8/16/32/64³with cold,
timestep-half, minmod, insulated-box and isolated boundary-candidate controls.
Ordinary64³refused at2.335s because CFL0.8011775exceeds unchanged0.8; full
failure reproducer retained. Gas pressure/divergence gates pass before refusal.
Cold32³mean return wind0.321686m/snearly matches warm0.319984m/s; halvingdt
changes full velocity~0.474%. Dominant late growth is not Fire heating or time
step. Pressure backflow copies interior momentum into incoming exterior donors;
reconstructed kinetic-advection rates and single-change controls isolate it.

A copied-source candidate supplies zero exterior advected momentum on wind
pressure backflow; ordinary solver source/defaults remain unchanged. Cold32³
fields match exactly through1.2s, then final KE is97%lower and decays after
shutoff. Candidate64³completes3.2s with original gates and exact native resume.
Candidate32→64³sampled wind sensitivity remains15.45%and full velocity
10.79%; no spatial/wake convergence or live fine-grid Fire coupling claim.

Fresh strict/sanitizer probes,3analysis tests, ordinary-worker roundoff parity,
existing original/candidate native pressure/inlet/box/rollback strict+sanitizer
controls pass.17distinct long cases complete; one deliberate original64³CFL
failure and one separate8³sanitizer control are recorded honestly. Selected
native fields/charts, exact source/worker identities, compact observations and
failure reproducers retained; successful unselected dumps/scratch retired after
hash/consumer checks. Primary result:
`_private_workspace_artifacts/fire-fluid-gust-wake-20261028/RESULT.md`.

Next: qualify the quiescent-backflow momentum contract and fully discrete
mechanical-energy ledger, expose explicit native policy, then repeat live fine
coupled gust/wake comparisons. Candidate is diagnostic-only and unadopted.
No commit, canonical/VERSION/release/install/Growth/shared/remote change. Minor
release recommended for added diagnostic workflow; approval before version bump.

## Reproduction and scope

The standalone `src/tools/physics_sim_gust_refinement_probe.c` uses existing native solver functions and same retained Fire packets. `scripts/coupling/gust_diagnostics.py` supplies common-grid conservative restriction, independent MAC sampling checks and fixed physical downstream observations. The task result contains exact compile commands, source/binary hashes, input recipes, case tables, native field layouts, snapshots and comparisons.

Ordinary air/scalar accounting and pressure/divergence checks do not establish mechanical-energy stability of pressure backflow. The current zero-gradient momentum ghost allows nonzero exterior incoming momentum. The candidate changes only incoming exterior advective donors for wind pressure reservoirs; it preserves prescribed inlet, solids, outflow and all numerical gates. It is an isolated copied-source diagnostic, not a new selectable runtime boundary policy. No general energy-stable boundary or calibrated wake accuracy claim follows.

The minmod comparison changes both momentum and scalar reconstruction. The binary probe uniformly divides each original source packet among native gas steps; ordinary-worker and original16cubed replay parity quantify the tiny rounding difference from cumulative packetization. The journal limit is unchanged. These are gas replay diagnostics, not live fine-grid Fire feedback.

[Detailed results and native charts](../../../_private_workspace_artifacts/fire-fluid-gust-wake-20261028/RESULT.md).

Subsequent source development formalized the [explicit quiescent-backflow option and discrete kinetic ledger](fire_fluid_quiescent_backflow.md), then ran actual32/64cubed live Fire–Fluid gust/wake histories. The original fixed-source diagnostic and its failure/candidate evidence remain frozen; use current_truth.md and the successor API for the current implementation.
