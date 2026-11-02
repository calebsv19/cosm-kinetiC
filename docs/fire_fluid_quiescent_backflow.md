# Explicit quiescent-backflow momentum and kinetic accounting

The source/headless open-atmosphere worker accepts the optional wind member:

```json
"momentum_backflow": "quiescent_exterior/v1"
```

Omitting this member preserves the legacy copied-velocity exterior donor.
Unknown values are refused. The option is part of the immutable request and
checkpoint configuration digest. A checkpoint cannot switch policy on resume.
This is an opt-in warm, incompressible Fire–Fluid development control, separate
from installed desktop controls and release adoption.

## Boundary meaning

For each advected momentum component, an upwind donor outside a pressure
reservoir has zero exterior advected velocity. This includes incoming exterior
donors in the lateral and top MAC dual-cell momentum fluxes. It does not zero
the solved normal boundary-face velocity: that velocity still follows the
existing predictor and pressure projection. Normal boundary faces themselves
are degrees of freedom inside the discrete domain, not exterior donors.

Prescribed inlet faces, impermeable ground, aligned no-slip insulated boxes,
channel walls and interior donors retain their existing operators. Outgoing
interior donors retain their values and reconstruction. The pressure operator,
pressure datum, scalar ambient inflow, diffusion, CFL limit and acceptance
tolerances are unchanged. This is not a full traction or nonreflecting boundary
condition, and it does not guarantee a physical inflow profile or zero pressure
work during backflow.

## Discrete kinetic energy

`kinetic_stored_j` is the native MAC energy
`K = sum_faces (rho * dual_volume * u_face^2 / 2)`.
The dual volume is a full cell volume for interior and tangential unknowns and
half a cell volume for normal unknowns on physical domain boundaries. Blocked
faces have exactly zero velocity. The Python reader independently recomputes K
from native f64 checkpoint velocities with the same stated geometric weights.

The ten signed cumulative entries of `kinetic_ledger_j`, in joules, are:

| Index | Entry | Discrete meaning |
|---|---|---|
| 0 | initial | K of the admitted initial velocity |
| 1 | advection_boundary | Old-velocity work of momentum fluxes on outer dual-control-volume faces adjoining pressure reservoirs |
| 2 | advection_interior | Remaining old-velocity work of conservative momentum advection |
| 3 | viscosity | Old-velocity work of the explicit viscous stencil, including wall ghosts |
| 4 | buoyancy | Old-velocity work of the warm buoyancy acceleration |
| 5 | explicit_euler | Nonnegative quadratic term `sum(m * (dt * acceleration)^2 / 2)` |
| 6 | inlet_enforcement | K after prescribed predictor faces minus K before enforcement |
| 7 | projection | K after pressure correction minus K before projection |
| 8 | roundoff_closure | K after the existing volume-closure correction minus K before it |
| 9 | arithmetic_roundoff | Measured predictor delta minus its linear-work and Euler-quadratic expansion |

The ledger closes `K = sum(ledger)`; native and independent reader gates refuse
nonfinite or inconsistent accounts. Cumulative entries may have either sign,
except initial and the Euler quadratic term. Boundary advection is a discrete
work contribution, not the sampled continuum `rho/2 * |u|^2 * u.n` integral.
Projection includes boundary-pressure work and numerical projection effects;
it is not labelled pure dissipation. The ledger measures those effects rather
than hiding them in a claimed physical energy-conservation law.

The entire candidate ledger publishes only after momentum, scalar positivity,
contrast, scalar conservation and air-volume checks pass. A rejected step leaves
accepted fields, scalar receipts, kinetic ledger, clocks and counters unchanged.
Native restart restores and checks the ledger as well as its stored-energy
identity. Derived pressure/solver scratch is not checkpoint state.

Heat and smoke retain their independent extensive scalar budgets:
`stored = initial + source + reservoir_inflow - reservoir_outflow`.
The new kinetic ledger does not turn viscous loss into scalar heat or imply
thermodynamic total-energy conservation, compressible hot-fire qualification,
or gas-to-Fire thermal feedback.

## Fine-grid live coupling and retained journals

