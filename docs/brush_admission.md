# 2D brush numeric admission

Main Edit brush coordinate mapping now clamps normalized float coordinates before
conversion to int, with the existing post-conversion grid clamp retained. This
keeps ordinary, negative and off-screen placement semantics while preventing
extreme native integer window coordinates from reaching an out-of-range cast.
The already-admitted allocation/config extent bounds the grid dimensions.

Brush application now requires positive window dimensions, known density/velocity
mode, finite supplied velocity and initialized fluid fields. Effective injection
velocity and all three target-cell additions must be finite before any write.
Existing nonfinite target density/velocity is refused. The target-cell clamp
matches Fluid2D's actual interior clamp, including its two-cell edge case. Refusal
preserves density and both velocity fields together; a successful velocity write
cannot precede a failing density operation, or vice versa. Unknown modes and
invalid window dimensions now refuse instead of taking legacy fallback behavior.
The accepted operation order and existing Fluid2D injection routines remain.

Reuse: existing backend grid admission and Fluid2D density/velocity methods are
retained. Numeric admission is an app-owned native brush boundary; core_math and
core_space do not own this input/state publication policy. No shared source/API/
version or adoption minimum changes. This does not admit every fluid value,
assert physical parameter bounds, repair prior nonfinite state or qualify later
solver stability; it only avoids an unsafe cast or a new nonfinite brush result.
Initialized truthful native storage remains required.

Validation passes 38 distinct methods: 20 actual brush cases and 18 freshly rebuilt
grid-identity regressions. The 20 brush cases also pass targeted ASan/UBSan and
float-cast-overflow instrumentation; repeated methods are not added to the count.
Native tests link the real full backend/runtime-field source, Fluid2D and existing
support. Accepted masks/fields are compared with the original Fluid2D injection
route using ordinary density/velocity, negative, edge, off-screen, extreme integer
and two-cell fixtures. Refusals cover nonfinite input/target values, scaling and
addition overflow, invalid mode/window/grid and NULL sample; all three field byte
arrays remain unchanged. Source-only hooks confirm no brush allocation/free.
Support objects are not all instrumented. No unsafe old-code sanitizer control
was launched. Seventeen safe old-code controls are terminal: six normal cases
pass and eleven invalid cases violate the new refusal/nonmutation contract.

Both existing Make 2D backend/runtime-field contracts pass in their own fresh
profile. The actual changed production backend object compiles through Make.
The brush-admission-20261007 profile retains normal PGM/asset bytes and source
hashes. Matching repeat preserves 18 outputs; clean preview admits 34 owned files,
with no cleanup applied. The packet retains a pre-turn backend snapshot and
incremental patch so pre-existing CFD/backend changes remain distinct.

All current tests, builds and proof sessions are terminal. Prior historical
sanitizer process holds remain unresolved and separate; this slice does not claim
reaping, restart or replace their binaries. No complete GUI link, installed app,
human native acceptance or numerical CFD accuracy qualification is implied.

Next numeric/inventory boundaries include object-motion world-coordinate conversion
before integer clamp and object/import/emitter inventory admission. Later emitter/
scratch publication and budgets, complete process recovery, retirement, canonical
adoption and independent newer backup coverage remain open. This packet is outside
the frozen seventy-packet snapshot. No commit, canonical/shared mutation, install,
release, transfer or user-evidence deletion occurred. Full TL01-TL13 remains active.

Evidence: data/experiments/lifecycle-validation/20261007-brush-admission.
