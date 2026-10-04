# Graded cube stress localization and next field

2026-10-04, PhysicsSim Main Edit. Independent signed stress observation of the
accepted graded L4 field completes1142.261s, sampled1101.156MiB, within the declared
3072MiB/1800s diagnostic allowance. All original P4/DG-P3 snapshot/DOF, strict
residual, conservation and energy checks remain. The original signed volume and
interior-facet stress algebra is unchanged; only input/resource guards and routing
change for the larger field. All saved inputs are hash-checked before/after.

The maximum signed identity error is4.8097e-14N. Raw-minus-weak signed drag is
-0.00055629785165N in both0.25m and0.4m lifts. Innermost0.025m centroid-band
contributions are-0.00043481092103N and-0.00042951660600N, respectively78.16% and
77.21% of the signed total. Highest-ranked cells remain near the front side edges,
about0.02310m from an edge. Centroid allocations and scores are diagnostics, not
clipped physical bands or force-error bounds. No weak/reaction force substitutes
for raw pressure or viscous traction.

Eight geometry controls cover bothL4/L8 and four profiles: body edge-strip0.05m,
edge-strip0.04m, normal0.025m alone, and edge0.05m/normal0.025m. Cube volume,
boundary planes/areas, actual cell reflections/YZ exchange and paired translated
inner cells pass for all eight. Elements remain81792/100608.

Both edge-only profiles retain global worst shape21.450/42.442 and Jacobian
condition107.872/181.230. The thinner-normal profiles worsen those metrics and fail
the predeclared prospective screen; no factor or field is launched for them.
The edge0.04m profile slightly improves volume-weighted shape3.2874→3.2779 and
4.8308→4.7936. Surface nodes move on unchanged cube planes; no nested/uniform
refinement is claimed. Two support controls reproduce actual archived meshes,
paired inner cells and exact observer source transforms.

The completed identity plus78/77% near-edge signed contributions permit the next
edge0.04m numerical trial. Its prepared field adapter changes only geometry/import
routing and receipt schema. Original guarded resource stop,8192MiB/1800s/120000tet,
complete Float factor as preconditioner, unchanged Float64 physical action/full FE,
all pressure modes, full1e-10/retained1e-11 and atomic publication remain.
The separate pre-result selection contract permits L8 only if an accepted L4 field
reduces raw surface/reaction mismatch at least10% relative; original physical1%
force/refinement/scalar gates remain. The reference is not physically qualified
until complete field and physical comparisons pass.

`build/c3d-graded-stress/checkpoint-audit.json` seals the completed observation,
signed arrays, sources and geometry controls. The separate
`build/c3d-graded-edge-field/` lane owns pending/new numerical trials and their
physical assessment. This diagnostic checkpoint does not assert those trials have
completed or passed, and does not adopt a native/default/package change.