The source host accepts explicit top-level
`"journal_codec": "lossless_zlib/v1"` or
`"journal_codec": "lossless_f64_zlib/v1"`. It stores canonical checkpoint JSON as
losslessly compressed SQLite BLOBs in the same atomic checkpoint/operation/head
transaction. The f64 variant packs long float arrays as exact little-endian IEEE754 byte
planes and deduplicates verified field/state arrays before compression. Decode
restores independent arrays and the original canonical JSON bytes. Numerical
values, JSON numeric types and scientific digests remain unchanged. It
admits at most 512 MiB on disk and 256 MiB per decoded checkpoint. History stays
compressed until each record is requested. Legacy TEXT journals retain their
64 MiB cap. No old journal is rebound or converted implicitly.

Within one Framework object, a verified prefix may reuse validation only while
all persisted payload bytes, metadata binding, head count and source/worker
hashes still match. A fresh object performs full chain, native-state, source
consumption, accepted-prior return and exact-clock validation. Corrupt, truncated,
trailing-data and unsupported-codec records are refused. Replay and failed
publication preserve the existing transaction semantics.

An optional top-level `native_packet_steps` bounds native subrequests inside a
single coupling window. Every subrequest uses ordinary exact restart and
acceptance, then the final state and fields are revalidated against the entire
window’s original state, clocks and sources. This preserves the0.1s hold and
atomic joint publication while retaining the120s native capture limit. It does
not add Fire exchanges or adapt the momentum timestep.

`scripts/coupling/live_gust.py` runs actual native Fire evolution on each
exchange window, passing the accepted prior gas velocity to Fire, then consuming
that window's exact Fire heat/smoke in the native gas solver. It supports nested
32³ and64³ gas grids with fixed physical source, inlet and obstacle geometry.
Its optional selected f64 field files are analysis views; joint SQLite
checkpoints remain numerical authority. Copied successful native-attempt
payloads retire only after accepted journal publication, recorded terminal
verification and active-consumer checks. Failures remain preserved.

Verification entry points are `make test-quiescent-native`,
`make test-quiescent-sanitize`, and `tests/test_quiescent_backflow.py` with the
same pinned Fire, Fluid and clock environment variables used by driven-wind
tests. Use a fresh contained `BUILD_DIR`; do not clean retained evidence.

The reusable local source CLI is `scripts/fire_fluid_live_gust.py`. Supply
`--growth-root`, `--fire-worker`, `--fluid-worker`, `--clock-worker`, `--grid`
(32or64), optional `--box`, and a fresh `--output`. Its default is the f64
lossless journal;64cubed box runs use5native steps per packet by default.
`--native-packet-steps` can explicitly select1–20. It requires the existing
CodeWork consumer-check helper before native admission. No remote route or
installed app is selected implicitly.

Coupled source accounting also preserves the Growth-owned SI partition
`generated_energy = transferred_energy + untransferred_energy`. Each accepted
window verifies that gas input increases by exactly that window’s transferred
energy. The Fire heat-proxy field used for the centroid is not SI stored
thermal energy; it must not be added to gas heat or kinetic energy. Buoyancy
work is reported mechanically but is not subtracted from scalar heat. These
are explicit source, scalar and mechanical accounts, not a closed global
thermodynamic-energy theorem.

Zero-step checkpoints also report the actual initial MAC kinetic energy for
admitted moving legacy-wind initial states. The dedicated
`tests/test_quiescent_initial_energy.py` covers this initialization, exact
native resume, and still-start/evolved parity with the live-study worker.
The live histories keep their original worker identity; the receipt-only
source-current fix does not rebind historical evidence.

## 2026-10-29 — Live wake timestep and voxel references completed

The completed64³ gas-timestep-half pair changes final whole MAC velocity
by 0.456%and Fire-plane wind by 0.374%.
At that same gas timestep,64→80³ changes them by 3.245%
and 3.640%. Both full live journals independently reopen;
source/scalar/mechanical accounting and selected native field bytes are checked.
See [reference capacity, comparisons and remaining accuracy limits](fire_fluid_wake_reference.md).
