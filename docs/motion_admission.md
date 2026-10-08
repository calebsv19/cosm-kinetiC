# 2D whole-pass object-motion admission

Main Edit object-motion injection now admits the whole requested pass before
changing fluid velocity. Previously, an invalid later object or cell could leave
earlier updates applied, and extreme world positions reached integer rounding
before a grid clamp. The new path requires matching allocation/config extents,
positive window dimensions, initialized velocity fields and a consistent native
ObjectManager inventory (nonnegative count/capacity, count <= capacity, at most
65,536 objects, nonnull storage for nonempty inventories). Empty inventories
return without allocation. Truthful native storage and owner-thread exclusion
remain requirements; count/capacity metadata does not authenticate a C pointer.

Selected dynamic, unlocked objects require finite position and velocity. Float
coordinates clamp to [0,1] before rounding, then use Fluid2D's interior clamp,
including a two-cell grid. Finite out-of-window positions retain edge placement;
static/locked objects' unused numeric fields are ignored.

One bounded candidate array holds at most 65,536 updates (native struct size is
recorded by the build ABI; approximately 2 MiB on the current host). Updates sort
by cell and original object index. Each cell's original-order float additions are
simulated and checked for intermediate overflow before any actual fluid write.
After all cells pass, existing Fluid2D injection helpers apply the updates,
preserving addition order per cell. Invalid input, invalid target values,
intermediate/aggregate overflow or staging allocation failure preserves all
velocity bytes. Density is untouched for both acceptance and refusal. Candidate
storage is freed on every staged exit. The operation remains a void callback;
refusal is currently observable as no mutation, without a structured reason.

Reuse: existing app grid admission and Fluid2D injection remain. This is app-owned
native input/update policy; no shared source, API, version or adoption minimum
changes. The bound is local to this callback, not a universal scene limit or a
full backend memory/RSS budget. It does not repair pre-existing nonfinite state,
validate unrelated cells or prove later solver numerical stability. Global
runtime/emitter budgets and remaining inventory consumers still need work.

Validation: 66 distinct methods pass (28 motion, 18 freshly rebuilt grid-identity,
20 freshly rebuilt brush). The 28 motion methods also pass targeted ASan/UBSan
and float-cast-overflow instrumentation; repeats are not counted twice. Real
backend/runtime-field source and Fluid2D are linked. Source-only hooks verify
zero allocation on early refusal, one allocation and matching free on staged
paths, and no free of a failed allocation. Support objects are not all instrumented.
The admitted maximum count is exercised with initialized static objects. Valid
ordinary, negative, off-screen, extreme finite, two-cell, static/locked and
interleaved-cell order cases compare whole fields with Fluid2D's original-order
route. Late invalid objects/targets and overflow cases assert whole-field byte
preservation.

Seventeen bounded old-code controls are terminal: nine normal cases preserve
behavior; eight invalid cases violate the new nonmutation contract. No old invalid
inventory reads, nonfinite/extreme position casts or old sanitizer control were
launched. Historical held sanitizer attempts remain separate and unresolved;
this packet makes no claim that those processes were reaped.

Both existing Make 2D backend/runtime-field contracts pass in a fresh profile.
The changed production object compiles through Make. Normal converter PGM/asset
bytes and input hashes match baseline. Matching repeat preserves 18 outputs;
read-only clean preview admits 34 files, with no cleanup applied. The build log
retains an existing signed/unsigned comparison warning in shape_asset_input.c;
no warning appeared in the changed motion function or contract builds.

All current build, harness, test and proof sessions are terminal. This is source
qualification, not a full GUI link, installed-app/human acceptance or physical
CFD accuracy result. Remaining work includes other object/import/emitter numeric
and inventory boundaries, later storage ownership, coherent recovery, retirement,
canonical adoption and independent backup of newer evidence. The frozen
seventy-packet archive does not cover this packet. No commit, canonical/shared
mutation, installation, release, transfer or user-evidence deletion occurred.
Full TL01-TL13 completion remains unproven and the goal stays active.

Evidence: data/experiments/lifecycle-validation/20261007-motion-admission.
