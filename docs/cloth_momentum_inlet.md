# Native interior momentum inlet and cloth coupling proof

PhysicsSim now exposes opt-in `cfd_open_atmosphere3d_step_momentum` for integrated
MAC face impulse in N·s. The inlet is component-major native order and applies
J/m using existing face dual mass before pressure projection. Initial support
is interior forcing, periodic XY, ground/open top, without obstacle, wind or
weather. Boundary-layer forcing, wrong size, nonfinite values and forced CFL
refuse. Receipt/accepted fields commit only after projection and scalar gates.
The ordinary step and existing worker JSON contract retain their behavior.

The corresponding CMS private native trial advances17×17pinned cloth and8³cold
gas through0.050s with a causal held field, exact staged interval overlap and
whole-joint rollback. It explicitly converts interleaved exchange IDs to native
component-major indices. No offline future fluid endpoint or Fire/thermal
continuation is used. Actual feedback changes fluid0.003197589m/s and cloth
0.779382µm versus the matched one-way control. Four failure boundaries and
checkpoint/retry preserve exact joint state. This is a short first-order lagged
drag proof, not impermeability, pressure traction, calibrated textile or general
long-time stability qualification.

`make test-open-momentum-inlet OPEN_MOMENTUM_TEST_ROOT=/private/tmp/NEW_ROOT`
uses a fresh contained root; select strict/headless/sanitize with
`OPEN_MOMENTUM_TEST_MODE`. The inlet's86checks pass in all three modes, and the
existing ordinary open-atmosphere regression passes. A captured pre-edit owner
and current default path reproduce3653binary64 values exactly. Full details and
joint build instructions are in CMS Main Edit `docs/cloth_fluid_two_way.md`;
compact evidence is `data/cms_audits/20261102-cloth-fluid-two-way/`.

Implementation is in the existing native owner plus a role-specific `.inc`, so
explicit-source legacy workers still link without a source-list migration.
No worker JSON field, Desktop refresh, source version, shared source, Registry,
remote or release action is part of this proof.
