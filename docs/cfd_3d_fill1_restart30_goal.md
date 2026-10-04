# Longer restarted Arnoldi enabled by a cheaper controlled-fill factor

Predecessorec56241628f48e539e41501e9acb2da22ed23404a2bb7a5bba314738d7228bf0 rejects
six-vector solve. Pattern/factor setup is stable and much smaller, but original
momentum residual stalls almost exactly after100iterations. Distinct control ONLY
changes restart6 to30 and its actual complete V/Z/work reservation. Full operator,
RHS, FE/reconstruction, preconditioner C/Python code, degree permutation, selected
fill, pressure/velocity coarse spaces and true-residual cadence remain untouched.
No numerical basis or cap/target relaxation. Native/default60 remains unchanged.

Literal-source transformation proof plus four support controls: independent mixed
solution with varying Float inverse at true1e-11; capture BOTH actual V/Z arrays and
complete reservation; iteration limit is not convergence; unselected restart values
reject before mesh. Reuse sealed eight factor/FE/lifecycle controls without reruns.
Fresh admission uses actual restart30 bases plus full factor/scratch/currentRSS,
pressure and velocity coarse reservations and32MiB. Original1800MiB/180s/3000iter/
50000tet/full1e-10/retained1e-11/flux/div1e-8/energy.03/atomic field gates unchanged.
ONE small4992tet full control first. Require complete numerical and field/force/
scalar equivalence to accepted small reference and whole<=2*11.750901166s. A useful
control permits one matched28416tet L4 with same identity/equivalence, whole<=1.25*
85.313668042s, >=300MiB factor plus new velocity-work saving versus873198472bytes.
Only useful large proof permits one saved quality-passed43008tet finer L4 admission.
No retry of failed unchanged restart6 or failed geometry, physical/native/general
adoption or force convergence claim. Broader goal active.
