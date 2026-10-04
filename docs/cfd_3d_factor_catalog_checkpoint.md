# Exact normal cube accepted: force convergence testing resumed

The bounded preconditioner/residency investigation reaches its immediate goal.
The exact previously stopped count6 normal mesh has an accepted immutable full
field and a completed base-to-normal force comparison. Original mesh, free DOFs,
RHS and complete velocity/coupling/pressure scalar hashes match stopped predecessors.
No equation, load, pressure mode, scale, target, gap or resource cap is relaxed.

Complete 3x3 vector blocks share every original velocity coefficient with exact
caller-owned coupled Cholesky; all intercomponent directions remain. Lossless
indexed full RHS and deterministic unused FE metadata/catalogs are restored bitwise
after factor destruction, before full FE verification and publication. Six new
controls, sealed vector/load/metadata proofs and installed Dofs/Basis/affine API
snapshots verify owners, numbering, all tables/scalars, anisotropic/refined action/
inverse, nonzero RHS/reconstruction/full FE, and fail-closed corruption/budget checks.
Explicit GC remains rejected. No old failed receipt or field is overwritten.

| Accepted reference control | Tets | Iterations | Receipt wall s | Owned peak MiB | Full FE residual |
| --- | ---: | ---: | ---: | ---: | ---: |
| Original L4 | 4992 | 150 | 16.061 | 385.609 | 9.80190e-12 |
| Count6 L4 base | 18816 | 160 | 127.062 | 1281.406 | 3.15008e-11 |
| Exact count6 L4 normal | 23616 | 170 | 97.775 | 1555.250 | 4.80554e-11 |

All meet requested 1e-10 and unchanged full FE/flux/divergence/energy/resource/
publication gates. Normal flux error 2.14874e-11, maximum volume divergence
1.70886e-10 and energy imbalance 2.40187e-11. Normal diagnostics use existing chunk
512; matched original/base use 128. Table times do not establish mesh-size speedup.
Base whole-process memory is worse than load-only/vector controls; preserve those
as smaller-grid choices. Adopt catalog suspension as an optional measured path to
accepted exact normal force testing, not a universal performance default.

Normal detached coordinate/index/affine array bytes are 23078360 and full catalog
table bytes 8168552. Symbolic-only estimate 1755.981 MiB admits an attempt. Its
numerical live guard independently estimates 1793.294 MiB with original 32-MiB
reserve; actual completed peak is 1555.250 MiB. Estimate/current RSS/API reports/
owned high-water remain separate; no peak reset. Caps remain 50000tet/1800MiB/
180s/3000iter. Full array ownership and original physical equations remain intact.

Base-to-normal changes: pressure force 1.117284%, raw symmetric viscous force
1.010313%, reaction .418502%, inlet pressure .290427%, dissipation .290866%.
Raw surface/reaction mismatch improves only 2.171233% to 2.107967%. The combined
1% physical force gate fails. This is numerical admission plus resumed force
assessment, not physical qualification or native certification.

Independent observers pass signed elementwise stress identities (maximum errors
2.43e-14/3.02e-14 N). Volume equilibrium defect falls .19847760 to .16594355
(16.39%) and interior stress jump .02544673 to .01988256 (21.87%). Global weighted
volume indicators are largely in centroid buckets farther than .4m from cube edges;
these are un-clipped centroid diagnostics and are not force error bounds. Alfeld
tet Jacobian condition max remains 215.3734 (median about 18.3). Use signed force
lifts and quality-aware refinement, not a global indicator alone, to select tests.

Evidence: `build/c3d-factor-catalog/checkpoint-audit.json`, `force-readiness.json`,
three accepted fields/receipts, exact normal stage and two independent observers.
Native sources/workers, all earlier fields/rejections/checkpoints remain preserved;
no native/shared API/version/commit/package/install/deploy change. Stage 1 and broad
object/wind-tunnel goal remain open. See [next physical gates](cfd_3d_force_restart_goal.md).
