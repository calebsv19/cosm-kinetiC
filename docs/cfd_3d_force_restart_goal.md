# Resumed force qualification: recommended next bounded sequence

Exact count6 normal L4 now passes numerical gates on the original stopped mesh
(170 iterations, 97.8s, 1555MiB, full residual 4.81e-11). Pressure/viscous changes
still exceed 1%, raw surface/reaction mismatch remains 2.108%, so physical Stage 1
is open. Retain accepted snapshots and all rejected preconditioner/resource evidence.

1. Matched L8 held-normal test is complete: 400 iterations,126.34s /1560MiB,
   full original residual8.74e-11, unchanged caps and complete numerical gates.
   Pressure/reaction length changes1.953%/1.418%,raw/reaction2.073% fail physical
   1% gates. Signed gap stays~0.837mN; end Jacobian condition reaches543 while
   inner mesh conditioning is unchanged. Next screen end-slab subdivision with
   unchanged inner planes/body/flow and exact live resource admission. Do not
   interpret elongated-cell length sensitivity as a proven domain plateau.
   See [matched field and signed attribution](cfd_3d_normal_domain_checkpoint.md).
2. Attribute remaining raw force gap through signed pressure/viscous weak lifts,
   local volume/jump contributions and mesh quality. Test a small quality-aware
   refinement of edge/face and surrounding cells against this exact base/normal
   evidence. Keep domain/flow/equations matched. Require component/scalar/raw force
   gates and independent full FE/divergence/flux/energy/resource/publication proof;
   do not infer force accuracy from smaller global indicators or total reaction.
3. If additional refinement/domain exceeds caps, pursue a bounded cheaper fixed
   SPD preconditioner using this accepted exact field as the comparison anchor:
   global pressure/coarse correction and nodal-vanishing velocity complement are
   candidates. Preserve every physical mode/equation and original full residual;
   rejected component/coarse paths demonstrate momentum stall and time limits.
4. Only after reference physical qualification, compare the native stationary
   aligned-box solver with matching boundary/flow/unit/force conventions. Then
   qualify authored fixed boxes, followed separately by transient/outlet/inertial
   wakes and measured cost/recovery. Curved/moving bodies/free surfaces/atmosphere
   stay distinct later work. Native/desktop/package/release evidence remains separate.

No resource relaxation, artificial pressure floor/shift/mode removal, changed load
or premature physical/native certificate. Broad goal remains active; the bounded
ability-to-resume-force-testing objective has been achieved.
