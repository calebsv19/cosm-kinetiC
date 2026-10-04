# Distributed macro P2 Galerkin correction with cheap controlled-fill local factor

Predecessor313a494da5be28f48bf8c61e8a4d3ef0d5b2dadd7a954cdccb7d2ad05f702e1a rejects
restart30/global velocity30. Reuse unchanged controlled-fill local C/Python inverse
and exact macro P2 interpolation. Replace global polynomial30 by sparse distributed
macro vertex/edge velocity modes. Balanced inverse E+(I-EA)G(I-AE), E=Z(Ac)^-1 Z^T.
Physical complete original P4/DG-P3/degree6 action, all pressure modes/load/boundaries
and exact condensation/reconstruction stay. Keep pressure-ten complement10 and
restart30, complete actual V/Z/work, full1e-10/retained1e-11 and original physical
force/flux/div1e-8/energy.03/atomic publication authority. No coarse diagonal shift.

Assemble Ac=Z^T A Z while original local mixed Schurs are already live; retain
physical-global-upper orientation when mirroring local velocity blocks. Only local
<=102x30 projection, <=64macro COO batches, hierarchical sparse accumulation. Check
local symmetry roundoff<=1e-10; averaging restores only coarse numerical symmetry.
Original physical assembly remains byte-identical, independently verified. Avoid
unbounded global A@Z or dense Nv-by-coarse columns. Essential projection injective
and exactly vanishes at original walls/body.

Before interpolation, conservative construction reservation512*nt+128*tet+32*nv+
32MiB, freshRSS+reservation+32MiB<=1800. Coarse hierarchy all-live numeric arrays and
predicted merge bound<=256MiB, reserve>=1MiB projection temporary, freshRSS+2*predicted
newCSR+32MiB guard before each sparse merge. Original resource checks after every
64macro batch and interpolation/coarse completion; owned peak never reset.

Separate byte-preserved Double workspace C library prepares exact coarse SYMBOLIC
only; encoded Float/library ABI remains separate and source/build-bound. Combined
fresh stage admission BEFORE either numeric factor includes local72*pattern_blocks+
1024, exact coarse factor bytes, max(coarse numeric scratch,32*nv), actualRSS,
both restart30 bases/work, pressure coarse reserve, distributed8*(20*nv+16*nc)+2MiB
work and32MiB. No double count omission of inputs; interpolation/coarse inputs are
already measured live. Validate actual coarse solve-workspace fits reserved work.
Both factors/inputs/coarse/interpolation retired before full FE reconstruction.

Independent support: local vs full global Galerkin and bitwise original physical
assembly on anisotropic FE/nonzero loads/pressure/reconstruction, dense balanced
formula/SPD/exact coarse action, real-factor symmetry/positivity/original operator,
injective boundary interpolation, source-transform and separated Double/Float
ABI, invalid/budget/partial/repeated owner cleanup. Reuse earlier factor/P2 evidence.
Full setup checks three fixed random coarse action/reproduction vectors against
original Float64 physical velocity; these are sampled diagnostics, not a spectrum
certificate. One small4992tet full control first: all full numerical/output identity
and separate force/scalar equivalence, whole<=2*11.750901166s. Only useful small
permits one matched28416tet L4, whole<=1.25*85.313668042s and>=300MiB complete new
velocity factor/work saving versus873198472. Only useful large proof permits saved
finer43008tet L4 under same fresh caps. Preserve all failures; no unchanged retries
or1800MiB/180s/3000iter/50000tet/target relaxation, native/default adoption or force
certification. Broader user goal active.
