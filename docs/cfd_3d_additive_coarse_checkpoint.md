# Exact additive cubic coarse/local checkpoint

Five support proofs establish the additive dense formula, symmetry/linearity/SPD,
exact Galerkin factor, cubic interpolation/left inverse/boundary constraints,
original FE/RHS/pressure/reconstruction and partial/100 owned cleanup. The initial
lifecycle test incorrectly expects balanced identity action; the retained failure
and corrected additive overlap expectation make the distinction explicit. No
balanced B A Z=Z property is claimed for this preconditioner.

Original L4 passes all unchanged numerical/resource/publication gates:
994 iterations, 38.3317 s, 624.46875 MiB owned peak, full residual 2.01586e-10.
The requested 1e-10 full-residual target is missed; unchanged 1e-8 numerical
acceptance passes. Same-mesh field/force/scalar equivalence passes; physical
raw/reaction mismatch remains about 3.64358%. Lower per-application cost improves
time versus the 40.7-second balanced cubic control despite more iterations.

Exact admitted count6 base stops at 180 s, sampled peak 1464.96875 MiB, last
iteration 1300 retained momentum 2.18277e-9, continuity 1.16405e-11. No accepted
refined field, full FE verification or force result exists. This fails the full
resource/publication contract even though its retained estimate is small. Reject
larger adoption/normal admission; no target/cap is relaxed.

Evidence: `build/c3d-additive-coarse/checkpoint-audit.json`, five support proofs,
two immutable receipts and one accepted original field. All predecessor/native
source and protected workers remain unchanged; no version/commit/package/install/
deploy change. Physical accuracy, Stage 1 and the full goal remain open.

Next: [invertible cubic/quartic coordinate block preconditioner](cfd_3d_hierarchical_goal.md),
which factors all component couplings within both polynomial blocks.
