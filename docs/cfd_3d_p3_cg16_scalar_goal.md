# Sixteen inner CG steps with unchanged filled factor and cubic coarse action

Predecessor6368e3d5e33e73079b2f22d7642cfb1bf4890c9f73294c23d5574fe7055304e3
strict9.014e-12/113outer but whole23.6356s misses23.5018s. No repeated timing.
One numerical change: fixed inner velocityCG8cap doubledto16, same original
Float64 velocity/localfilledfactor/startzero/positivecurvature/gauges/pressureten
fixedtriple/P3Floatthreecorrections/onepasscoarseaction/restart30. No extraCGbasis/
matrix/factor/newworkarrays; same40nv+24ncwork and both actual V/Z/work reserved.
Hypothesis lowerouter iterations/coarse solves outweigh doubledinner work; no
assumed improvement. Distinct source literaltransform proves only loopcap and
corresponding names/metadata. Independently compare transformed SciPyCG16 and
physicalFE/pressurefixedproxy/homogeneity/partialcleanup; all older support retained.

Original4992tet full first; full1e-10/retained1e-11, bitwise originalphysical/RHS/
Floatpredictor, separateforces/Pin/D<=1e-7 equivalence, originalflux/div1e-8/energy.03/
atomicfield/1800MiB/180s/3000iter/50000tet. Smallwhole<=23.5018023327s only permits
exactmatched28416tet L4 withwhole<=106.642085s and>=300MiB complete velocityfactor/
work saving vs873198472bytes, fulloriginalidentity/forceequivalence. Only useful
matched permits savedquality43008tet finerL4. Any field/workspace/admission checks
hold; no cap/gate relaxation or native/default/mesh/forcecertification. Broadgoalactive.
