# Bounded native Fire–Fluid diagnostic controls

`coupling.overnight_cases.configuration(workers,name)` supplies named finite-gust,
wind-removal,buoyancy,balanced positive/reversing-X,object-placement and inlet-
aperture controls. All share the authored native Fire source and inert law.
`SMALL`lists8cases;`EXPENSIVE`lists4matched32³/64³or signed-wind refinements.

Physical source cutoff remains1.6s; runs end6.4s. Solver steps remain Fire.02s,
gas.005s.32³/64³spatial pair uses.2s exchange;32³.4s predecessor supplies the
matching exchange-timing comparison. Source feedback remains live. Actual
source budgets must be reported; source output is not amplified to make images.

Channel controls close Y/top;balanced-X controls use ambient Y/open-top. Compare
signed reversal with its balanced-positive counterpart; do not attribute channel
versus balanced boundary changes solely to wind sign. Object placement changes
only the authored boxYposition;aperture changes only named inletYZbounds.

Existing numerical,scalar/CFL/pressure/contrast/bounds and checkpoint gates
apply.64³uses admitted262144cells,512MiBnumericalcap,native20step packets and
existing lossless1GiBjournal selection; this is capacity selection,not relaxed
physics acceptance. No convergence/material/hot-fire qualification is implied.

Tests:`tests/test_overnight_cases.py` checks admission,unchanged source/object
law,physical-time cutoff,matched spatial cadence and exact reversal pairing.
Native outcomes and current limitations are in `current_truth.md`;the bounded
campaign is complete: 12 cases, 236 checkpoints, 48 native snapshots and 53
aggregate checks pass. Matched 32³/64³ spatial sensitivity remains 28.74%;
fixed-input transport isolation is next. Physical solver bytes are unchanged. No default/app/release activation occurs.
