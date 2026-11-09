# RC1 local isothermal chemistry

Renderer-free, opt-in source tool; not linked into existing application modes.
`reactor.h` admits at most 16 species, 32 directed elementary reactions and four
synthetic elements. First/second-order mass action uses mol, m3, seconds and kg/mol.
Reverse reactions are separate directed channels. Explicit fixed-step RK4 advances
reaction extents and coupled inventories; invalid intermediate/final states refuse
without mutation. No clipping, adaptive error controller, stiff solver, transport,
EOS, solvent properties or thermal prediction is provided.

Build into a new directory:

```
python3 scripts/build_chemistry.py --output /absolute/fresh-build
CHEMISTRY_LIBRARY=/absolute/fresh-build/reactor.dylib python3 -B tests/test_chemistry_reactor.py -v
python3 -B scripts/chemistry_reactor.py --library /absolute/fresh-build/reactor.dylib --config /path/network.json --steps 200 --checkpoint /path/new-checkpoint.json
```

The CLI emits JSONL initial and every accepted state, rates, cumulative directed
extents, mass and element/charge residuals. Checkpoint output refuses existing files.
It is a single-writer local export, not a crash-durable publication system. Checksum
checks corruption, not hostile alteration. Resume binds exact library bytes and full
configuration including dt, volume and initial state; float JSON roundtrips preserve
binary64. Restart is bitwise identical for the same engine, platform and step sequence.

Source API callers supply correctly allocated structs and output arrays. Python
adapter validates serialized input before constructing native structs. Supported
model is explicitly synthetic/isothermal; unsupported phase/property claims refuse.
For real chemistry this requires a separate validated data/mechanism stage.

## RC2 prescribed spatial transport

`transport.h`/`transport.c` add uniform fully occupied1D/2D/3D cells with shared
constant D, prescribed constant velocity and periodic/sealed boundaries. Upwind
advection is first-order in space; centered diffusion is second-order. SSPRK2
transport composes between RC1 half reactions. The combined temporal order is
qualified on the selected nonlinear case; no general spatial mixing claim.
All stages commit together or leave the entire accepted field unchanged.

```
python3 -B scripts/build_chemistry_transport.py --output /absolute/new-transport-build
CHEMISTRY_TRANSPORT_LIBRARY=/absolute/new-transport-build/transport.dylib python3 -B tests/test_chemistry_transport.py -v
python3 -B scripts/chemistry_transport.py --library /absolute/new-transport-build/transport.dylib --config /path/rc2.json --steps 20 --checkpoint /path/new-spatial-state.json
```

Spatial schema: `rc2/v1`, nested RC1 chemistry plus n,boundary,spacing_m,velocity_m_s,
diffusion_m2_s,dt_s,reacting,initial_mol. Field arrays are x-fastest; n=1 axes set
thickness via spacing and require zero velocity. At most4096cells. Sealed axes also
require zero velocity. No open boundary or flow solve. JSONL exposes full amounts,
concentrations, totals, cumulative domain reaction extents and global budget residuals.
Reaction extents stay with fixed cells as history; they are not transported species.
Checkpoints bind entire config, binary and tick; same-engine continuation is exact.
Full fields/history can grow with cells×steps; callers select small bounded proofs.

## RC4 periodic solved flow

`flow.h`/`flow.c` reuse the existing Cartesian viscous/pressure solver operators.
Solutal Boussinesq unsteady Stokes is an explicit creeping-flow model, not a general
Navier–Stokes or liquid EOS replacement. Only buoyancy uses relative density contrast.
Limits: <=1% contrast, Re<=0.1, fully periodic3D, >=4cells/axis, <=4096total. First-order
backward-Euler flow and lagged composition force; face species transport uses the
actual divergence-free solved velocity, SSPRK2 upwinding/diffusion and RC1 reactions.

```
python3 -B scripts/build_chemistry_flow.py --output /absolute/new-flow-build
CHEMISTRY_FLOW_LIBRARY=/absolute/new-flow-build/flow.dylib python3 -B tests/test_chemistry_flow.py -v
python3 -B scripts/chemistry_flow.py --library /absolute/new-flow-build/flow.dylib --config /path/rc4.json --steps 20 --checkpoint /path/new-flow-state.json
```

RC4 JSON extends RC2 fields with schema rc4/v1 and flow: model
periodic-unsteady-stokes-solutal/v1, rho_kg_m3, nu_m2_s, gravity_m_s2,
beta_m3_mol, reference_mol_m3, feedback and initial_velocity_m_s (3*cells lower
faces, component-major). Top-level prescribed velocity must be zero; boundaries all
periodic. Concentration coefficients follow stable mechanism species order.

Outputs add solved velocity, reduced pressure, relative density and mechanical
work/viscous-loss/BE-time-defect ledgers. The work is external buoyancy work, not
chemical heat. Flow, chemistry and observations publish together or refuse unchanged.
Same-engine checkpoints preserve exact fresh-process continuation. No new solver
history is needed by this backward-Euler flow update. Original RC1/RC2 source/API
and retained worker identities are unchanged.
