# Bounded nested macro-quadratic velocity coarse control

Macro-linear balanced correction passes the original but removes only 8.63% of
iterations and raises time 24.06%; reject larger extension. The nested macro P2
coarse space adds global edge modes to represent more of the slow momentum error.
Use original macro vertices/edge midpoints and exact P2 interpolation onto the
original P4 trace; shared global edge identity/orientation must agree. Prove
constant, affine and quadratic reproduction, vertex/edge nodal injection and
projected injectivity/essential boundary exclusion on anisotropic/refined meshes.

Reuse the fixed balanced SPD inverse and exact Galerkin coarse factor, with the
same complete coupled block sweep. Dense/FE proofs must establish Galerkin action,
symmetry/linearity/positivity, exact coarse action and original mixed/RHS/pressure/
reconstruction preservation plus owned cleanup. No physical coefficient, pressure
mode, equations, residual/force/resource criterion changes; no floor/shift/scaling.

Original L4 one local sweep first; only useful accepted original controls may
extend to admitted count6 base and exact stopped 23616-tet normal. Existing chunk
size 512 may be used with measured cost/headroom. Retain all failures; cap at
50000 tets / 1800 MiB / 180 s / 3000 iterations. Report target misses explicitly.
Record coarse dimension/factor bytes and complete setup/solve/reconstruction/
diagnostics/publication cost. Normal numerical admission enables subsequent force
refinement, not physical certification. Stage 1 and full object/wind-tunnel goal
remain open. No native/shared API/dependency/version/commit/package/install/deploy
change; generic extraction remains deferred.
