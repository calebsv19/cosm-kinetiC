# Fixed-pattern block IC0 plus balanced global velocity modes

After finer-mesh audit953dff4912b324678289d5d059e257ea15eec68495d0b0c819ca93cb2491b089,
retain287.527MiB admission gap/all rejected graph/ILU/AMG/cubic-velocity evidence.
New PC only: 3x3 block incomplete Cholesky in original node graph, no fill, using
same Float predictor as old factor but Double factor arithmetic/storage. Natural
node ordering, fixed complete original graph; missing update locations drop only
PC fill, never physical coefficients. A failed3x3 pivot receives ONE declared
PC-only Gershgorin shift plus.05*block maximum magnitude; if finite positive pivot
still fails, reject. No physical diagonal/equation/pressure/residual alteration.
Record every compensation count/sum/max and positive diagonal. Exact model L L^T
is SPD; Float input and approximate action do not certify physical spectrum/error.

Add balanced global velocity coarse E+(I-EA)G(I-AE), with all ten quadratic scalar
modes times each component(30columns). Nodal coordinate functions vanish at walls
and cube via y(2-y)z(2-z) and capped exterior body distance. Fixed FE-free vector
space, mass-free QR; same actual Float64 physical velocity action produces coarse
columns and exact dense coarse block. Rank/skew/positive pivots/coarse reproduction
must pass; complementary physical vector space remains. No auxiliary operator or
solution-dependent basis. Keep pressure balanced-ten complement10.

Known IC storage8*9*upper_blocks+bounded handle reserve, solve scratch8*nv. Reserve
8*(4*nv*30+20*nv+8*30^2)+2MiB for velocity coarse construction/work, beyond original
both Arnoldi bases/work, pressure coarse, current RSS and32MiB. Fresh admission
before C factor/QR allocations, caller-owned lifecycle/highwater unchanged. C
physical prefix remains byte-identical to sealed encoded kernel. Full Float64
mixed action/all P4/DG-P3 modes/degree6 quadrature/reconstruction remain authority.
1800MiB/180s/3000iter/50000tet, full1e-10/retained1e-11/flux/div1e-8/energy.03 and
atomic publication/force definitions stay unchanged.

Independent tests: dense coupled exact full-pattern result/coordinates, sparse SPD
model and positive compensation, analytic balanced coarse formula/reproduction/
complement retention, anisotropic FE/nonzero eliminated loads/pressure/full action,
invalid/nonfinite/rank/owned cleanup and reservations. One small original4992tet
full solve first; strict full equivalence and whole<=2*11.750901166s are readiness,
not general adoption. If useful, one matched28416tet L4 control needs whole<=1.25*
85.313668042s and factor+velocity-coarse reservation at least300MiB below old873198472
factor bytes, all full output/identity checks. Only useful complete large control
permits one saved finer43008tet L4 candidate under same fresh/caps. No failed
same-case retries/cap raises/default/native/physical certification. Broad goal active.
