# Screen a scalar sparse graph for the existing Float preconditioner

The conforming L4 second-normal cut preserves the surface triangles and original
macro partition but requires 41216 tetrahedra. Its block3 Float numerical stage
estimate is2474949680bytes, above1800MiB; no numeric factor or field was attempted.

Declare a separate representation experiment: expose the identical rounded Float
velocity coefficients as scalar lower CSC in node-interleaved coordinates. Omit
only exact zero coefficients; no threshold, shift, scaling or pressure mode
restriction. Keep the complete original float64 mixed operator, load, equations,
P4/DG-P3 full residual, restart60 flexible iteration and physical/resource gates.
Use public Accelerate blockSize1 symbolic ordering with unchanged caller-owned
factor/scratch lifecycle. Record scalar input ownership and every extra allocation.
Prove conversion and coordinate action on independent symmetric coupled matrices,
Float factor solve/cleanup on a bounded fixture, original operator bit preservation,
and then screen the exact second-normal mesh under the same1800MiB/180s caps.
A symbolic admission is not an accepted inverse or field. Only if admitted may a
separate solve and original fullFE/force observer establish numerical usefulness.
Keep old block3 source and receipts immutable; integrate no physically unqualified
mesh. If scalar ordering is worse or inadmissible, retain the result and select a
distinct measured next experiment.
