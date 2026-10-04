# Independent empty-duct baseline after local geometry rejection

Preserve all16signed-edge rejections and accepted cube fields. Use existing continuous
rectangular-duct Fourier reference from src/app/cfd_duct3d.c/docs/cfd_3d_goal.md and
independently verify its conductance with a double-sine-mode sum and explicit
one-dimensional tail bound. Same2x2m cross-section,mu.1Pa s,Q.008m3/s and natural
inlet/outlet pressure/side-wall no-slip boundary as the cube reference; no obstacle.

Declare separate original P4/DG-P3 empty box probes on uniform cubical macros:
L4/L8 at n4 then n6 cross-section,dx=dy=dz=2/n. Preserve original exact condensation,
full physical equations/load/pressure modes, Float coupled factor, flexible6,
retained1e-11/full1e-10, independent reconstruction, flux/maxdiv1e-8,energy.03 and
1800MiB/180s/3000iterations/50000tet plus actual both-basis/work/factor/scratch/
32MiBreserve admission. Empty box has no body-force/traction/lift checks: classify
those not applicable, not passed. Keep cube observers and source immutable.

Compare pressure drop and dissipation to independent analytical values, pressure
field to analytical linear profile, and n4→n6 refinement. Baseline pressure,
dissipation/field accuracy gate.1% is separately declared; numeric field acceptance
is not that certificate. Only complete calibrated baselines may define diagnostic
obstacle-excess pressure/dissipation by subtracting baseline at the same length.
Retain raw cube totals, original25%scalar failures,1.7567%raw force gap and all
original physical1%qualification gates. No retrospective gate replacement or
native certification. Continue quality-controlled fixed-domain body resolution
once baseline separates wall-length effects from the remaining body-stress error.
