# Native execution cost of the original strict-convergent CG8 recipe

This is a distinct performance candidate after both fill-ranking gates failed.
Keep the ORIGINAL local graph, degree permutation, Float predictor, compensation,
P3 interpolation/physical Galerkin/Float3 coarse, fixed linear pressure proxy,
ten directions/complement10, original Double physical matrix and all stopping/caps.
Move only the existing cap-eight inner CG recurrence to one native FFI call. Invoke
exact existing vector/encoded physical action and existing IC factor solves, with
same coordinate bijection. Native Double dot sums can differ by roundoff; do not
claim bit-identical PC outputs. Full flexible outer and independent FE remain authority.

Seven caller-owned scratch vectors plus one output, no heap allocation inside C;
covered by unchanged40nv+24nc work reservation. Reject insufficient scratch, invalid
permutation, overlapping work/output/RHS/operator/factor arrays, nonfinite inputs,
nonpositive work/curvature and closed handle. Share original native implementation
as a byte-exact prefix; append only the fused action. Original graph construction
adds complete work reservation to fresh symbolic admission; both numerics retain
fresh original factor/storage/work/32MiB checks. All owners retire before full FE.

Independent dense/SciPy and real anisotropic FE comparisons, Double/encoded actions,
fixed pressure equivalence, bit-preserved inputs, short/zero/negative-curvature/
nonfinite/alias/permutation refusals, buffer lifetime, budgets and exact-source
transforms precede one paired original4992tet performance control. Use20 fixed loads
(ten physical pressure columns plus ten deterministic normalized random momentum
loads), three alternating paired batches, complete balanced velocity action timings.
Compare every output and positive work. Require max relative output difference<=1e-10
and median new/old complete-action time<=.95. This is a cost gate for an unchanged
mathematical preconditioner; do not require or claim pressure-column accuracy gains.
Reserve all loads/results/validation scratch before construction/numerics. No gate,
iteration depth, factor/pressure model or benchmark size changes after measurement.

Only useful control permits one full original small field with FE1e-10/retained1e-11,
original force/Pin/D equivalence1e-7 and whole<=23.5018023327s. All original physical
flux/div/energy/resource/atomic publication gates stay. Only useful small permits
matched28416tetL4 whole<=106.642085s and>=300MiB complete velocity factor/work saving
vs873198472bytes; only useful matched permits saved quality-passed finerL4 and force
convergence checks. Native/default/product adoption requires its own evidence.
No commits, package/install/deployment/shared change. Goal remains active.

Before any support or performance measurement, the cost eligibility threshold is
5% rather than15%: the unchanged full-field cost gate remains authoritative, and
the previous whole-run miss was only0.134s. SDK modern ILP64 declarations are
selected by compile flags; C prefix/source and warnings-as-errors stay intact.
