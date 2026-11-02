# Bounded Fire–Fluid wake references

The source headless adapter has an explicit `capacity_profile: "wake80/v1"`
for one 80³ wake reference. This uses the existing CFD operators and numerical
acceptance checks. Ordinary requests keep their 64-cell axis and 262,144-cell
limits. The reference is a numerical sensitivity fixture, not a general
80³ accuracy qualification.

## Physical fixture

The profile requires all of the following:

- Grid 80 × 80 × 80 in the original 1 m unit cube, with gas timestep 0.0025 s.
- Ambient open horizontal boundaries and a solid ground with open top.
- Explicit `quiescent_exterior/v1` momentum backflow.
- The stationary insulated, no-slip box from 0.375 to 0.625 m on each axis.

The comparison keeps the 17 × 17 Fire surface, its 0.02 s native timestep,
physical source footprint and 0.125 m injection depth. The inlet gust starts
at 0.4 s and stops at 1.2 s; its absolute gas-step knots are therefore 160 and
480. Accepted-prior gas velocity returns to Fire every 0.1 s. The full fixture
uses 32 exchange windows through 3.2 s.

The obstacle spans 16 cells at 64³ and 20 cells at 80³. Fire resolution stays
fixed, so these comparisons isolate changes in the gas discretization within
the existing live coupling.

## Capacity and publication

`open_atmosphere.validate` and the native worker both require the explicit
profile before admitting 80³. Unsupported profiles and mixed sparse movie or
domain-qualification modes are refused. The normal grid cap stays in effect
when the profile is absent. Native numerical allocation remains limited to
512 MiB; request and output captures remain limited to 256 MiB each, with the
existing 120 s capture timeout. The fixture selects 512,000 maximum cells and
the unchanged one-billion scalar-work-cell ceiling.

The explicit journal option `lossless_f64_zlib_1g/v1` uses exactly the same
lossless byte-plane f64 payload encoding as `lossless_f64_zlib/v1`, with a
separate 1 GiB on-disk ceiling. The ordinary option keeps its 512 MiB ceiling;
decoded records remain capped at 256 MiB in both modes. Numerical values,
types, signed zero, state hashes and atomic publication retain their existing
meaning. Historical journals retain their original worker and control bindings.

The fixture uses `native_packet_steps: 10` inside each unchanged 40-step gas
exchange window. Every native subpacket is accepted independently, followed
by full-window revalidation before joint publication. This adds no Fire
exchanges and does not adapt the timestep. Selected binary fields are analysis
views of accepted SQLite checkpoints, rather than separate numerical authority.

Use `coupling.framework.Framework` with an explicit fixture configuration for
this reference. The existing `fire_fluid_live_gust.py` recipe CLI offers its
32³ and 64³ cases. Always select fresh task-owned output, verify worker hashes,
and preserve failed attempts. Successful copied payloads may retire after
publication, terminal-process verification and active-consumer checks.

## Comparison method

The temporal pair compares 64³ at 0.005 s and 0.0025 s gas timesteps, holding
Fire, exchange cadence, physical gust and geometry fixed. The spatial pair
compares 64³ and 80³ at the same 0.0025 s gas timestep. Both are actual live
Fire evolutions driven by their own accepted prior gas wind.

Compare fields at the same physical times. Extensive heat and smoke use exact
Cartesian volume-overlap weights. MAC velocity uses face-area overlap and
linear interpolation only in the normal direction, preserving integrated
discrete divergence. A separate 16³ common partition has normal faces aligned
with both grids and corroborates the nonnested mapping. Native Fire-plane
vectors are also compared directly at the unchanged feedback points.

The downstream measurement region is x = 0.625–0.875 m and
y,z = 0.375–0.625 m. Report reversal with both the raw sign and a fixed
−0.001 m/s deadband. Distinguish local wake and returned wind from whole-domain
kinetic energy. Source SI partitions, gas scalar budgets and the signed MAC
kinetic ledger retain their separate accounts.

Reversed volume classifies each native cell by its centered Ux and counts its
full volume. It is a grid-dependent approximation to the backflow region. A
second classification after conservative remapping to the same 64³ grid helps
distinguish that sampling effect from the native-grid counts.

These are paired coupled sensitivities. They do not add into a percentage
error budget, establish spatial convergence, or qualify a physical hot-fire
wake. A finer gas reference also does not refine the fixed Fire surface.

