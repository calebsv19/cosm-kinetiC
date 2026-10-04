# Balanced ten-direction global pressure preconditioner on unchanged cube

Fresh bounded goal after balanced-body audit542c0bd7cf1c709863128aa37a560ecdecaaaf32155618d6c2ec0df116241d27.
Preserve all geometry failures, original equations/full pressure modes/fields/native.
Declare paired mass-control and quadratic-correction runs at original L4 body2
then normal L4 body6, exact prior retained-margin meshes, before matched L8.

Build Z from ten complete quadratic polynomials1,x,y,z,x2,y2,z2,xy,xz,yz at actual
macro centers, scaled spatially and mass-orthonormalized without dropping any
column. Constant direction remains in coarse and complete physical pressure space.
Positive approximate Schur action W=(-D+B Float(A)^-1 Btranspose)Z uses unchanged
condensed pressure block and coupled Float velocity inverse, preconditioner only.
C=sym(Ztranspose W) must be SPD; relative skew<1e-5 and coarse reproduction<1e-5.
Balanced pressure inverse E+(I-E S)M^-1(I-S E), E=Z C^-1 Ztranspose, uses cached
W: t=M^-1(x-W C^-1 Ztranspose x); result=t+Z C^-1(Ztranspose x-Wtranspose t).
Float Schur/roundoff are disclosed; no physical operator regularization or column
projection, and no full pressure nullspace/inf-sup/forward certificate inferred.

Independent synthetic tests verify exact SPD balanced matrix, symmetry/positivity,
coarse reproduction, nonzero action outside Z, mass-orthonormal polynomial span
including constants, invalid/singular refusal. Same exact float64 physical
operator/load/mesh identity, strict reconstruction full1e-10 and retained1e-11,
original flux/div1e-8, energy.03, force checks/caps unchanged. Flexible6 both basis
arrays/work remain exactly accounted. Declare additional coarse reserve bytes
8*(3*np*10+8*np+4*nv+8*10^2)+2MiB for live pressure tables/work, velocity RHS/solve
scratch and small dense matrices. Numeric-stage admission adds this reserve to
exact original Arnoldi reservation, factor/scratch/currentRSS+32MiB. Coarse setup
and solver phases retain1800MiB/180s/3000iterations/50000tet, including actual peaks.
The extra setup is not omitted from measured setup/whole costs.

Only paired numerical/physical-equivalence controls may admit matched L8. Require
same original identities and force/Pin/D changes below1e-7, iterations or setup+
solve time improve>=10%, whole time and owned peak no worse than paired mass by
more than10%. If either control fails/rejects cost/target, no matched L8 adoption.
Predeclare one pair per mesh, no retries or cap increases. Original raw1.757%force
and raw Pin/D domain gates stay failed; correction does not certify physics.
Only useful measured corrections are optional reference functionality; mass/default
restart60/native/desktop remain unchanged. Broad Stage1/general goal stays active.
