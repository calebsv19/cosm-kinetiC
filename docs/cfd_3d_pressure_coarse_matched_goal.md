# Matched L8 after two actual paired pressure controls pass

Original pair iterations260→216, normal290→216. Whole costs12.200→11.630s and
86.438→73.074s. Same original physical identities and Float input; separate force,
Pin/D differences<1.5e-10. Both declared cost/equivalence gates pass, recorded in
build/c3d-pressure-coarse/matched-eligibility.json before this matched run.

One predeclared quadratic-pressure run on exact accepted33216tet L8 second-normal
held-outer2 mesh, with unchanged complete physics, all modes, Float velocity PC,
flexible6 retained1e-11/full1e-10, full reconstruction and original physical checks.
Explicit added coarse reservation participates in fresh actual numeric admission.
All1800MiB/180s/3000iteration/50000tet and flux/div/energy gates unchanged.
No retry/cap increase on failure. Compare exact identity and component/Pin/D
numerical equivalence<1e-7 to prior accepted retained-margin L8. Disclose historical
rather than freshly paired long-domain cost baseline. Require iterations or setup+
solve improve>=10%, whole and owned peak no worse>10% as for controls, plus strict
numerical acceptance. Only a useful complete matched result is adopted as optional
reference functionality. Preserve default mass/restart60, all old force/rawtotal
failures and native/package boundaries. Coarse spectrum/reproduction do not certify
full pressure nullspace or physical forces. Broad Stage1/general goal remains active.