## Measured live comparison

Both full live histories through 3.2 s and independent restart checks pass.

| Controlled change at 3.2 s | Whole-gas velocity L2 | Native Fire-plane wind L2 | Wake ROI velocity L2 |
|---|---:|---:|---:|
|64³ gas dt = 0.005 → 0.0025 s|0.4561%|0.3741%|0.3290%|
|64³→80³ at gas dt = 0.0025 s|3.2449%|3.6401%|4.8316%|

Gas resolution produces the larger observed change in this fixture. The
aligned common 16³ MAC comparison gives3.0605%
for the spatial pair, corroborating the nonnested mapping. Each case has its
own live source history under the same accepted-prior coupling contract.

Native-cell reversal estimates are sampling-sensitive: the final 64³ half-step
case gives 13.3789%, native 80³ gives 14.1000%, and the 80³ field mapped to 64³ gives
13.3301% below the same 1 mm/s deadband. Interpret native volume counts alongside
the velocity norms and fixed-point feedback rather than as exact physical wake volume.

## Verification

The source fixture is covered by `tests/test_wake_capacity.py`, existing journal,
quiescent-backflow, inlet, ventilation and sensitivity regressions, and fresh
native strict and ASan/UBSan quiescent tests. Qualification also checks legacy
worker numerical parity and exact 80³ warm-box 10-step versus 5+5-step restart.
Use fresh contained builds; retain pinned workers and exact control closures
needed by scientific journals.

See [quiescent backflow and energy accounting](fire_fluid_quiescent_backflow.md)
for the momentum and conservation contracts.

## Additional controlled recipes

`coupling.wake_reference.gas_grid(base, grid)` prepares48³,64³ or80³ from
the existing still, unit-domain wake configuration at gas dt=.0025 s. It
preserves physical Fire, injection depth, box and wind while rebuilding only
the gas sampling and receiver masks. The48³ control completes a12/16/20-cell
obstacle sequence with64³/80³. All three grids align with the common16MAC
partition. This adds no new native capacity or solver operator.

`coupling.wake_reference.fire_surface(base)` prepares33×33 Fire nodes on
fixed64³ gas, doubling surface intervals and halving node spacing over the
same .75m footprint. Physical rates, ignition rectangle, calibration, Fire
clock and exchange cadence stay fixed. Initial ignition uses the native
clipped-dual-area overlap, so its integrated area is conserved to f32 rounding.
The existing Fire stability rule may increase internal microsteps on the finer
surface. Compare explicit source timestep controls before attributing the
whole change to spatial resolution. A common-wind replay is a source diagnostic;
it does not replace a live coupled history.

The recipes are covered by `tests/test_wake_reference.py` with the existing
four worker/root environment selectors. The completed live references are described below; recipe admission alone
carries no physical-accuracy claim.

## Three-grid and Fire-surface results

The48³ control and33×33 Fire /64³ gas case each completed32 exchange windows
through3.2s and independently reopened their exact joint heads. No solver,
capacity, physical forcing or acceptance threshold changed.

On the common16MAC partition,48→64 and64→80 velocity increments are4.2995%
and3.0605% of the same80³ norm; wake increments are6.0116% and4.4333%. At the
fixed17×17 Fire observation points, wind increments shrink5.3201%→3.6401%.
Their direction cosines are.982–.994. Unequal grid-spacing increments mean
this is a numerical trend, not a fitted convergence order or wake calibration.

Live Fire17→33 at fixed64³ gas changes its area-weighted heat-proxy field by
4.1130% and centroid by+4.8807mm, while the gas velocity changes.3324%, returned
wind at common points.9280% and wake velocity.1066%. Separate common-wind source
controls reproduce every original17×17 native Fire state; spatial source heat
changes4.1753% at dt=.02s and4.3337% at dt=.01s versus timestep changes.1777%
at17nodes and.1401% at33nodes. Initial area integrals match within native f32
rounding. Ignition rasterization and existing stability subcycling remain part
of the source discretization comparison.

All24 selected native f64 snapshots match authoritative journal bytes; native
feedback sampling, SI source/gas consumption and separate MAC accounts pass.
Final native wake and Fire surface figures were visually reviewed.36 Python
checks plus native48/Fire17/33 packet restart and conservative mapping checks
pass. Physical hot-fire accuracy and an asymptotic spatial order remain open.
