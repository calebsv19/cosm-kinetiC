# Bound restart storage before changing the complete velocity inverse

Compensated pruning is not useful:1e-2 misses original1e-10 at3000iterations;
1e-3 normal passes but costs~21%more time/~4.5%more peak memory while saving only
~2.15%factor storage. A fresh complete Float control has the same cost/memory as
zero pruning. Retain all failures and pressure coefficient/mass/global-mean
readbacks. Adopt neither approximation nor zero-pruning representation. Preserve
the useful same-surface L4 force result and still-withheld matched L8 field.

Next make a distinct outer-storage experiment using the unchanged complete Float
velocity factor, negative macro pressure mass and full float64 physical mixed
operator. Calibrate restart60,24,12 on the original cube and accepted normal mesh.
Change only the restart length and its complete V/Z/work reservation. Do not
replace flexible right-preconditioned Arnoldi, two-pass orthogonalization, actual
residual checks/reconstruction, pressure modes, load, physical equations, full
1e-10 target or1800MiB/180s/3000iterations/50000tet limits. Keep restart60 as the
existing/default control. Source/receipt identities and both basis capacities
must match the selected restart; an optimistic basis estimate cannot admit a
larger run. Record every extra restart, momentum/continuity residual and whole
process cost, with a fresh matched control if apparent improvement depends on
historical allocator variability.

Only a complete numerically useful original/normal short-restart candidate may
stage-screen and solve the exact33216-tet matched L8/end2/second-normal cube.
Keep all force components/scalars/raw mismatch and signed stress identities, with
unchanged separate1%physical gates. If short restarts stall, do not expand iteration
or time caps. Inspect coarse global pressure corrections as a separate subsequent
preconditioner experiment with complete physical pressure space retained, explicit
restricted spectrum scope and measured setup/storage/convergence costs. No global
pressure null-space certificate or forward-error bound follows from a1e-10 residual.

Stage1 physical qualification, native pressure/traction, stationary-object authoring,
transient/outlet/wake and general-object behavior remain open. These controls are
reference evidence, not installed desktop/native certification.
