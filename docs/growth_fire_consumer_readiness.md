# GrowthSim Fire consumer readiness and bounded implementation plan

Assessment: 2026-10-04, after the CFD owner paused at its finite stationary-box
checkpoint. This pass preserves that Main Edit source as a development checkpoint.
It does not resume the broader CFD research goal or implement a thermal consumer.
Canonical PhysicsSim has a separate active release owner and pending version/docs
work. Leave it untouched; canonical adoption must reconcile that lane separately.

## Readiness decision

The retained Main Edit is a buildable foundation for bounded consumer development.
Begin with offline validation/admission and a conservative allocation oracle.
It is not yet a physical Fire-to-fluid implementation. The current masked obstacle
backend is steady Stokes with time zero, no thermal energy/temperature state, no
qualified source energy transfer, and no persistent session restart. Existing
emitter buoyancy/density scales are a model scaffold, not a joule/kelvin consumer.

GrowthSim Main Edit source 1e24d71 delivers the offline surface source v1 producer
and explicit Clang/fisiCs policy. Compiler accepted baseline cd1673aa supports it.
The cross-program interface is the JSON frame/bundle contract; the GFSRC001 binary
worker protocol remains private to GrowthSim. Numeric equality does not migrate
source stream identity. Always pin the worker/adapter bytes and source event digest.

Relevant PhysicsSim entry points:

- Native session ownership: `include/app/cfd_3d_session.h`, `src/app/cfd_3d_session.c`.
- Obstacle operator: `src/app/cfd_obstacle3d_mixed.c`, `include/app/cfd_obstacle3d.h`.
  The private mass member is unused by the current factory/build; do not infer
  transient support from its existence.
- Host admission/control: `scripts/agent_session/service.py`, `protocol.py`,
  `cartesian3d.py`; CLI/MCP reuse the same local service.
- Existing cooperative solver checkpoint callbacks are not serialized restart.
  Service capability currently declares `checkpoint_restart: false`.

## Baseline verification and qualification limits

The paused owner is idle. Its stationary-box seal matches current source and saved
artifacts (27 source, 26 evidence and 7 protected identity hashes). A fresh audit
worker builds in `build/growth-fire-readiness`, leaving sealed workers/reports alone.
Fresh box backend/session ordinary and ASAN/UBSAN checks, box scene controls and
existing cube/Cartesian controls are recorded there. All 865 newly pending Python
source files parse. Source whitespace checks are separate from physical validation.
The checkpoint retains earlier 2D/3D source and extensive reference experiments as
history; this is not a new qualification of every experimental solver variant.
Generated binaries, build results and virtual environments are not source commits.

Use [stationary-box checkpoint](cfd_3d_box_checkpoint.md) and
[startup specification](cfd_3d_obstacle_startup_spec.md) for authoritative measured
limits. The example pressure/viscous force grid changes remain 6.72%/2.93%; cube
reference raw discrepancies remain about 1.13–1.20% against the original 1% gate.
These block certified force claims, not pure source admission or independently
qualified thermal/startup development. Preserve original equations, failures and
thresholds; do not restart open-ended mesh research for this coupling project.

## Work sequence and stopping points

1. **Offline source admission, without solver mutation.** Implement a PhysicsSim
   adapter for `growth_sim_fire_surface_source/v1` and bundle v1; no live producer
   process. Apply strict shape/number/budget/geometry/time/calibration/provenance
   checks and the contract's numeric digest normalization. Explicitly declare
   model, accepted calibration and supported stationary XY surface. Do not trust
   the presence of a digest without recomputing it. Equal event/digest replays;
   conflicting content, order or unsupported inputs reject with journal/cursor and
   simulation unchanged. Stop at positive prescribed/zero/native frame receipts
   plus malformed/nonfinite/duplicate-key/tamper/order/conflict rejection controls.
