# Bounded native stationary-screen inlet

`cfd_open_atmosphere3d_step_screen_momentum` is a separate opt-in API for the
CMS-owned stationary uniform surface-resistance bench. It retains the existing
open-reservoir momentum/projection/scalar/commit operators. Original ground/
interior momentum admission and default paths are unchanged.

The closed profile is a unit 8³/16³ cube, periodic XY with two open Z pressure
reservoirs, cold rho1.2/mu1.8e-5/dt0.005, top gauge pressure0 and bottom±0.4Pa,
normal uniform flow<=2m/s and at most20native steps. Only uniform-per-plane
interior normal-Z passive impulses, absolute total<=0.002Ns/step, are allowed.
Source heat/smoke, boundary/tangential/nonuniform/nonfinite impulses and
unsupported geometry/transport/driver modes refuse. Numerical uniformity is
checked at1e-10m/s, without field clipping. Receipt commits only on full native
success. The source is in `src/app/cfd_open_atmosphere3d_screen.inc`.

Final strict/optimized and ASan/UBSan native proofs pass. CMS's24cases at0.100s
measure actual native pressure/flux and match uniform-screen momentum/energy
and startup references. Raw late-scalar receipt/accepted-field rollback,
profile refusals,20step cap and empty-screen ordinary parity pass. Original
inlet86checks and ordinary-atmosphere suite pass C11strict. Old moving-cloth
240Hz physical prefix exact to retained proof (source identity excluded).

CMS owns resistance, layer gather/scatter, native measurement/correction and
configuration/checkpoint CLI. PhysicsSim owns the fluid admission and operators.
No worker JSON, standalone headless CLI, app menu, package or remote feature is
added. Existing frozen jobs/source proof are not rebound. This does not qualify
arbitrary porous meshes, high-pressure textiles or a general pressure boundary.

See CMS `docs/cloth_pressure_screen.md` and CodeWork evidence
`data/cms_audits/20261102-cloth-pressure-screen/REPORT.md` for inputs, selected
cases, editable examples, native histories and limits. Source VERSION0.5.0 held;
no release, installation or app launch. The original inlet script now includes
all matching native includes in its identity, and worker rebuild prerequisites
track them. No unrelated fluid/structural behavior is changed.
