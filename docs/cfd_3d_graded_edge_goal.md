# Prospective graded edge/normal controls

Before numerical factorization or any geometry results, declare four profiles:
edge first-strip0.05m or0.04m with current side-normal0.03125m; normal0.025m with
unchanged surface; coupled edge0.05m/normal0.025m. Same vertex/element counts as the
accepted graded pair. Original cube geometry, fluid volume, boundary areas,
reflections/YZ symmetry and paired translated inner cells must hold. Surface node
redistribution is explicit; no uniform/nested refinement claim.

Geometry allowance2048MiB/600s,120000tet. Global worst shape and Jacobian condition
must not increase over the accepted graded parent for numerical selection. Local
quality is fully reported. The survey is not a field or force-error certificate.
Signed stress observations guide selection only after their own identity checks.
Any new numerical field requires an explicit archived geometry/source contract and
unchanged strict physical gates under the existing8192MiB/1800s allowance.
