# Accuracy first: resume physical reference convergence

User steering: physical CFD accuracy precedes speed/efficiency. Recent performance investigations do not qualify physical behavior. Preserve
all previous failures and recipes; stop gating physics on23.50s/106.64s/300MiB savings.
New bounded local lane:3072MiB/600s/3000iterations/50000tet, on verified16GiB host.
Keep fullFE1e-10/retained1e-11, flux/div1e-8, energy.03 and raw1%force/component
convergence gates. All currentRSS+factor/scratch/bothVZ/pressure+32MiB admissions and
pre/postserialization atomic checks remain. No native/default/package changes.

Use the previously qualified complete Float Cholesky PC against original Double
P4/DG-P3 FE action; no new PC work required first. Saved dual-domain floor-balanced8
geometry passed quality/symmetry/area/body-edge screening and is immutable. Solve
43008tetL4 once; independently verify full residual/field, separate rawpressure/
viscous/reaction/Pin/D, flux, divergence and energy against prior qualified L4.
If accepted numerical field exists, solve49920tetL8 once and compare both local
resolution sensitivity and domain dependence. Report each original1%force/3%energy
failure; no corrected force, domain subtraction or tiny residual substitutes.

This is steady low-Re Stokes reference qualification. It does not certify transient,
advective inertial, wake, turbulence, moving body or free surface physics. After
reference qualification, use exact projection to isolate native pressure/operator
versus traction errors, then native scene contracts and physical transient low-Re
transport. Broader object/model extensions require their own known-answer tests.
