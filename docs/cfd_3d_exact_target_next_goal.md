# Next bounded improvement: cubic global pressure directions on the exact cube

The exact cube now has another strict full FE numerical field under unchanged
1800MiB/180s/3000iter/50000tet and original equations/modes. Ten-column correction
improves8.45percent, below10percent adoption gate; retain this failure and all
previous general cost failures. At iterations100/200/300, retained constant
continuity still dominates momentum (5.50e-7 vs5.21e-8,2.31e-9 vs5.93e-10,
1.61e-10 vs6.67e-11 in original RHS scaling); full reconstruction remains authority.
Mesh conditioning and full operator are unchanged, not repaired by storage.

Declare a distinct preconditioner-only20-column complete polynomial span through
cubic degree: retain existing1,x,y,z,x2,y2,z2,xy,xz,yz, add all ten cubic monomials.
Mass orthonormalize without dropping constant/any column. Extend balanced pressure
inverse only; old quadratic helper stays sealed. Require SPD/skew/reproduction
checks, independent polynomial span/coarse/outside-space/invalid tests, unchanged
full mixed/FE/Float identities and full targets. Before numerical factor update
exact pressure reservation for20columns; no physical pressure regularization or
residual projection. Previous 'cubic-coarse' rejection was a macro-cubic VELOCITY
hierarchy, not this global pressure candidate; preserve it, do not reuse its failed
fields or infer success. Inspect that scope/source before declaration.

One small full numerical control precedes one distinct exact target; no retries or
relaxed performance/physical/resource criteria. Compare against sealed ten-column
field and accepted mass baseline. Retain>=10percent useful benefit requirement and
full resource/force equivalence. Only useful exact-cube proof can qualify optional
reference recipe; general/native/default/desktop remain separate. Then resume
spatial force convergence with the verified baseline/new diagnostic fields, focusing
on signed downstream cube-edge traction defect and mesh-quality-constrained normal/
edge refinement. Raw/reaction1.757percent and raw Pin/D domain gates remain FAILED;
empty-duct-subtracted excess is diagnostic only. Broader goal remains active.
