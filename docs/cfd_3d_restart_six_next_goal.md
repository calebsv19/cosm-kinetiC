# Bound observation reuse before another matched force solve

Preserve restart6/8/4 time failures and all sources. Full equations, P4/DG-P3 physical
pressure modes, original fullFE residual1e-10, flux/div1e-8, energy.03, all original
order4/8 traction and both consistency lifts remain mandatory. Resource caps remain
1800MiB/180s/3000iterations/50000tet and32MiB reserve plus actual basis/work.

Inspect measured postsolve cost on an already accepted same-surface L4 field.
Declare a separate fused bounded volume observer that reuses only exact identical
per-chunk velocity basis/gradients for volume metrics and volume consistency lifts.
Retain original pressure interpolation, all integrals, quadrature, chunk order,
independent boundary checks and old implementation as an oracle. No field cache
beyond a chunk, no physical operator/residual changes, no pressure-space changes.
Prove independent affine manufactured observations and actual saved-field equality,
source/input immutability and lower measured cost. If useful, isolated reference
probes may use it with unchanged restart6 and same exact L8 stage/live checks.
Only completed all-gate acceptance enables matched saved-mesh force comparisons
and signed observer. Default/native/desktop changes remain outside this bound.
