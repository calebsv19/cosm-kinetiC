# Pressure proxy refinement: diagnostic improvement, full usefulness rejection

Three added independent controls pass. Same originalphysical/P3/local/Floatfactors,
CG8 velocity andpressureten/complement10; onlypressurecolumn inverse applies3fixed
balancedphysical residualcorrections. New48nv+24nc work explicitly reserved before
both numeric allocations, completeouterVZ andpressure reserve retained.
One paired original4992tet diagnostic bundle50dd3000b028fc24be499d8d474e61a2028565f9e5750c465c9fd634b0502d42,
14.05075s/sampled860.953125MiB, originalinputs/owners/pressuregates pass. Projected
Kerror.175094->.120009 (ratio.685396), SchurWerror.192569->.137236 (ratio.712658).
Both meet<=.75prospectiveeligibility. Newskew1.2005e-13, sampledtencondition
5.28741->4.88497. Newpressurecolumn1.47318s versusold.48941s; diagnostic only.

One full original4992tet bundle2cdc7c452ceb8eeac03bda60524b03f1061b0322b8e0fff8d28e2e631f7a6d58,
L4-body2-original-pressure-refine3-receipt.json:114iter/24.9088959591s,
owned402.515625MiB/sampled400.71875MiB/full9.00776247e-12. Originalmatrix/RHS/Float
identity/forces/Pin/D<=1e-7 and all originalflux/div/physicalenergy/resource/atomicfield/
completedownerretirement checks pass. Setup3.18416s/solve11.14055s: improvedpressure
proxy accuracy does NOT reduce outeriterations versus113CG8 and costs more. Strict
field valid, butunchangedwhole23.5018023327s usefulness fails. No larger/finertrial,
native/default/acceptedrecipe/physicalcertification. Broadergoalactive.

Nextdistinct hypothesis: augment pressureten by SIX fixed sampledpressure directions
from originalDoubleSchur58-subspace, keepingconstant/tenpolynomialspan/complement10.
Lowest samplemode tenenergy3.339percent/end55.85/wall64.00/outside-span residual.6466:
coveragegap motivates testing butisnot a full spectrum/stalledmode certificate.
Project selectedsampledweakvectors outsideoldten in coefficientspace, orthonormalize,
freeze their58-coefficient vectors andprescribedbasis names. Transportonly through
same prescribedgeometry/massbasis; anyrank/name mismatch rejects. IndependentSPD/
rank/coverage/pressuregates andone paired originalcolumn control beforeone fullfield.
Full originalresidual/resource/cost/forcegates stay; no newphysicalpressurepenalty.
