# Cubic global pressure correction rejected on exact cube time cap

Four independent support tests pass: all twenty polynomial directions including
constant/quadratic span, balanced SPD/symmetry/coarse reproduction and complement
retention, synthetic coupled Schur sign/input preservation and exact20-column
reserve, and invalid/rank-deficient rejection. This is global PRESSURE correction,
not the earlier rejected macro-cubic velocity hierarchy; both failures are retained.

Small4992tet original L4 completes246iterations/14.816s/owned422.141MiB with full
original FE8.439e-12 and original mesh/matrix/load/Float identity. p/rawv/r/Pin/D
relative changes<4e-11; numerical/conservation/restoration/publication pass. Its
iteration count worsens versus ten-column200; not promoted as a performance win.

One declared exact33216tet L8 target admits fresh numeric stage with revised
20-column reserve and original factor/scratch/currentRSS/bothbases/work/32reserve.
It reaches iterative-solve completion129.399s, workspace cleanup129.929s and full
residual phase marker166.882s, then is terminated at180.035s by original180s cap.
Sampled1535.313MiB stays under1800MiB, but time is rejected. Last logged iteration500
has retained momentum3.029e-12 versus constant continuity4.506e-11 (original RHS
scaling), so continuity remains slower. Phase marker supplies no saved full residual
value/complete physical checks; no result/field is published. No full-target or
physical success is inferred from that marker. NO adoption/retry/cap increase.

The exact ten-direction390iter/156.923s/1520.375MiB field remains the numerical
anchor, with8.45percent benefit insufficient for10percent adoption and raw force
1.757percent still failed. More polynomial directions did not improve this solver.
Next distinct hypothesis is one fixed Double-physical residual correction of each
Float velocity PC solve, including Schur-column construction, to measure sensitivity
to approximate velocity-inverse accuracy. Original physical coefficients/action/
full FE/modes/resources stay unchanged. Native/default/desktop and broader Stage1
qualification remain open. Evidence build/c3d-pressure-cubic, frozen sources,
small receipt/field, terminal cap receipt and once-only checkpoint audit.