2. **Conservative quantity allocation, still independent of fluid evolution.**
   Split half-open intervals across unequal substeps at the declared uniform rate,
   and surface dual areas onto an explicit physical receiving face/cell map.
   Preserve J and kg totals and unapplied remainders; no clamp, nearest-node
   copying or implicit arbitrary-surface interpolation. Reuse the prescribed
   6000 J / 0.0008 kg fixture: temporal allocations 600/1800/3600 J and
   0.00008/0.00024/0.00048 kg. Stop at deterministic space/time allocation proofs.
   For the first native surface, use a commensurate box-top patch: native box
   [1.5,.375,.375] to [2.25,1.125,1.625], grid [32,16,16], and GrowthSim 7x7 nodes
   with h=.125 m, origin [1.5,.375,1.625], normal +Z. This is a proposed explicit
   mapping fixture, not an implemented attachment or general geometry capability.
3. **One physical transient-obstacle foundation.** Follow the existing startup
   specification: masked mass/hierarchy support, current/older accepted history,
   private candidate publication, BE then BDF2, and transient momentum/work budgets.
   Qualify one pressure-driven cube-from-rest case with the independent semi-
   discrete time oracle and original residual/conservation/cancellation/memory
   gates. Keep zero-mass steady cube/box behavior bitwise unchanged. Stop at one
   accepted startup packet. No nonlinear wake, arbitrary body or new reference
   mesh investigation belongs here. Admission/allocation may precede this step;
   physical evolving obstacle/thermal response requires it or an explicitly
   different independently qualified receiving backend.
4. **Thermal quantity state and independently tested energy transport.** Choose
   the admitted temperature/enthalpy model, rho, heat capacity, conductivity,
   reference temperature and boundary/outflow policies. Convert interval J to
   source rates using the actual timestep/receiving volume once. Do not equate
   authored fuel heat, tracer density or velocity increments with temperature.
   Start without buoyancy: insulated known-volume heating must satisfy
   delta_T=Q/(rho*cp*V), with separate transport/diffusion known answers and
   temporal/spatial refinement. Smoke kg begins as a declared passive tracer;
   bulk fluid mass changes require a separate formulation. Stop at energy/tracer
   conservation and zero-source baseline parity, not a visual plume.
5. **Restartable transactional consumption.** Persist fluid histories, thermal/
   tracer state, admitted/applied cursors, event digests, remaining intervals and
   immutable receipts in one accepted checkpoint. Candidate failure/cancel does
   not consume source or advance time. Prove partial-interval restart, repeated
   event, conflict/out-of-order and crash/retry behavior. This is required before
   a restartable end-to-end consumer is declared; current session restart is absent.
6. **Qualified buoyancy and one offline vertical slice.** Select gravity and an
   explicit small-temperature-change Boussinesq or other justified model. Preserve
   momentum/energy terms and verify a manufactured/known-answer buoyancy case,
   refined response and applicability limits. Do not clip results into low-Re
   admission. Run one frozen GrowthSim frame stream into one accepted fluid
   checkpoint, reporting accepted/applied/retained/outflow/loss/remainder budgets.
   Export full fields with identity-linked receipts for copied renderer input.
   Absolute body-force accuracy remains provisional unless separately qualified.
7. **Local MCP and scene usability after backend acceptance.** Expose inspect,
   admit, step, cancel and result through existing supervision; same authoritative
   validators and transactions as offline use. Advertise actual capabilities and
   limitations. Real tool acceptance follows source tests. Installed refresh,
   canonical adoption, releases, moving surfaces, wind feedback and bidirectional
   coupling are separate subsequent boundaries.

Immediate next bounded implementation is steps 1–2, returning admission/allocation
receipts and original solver parity. The next backend prerequisite is the one
already specified transient startup. This keeps contract progress separate from
physical capability and gives each batch a concrete acceptance endpoint.

## Evidence and source adoption

`build/growth-fire-readiness/source-inventory.json` records every pre-existing
pending source byte. `readiness.json` records current checks and the committed
checkpoint. The new source commit is a development checkpoint, not an installed
package, release, universal CFD certificate or adoption of canonical release work.
The report intentionally does not certify all historical reference experiments.
Keep the CFD owner's earlier sealed results and failed cases unchanged.


## Superseding body-free receiving direction

The later user-selected general-atmosphere direction uses a body-free periodic
passive transport qualification domain, rather than making masked obstacle startup
a prerequisite. See surface_source_admission.md and passive_atmosphere.md for the
completed offline batches. The earlier obstacle sequence above remains historical
and applies when an actual receiving case introduces obstacles; it is not the
current next gate for body-free transport.
