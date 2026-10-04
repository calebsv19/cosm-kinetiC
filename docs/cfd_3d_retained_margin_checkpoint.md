# Matched-cube force convergence testing resumed

The exact declared same-surface second-normal33216tet L8 held-L4 field now passes
all field-publication checks. Whole160.383s, owned1619.547MiB, sampled1617.359MiB,
426iterations/71restarts. Full original FE residual1.179422e-11, momentum9.8296e-12,
continuity6.5179e-12, flux6.6019e-11, maximumdivergence2.4549e-9/s and energy8.9213e-11.
Requested full target stays1e-10, flux/div1e-8,energy.03,1800MiB/180s/3000iterations/
50000tet. Publication is atomic and follows all checks. The inner retained stop
is explicitly1e-11; terminal retained8.1561e-12 is an estimate, not full authority.

## Implemented and verified

Separate restart8/4 controls recovered exact memory admission, but8 normal divergence
and4 L8 time failures remain sealed. Separate6 control improved solve time but L8
still hit180s. Exact bounded observation reuse then proved analytical affine metrics
and bit-exact old/new accepted-field outputs. It reuses only identical per-chunk
velocity basis/gradient work: old combined31.767→21.789s,31.4%less time. All pressure
interpolation, integrals, quadrature/chunk order and independent boundary checks
remain; no persistent field cache. Fused L8 finished155.708s but failed maxdiv1.978e-8:
retain its unaccepted/unpublished result. No cap or physical gate was loosened.

The final distinct retained-margin controls pass three independent mixed-solution,
parameter and invalid-margin tests. Original186iterations/10.967s/full7.177e-12,
maxdiv4.086e-10; normal246/75.564s/full7.454e-12,maxdiv1.560e-10. Requested full1e-10
and stricter retained1e-11 are separately recorded. All original float64 physical
coefficients/load and reconstructed P4/DG-P3 pressure modes remain. Complete Float
coupled factor coefficients and original physical matrix identities match prior
controls. No pruning, mode removal, pressure pin, reduced residual, native/API or
desktop default change. Flexible default60 stays unchanged. Optional6/margin/fused
reference probes are calibrated for this bounded cube, not all objects.

Continuity dominates the iteration200 retained residual (~4.912e-8 vs momentum
4.136e-9); terminal full momentum9.830e-12 and continuity6.518e-12 both pass.
This is slow converging pressure/continuity work, not a terminal stall. Global mean
pressure differences and mass/force readbacks do not certify full pressure spectra,
nullspaces or coefficient forward error. Physical pressure space remains complete.

Preflight1,820,487,125bytes(1736.152MiB); fresh live admission1,807,789,525bytes
(1724.042MiB) includes exact factor1,125,998,956bytes, scratch38,745,361,32MiB reserve,
and both-basis/work reservation39,163,736. Actual basis arrays24,244,472bytes.
CurrentRSS varies between processes; preflight is not completed-run resource proof.
Global maximum Jacobian condition271.723 stays unchanged. Declared remesh near-edge
shape measures worsen (radius.05m worst6.532→7.724); no nested tetrahedron or general
mesh-quality improvement is claimed.

## Completed independent force comparisons

Saved meshes preserve the translated cube triangles and all23616held inner cells.
Force components below are streamwise N.

| Quantity | L4 same-surface second normal | L8 same-surface second normal | Relative change |
|---|---:|---:|---:|
| Pressure force |.024838233|.024912328|.2983%|
| Raw viscous force |.014217810|.014225992|.0575%|
| Reaction force |.039759879|.039838175|.1969%|
| Inlet pressure Pa |.022347467|.028070894|25.6111%|
| Total dissipation W |.000178979|.000224572|25.4740%|
| Raw/reaction mismatch |1.7702%|1.7567%|both exceed1%|

Body forces are stable under this domain extension, while complete combined1%
physical qualification still fails raw/reaction and global scalar gates. Preserve
those failures. A separate empty-duct baseline can diagnose length-dependent wall
resistance and excess obstacle pressure/dissipation; it cannot silently replace
existing global gates. Same-L8 prior normal→second-normal pressure/viscous/reaction
changes .1652/.3366/.1268%, inlet/dissipation .0701%; raw mismatch2.1040→1.7567%.
This is useful evidence from a declared non-nested remesh, not a certified mesh limit.

Independent signed full-stress observer passes135.710s/sampled505.141MiB and closes
both lift pressure/viscous elementwise identities to4.261e-14N. It accounts for
-.699856mN total raw-minus-weak gap. Shell.25 pressure-5.551425mN plus viscosity
+4.851569mN cancel; shell.4-11.637874+10.938018mN gives the same total. Retain separate
parts and original signed volume/interior face arrays; no corrected-force replacement.
Largest score1.048e-5N occurs about.020m from a cube edge (e.g x4.495813,z.480469,
body downstream/lower faces4.5/.5). Scores are diagnostic ranks, not force-error bounds.
Volume equilibrium norm.231542 and interior jump norm.027681 remain diagnostics,
not certification and not directly normalized across different length domains.

## Evidence and scope

`build/c3d-retained-margin/` contains frozen source/supervisor receipts, support
control proof, exact stage/live admission, accepted full field, saved-mesh force
comparisons, retained/full residual history, signed attribution and once-only audit.
Predecessor roots `c3d-restart-small`, `c3d-restart-six`, `c3d-observation-reuse` remain
immutable with every rejected attempt preserved. Native binary hashes remain intact.
Only current-truth/README preexisting docs change. No commit, package or installation.
The immediate force-testing capability is recovered. Stage1 physical/native and the
broader stationary/general/transient-object goal remain open.
