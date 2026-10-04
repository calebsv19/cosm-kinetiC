# Next: bounded global pressure correction on accepted reference meshes

Classification PROGRESS: eight actual body remeshes were screened and rejected;
no geometry/factor/field adopted. Preserve these quality gates and all failures.
Do not retry the family with relaxed transition-region acceptance.

Investigate a ten-direction coarse-pressure correction (complete quadratic spatial
polynomials including the constant) applied only to the preconditioner on original
accepted meshes. Retain all original global and local DG pressure directions in the
physical operator and full residual. Use a balanced mass/coarse inverse; construct
coarse Schur actions using the existing coupled Float velocity inverse plus the
unchanged condensed pressure block. Independent synthetic SPD/block tests must
prove coarse reproduction, symmetry/positivity and complete uncorrected directions.
Record Float approximation/asymmetry; reject singular/invalid coarse controls.

Measure original L4 calibration, then normal L4, against exact same mesh/operator/load
identities and prior retained-margin controls. Admit additional live basis, coarse
arrays/setup scratch and unchanged both Arnoldi bases under actual1800MiB/180s/
3000iteration/50000tet; retain retained1e-11/full1e-10 and all physical checks. Full
reconstructed FE remains authoritative. Adopt only measured useful cost/accuracy;
optional current full/modes/caps cannot be weakened. If controls show promise,
continue matched L8 and fixed-domain finer-body storage readiness. No native/default
change or certification. Broad goal stays active.
