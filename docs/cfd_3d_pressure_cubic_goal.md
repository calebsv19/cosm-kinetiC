# Twenty-direction cubic global pressure correction on the exact cube

Distinct target-only preconditioner after exact-target audit0d62ca6a8852ed290468adf24553cb9fcd65f25759ca76e6431c0e5618fb5966.
Previous ten-direction390iter/156.923s/1520.375MiB strict full field remains valid
but fails>=10percent performance gate; all older policy/velocity-hierarchy/mesh
failures stay sealed. Global pressure basis, not prior rejected macro-cubic velocity
hierarchy, extends complete quadratic1,x,y,z,x2,y2,z2,xy,xz,yz with ALL ten cubic
monomials x3,y3,z3,x2y,x2z,xy2,y2z,xz2,yz2,xyz at existing macro centers. Keep
constant/all directions; mass-orthonormal QR must reproduce complete span, rank
loss refused. Reuse sealed balanced mass/coarse inverse and unchanged approximate
Float velocity Schur columns (-D+Btranspose Float(A)^-1 B)Z. Require SPD/coarse
relative skew<1e-5/reproduction<1e-5, no physical regularization or projection.
Original full pressure/momentum/FE/mesh/load/Float identities remain unchanged.

Exact added reservation for20columns:8*(3*np*20+8*np+4*nv+8*20^2)+2MiB. Fresh numeric
admission adds this to both flexible6 bases/work, factor/scratch/currentRSS+32MiB
reserve. Every setup/solve/full reconstruction/observation/serialization phase
keeps1800MiB/180s/3000iter/50000tet, retained1e-11/full1e-10, flux/div1e-8/energy.03.
Independent polynomial/full-span/SPD/coarse/complement/Schur-sign/invalid tests and
small original L4 body2 complete numerical/physical-equivalence control precede
ONE new exact33216tet L8 cube. Small comparison to sealed ten-column field must
preserve original identities/Float-input and p/rawv/r/Pin/D<1e-7; control is numerical
readiness, not a smaller substitute for target qualification. No retries/cap increases.
L8 compares ten-column and accepted mass fields with original identities/Float and
physical output equivalence<1e-7; require>=10percent iterations OR setup+solve
improvement versus accepted mass, whole/owned<=1.1 both old fields, and no>10percent
setup+solve regression versus ten-column. Only complete target proof qualifies
optional exact-cube recipe; default/general/native/desktop remain separate.
Original raw/reaction1.757percent and raw domain energy gates remain failures.
After useful target proof, resume quality-constrained force refinement; broader
Stage1/general objects/physical qualification goal remains active.
