# Bounded nested macro-linear velocity coarse correction

Two exact coupled component blocks pass the original cube in 52.6 seconds, but
on the admitted 18816-tet base hit 180 seconds at momentum residual 2.43e-5,
continuity 2.00e-8, about 1197 MiB. Local sweeps alone do not address global slow
momentum modes. No larger field is accepted or path adopted from that control.

Build an explicit P1 macro-vertex velocity interpolation onto the original P4
retained trace DOFs. Original affine macro tetrahedra and the existing essential
boundary constraints define this space; injectivity follows from its retained
free vertex rows. Verify shared trace weights, exact vertex identity, constant/
affine reproduction before boundary projection, boundary exclusion and permutation
consistency on anisotropic/refined meshes. No pressure mode or physical matrix
entry is removed. Coarse operators are exact Galerkin projections Z^T A Z of the
unchanged positive velocity block, factored with the installed exact Cholesky.

Use a fixed balanced SPD two-level inverse:
B = C + (I-C A) S (I-A C), C=Z(Z^T A Z)^-1 Z^T,
where S is the proved fixed coupled block sweep. Prove dense formula, symmetry,
linearity, positivity, exact coarse action B A Z=Z, exact principal/coarse factors,
original FE/RHS/pressure/reconstruction preservation and owned partial cleanup.
Record coarse dimensions/storage/setup and total cost; no shifted/floored/scaled
physical or coarse operator, altered residual/force or relaxed resource cap.

Start original L4 against accepted exact fields with one fixed local sweep;
allow two fixed sweeps only if measured diagnostics warrant. Only useful accepted
original controls may extend to the admitted count6 base, then the exact stopped
23616-tet normal cube. All full FE/divergence/flux/energy/publication gates remain;
requested target misses must be explicit. Keep all failed controls. Existing
chunk size 512 may be used with measured headroom. An accepted normal is permission
to resume force refinement measurement, not physical certification. Physical
component/domain/raw-reaction/stress qualification, native traction, authored
objects and transient/outlet/inertial-wake follow separately. No native/shared
API/dependency/version/commit/package/install/deploy change; extraction deferred.
The full goal and Stage 1 remain open.
