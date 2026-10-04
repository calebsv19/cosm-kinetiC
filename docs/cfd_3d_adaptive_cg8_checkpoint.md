# Adaptive inner velocity solve: work saved, accuracy rejected

Four support tests pass. The one frozen 4,992-tetrahedron paired diagnostic
completed in 14.0799 seconds with 822.766 MiB sampled peak RSS. All original
physical inputs and the Float predictor match the qualified control; pressure
construction keeps the unchanged fixed linear proxy, ten directions and scale 10.
All diagnostic factor owners retire. No flow field was published.

The cap-eight inner velocity action used a preconditioned residual proxy of 0.25
for early stopping. Across the same ten pressure-column loads it reduced actual
local factor applications from 80 to 58 and velocity applications from 80 to 48.
The work ratio 0.725 passes the prospective 0.75 gate. Against the complete Double
velocity inverse, however, energy errors increased by factors 1.070 to 1.448.
Five columns exceeded the prospective 1.25 accuracy limit. The candidate is
rejected before a full flow solve; neither the proxy threshold nor its gate is
adjusted after measurement. No larger or finer trial is permitted.

The original full-equation residual target, pressure modes, resource limits and
raw force/energy failures remain authoritative. The persistent CFD goal remains
active. The next distinct investigation should improve the local velocity factor
at the same graph and storage cost, rather than buy accuracy with more inner
steps or pressure directions.
